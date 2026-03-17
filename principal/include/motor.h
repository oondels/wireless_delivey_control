/**
 * motor.h — Controle dos relés de direção do motor
 *
 * Exclusividade mútua: apenas um relé de direção ativo por vez.
 * Dead-time: 100ms entre desligar um e ligar o oposto (via millis()).
 *
 * Ref: motor/SPEC.md §2
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    DIR_NENHUMA = 0,
    DIR_SUBIR   = 1,
    DIR_DESCER  = 2
} Direcao;

#define DEAD_TIME_MS 100

void motor_init();

// Aciona motor na direção indicada. Gerencia dead-time internamente.
// Retorna true se o motor foi efetivamente acionado (false se em dead-time).
bool acionar_motor(Direcao dir);

// Desliga ambos os relés de direção imediatamente.
void desligar_motor();

// Retorna a direção atualmente ativa (DIR_NENHUMA se desligado ou em dead-time).
Direcao motor_direcao_atual();

// Retorna true se o motor está em período de dead-time.
bool motor_em_dead_time();

#endif // MOTOR_H
