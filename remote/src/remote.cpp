/**
 * remote.cpp — Loop principal do Módulo Remote (Escravo)
 *
 * Integra todos os módulos: botões, comunicação, LEDs.
 *
 * Sequência do loop:
 *   1. Ler botões (debounce interno)
 *   2. Montar PacoteRemote
 *   3. Enviar pacote (heartbeat 200ms + imediato em mudança)
 *   4. Atualizar LEDs com base no último PacoteStatus recebido
 *
 * Ref: IMPLEMENTATION_PLAN §2.5
 */

#include <Arduino.h>
#include "pinout.h"
#include "protocolo.h"
#include "botoes.h"
#include "comunicacao.h"
#include "leds.h"
#include "atualizar_leds.h"

// Instâncias globais
Botoes      botoes;
Comunicacao comunicacao;

// 7 LEDs dedicados do Remote
Led ledLink(PIN_LED_LINK);
Led ledMotor(PIN_LED_MOTOR);
Led ledVel1(PIN_LED_VEL1);
Led ledVel2(PIN_LED_VEL2);
Led ledVel3(PIN_LED_VEL3);
Led ledEmergencia(PIN_LED_EMERGENCIA);
Led ledAlarme(PIN_LED_ALARME);

// Controle de envio periódico
uint32_t ultimoEnvioMs = 0;

// Estado anterior dos botões para detectar mudança
EstadoBotoes btnAnterior = {};

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Remote — Inicializando ===");

    botoes.init();
    comunicacao.init();

    Serial.println("=== Módulo Remote — Pronto ===");
}

void loop() {
    // 1. Ler botões locais (debounce interno)
    EstadoBotoes btn = botoes.ler();

    // 2. Montar PacoteRemote
    PacoteRemote pacote = {};
    pacote.emergencia = btn.emergencia ? 1 : 0;
    pacote.timestamp  = millis();

    // Determinar comando e botao_hold
    if (btn.subir_hold) {
        pacote.comando    = CMD_SUBIR;
        pacote.botao_hold = 1;
    } else if (btn.descer_hold) {
        pacote.comando    = CMD_DESCER;
        pacote.botao_hold = 1;
    } else if (btn.vel1_pulso) {
        pacote.comando    = CMD_VEL1;
        pacote.botao_hold = 0;
    } else if (btn.vel2_pulso) {
        pacote.comando    = CMD_VEL2;
        pacote.botao_hold = 0;
    } else if (btn.vel3_pulso) {
        pacote.comando    = CMD_VEL3;
        pacote.botao_hold = 0;
    } else {
        pacote.comando    = CMD_HEARTBEAT;
        pacote.botao_hold = 0;
    }

    // 3. Enviar pacote: imediato em mudança OU periódico a cada 200ms
    bool mudouEstado = (btn.subir_hold  != btnAnterior.subir_hold)  ||
                       (btn.descer_hold != btnAnterior.descer_hold) ||
                       (btn.emergencia  != btnAnterior.emergencia)  ||
                       btn.vel1_pulso || btn.vel2_pulso || btn.vel3_pulso;

    bool envioPeriodicoVencido = (millis() - ultimoEnvioMs >= HEARTBEAT_INTERVALO_MS);

    if (mudouEstado || envioPeriodicoVencido) {
        comunicacao.enviarPacote(pacote);
        ultimoEnvioMs = millis();
    }

    btnAnterior = btn;

    // 4. Atualizar LEDs com base no último status recebido
    atualizarLeds(
        comunicacao.ultimoStatus(),
        comunicacao.ultimoStatusRecebidoMs(),
        ledLink, ledMotor,
        ledVel1, ledVel2, ledVel3,
        ledEmergencia, ledAlarme
    );
}
