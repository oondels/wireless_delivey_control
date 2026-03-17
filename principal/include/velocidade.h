/**
 * velocidade.h — Controle de velocidade via relés VEL1/VEL2/VEL3
 *
 * Exclusividade mútua: apenas um relé de velocidade ativo por vez.
 * Padrão na inicialização: VEL1.
 *
 * Ref: motor/SPEC.md §3
 */

#ifndef VELOCIDADE_H
#define VELOCIDADE_H

#include <Arduino.h>
#include "pinout.h"

class Velocidade {
public:
    void init();

    // Seleciona nível de velocidade (1, 2 ou 3).
    void selecionar(uint8_t nivel);

    uint8_t atual() const { return _nivel; }

private:
    uint8_t _nivel = 1;
    static const uint8_t _pinos[3];

    void desacionarTodos();
    void acionarNivel(uint8_t nivel);
};

#endif // VELOCIDADE_H
