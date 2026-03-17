/**
 * comunicacao.h — Comunicação ESP-NOW do Módulo Principal
 *
 * Inicialização WiFi + ESP-NOW, registro de peer (MAC do Remote),
 * callback OnDataRecv, envio de PacoteStatus.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include "protocolo.h"

// Último pacote recebido do Remote (atualizado no callback)
extern volatile PacoteRemote ultimo_pacote_remote;
extern volatile bool novo_pacote_recebido;

void comunicacao_init();

// Envia PacoteStatus para o Remote. Chamar a cada 200ms e em mudança de estado.
void comunicacao_enviar_status(const PacoteStatus* status);

#endif // COMUNICACAO_H
