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

#include <stdbool.h>
#include <stdint.h>

// Timestamp do último pacote válido recebido do Remote
extern volatile uint32_t ultimo_pacote_remote_ms;

void watchdog_comm_init();
void watchdog_comm_resetar();          // Chamar ao receber pacote válido
bool watchdog_comm_expirado();         // true se timeout excedido

#endif // WATCHDOG_COMM_H
