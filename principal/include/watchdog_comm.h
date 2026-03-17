/**
 * watchdog_comm.h — Watchdog de comunicação com o Remote
 *
 * Timeout: 500 ms (WATCHDOG_TIMEOUT_MS em protocolo.h).
 * Se nenhum pacote válido recebido no timeout → FALHA_COMUNICACAO.
 *
 * Ref: seguranca/SPEC.md §4
 */

#ifndef WATCHDOG_COMM_H
#define WATCHDOG_COMM_H

#include <Arduino.h>
#include "protocolo.h"

class WatchdogComm {
public:
    void init();
    void resetar();          // Chamar ao receber pacote válido
    bool expirado() const;   // true se timeout excedido

private:
    volatile uint32_t _ultimoPacoteMs = 0;
};

#endif // WATCHDOG_COMM_H
