/**
 * atualizar_leds.h — Função de atualização dos 7 LEDs do Remote
 *
 * Recebe PacoteStatus do Principal e atualiza cada LED conforme spec.
 * Todos os LEDs são instanciados externamente (remote.cpp) e passados
 * por referência.
 *
 * Ref: leds/SPEC.md §3.2
 */

#ifndef ATUALIZAR_LEDS_H
#define ATUALIZAR_LEDS_H

#include <Arduino.h>
#include "protocolo.h"
#include "leds.h"

/**
 * Atualiza os 7 LEDs do Remote com base no PacoteStatus recebido.
 *
 * @param status           Último PacoteStatus recebido do Principal
 * @param ultimoStatusMs   Timestamp (millis()) do último status recebido
 * @param ledLink          LED de comunicação
 * @param ledMotor         LED de motor ativo
 * @param ledVel1          LED velocidade 1
 * @param ledVel2          LED velocidade 2
 * @param ledVel3          LED velocidade 3
 * @param ledEmergencia    LED emergência/falha
 * @param ledAlarme        LED alarme (rearme com botão travado)
 */
void atualizarLeds(
    const volatile PacoteStatus& status,
    uint32_t ultimoStatusMs,
    Led& ledLink,
    Led& ledMotor,
    Led& ledVel1,
    Led& ledVel2,
    Led& ledVel3,
    Led& ledEmergencia,
    Led& ledAlarme
);

#endif // ATUALIZAR_LEDS_H
