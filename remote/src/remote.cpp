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
 *   4. Atualizar LEDs e bloqueio local com base no status do Principal
 */

#include <Arduino.h>
#include <WiFi.h>
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

// LEDs dedicados do Remote (GPIO 18 e GPIO 21 não utilizados nesta arquitetura)
Led ledLink(PIN_LED_LINK);
Led ledMotor(PIN_LED_MOTOR);
Led ledVel1(PIN_LED_VEL1);
Led ledVel2(PIN_LED_VEL2);
Led ledEmergencia(PIN_LED_EMERGENCIA);

// Controle de envio periódico
uint32_t ultimoEnvioMs = 0;

// Estado anterior dos botões para detectar mudança e logging
EstadoBotoes btnAnterior = {};
bool    linkAnteriorOk          = false;
bool    fimCursoDescidaAnterior = false;
bool    bloqueioMovimentoAnterior = false;
bool    microFreioAnteriorAtiva   = false;
bool    aguardandoPartidaAnterior = false;

void setup() {
    Serial.begin(115200);
    LOG_ALWAYS("BOOT", "=== Modulo Remote - Inicializando ===");

    botoes.init();
    fimCursoDescida.init();
    comunicacao.init();

    LOG_ALWAYS_VAL("BOOT", "MAC local: ", WiFi.macAddress());
    LOG_ALWAYS("BOOT", "=== Modulo Remote - Pronto ===");
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
        LOG_WARN("BOTAO", "Botao EMERGENCIA ativado (NC aberto)");
    } else if (!btn.emergencia && btnAnterior.emergencia) {
        LOG_INFO("BOTAO", "Botao EMERGENCIA liberado (NC fechado)");
    }

    const volatile PacoteStatus& st = comunicacao.ultimoStatus();
    bool statusPrincipalValido = (st.link_ok == 1) &&
                                 (millis() - comunicacao.ultimoStatusRecebidoMs() <= WATCHDOG_TIMEOUT_MS);
    bool bloqueioMovimento = !statusPrincipalValido || btn.emergencia || (st.emergencia_ativa == 1);

    if (bloqueioMovimento && !bloqueioMovimentoAnterior) {
        LOG_WARN("BLOQUEIO", "Comandos SUBIR/DESCER bloqueados por emergencia local, emergencia do Principal ou falta de link");
    } else if (!bloqueioMovimento && bloqueioMovimentoAnterior) {
        LOG_INFO("BLOQUEIO", "Comandos SUBIR/DESCER liberados");
    }
    bloqueioMovimentoAnterior = bloqueioMovimento;

    bool microFreioAtiva = (st.micro_freio_ativa == 1);
    if (microFreioAtiva != microFreioAnteriorAtiva) {
        if (microFreioAtiva) {
            LOG_INFO("FREIO", "Freio reportado como ativo pelo Principal");
        } else {
            LOG_INFO("FREIO", "Freio reportado como liberado pelo Principal");
        }
        microFreioAnteriorAtiva = microFreioAtiva;
    }

    bool solicitacaoMovimento = (btn.subir_hold || btn.descer_hold) && !bloqueioMovimento;
    bool aguardandoPartida = solicitacaoMovimento &&
                             !((st.motor_ativo == 1) && (st.micro_freio_ativa == 0));

    if (aguardandoPartida && !aguardandoPartidaAnterior) {
        LOG_INFO("MOTOR", "Aguardando freio liberar e feedback de motor ativo");
    } else if (!aguardandoPartida && aguardandoPartidaAnterior) {
        LOG_INFO("MOTOR", "Partida concluida ou solicitacao de movimento encerrada");
    }
    aguardandoPartidaAnterior = aguardandoPartida;

    // 2. Montar PacoteRemote
    PacoteRemote pacote = {};
    pacote.emergencia        = btn.emergencia ? 1 : 0;
    pacote.fim_curso_descida = fcDescida ? 1 : 0;
    pacote.timestamp         = millis();

    // Determinar comando e botao_hold
    if (btn.subir_hold && !bloqueioMovimento) {
        pacote.comando    = CMD_SUBIR;
        pacote.botao_hold = 1;
    } else if (btn.descer_hold && !bloqueioMovimento) {
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
    bool linkAtualOk = statusPrincipalValido;
    if (linkAtualOk && !linkAnteriorOk) {
        LOG_INFO("LINK", "Comunicacao com Principal restabelecida");
    } else if (!linkAtualOk && linkAnteriorOk) {
        LOG_WARN("LINK", "Comunicacao com Principal perdida (timeout > 500ms)");
    }
    linkAnteriorOk = linkAtualOk;

    // 4. Atualizar LEDs com base no status do Principal
    atualizarLeds(
        comunicacao.ultimoStatus(),
        comunicacao.ultimoStatusRecebidoMs(),
        btn.emergencia,
        aguardandoPartida,
        ledLink, ledMotor,
        ledVel1, ledVel2,
        ledEmergencia
    );
}
