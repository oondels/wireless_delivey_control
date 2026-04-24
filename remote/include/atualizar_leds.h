/**
 * atualizar_leds.h — Função de atualização dos LEDs do Remote
 *
 * LEDs baseados no status recebido do Principal.
 *
 * Ref: leds/SPEC.md §3.2
 */

#ifndef ATUALIZAR_LEDS_H
#define ATUALIZAR_LEDS_H

#include <Arduino.h>
#include "protocolo.h"
#include "leds.h"

/**
 * Atualiza os LEDs do Remote com base no status recebido do Principal.
 *
 * @param status           Último PacoteStatus recebido do Principal
 * @param ultimoStatusMs   Timestamp (millis()) do último status recebido
 * @param emergenciaLocal  true = botão emergência com trava ativo
 * @param aguardandoPartida true = comando SUBIR/DESCER ativo aguardando freio liberar e motor partir
 * @param ledLink          LED de comunicação
 * @param ledMotor         LED de motor ativo
 * @param ledVel1          LED velocidade 1
 * @param ledVel2          LED velocidade 2
 * @param ledEmergencia    LED emergência
 */
void atualizarLeds(
    const volatile PacoteStatus& status,
    uint32_t ultimoStatusMs,
    bool emergenciaLocal,
    bool aguardandoPartida,
    Led& ledLink,
    Led& ledMotor,
    Led& ledVel1,
    Led& ledVel2,
    Led& ledEmergencia
);

#endif // ATUALIZAR_LEDS_H
