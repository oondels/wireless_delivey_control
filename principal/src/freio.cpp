/**
 * freio.cpp — Implementação do controle do relé de freio
 *
 * Ref: motor/SPEC.md §4
 */

#include "freio.h"
#include "logger.h"

void Freio::init() {
    pinMode(PIN_RELE_FREIO, OUTPUT);
    acionar(); // Estado padrão: freio aplicado
}

void Freio::acionar() {
    if (digitalRead(PIN_RELE_FREIO) == LOW) {
        LOG_INFO("FREIO", "Acionando freio");
    }
    digitalWrite(PIN_RELE_FREIO, HIGH);
}

void Freio::liberar() {
    if (digitalRead(PIN_RELE_FREIO) == HIGH) {
        LOG_INFO("FREIO", "Liberando freio");
    }
    digitalWrite(PIN_RELE_FREIO, LOW);
}
