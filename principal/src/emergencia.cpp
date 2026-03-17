/**
 * emergencia.cpp — Implementação da lógica de emergência
 *
 * Botão EMERGÊNCIA é do tipo com trava mecânica: sinal LOW enquanto
 * travado (pull-up interno via INPUT_PULLUP). O firmware lê nível contínuo, não borda.
 *
 * Auto-liberação: se nenhuma fonte estiver ativa (botão local solto E
 * emergenciaRemote == false), _ativa é limpa automaticamente.
 * REARME é necessário apenas para sobrepor emergência remote travada.
 *
 * Ref: seguranca/SPEC.md §3
 */

#include "emergencia.h"
#include "logger.h"

void Emergencia::init() {
    pinMode(PIN_BTN_EMERGENCIA, INPUT_PULLUP);  // GPIO 33 — pull-up interno
    _ativa              = false;
    _botaoLocalAnterior = false;  // false = não estava pressionado
}

bool Emergencia::botaoLocalAtivo() const {
    // LOW = pressionado/travado (pull-up interno)
    return (digitalRead(PIN_BTN_EMERGENCIA) == LOW);
}

bool Emergencia::verificar(bool emergenciaRemote) {
    bool localAtivo = botaoLocalAtivo();

    // Log de transição do botão local: ativo→inativo = destravado/solto
    if (_botaoLocalAnterior && !localAtivo) {
        LOG_INFO("EMERG", "Botao emergencia local LIBERADO (destravado)");
    }
    _botaoLocalAnterior = localAtivo;

    // Auto-liberação: se nenhuma fonte ativa, limpa a flag
    if (_ativa && !localAtivo && !emergenciaRemote) {
        _ativa = false;
        LOG_INFO("EMERG", "Emergencia LIBERADA — todas as fontes inativas");
        return false;
    }

    // Ativação: botão local ou sinal do Remote
    if (localAtivo || emergenciaRemote) {
        if (!_ativa) {
            // Loga apenas na borda de ativação
            if (localAtivo) {
                LOG_WARN("EMERG", "Emergencia ATIVADA — botao local pressionado");
            }
            if (emergenciaRemote) {
                LOG_WARN("EMERG", "Emergencia ATIVADA — sinal do Remote");
            }
        }
        _ativa = true;
    }

    return _ativa;
}
