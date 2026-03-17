/**
 * velocidade.cpp — Implementação do controle de velocidade
 *
 * Ref: motor/SPEC.md §3
 */

#include "velocidade.h"

const uint8_t Velocidade::_pinos[3] = {PIN_RELE_VEL1, PIN_RELE_VEL2, PIN_RELE_VEL3};

void Velocidade::desacionarTodos() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(_pinos[i], LOW);
    }
}

void Velocidade::acionarNivel(uint8_t nivel) {
    if (nivel >= 1 && nivel <= 3) {
        digitalWrite(_pinos[nivel - 1], HIGH);
    }
}

void Velocidade::init() {
    for (int i = 0; i < 3; i++) {
        pinMode(_pinos[i], OUTPUT);
    }
    _nivel = 1;
    desacionarTodos();
    acionarNivel(1);
}

void Velocidade::selecionar(uint8_t nivel) {
    if (nivel < 1 || nivel > 3 || nivel == _nivel) {
        return;
    }
    desacionarTodos();
    acionarNivel(nivel);
    _nivel = nivel;
}
