/**
 * atualizar_leds.cpp — Implementação da atualização dos LEDs do Remote
 *
 * Lógica baseada em estado local (sem feedback do CLP):
 * - LINK:       fixo se Principal respondeu recentemente; pisca 1Hz se timeout > 1s
 * - MOTOR:      fixo se SUBIR ou DESCER hold ativo + sem emergência local
 * - VEL1/VEL2:  fixo conforme velocidade selecionada localmente
 * - EMERGÊNCIA: pisca 4Hz se botão emergência ativo; fixo se link perdido > 500ms
 * - ALARME:     pisca 2Hz se link com Principal perdido
 *
 * Ref: leds/SPEC.md §3.2
 */

#include "atualizar_leds.h"

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
) {
    uint32_t agora = millis();
    bool linkOk = (status.link_ok == 1) && (agora - ultimoStatusMs <= 1000);

    // LINK — timeout 1000ms sem status do Principal = pisca 1Hz
    if (linkOk) {
        ledLink.ligar();
    } else {
        ledLink.piscar(500);  // 1 Hz
    }

    // MOTOR — fixo se SUBIR ou DESCER hold ativo e sem emergência local
    bool motorAtivo = (subirHold || descerHold) && !emergenciaLocal;
    if (motorAtivo) {
        ledMotor.ligar();
    } else {
        ledMotor.desligar();
    }

    // VELOCIDADE — exclusividade mútua (VEL1 e VEL2)
    ledVel1.desligar();
    ledVel2.desligar();
    if (velLocal == 1) ledVel1.ligar();
    if (velLocal == 2) ledVel2.ligar();

    // EMERGÊNCIA
    // - pisca 4Hz se botão emergência local ativo
    // - fixo se link com Principal perdido há mais de 500ms
    bool linkPerdido = (agora - ultimoStatusMs > 500);
    if (emergenciaLocal) {
        ledEmergencia.piscar(125);  // 4 Hz
    } else if (linkPerdido) {
        ledEmergencia.ligar();      // fixo: sem link com Principal
    } else {
        ledEmergencia.desligar();
    }

    // ALARME — pisca 2Hz se link com Principal perdido
    if (linkPerdido) {
        ledAlarme.piscar(250);  // 2 Hz
    } else {
        ledAlarme.desligar();
    }

    // Processar piscar não-bloqueante em todos os LEDs
    ledLink.atualizar();
    ledMotor.atualizar();
    ledVel1.atualizar();
    ledVel2.atualizar();
    ledEmergencia.atualizar();
    ledAlarme.atualizar();
}
