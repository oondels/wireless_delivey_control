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

#include <Arduino.h>
#include "pinout.h"

enum Direcao {
    DIR_NENHUMA = 0,
    DIR_SUBIR   = 1,
    DIR_DESCER  = 2
};

class Motor {
public:
    static constexpr uint32_t DEAD_TIME_MS = 100;

    void init();

    // Aciona motor na direção indicada. Gerencia dead-time internamente.
    // Retorna true se o motor foi efetivamente acionado (false se em dead-time).
    bool acionar(Direcao dir);

    // Desliga ambos os relés de direção imediatamente.
    void desligar();

    Direcao direcaoAtual() const { return _direcao; }
    bool emDeadTime();

private:
    void relesOff();
    void ativarDirecao(Direcao dir);

    Direcao  _direcao         = DIR_NENHUMA;
    bool     _emDeadTime      = false;
    uint32_t _deadTimeInicio  = 0;
    Direcao  _direcaoPendente = DIR_NENHUMA;
};

#endif // MOTOR_H
