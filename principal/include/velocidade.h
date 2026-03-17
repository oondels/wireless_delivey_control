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

#include <stdint.h>

void velocidade_init();

// Seleciona nível de velocidade (1, 2 ou 3).
// Desaciona relé anterior e aciona o novo.
void velocidade_selecionar(uint8_t nivel);

// Retorna velocidade atual (1, 2 ou 3).
uint8_t velocidade_atual();

#endif // VELOCIDADE_H
