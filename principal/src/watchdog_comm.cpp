/**
 * watchdog_comm.cpp — Implementação do watchdog de comunicação
 *
 * Ref: seguranca/SPEC.md §4
 */

#include <Arduino.h>
#include "protocolo.h"
#include "watchdog_comm.h"

volatile uint32_t ultimo_pacote_remote_ms = 0;

void watchdog_comm_init() {
    ultimo_pacote_remote_ms = millis();
}

void watchdog_comm_resetar() {
    ultimo_pacote_remote_ms = millis();
}

bool watchdog_comm_expirado() {
    return (millis() - ultimo_pacote_remote_ms) > WATCHDOG_TIMEOUT_MS;
}
