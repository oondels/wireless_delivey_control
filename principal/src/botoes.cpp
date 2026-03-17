/**
 * botoes.cpp — Implementação da leitura de botões do Principal
 *
 * Debounce genérico via millis(). SUBIR/DESCER retornam nível (hold).
 * VEL1/2/3 retornam pulso (borda HIGH→LOW, consumido após leitura).
 * EMERGÊNCIA e REARME são tratados em classes dedicadas.
 *
 * Ref: hardware_io/SPEC.md §4
 */

#include "botoes.h"

const uint8_t Botoes::_pinos[NUM_BOTOES] = {
    PIN_BTN_SUBIR, PIN_BTN_DESCER,
    PIN_BTN_VEL1, PIN_BTN_VEL2, PIN_BTN_VEL3
};

void Botoes::init() {
    for (int i = 0; i < NUM_BOTOES; i++) {
        // GPIOs 32, 33: pull-up interno; GPIOs 34-39: pull-up externo (input-only)
        if (_pinos[i] == 32 || _pinos[i] == 33) {
            pinMode(_pinos[i], INPUT_PULLUP);
        } else {
            pinMode(_pinos[i], INPUT);
        }
        _ultimaLeitura[i]  = HIGH;
        _estadoFiltrado[i] = HIGH;
        _ultimoCambio[i]   = 0;
    }
}

EstadoBotoes Botoes::ler() {
    uint32_t agora = millis();
    bool estadoAnterior[NUM_BOTOES];

    // Salvar estado anterior e atualizar debounce
    for (int i = 0; i < NUM_BOTOES; i++) {
        estadoAnterior[i] = _estadoFiltrado[i];

        bool leitura = digitalRead(_pinos[i]);
        if (leitura != _ultimaLeitura[i]) {
            _ultimoCambio[i] = agora;
            _ultimaLeitura[i] = leitura;
        }
        if ((agora - _ultimoCambio[i]) >= DEBOUNCE_MS) {
            _estadoFiltrado[i] = _ultimaLeitura[i];
        }
    }

    EstadoBotoes resultado = {};

    // SUBIR/DESCER: hold (LOW = pressionado)
    resultado.subir_hold  = (_estadoFiltrado[IDX_SUBIR]  == LOW);
    resultado.descer_hold = (_estadoFiltrado[IDX_DESCER] == LOW);

    // VEL1/2/3: pulso — borda de descida (HIGH → LOW)
    resultado.vel1_pulso = (estadoAnterior[IDX_VEL1] == HIGH && _estadoFiltrado[IDX_VEL1] == LOW);
    resultado.vel2_pulso = (estadoAnterior[IDX_VEL2] == HIGH && _estadoFiltrado[IDX_VEL2] == LOW);
    resultado.vel3_pulso = (estadoAnterior[IDX_VEL3] == HIGH && _estadoFiltrado[IDX_VEL3] == LOW);

    return resultado;
}
