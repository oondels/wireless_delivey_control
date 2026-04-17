/**
 * principal.cpp — Loop principal do Módulo Principal (Bridge ESP → CLP)
 *
 * O Principal não possui entradas físicas. Recebe comandos do Módulo Remote
 * via ESP-NOW e os repassa para as entradas digitais do CLP via GPIO.
 * Toda a lógica de controle (motor, freio, estados, segurança) é executada
 * pelo CLP programado em Ladder.
 *
 * Lógica de nível para o CLP: ativo em LOW (GND)
 *   LOW  = sinal ativo → CLP lê entrada como acionada
 *   HIGH = sinal inativo (repouso)
 *
 * Fail-safe: se o Remote ficar silencioso > WATCHDOG_TIMEOUT_MS,
 * PIN_CLP_EMERGENCIA vai a LOW imediatamente (emergência ao CLP).
 * Todos os demais sinais de movimento ficam em HIGH (inativos).
 *
 * Sequência do loop:
 *   1. Verificar watchdog — se expirado, acionar emergência no CLP
 *   2. Se novo pacote recebido:
 *      a. Resetar watchdog
 *      b. Mapear campos do PacoteRemote para GPIOs do CLP
 *   3. Enviar PacoteStatus ao Remote (heartbeat 200ms)
 *   4. Atualizar LED LINK
 */

#include <Arduino.h>
#include "pinout.h"
#include "protocolo.h"
#include "comunicacao.h"
#include "watchdog_comm.h"
#include "leds.h"
#include "logger.h"

// ============================================================
// GPIOs de saída para o CLP
// ============================================================

static const uint8_t PINOS_CLP[] = {
    PIN_CLP_SUBIR,
    PIN_CLP_DESCER,
    PIN_CLP_VEL1,
    PIN_CLP_VEL2,
    PIN_CLP_EMERGENCIA,
    PIN_CLP_RESET,
    PIN_CLP_FIM_CURSO
};
static constexpr int NUM_PINOS_CLP = sizeof(PINOS_CLP) / sizeof(PINOS_CLP[0]);

// ============================================================
// Estado de pulso (VEL1, VEL2, RESET são pulsos, não níveis)
// ============================================================

static uint32_t pulsoVel1Inicio   = 0;
static uint32_t pulsoVel2Inicio   = 0;
static uint32_t pulsoResetInicio  = 0;
static bool     pulsoVel1Ativo    = false;
static bool     pulsoVel2Ativo    = false;
static bool     pulsoResetAtivo   = false;

// ============================================================
// Instâncias globais
// ============================================================

WatchdogComm   watchdog;
Comunicacao    comunicacao;
Led            ledLink(PIN_LED_LINK);

// Controle de envio periódico de status
uint32_t       ultimoEnvioStatusMs = 0;

// Logging anti-spam
bool           watchdogAnteriorExpirado = false;

// ============================================================
// Funções auxiliares
// ============================================================

/** Seta todos os sinais de movimento para HIGH (inativo). */
static void pararMovimento() {
    digitalWrite(PIN_CLP_SUBIR,  HIGH);
    digitalWrite(PIN_CLP_DESCER, HIGH);
}

/** Seta emergência no CLP (LOW = ativo). */
static void setEmergencia(bool ativa) {
    digitalWrite(PIN_CLP_EMERGENCIA, ativa ? LOW : HIGH);
}

/** Ativa um pulso no CLP (VEL1, VEL2 ou RESET). */
static void iniciarPulso(uint8_t pino, uint32_t& inicio, bool& ativo) {
    digitalWrite(pino, LOW);
    inicio = millis();
    ativo  = true;
}

