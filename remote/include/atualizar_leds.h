/**
 * atualizar_leds.h — Função de atualização dos LEDs do Remote
 *
 * LEDs baseados em estado local do Remote (sem feedback do CLP).
 * O CLP gerencia toda a lógica de controle; o Remote exibe o que
 * ele mesmo enviou ao Principal.
 *
 * Ref: leds/SPEC.md §3.2
 */

#ifndef ATUALIZAR_LEDS_H
#define ATUALIZAR_LEDS_H

#include <Arduino.h>
#include "protocolo.h"
#include "leds.h"

/**
 * Atualiza os LEDs do Remote com base em estado local e link com Principal.
 *
 * @param status           Último PacoteStatus recebido do Principal
 * @param ultimoStatusMs   Timestamp (millis()) do último status recebido
 * @param subirHold        true = botão SUBIR pressionado agora
 * @param descerHold       true = botão DESCER pressionado agora
 * @param emergenciaLocal  true = botão emergência com trava ativo
 * @param velLocal         Velocidade selecionada localmente (1 ou 2)
 * @param ledLink          LED de comunicação
 * @param ledMotor         LED de motor ativo
 * @param ledVel1          LED velocidade 1
 * @param ledVel2          LED velocidade 2
 * @param ledEmergencia    LED emergência
 * @param ledAlarme        LED alarme (link perdido)
 */
void atualizarLeds(
    const volatile PacoteStatus& status,
    uint32_t ultimoStatusMs,
    bool subirHold,
    bool descerHold,
    bool emergenciaLocal,
    uint8_t velLocal,
    Led& ledLink,
    Led& ledMotor,
    Led& ledVel1,
    Led& ledVel2,
    Led& ledEmergencia,
    Led& ledAlarme
);

#endif // ATUALIZAR_LEDS_H
