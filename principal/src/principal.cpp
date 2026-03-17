/**
 * principal.cpp — Loop principal do Módulo Principal (Mestre)
 *
 * Integra todos os módulos: botões, emergência, rearme, watchdog,
 * máquina de estados, motor, freio, velocidade, comunicação e LED.
 *
 * Sequência do loop:
 *   1. Ler botões
 *   2. Verificar rearme
 *   3. Atualizar máquina de estados
 *   4. Processar velocidade
 *   5. Montar e enviar PacoteStatus
 *   6. Atualizar LED LINK
 *
 * Estado inicial: PARADO, freio acionado, velocidade 1.
 *
 * Ref: IMPLEMENTATION_PLAN §2.6
 */

#include <Arduino.h>
#include "pinout.h"
#include "protocolo.h"
#include "freio.h"
#include "sensores.h"
#include "emergencia.h"
#include "rearme.h"
#include "watchdog_comm.h"
#include "motor.h"
#include "velocidade.h"
#include "botoes.h"
#include "comunicacao.h"
#include "maquina_estados.h"
#include "leds.h"
#include "logger.h"

// Instâncias globais dos módulos
Freio          freio;
Sensores       sensores;
Emergencia     emergencia;
Rearme         rearme;
WatchdogComm   watchdog;
Motor          motor;
Velocidade     velocidade;
Botoes         botoes;
Comunicacao    comunicacao;
MaquinaEstados maquinaEstados;
Led            ledLink(PIN_LED_LINK);

// Controle de envio periódico de status
uint32_t       ultimoEnvioStatusMs = 0;
EstadoSistema  estadoAnterior      = ESTADO_PARADO;

// Controle de logging de transições (evitar spam)
bool           watchdogAnteriorExpirado = false;
bool           subirAnterior            = false;
bool           descerAnterior           = false;
bool           emergenciaRemotaAnterior = false;
uint8_t        velocidadeAnteriorLog    = 1;

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Principal — Inicializando ===");

    // Inicializar todos os módulos
    freio.init();            // Configura GPIO + aciona freio (estado seguro)
    sensores.init();
    emergencia.init();
    rearme.init();
    watchdog.init();
    motor.init();
    velocidade.init();       // Padrão VEL1
    botoes.init();
    comunicacao.init(watchdog, emergencia);
    maquinaEstados.init();   // Estado PARADO

    Serial.println("=== Módulo Principal — Pronto ===");
}