/** Verifica e encerra pulsos vencidos. */
static void atualizarPulsos() {
    uint32_t agora = millis();

    if (pulsoVel1Ativo && (agora - pulsoVel1Inicio >= PULSO_CLP_MS)) {
        digitalWrite(PIN_CLP_VEL1, HIGH);
        pulsoVel1Ativo = false;
    }
    if (pulsoVel2Ativo && (agora - pulsoVel2Inicio >= PULSO_CLP_MS)) {
        digitalWrite(PIN_CLP_VEL2, HIGH);
        pulsoVel2Ativo = false;
    }
    if (pulsoResetAtivo && (agora - pulsoResetInicio >= PULSO_CLP_MS)) {
        digitalWrite(PIN_CLP_RESET, HIGH);
        pulsoResetAtivo = false;
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Principal (Bridge ESP→CLP) — Inicializando ===");

    // Inicializar todas as saídas CLP como HIGH (inativo)
    for (int i = 0; i < NUM_PINOS_CLP; i++) {
        pinMode(PINOS_CLP[i], OUTPUT);
        digitalWrite(PINOS_CLP[i], HIGH);
    }

    watchdog.init();
    comunicacao.init(watchdog);

    Serial.println("=== Módulo Principal — Pronto. Aguardando Remote... ===");
}

// ============================================================
// Loop
// ============================================================

void loop() {
    uint32_t agora = millis();

    // 1. Verificar watchdog
    bool watchdogExpirado = watchdog.expirado();

    if (watchdogExpirado) {
        // Fail-safe: Remote silencioso → emergência no CLP, movimento parado
        pararMovimento();
        setEmergencia(true);

        if (!watchdogAnteriorExpirado) {
            LOG_ERROR("WDOG", "Watchdog EXPIRADO — emergencia ativada no CLP");
        }
    } else if (watchdogAnteriorExpirado) {
        // Comunicação restaurada → liberar emergência
        setEmergencia(false);
        LOG_INFO("WDOG", "Watchdog recuperado — emergencia CLP liberada");
    }
    watchdogAnteriorExpirado = watchdogExpirado;

    // 2. Processar novo pacote do Remote
    if (comunicacao.novoPacoteRecebido()) {
        const volatile PacoteRemote& pkt = comunicacao.ultimoPacote();

        // Emergência (nível contínuo)
        bool emergencia = (pkt.emergencia == 1);
        if (!watchdogExpirado) {
            // Watchdog expirado já setou emergência — não sobrescrever
            setEmergencia(emergencia);
        }
        if (emergencia) {
            LOG_WARN("CLP", "Emergencia ATIVA — sinal enviado ao CLP");
        }

        // Fim de curso de descida (nível contínuo)
        digitalWrite(PIN_CLP_FIM_CURSO, (pkt.fim_curso_descida == 1) ? LOW : HIGH);

        // Movimento (hold): SUBIR ou DESCER enquanto botão pressionado
        uint8_t cmd        = pkt.comando;
        bool    botaoHold  = (pkt.botao_hold == 1);

        if (cmd == CMD_SUBIR && botaoHold) {
            digitalWrite(PIN_CLP_SUBIR,  LOW);
            digitalWrite(PIN_CLP_DESCER, HIGH);
            LOG_INFO("CLP", "Sinal SUBIR enviado ao CLP");
        } else if (cmd == CMD_DESCER && botaoHold) {
            digitalWrite(PIN_CLP_DESCER, LOW);
            digitalWrite(PIN_CLP_SUBIR,  HIGH);
            LOG_INFO("CLP", "Sinal DESCER enviado ao CLP");
        } else if (cmd == CMD_HEARTBEAT || !botaoHold) {
            // Botão solto ou heartbeat sem movimento → parar
            pararMovimento();
        }

        // VEL1 (pulso)
        if (cmd == CMD_VEL1) {
            digitalWrite(PIN_CLP_VEL2, HIGH);  // garante exclusividade
            iniciarPulso(PIN_CLP_VEL1, pulsoVel1Inicio, pulsoVel1Ativo);
            LOG_INFO("CLP", "Pulso VEL1 enviado ao CLP");
        }

        // VEL2 (pulso)
        if (cmd == CMD_VEL2) {
            digitalWrite(PIN_CLP_VEL1, HIGH);  // garante exclusividade
            iniciarPulso(PIN_CLP_VEL2, pulsoVel2Inicio, pulsoVel2Ativo);
            LOG_INFO("CLP", "Pulso VEL2 enviado ao CLP");
        }

        // RESET (pulso)
        if (cmd == CMD_RESET) {
            iniciarPulso(PIN_CLP_RESET, pulsoResetInicio, pulsoResetAtivo);
            LOG_INFO("CLP", "Pulso RESET enviado ao CLP");
        }

        comunicacao.limparNovoPacote();
    }

    // Encerrar pulsos vencidos
    atualizarPulsos();

    // 3. Enviar PacoteStatus ao Remote (heartbeat 200ms)
    if (agora - ultimoEnvioStatusMs >= STATUS_INTERVALO_MS) {
        PacoteStatus status;
        status.link_ok  = 1;
        status.checksum = 0;
        comunicacao.enviarStatus(status);
        ultimoEnvioStatusMs = agora;
    }

    // 4. Atualizar LED LINK
    if (!watchdogExpirado) {
        ledLink.ligar();
    } else {
        ledLink.piscar(250);  // 2 Hz quando sem link
    }
    ledLink.atualizar();
}
