/**
 * sensores.cpp — Implementação da leitura do fim de curso
 *
 * Debounce via millis(): o estado só muda após permanecer
 * estável por DEBOUNCE_FIM_CURSO_MS (20 ms).
 *
 * Bloqueio pós-liberação: após o sensor físico ser liberado, o bloqueio
 * permanece por BLOQUEIO_POS_FIM_CURSO_MS (10 s). O timer inicia na
 * liberação — independente do tempo que o sensor ficou ativo.
 *
 * Ref: motor/SPEC.md §5, seguranca/SPEC.md §6.2
 */

#include "sensores.h"
#include "logger.h"

void Sensores::init() {
    pinMode(PIN_FIM_DE_CURSO, INPUT_PULLUP);  // GPIO 26 — pull-up interno
    _estadoFiltrado      = false;
    _ultimaLeitura       = HIGH;
    _ultimoCambioMs      = 0;
    _ultimaLiberacaoMs   = 0;
}

bool Sensores::fimDeCursoAcionado() {
    bool leitura = digitalRead(PIN_FIM_DE_CURSO);

    if (leitura != _ultimaLeitura) {
        _ultimoCambioMs = millis();
        _ultimaLeitura = leitura;
    }

    if ((millis() - _ultimoCambioMs) >= DEBOUNCE_FIM_CURSO_MS) {
        // LOW = acionado (pull-up interno)
        bool novoEstado = (leitura == LOW);
        if (novoEstado != _estadoFiltrado) {
            if (novoEstado) {
                LOG_WARN("SENSOR", "Fim de curso ACIONADO");
            } else {
                _ultimaLiberacaoMs = millis();
                LOG_INFO("SENSOR", "Fim de curso liberado — bloqueio de 10 s iniciado");
            }
        }
        _estadoFiltrado = novoEstado;
    }

    // Sensor fisicamente acionado → bloqueio imediato
    if (_estadoFiltrado) {
        return true;
    }

    // Sensor liberado, mas ainda dentro do periodo de segurança pos-liberacao
    if (_ultimaLiberacaoMs > 0 &&
        (millis() - _ultimaLiberacaoMs) < BLOQUEIO_POS_FIM_CURSO_MS) {
        return true;
    }

    return false;
}
