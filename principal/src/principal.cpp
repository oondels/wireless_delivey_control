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

    // 4. Processar velocidade (pulsos de VEL1/VEL2/VEL3)
    if (btn.vel1_pulso) velocidade.selecionar(1);
    if (btn.vel2_pulso) velocidade.selecionar(2);
    if (btn.vel3_pulso) velocidade.selecionar(3);

    // Processar velocidade do Remote (se novo pacote com comando de velocidade)
    if (comunicacao.novoPacoteRecebido()) {
        uint8_t cmd = comunicacao.ultimoPacote().comando;
        if (cmd == CMD_VEL1) velocidade.selecionar(1);
        if (cmd == CMD_VEL2) velocidade.selecionar(2);
        if (cmd == CMD_VEL3) velocidade.selecionar(3);
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
