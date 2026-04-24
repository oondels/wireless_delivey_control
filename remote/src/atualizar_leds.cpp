/**
 * atualizar_leds.cpp — Implementação da atualização dos LEDs do Remote
 *
 * Lógica baseada no feedback recebido do Principal:
 * - LINK:       fixo se Principal respondeu recentemente; pisca 1Hz se timeout > 500ms
 * - MOTOR:      pisca enquanto aguarda freio liberar e CLP reportar motor ativo; fixo quando motor ativo
 * - VEL1/VEL2:  fixo conforme feedback do CLP
 * - EMERGÊNCIA: pisca 4Hz se botão emergência ativo; fixo se CLP reporta emergencia
 *
 * Ref: leds/SPEC.md §3.2
 */

#include "atualizar_leds.h"

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
) {
    uint32_t agora = millis();
    bool linkOk = (status.link_ok == 1) && (agora - ultimoStatusMs <= WATCHDOG_TIMEOUT_MS);

    // LINK — timeout 500ms sem status do Principal = pisca 1Hz
    if (linkOk) {
        ledLink.ligar();
    } else {
        ledLink.piscar(500);  // 1 Hz
    }

    // MOTOR
    // - pisca enquanto existe solicitacao de movimento aguardando o freio liberar
    //   e o CLP confirmar motor ativo
    // - fica fixo quando o motor esta efetivamente ativo
    if (status.motor_ativo == 1) {
        ledMotor.ligar();
    } else if (aguardandoPartida) {
        ledMotor.piscar(250);  // 2 Hz
    } else {
        ledMotor.desligar();
    }

    // VELOCIDADE — reflete feedback do CLP
    ledVel1.desligar();
    ledVel2.desligar();
    if (status.vel1_ativa == 1) ledVel1.ligar();
    if (status.vel2_ativa == 1) ledVel2.ligar();

    // EMERGÊNCIA
    // - pisca 4Hz se botão emergência local ativo
    // - fixo se o CLP reportar emergência ativa
    if (emergenciaLocal) {
        ledEmergencia.piscar(125);  // 4 Hz
    } else if (status.emergencia_ativa == 1) {
        ledEmergencia.ligar();      // fixo: emergencia ativa reportada pelo CLP
    } else {
        ledEmergencia.desligar();
    }

    // Processar piscar não-bloqueante em todos os LEDs
    ledLink.atualizar();
    ledMotor.atualizar();
    ledVel1.atualizar();
    ledVel2.atualizar();
    ledEmergencia.atualizar();
}
