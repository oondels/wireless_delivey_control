/**
 * velocidade.cpp — Implementação do controle de velocidade
 *
 * Ref: motor/SPEC.md §3
 */

#include <Arduino.h>
#include "pinout.h"
#include "velocidade.h"

static uint8_t vel_atual = 1;

static const uint8_t pinos_vel[] = {PIN_RELE_VEL1, PIN_RELE_VEL2, PIN_RELE_VEL3};

static void desacionar_todos() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(pinos_vel[i], LOW);
    }
}

static void acionar_nivel(uint8_t nivel) {
    if (nivel >= 1 && nivel <= 3) {
        digitalWrite(pinos_vel[nivel - 1], HIGH);
    }
}

void velocidade_init() {
    for (int i = 0; i < 3; i++) {
        pinMode(pinos_vel[i], OUTPUT);
    }
    vel_atual = 1;
    desacionar_todos();
    acionar_nivel(1);
}

void velocidade_selecionar(uint8_t nivel) {
    if (nivel < 1 || nivel > 3 || nivel == vel_atual) {
        return;
    }
    desacionar_todos();
    acionar_nivel(nivel);
    vel_atual = nivel;
}

uint8_t velocidade_atual() {
    return vel_atual;
}