void loop() {
    // 1. Ler botões locais (debounce interno)
    EstadoBotoes btn = botoes.ler();

    // 2. Verificar rearme (antes da máquina de estados)
    EstadoSistema estadoAtual = maquinaEstados.estadoAtual();
    rearme.verificar(
        &estadoAtual,
        comunicacao.ultimoPacote().emergencia,
        emergencia
    );

    // Log de botões de direção (transições)
    if (btn.subir_hold && !subirAnterior) {
        LOG_INFO("BOTAO", "Botao SUBIR pressionado (hold)");
    } else if (!btn.subir_hold && subirAnterior) {
        LOG_INFO("BOTAO", "Botao SUBIR solto");
    }
    if (btn.descer_hold && !descerAnterior) {
        LOG_INFO("BOTAO", "Botao DESCER pressionado (hold)");
    } else if (!btn.descer_hold && descerAnterior) {
        LOG_INFO("BOTAO", "Botao DESCER solto");
    }
    subirAnterior  = btn.subir_hold;
    descerAnterior = btn.descer_hold;

    // Log de botões de velocidade (pulso)
    if (btn.vel1_pulso) LOG_INFO("BOTAO", "Botao VEL1 pressionado");
    if (btn.vel2_pulso) LOG_INFO("BOTAO", "Botao VEL2 pressionado");
    if (btn.vel3_pulso) LOG_INFO("BOTAO", "Botao VEL3 pressionado");

    // 3. Atualizar máquina de estados (prioridade sequencial)
    EstadoSistema novoEstado = maquinaEstados.atualizar(
        emergencia,
        watchdog,
        sensores,
        motor,
        freio,
        btn,
        comunicacao.ultimoPacote()
    );

    // Log de transição de estado
    if (novoEstado != estadoAnterior) {
        LOG_INFO_VAL("ESTADO", "Transicao: ", String(estadoParaString(estadoAnterior)) + " -> " + estadoParaString(novoEstado));
    }

    // Log de watchdog
    bool watchdogAtualExpirado = watchdog.expirado();
    if (watchdogAtualExpirado && !watchdogAnteriorExpirado) {
        LOG_ERROR("WDOG", "Watchdog EXPIRADO — sem comunicacao com Remote");
    } else if (!watchdogAtualExpirado && watchdogAnteriorExpirado) {
        LOG_INFO("WDOG", "Watchdog recuperado — comunicacao restabelecida");
    }
    watchdogAnteriorExpirado = watchdogAtualExpirado;

    // 4. Processar velocidade (pulsos de VEL1/VEL2/VEL3)
    if (btn.vel1_pulso) velocidade.selecionar(1);
    if (btn.vel2_pulso) velocidade.selecionar(2);
    if (btn.vel3_pulso) velocidade.selecionar(3);

    // Processar velocidade do Remote (se novo pacote com comando de velocidade)
    if (comunicacao.novoPacoteRecebido()) {
        const volatile PacoteRemote& pkt = comunicacao.ultimoPacote();
        uint8_t cmd = pkt.comando;
        if (cmd == CMD_VEL1 || cmd == CMD_VEL2 || cmd == CMD_VEL3) {
            LOG_INFO_VAL("REMOTO", "Comando de velocidade recebido do Remote: ", comandoParaString(cmd));
        }

        if (cmd == CMD_VEL1) velocidade.selecionar(1);
        if (cmd == CMD_VEL2) velocidade.selecionar(2);
        if (cmd == CMD_VEL3) velocidade.selecionar(3);

        if (cmd == CMD_SUBIR || cmd == CMD_DESCER) {
            bool emergenciaPrincipalAtiva = emergencia.isAtiva();
            bool emergenciaRemoteAtiva = (pkt.emergencia == 1);

            if (emergenciaPrincipalAtiva || emergenciaRemoteAtiva) {
                String origemBloqueio = "principal";
                if (emergenciaPrincipalAtiva && emergenciaRemoteAtiva) {
                    origemBloqueio = "principal+remote";
                } else if (emergenciaRemoteAtiva) {
                    origemBloqueio = "remote";
                }

                LOG_WARN_VAL(
                    "REMOTO",
                    "Comando de movimentacao bloqueado por emergencia (origem: ",
                    origemBloqueio + "): " + comandoParaString(cmd)
                );
            } else {
                LOG_INFO_VAL("REMOTO", "Comando de movimentacao recebido do Remote: ", comandoParaString(cmd));
            }
        }

        if (pkt.emergencia == 1 && !emergenciaRemotaAnterior) {
            LOG_WARN("REMOTO", "Comando de EMERGENCIA recebido do Remote");
        } else if (pkt.emergencia == 0 && emergenciaRemotaAnterior) {
            LOG_INFO("REMOTO", "Sinal de EMERGENCIA liberado no Remote");
        }
        emergenciaRemotaAnterior = (pkt.emergencia == 1);

        comunicacao.limparNovoPacote();
    }

    // 5. Montar e enviar PacoteStatus
    bool estadoMudou = (novoEstado != estadoAnterior);
    bool envioPeriodicoVencido = (millis() - ultimoEnvioStatusMs >= STATUS_INTERVALO_MS);

    if (estadoMudou || envioPeriodicoVencido) {
        PacoteStatus status;
        status.estado_sistema = novoEstado;
        status.velocidade     = velocidade.atual();
        status.trava_logica   = (novoEstado == ESTADO_EMERGENCIA ||
                                 novoEstado == ESTADO_FALHA_COMUNICACAO) ? 1 : 0;
        status.rearme_ativo   = rearme.isRearmeAtivo() ? 1 : 0;
        status.checksum       = 0; // Será calculado em enviarStatus()

        comunicacao.enviarStatus(status);
        ultimoEnvioStatusMs = millis();
    }

    estadoAnterior = novoEstado;

    // Limpar flag rearme_ativo quando Remote soltar emergência
    if (rearme.isRearmeAtivo() && comunicacao.ultimoPacote().emergencia == 0) {
        rearme.limparRearmeAtivo();
    }

    // 6. Atualizar LED LINK (watchdog OK = aceso, expirado = apagado)
    if (!watchdog.expirado()) {
        ledLink.ligar();
    } else {
        ledLink.desligar();
    }
    ledLink.atualizar();
}
