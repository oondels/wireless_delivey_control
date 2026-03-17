/**
 * freio.cpp — Implementação do controle do relé de freio
 *
 * Ref: motor/SPEC.md §4
 */

#include <Arduino.h>
#include "pinout.h"
#include "freio.h"

void freio_init() {
    pinMode(PIN_RELE_FREIO, OUTPUT);
    acionar_freio(); // Estado padrão: freio aplicado
}

void acionar_freio() {
    digitalWrite(PIN_RELE_FREIO, HIGH);
}

void liberar_freio() {
    digitalWrite(PIN_RELE_FREIO, LOW);
}
