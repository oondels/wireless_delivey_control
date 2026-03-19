/**
 * atualizar_leds.cpp — Implementação da atualização dos LEDs do Remote
 *
 * Lógica conforme leds/SPEC.md §3.2:
 * - LINK: fixo se comunicação ativa, piscar 1Hz se timeout > 1000ms
 * - MOTOR: fixo se SUBINDO ou DESCENDO
 * - VEL1/2/3: fixo conforme campo velocidade
 * - EMERGÊNCIA: piscar 4Hz se EMERGENCIA, fixo se FALHA_COMUNICACAO, piscar 2Hz se FALHA_ENERGIA
 * - ALARME: piscar 2Hz se rearme_ativo E botão local travado
 *
 * Ref: leds/SPEC.md §3.2
 */

#include "atualizar_leds.h"
#include "pinout.h"

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
) {
    // LINK — timeout 1000ms sem status = piscar 1Hz
    if (millis() - ultimoStatusMs > 1000) {
        ledLink.piscar(500);       // 1 Hz
    } else {
        ledLink.ligar();
    }

    // MOTOR — fixo se em movimento
    if (status.estado_sistema == ESTADO_SUBINDO || status.estado_sistema == ESTADO_DESCENDO) {
        ledMotor.ligar();
    } else {
        ledMotor.desligar();
    }

    // VELOCIDADE — exclusividade mútua
    ledVel1.desligar();
    ledVel2.desligar();
    ledVel3.desligar();
    if (status.velocidade == 1) ledVel1.ligar();
    if (status.velocidade == 2) ledVel2.ligar();
    if (status.velocidade == 3) ledVel3.ligar();

    // EMERGÊNCIA
    if (status.estado_sistema == ESTADO_EMERGENCIA) {
        ledEmergencia.piscar(125);          // 4 Hz
    } else if (status.estado_sistema == ESTADO_FALHA_COMUNICACAO) {
        ledEmergencia.ligar();              // fixo
    } else if (status.estado_sistema == ESTADO_FALHA_ENERGIA) {
        ledEmergencia.piscar(250);          // 2 Hz — link OK, energia ausente
    } else {
        ledEmergencia.desligar();
    }

    // ALARME — piscar 2Hz se rearme_ativo E botão emergência local ainda travado (LOW)
    if (status.rearme_ativo == 1 && digitalRead(PIN_BTN_EMERGENCIA) == LOW) {
        ledAlarme.piscar(250);       // 2 Hz
    } else {
        ledAlarme.desligar();
    }

    // Atualizar todos os LEDs (processar piscar não-bloqueante)
    ledLink.atualizar();
    ledMotor.atualizar();
    ledVel1.atualizar();
    ledVel2.atualizar();
    ledVel3.atualizar();
    ledEmergencia.atualizar();
    ledAlarme.atualizar();
}
