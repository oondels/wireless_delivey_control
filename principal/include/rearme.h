/**
 * rearme.h — Lógica de rearme (desativação de emergência)
 *
 * Botão REARME é do tipo pulso (borda de descida).
 * Limpa emergencia_ativa e FALHA_COMUNICACAO.
 * Caso especial: se emergência Remote ainda travada, seta rearme_ativo.
 *
 * Ref: seguranca/SPEC.md §3.4-3.5
 */

#ifndef REARME_H
#define REARME_H

#include <stdbool.h>
#include "protocolo.h"

// Flag rearme_ativo: 1 = rearme feito com botão Remote ainda travado
extern bool rearme_ativo;

void rearme_init();

// Verifica botão REARME e executa rearme se condições atendidas.
// estado_atual: ponteiro para o estado do sistema (será alterado para PARADO).
// emergencia_remote: true se botão emergência do Remote ainda está travado.
void rearme_verificar(EstadoSistema* estado_atual, bool emergencia_remote);

#endif // REARME_H
