/**
 * remote.cpp — Loop principal do Módulo Remote (Escravo)
 *
 * Lê botões locais e transmite comandos via ESP-NOW para o Principal,
 * que repassa os sinais para as entradas do CLP.
 * O CLP gerencia toda a lógica de controle (motor, freio, estados, segurança).
 *
 * Sequência do loop:
 *   0. Atualizar sensor fim de curso (debounce)
 *   1. Ler botões (debounce interno)
 *   2. Montar PacoteRemote
 *   3. Enviar pacote (heartbeat 100ms + imediato em mudança)
 *   4. Atualizar LEDs com base em estado local e link com Principal
 */

#include <Arduino.h>
#include "pinout.h"
#include "protocolo.h"
#include "botoes.h"
#include "comunicacao.h"
#include "fim_curso.h"
#include "leds.h"
#include "atualizar_leds.h"
#include "logger.h"

// Instâncias globais
Botoes      botoes;
Comunicacao comunicacao;
FimCurso    fimCursoDescida(PIN_FIM_CURSO_DESCIDA);

// LEDs dedicados do Remote (GPIO 18 não utilizado nesta arquitetura)
Led ledLink(PIN_LED_LINK);
Led ledMotor(PIN_LED_MOTOR);
Led ledVel1(PIN_LED_VEL1);
Led ledVel2(PIN_LED_VEL2);
Led ledEmergencia(PIN_LED_EMERGENCIA);
Led ledAlarme(PIN_LED_ALARME);

// Controle de envio periódico
uint32_t ultimoEnvioMs = 0;

// Estado anterior dos botões para detectar mudança e logging
EstadoBotoes btnAnterior = {};
bool    linkAnteriorOk          = false;
bool    fimCursoDescidaAnterior = false;

// Velocidade selecionada localmente (1 ou 2)
uint8_t velLocal = 1;

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Remote — Inicializando ===");

    botoes.init();
    fimCursoDescida.init();
    comunicacao.init();

    Serial.println("=== Módulo Remote — Pronto ===");
}

void loop() {
    // 0. Atualizar sensor fim de curso (debounce + timer pós-liberação)
    fimCursoDescida.atualizar();
    bool fcDescida = fimCursoDescida.acionado();

    // 1. Ler botões locais (debounce interno)
    EstadoBotoes btn = botoes.ler();

    // Log de transições de botões
    if (btn.subir_hold && !btnAnterior.subir_hold) {
        LOG_INFO("BOTAO", "Botao SUBIR pressionado (hold)");
    } else if (!btn.subir_hold && btnAnterior.subir_hold) {
        LOG_INFO("BOTAO", "Botao SUBIR solto");
    }
    if (btn.descer_hold && !btnAnterior.descer_hold) {
        LOG_INFO("BOTAO", "Botao DESCER pressionado (hold)");
    } else if (!btn.descer_hold && btnAnterior.descer_hold) {
        LOG_INFO("BOTAO", "Botao DESCER solto");
    }
    if (btn.vel1_pulso)  LOG_INFO("BOTAO", "Botao VEL1 pressionado");
    if (btn.vel2_pulso)  LOG_INFO("BOTAO", "Botao VEL2 pressionado");
    if (btn.reset_pulso) LOG_INFO("BOTAO", "Botao RESET pressionado");
    if (btn.emergencia && !btnAnterior.emergencia) {
        LOG_WARN("BOTAO", "Botao EMERGENCIA ativado (trava)");
    } else if (!btn.emergencia && btnAnterior.emergencia) {
        LOG_INFO("BOTAO", "Botao EMERGENCIA liberado");
    }

    // Atualizar velocidade local (pulso seleciona)
    if (btn.vel1_pulso) velLocal = 1;
    if (btn.vel2_pulso) velLocal = 2;

    // 2. Montar PacoteRemote
    PacoteRemote pacote = {};
    pacote.emergencia        = btn.emergencia ? 1 : 0;
    pacote.fim_curso_descida = fcDescida ? 1 : 0;
    pacote.timestamp         = millis();

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
    } else if (btn.reset_pulso) {
        pacote.comando    = CMD_RESET;
        pacote.botao_hold = 0;
    } else {
        pacote.comando    = CMD_HEARTBEAT;
        pacote.botao_hold = 0;
    }

    // 3. Enviar pacote: imediato em mudança OU periódico a cada 100ms
    bool mudouEstado = (btn.subir_hold  != btnAnterior.subir_hold)  ||
                       (btn.descer_hold != btnAnterior.descer_hold) ||
                       (btn.emergencia  != btnAnterior.emergencia)  ||
                       (fcDescida != fimCursoDescidaAnterior)       ||
                       btn.vel1_pulso || btn.vel2_pulso || btn.reset_pulso;

    bool envioPeriodicoVencido = (millis() - ultimoEnvioMs >= HEARTBEAT_INTERVALO_MS);

    if (mudouEstado || envioPeriodicoVencido) {
        comunicacao.enviarPacote(pacote);
        ultimoEnvioMs = millis();
    }

    btnAnterior             = btn;
    fimCursoDescidaAnterior = fcDescida;

    // Log de link (comunicação)
    const volatile PacoteStatus& st = comunicacao.ultimoStatus();
    bool linkAtualOk = (st.link_ok == 1) && (millis() - comunicacao.ultimoStatusRecebidoMs() <= 1000);
    if (linkAtualOk && !linkAnteriorOk) {
        LOG_INFO("LINK", "Comunicacao com Principal restabelecida");
    } else if (!linkAtualOk && linkAnteriorOk) {
        LOG_WARN("LINK", "Comunicacao com Principal perdida (timeout > 1s)");
    }
    linkAnteriorOk = linkAtualOk;

    // 4. Atualizar LEDs com base em estado local e link com Principal
    atualizarLeds(
        comunicacao.ultimoStatus(),
        comunicacao.ultimoStatusRecebidoMs(),
        btn.subir_hold,
        btn.descer_hold,
        btn.emergencia,
        velLocal,
        ledLink, ledMotor,
        ledVel1, ledVel2,
        ledEmergencia, ledAlarme
    );
}
