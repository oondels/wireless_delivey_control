/**
 * watchdog_comm.h — Watchdog de comunicação com o Remote
 *
 * Timeout curto: 500 ms (WATCHDOG_TIMEOUT_MS em protocolo.h).
 * Se nenhum pacote valido recebido no timeout, o movimento remoto e bloqueado.
 * A emergencia por perda prolongada usa timeout separado configuravel.
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
    uint32_t tempoSemPacoteMs() const;

private:
    volatile uint32_t _ultimoPacoteMs = 0;
};

#endif // WATCHDOG_COMM_H
