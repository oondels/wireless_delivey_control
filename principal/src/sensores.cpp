/**
 * sensores.cpp — Implementação da leitura do fim de curso
 *
 * Debounce via millis(): o estado só muda após permanecer
 * estável por DEBOUNCE_FIM_CURSO_MS (20 ms).
 *
 * Ref: motor/SPEC.md §5
 */

#include "sensores.h"

void Sensores::init() {
    pinMode(PIN_FIM_DE_CURSO, INPUT);
    _estadoFiltrado = false;
    _ultimaLeitura  = HIGH;
    _ultimoCambioMs = 0;
}

bool Sensores::fimDeCursoAcionado() {
    bool leitura = digitalRead(PIN_FIM_DE_CURSO);

    if (leitura != _ultimaLeitura) {
        _ultimoCambioMs = millis();
        _ultimaLeitura = leitura;
    }

    if ((millis() - _ultimoCambioMs) >= DEBOUNCE_FIM_CURSO_MS) {
        // LOW = acionado (pull-up externo)
        _estadoFiltrado = (leitura == LOW);
    }

    return _estadoFiltrado;
}
