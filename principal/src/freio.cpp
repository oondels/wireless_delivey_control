/**
 * freio.cpp — Implementação do controle do relé de freio
 *
 * Ref: motor/SPEC.md §4
 */

#include "freio.h"

void Freio::init() {
    pinMode(PIN_RELE_FREIO, OUTPUT);
    acionar(); // Estado padrão: freio aplicado
}

void Freio::acionar() {
    digitalWrite(PIN_RELE_FREIO, HIGH);
}

void Freio::liberar() {
    digitalWrite(PIN_RELE_FREIO, LOW);
}
