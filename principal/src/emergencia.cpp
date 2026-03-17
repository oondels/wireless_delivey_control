/**
 * emergencia.cpp — Implementação da lógica de emergência
 *
 * Botão EMERGÊNCIA é do tipo com trava mecânica: sinal LOW enquanto
 * travado (pull-up externo). O firmware lê nível contínuo, não borda.
 *
 * Uma vez _ativa = true, a flag NUNCA é limpa automaticamente.
 * Apenas a classe Rearme pode limpá-la.
 *
 * Ref: seguranca/SPEC.md §3
 */

#include "emergencia.h"

void Emergencia::init() {
    pinMode(PIN_BTN_EMERGENCIA, INPUT);
    _ativa = false;
}

bool Emergencia::botaoLocalAtivo() const {
    // LOW = pressionado/travado (pull-up externo)
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
    }

    return _ativa;
}
