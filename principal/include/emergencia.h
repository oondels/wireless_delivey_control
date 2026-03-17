/**
 * emergencia.h — Lógica de emergência do Módulo Principal
 *
 * Gerencia a flag emergencia_ativa e a leitura do botão EMERGÊNCIA
 * (trava mecânica, nível contínuo).
 *
 * Ref: seguranca/SPEC.md §3
 */

#ifndef EMERGENCIA_H
#define EMERGENCIA_H

#include <stdbool.h>

// Flag global — true = emergência ativa, movimentação bloqueada
extern volatile bool emergencia_ativa;

void emergencia_init();

// Verifica botão local e emergência do Remote. Se qualquer um ativo,
// seta emergencia_ativa = true. Retorna estado atual da flag.
bool emergencia_verificar(bool emergencia_remote);

// Retorna true se o botão EMERGÊNCIA local está fisicamente pressionado (LOW)
bool emergencia_botao_local_ativo();

#endif // EMERGENCIA_H
