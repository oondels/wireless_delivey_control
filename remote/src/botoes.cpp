/**
 * botoes.cpp — Implementação da leitura de botões do Remote
 *
 * Debounce genérico via millis(). SUBIR/DESCER retornam nível (hold).
 * VEL1/VEL2/RESET retornam pulso (borda HIGH→LOW, consumido após leitura).
 * EMERGÊNCIA retorna nível contínuo (NC: HIGH = ativo, contato aberto).
 *
 * Ref: hardware_io/SPEC.md §6
 */

#include "botoes.h"

const uint8_t Botoes::_pinos[NUM_BOTOES] = {
    PIN_BTN_SUBIR, PIN_BTN_DESCER,
    PIN_BTN_VEL1, PIN_BTN_VEL2, PIN_BTN_RESET,
    PIN_BTN_EMERGENCIA
};

void Botoes::init() {
    for (int i = 0; i < NUM_BOTOES; i++) {
        if (_pinos[i] == PINO_DESABILITADO) {
            _ultimaLeitura[i]  = HIGH;
            _estadoFiltrado[i] = HIGH;
            _ultimoCambio[i]   = 0;
            continue;
        }

        // GPIOs 13, 32, 33: pull-up interno; GPIOs 34-39: pull-up externo (input-only)
        if (_pinos[i] == 13 || _pinos[i] == 32 || _pinos[i] == 33) {
            pinMode(_pinos[i], INPUT_PULLUP);
        } else {
            pinMode(_pinos[i], INPUT);
        }
        // Botão NC (emergência) em repouso: contato fechado → LOW; demais botões: HIGH
        _ultimaLeitura[i]  = (i == IDX_EMERGENCIA) ? LOW : HIGH;
        _estadoFiltrado[i] = (i == IDX_EMERGENCIA) ? LOW : HIGH;
        _ultimoCambio[i]   = 0;
    }
}

EstadoBotoes Botoes::ler() {
    uint32_t agora = millis();
    bool estadoAnterior[NUM_BOTOES];

    // Salvar estado anterior e atualizar debounce
    for (int i = 0; i < NUM_BOTOES; i++) {
        estadoAnterior[i] = _estadoFiltrado[i];

        bool leitura = (_pinos[i] == PINO_DESABILITADO) ? HIGH : digitalRead(_pinos[i]);
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

    // VEL1/VEL2/RESET: pulso — borda de descida (HIGH → LOW)
    resultado.vel1_pulso  = (estadoAnterior[IDX_VEL1]  == HIGH && _estadoFiltrado[IDX_VEL1]  == LOW);
    resultado.vel2_pulso  = (estadoAnterior[IDX_VEL2]  == HIGH && _estadoFiltrado[IDX_VEL2]  == LOW);
    resultado.reset_pulso = (estadoAnterior[IDX_RESET]  == HIGH && _estadoFiltrado[IDX_RESET] == LOW);

    // EMERGÊNCIA: nível contínuo (NC: HIGH = botão pressionado, contato aberto)
    resultado.emergencia = (_estadoFiltrado[IDX_EMERGENCIA] == HIGH);

    return resultado;
}
