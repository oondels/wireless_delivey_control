/**
 * emergencia.cpp — Implementação da lógica de emergência
 *
 * Botão EMERGÊNCIA é do tipo com trava mecânica: sinal LOW enquanto
 * travado (pull-up externo). O firmware lê nível contínuo, não borda.
 *
 * Uma vez emergencia_ativa = true, a flag NUNCA é limpa automaticamente.
 * Apenas o módulo de rearme pode limpá-la (T-009).
 *
 * Ref: seguranca/SPEC.md §3
 */

#include <Arduino.h>
#include "pinout.h"
#include "emergencia.h"

volatile bool emergencia_ativa = false;

void emergencia_init() {
    pinMode(PIN_BTN_EMERGENCIA, INPUT);
    emergencia_ativa = false;
}

bool emergencia_botao_local_ativo() {
    // LOW = pressionado/travado (pull-up externo)
    return (digitalRead(PIN_BTN_EMERGENCIA) == LOW);
}

bool emergencia_verificar(bool emergencia_remote) {
    // Se já está em emergência, mantém (nunca limpa automaticamente)
    if (emergencia_ativa) {
        return true;
    }

    // Verifica botão local ou sinal do Remote
    if (emergencia_botao_local_ativo() || emergencia_remote) {
        emergencia_ativa = true;
    }

    return emergencia_ativa;
}
