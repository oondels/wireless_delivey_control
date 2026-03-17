/**
 * watchdog_comm.cpp — Implementação do watchdog de comunicação
 *
 * Ref: seguranca/SPEC.md §4
 */

#include "watchdog_comm.h"

void WatchdogComm::init() {
    _ultimoPacoteMs = millis();
}

void WatchdogComm::resetar() {
    _ultimoPacoteMs = millis();
}

bool WatchdogComm::expirado() const {
    return (millis() - _ultimoPacoteMs) > WATCHDOG_TIMEOUT_MS;
}
