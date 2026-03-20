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
    digitalWrite(PIN_RELE_FREIO_ON, LOW);
    digitalWrite(PIN_RELE_FREIO_OFF, LOW);
    acionar(); // Estado padrão: freio aplicado
}

void Freio::acionar() {
    if (digitalRead(PIN_RELE_FREIO_ON) == LOW) {
        LOG_INFO("FREIO", "Acionando freio");
        digitalWrite(PIN_RELE_FREIO_OFF, LOW);
        delay(10);
        digitalWrite(PIN_RELE_FREIO_ON, HIGH);
    }
}

void Freio::liberar() {
    if (digitalRead(PIN_RELE_FREIO_ON) == HIGH) {
        LOG_INFO("FREIO", "Liberando freio");
        digitalWrite(PIN_RELE_FREIO_ON, LOW);
        delay(10);
        digitalWrite(PIN_RELE_FREIO_OFF, HIGH);
    }
}
