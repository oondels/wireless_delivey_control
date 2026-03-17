/**
 * sensores.cpp — Implementação da leitura do fim de curso
 *
 * Debounce via millis(): o estado só muda após permanecer
 * estável por DEBOUNCE_FIM_CURSO_MS (20 ms).
 *
 * Ref: motor/SPEC.md §5
 */

#include <Arduino.h>
#include "pinout.h"
#include "sensores.h"

static bool     estado_filtrado    = false;  // false = não acionado
static bool     ultima_leitura     = HIGH;
static uint32_t ultimo_cambio_ms   = 0;

void sensores_init() {
    pinMode(PIN_FIM_DE_CURSO, INPUT);
    estado_filtrado  = false;
    ultima_leitura   = HIGH;
    ultimo_cambio_ms = 0;
}

bool fim_de_curso_acionado() {
    bool leitura = digitalRead(PIN_FIM_DE_CURSO);

    if (leitura != ultima_leitura) {
        ultimo_cambio_ms = millis();
        ultima_leitura = leitura;
    }

    if ((millis() - ultimo_cambio_ms) >= DEBOUNCE_FIM_CURSO_MS) {
        // LOW = acionado (pull-up externo)
        estado_filtrado = (ultima_leitura == LOW);
    }

    return estado_filtrado;
}
