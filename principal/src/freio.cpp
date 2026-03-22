/**
 * freio.cpp — Implementação do controle do freio solenoide de dupla bobina
 *
 * Ref: motor/SPEC.md §4
 */

#include "freio.h"
#include "logger.h"

void Freio::init() {
    pinMode(PIN_RELE_FREIO_ON, OUTPUT);
    pinMode(PIN_RELE_FREIO_OFF, OUTPUT);
    digitalWrite(PIN_RELE_FREIO_ON, HIGH);
    digitalWrite(PIN_RELE_FREIO_OFF, HIGH);
    acionar(); // Estado padrão: freio aplicado
}

void Freio::acionar() {
    if (digitalRead(PIN_RELE_FREIO_ON) == HIGH) {
        LOG_INFO("FREIO", "Acionando freio");
        digitalWrite(PIN_RELE_FREIO_OFF, HIGH);
        delay(10);
        digitalWrite(PIN_RELE_FREIO_ON, LOW);
    }
}

void Freio::liberar() {
    if (digitalRead(PIN_RELE_FREIO_ON) == LOW) {
        LOG_INFO("FREIO", "Liberando freio");
        digitalWrite(PIN_RELE_FREIO_ON, HIGH);
        delay(10);
        digitalWrite(PIN_RELE_FREIO_OFF, LOW);
    }
}
