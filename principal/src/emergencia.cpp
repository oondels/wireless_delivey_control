/**
 * emergencia.cpp — Implementação da lógica de emergência
 *
 * Botão EMERGÊNCIA é do tipo com trava mecânica: sinal LOW enquanto
 * travado (pull-up interno via INPUT_PULLUP). O firmware lê nível contínuo, não borda.
 *
 * Uma vez _ativa = true, a flag NUNCA é limpa automaticamente.
 * Apenas a classe Rearme pode limpá-la.
 *
 * Ref: seguranca/SPEC.md §3
 */

#include "emergencia.h"
#include "logger.h"

void Emergencia::init() {
    pinMode(PIN_BTN_EMERGENCIA, INPUT_PULLUP);  // GPIO 33 — pull-up interno
    _ativa = false;
}

bool Emergencia::botaoLocalAtivo() const {
    // LOW = pressionado/travado (pull-up interno)
    return (digitalRead(PIN_BTN_EMERGENCIA) == LOW);
}

bool Emergencia::verificar(bool emergenciaRemote) {
    // Se já está em emergência, mantém (nunca limpa automaticamente)
    if (_ativa) {
        return true;
    }

    // Verifica botão local ou sinal do Remote
    if (botaoLocalAtivo() || emergenciaRemote) {
        _ativa = true;
        if (botaoLocalAtivo()) {
            LOG_WARN("EMERG", "Emergencia ATIVADA — botao local pressionado");
        }
        if (emergenciaRemote) {
            LOG_WARN("EMERG", "Emergencia ATIVADA — sinal do Remote");
        }
    }

    return _ativa;
}
