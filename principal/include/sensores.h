/**
 * sensores.h — Leitura do sensor fim de curso com debounce
 *
 * Debounce de 20 ms via millis() (não-bloqueante).
 * Lógica: LOW = sensor acionado (pull-up externo).
 *
 * Ref: motor/SPEC.md §5
 */

#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>
#include "pinout.h"

class Sensores {
public:
    static constexpr uint32_t DEBOUNCE_FIM_CURSO_MS = 20;

    void init();
    bool fimDeCursoAcionado();

private:
    bool     _estadoFiltrado  = false;
    bool     _ultimaLeitura   = HIGH;
    uint32_t _ultimoCambioMs  = 0;
};

#endif // SENSORES_H
