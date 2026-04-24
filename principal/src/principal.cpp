/**
 * principal.cpp — Loop principal do Módulo Principal (Bridge ESP → CLP)
 *
 * O Principal recebe comandos do Módulo Remote via ESP-NOW, repassa esses
 * sinais para as entradas digitais do CLP via GPIO e lê feedbacks do CLP
 * e da micro do freio para retransmitir ao Remote.
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
 *   3. Enviar PacoteStatus ao Remote (heartbeat 200ms + imediato em mudança)
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

static const uint8_t PINOS_FEEDBACK[] = {
    PIN_FB_MOTOR_ATIVO,
    PIN_FB_EMERGENCIA_ATIVA,
    PIN_FB_VEL1_ATIVA,
    PIN_FB_VEL2_ATIVA,
    PIN_MICRO_FREIO
};
static constexpr int NUM_PINOS_FEEDBACK = sizeof(PINOS_FEEDBACK) / sizeof(PINOS_FEEDBACK[0]);

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
// Estado persistente do último comando remoto válido
// ============================================================

enum DirecaoRemota {
    DIRECAO_REMOTA_NENHUMA = 0,
    DIRECAO_REMOTA_SUBIR,
    DIRECAO_REMOTA_DESCER
};

static DirecaoRemota direcaoRemotaAtual = DIRECAO_REMOTA_NENHUMA;
static bool          holdRemotoAtivo    = false;
static bool          emergenciaRemotaAtiva = false;
static bool          fimCursoRemotoAtivo   = false;

// ============================================================
// Instâncias globais
// ============================================================

WatchdogComm   watchdog;
Comunicacao    comunicacao;
Led            ledLink(PIN_LED_LINK);

// Controle de envio periódico de status
uint32_t       ultimoEnvioStatusMs = 0;
PacoteStatus   ultimoStatusEnviado = {};

// Logging anti-spam
bool           watchdogAnteriorExpirado = false;
bool           testeSubirAnterior       = false;
bool           testeDescerAnterior      = false;
bool           microFreioAnteriorAtiva  = false;
bool           fbMotorAnteriorAtivo     = false;
bool           fbEmergenciaAnteriorAtiva = false;
bool           fbVel1AnteriorAtiva      = false;
bool           fbVel2AnteriorAtiva      = false;
bool           sinalSubirAnteriorAtivo  = false;
bool           sinalDescerAnteriorAtivo = false;
bool           bloqueioRemotoAnterior   = false;

static bool statusMudou(const PacoteStatus& atual, const PacoteStatus& anterior) {
    return memcmp(&atual, &anterior, sizeof(PacoteStatus) - 1) != 0;
}

static void registrarMudancaFeedbackClp(
    bool estadoAtual,
    bool& estadoAnterior,
    const char* modulo,
    const char* mensagemAtivo,
    const char* mensagemInativo
) {
    if (estadoAtual == estadoAnterior) {
        return;
    }

    if (estadoAtual) {
        LOG_INFO(modulo, mensagemAtivo);
    } else {
        LOG_INFO(modulo, mensagemInativo);
    }

    estadoAnterior = estadoAtual;
}

// ============================================================
// Funções auxiliares
// ============================================================

/** Seta todos os sinais de movimento para HIGH (inativo). */
static void pararMovimento() {
    digitalWrite(PIN_CLP_SUBIR,  HIGH);
    digitalWrite(PIN_CLP_DESCER, HIGH);
}

static void aplicarMovimento(bool subirAtivo, bool descerAtivo) {
    digitalWrite(PIN_CLP_SUBIR,  subirAtivo ? LOW : HIGH);
    digitalWrite(PIN_CLP_DESCER, descerAtivo ? LOW : HIGH);
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

static void registrarMudancaSaidaMovimento(bool subirAtivo, bool descerAtivo) {
    if (subirAtivo != sinalSubirAnteriorAtivo) {
        if (subirAtivo) {
            LOG_INFO("CLP", "Sinal SUBIR enviado ao CLP");
        } else {
            LOG_INFO("CLP", "Sinal SUBIR desativado");
        }
        sinalSubirAnteriorAtivo = subirAtivo;
    }

    if (descerAtivo != sinalDescerAnteriorAtivo) {
        if (descerAtivo) {
            LOG_INFO("CLP", "Sinal DESCER enviado ao CLP");
        } else {
            LOG_INFO("CLP", "Sinal DESCER desativado");
        }
        sinalDescerAnteriorAtivo = descerAtivo;
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

    // Botões de teste local
    pinMode(PIN_BTN_TESTE_SUBIR,  INPUT_PULLUP);
    pinMode(PIN_BTN_TESTE_DESCER, INPUT_PULLUP);

    // Feedbacks do CLP e micro do freio
    for (int i = 0; i < NUM_PINOS_FEEDBACK; i++) {
        pinMode(PINOS_FEEDBACK[i], INPUT_PULLUP);
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

    // Lê botões de teste local (LOW = pressionado com INPUT_PULLUP)
    bool btnSubirLocal  = (digitalRead(PIN_BTN_TESTE_SUBIR)  == LOW);
    bool btnDescerLocal = (digitalRead(PIN_BTN_TESTE_DESCER) == LOW);
    bool fbMotorAtivo      = (digitalRead(PIN_FB_MOTOR_ATIVO) == LOW);
    bool fbEmergenciaAtiva = (digitalRead(PIN_FB_EMERGENCIA_ATIVA) == LOW);
    bool fbVel1Ativa       = (digitalRead(PIN_FB_VEL1_ATIVA) == LOW);
    bool fbVel2Ativa       = (digitalRead(PIN_FB_VEL2_ATIVA) == LOW);
    bool microFreioAtiva   = (digitalRead(PIN_MICRO_FREIO) == HIGH);

    registrarMudancaFeedbackClp(
        fbMotorAtivo,
        fbMotorAnteriorAtivo,
        "CLP",
        "Feedback MOTOR_ATIVO acionado",
        "Feedback MOTOR_ATIVO desacionado"
    );
    registrarMudancaFeedbackClp(
        fbEmergenciaAtiva,
        fbEmergenciaAnteriorAtiva,
        "CLP",
        "Feedback EMERGENCIA_ATIVA acionado",
        "Feedback EMERGENCIA_ATIVA desacionado"
    );
    registrarMudancaFeedbackClp(
        fbVel1Ativa,
        fbVel1AnteriorAtiva,
        "CLP",
        "Feedback VEL1_ATIVA acionado",
        "Feedback VEL1_ATIVA desacionado"
    );
    registrarMudancaFeedbackClp(
        fbVel2Ativa,
        fbVel2AnteriorAtiva,
        "CLP",
        "Feedback VEL2_ATIVA acionado",
        "Feedback VEL2_ATIVA desacionado"
    );

    if (microFreioAtiva != microFreioAnteriorAtiva) {
        if (microFreioAtiva) {
            LOG_WARN("FREIO", "Micro do freio aberta/acionada");
        } else {
            LOG_INFO("FREIO", "Micro do freio voltou ao estado normal");
        }
        microFreioAnteriorAtiva = microFreioAtiva;
    }

    // Botão de teste ativo → resetar watchdog para evitar emergência por timeout
    if (btnSubirLocal || btnDescerLocal) {
        watchdog.resetar();
    }

    // 1. Verificar watchdog
    bool watchdogExpirado = watchdog.expirado();

    if (watchdogExpirado) {
        // Fail-safe: Remote silencioso → emergência no CLP, movimento parado
        pararMovimento();
        setEmergencia(true);
        holdRemotoAtivo       = false;
        direcaoRemotaAtual    = DIRECAO_REMOTA_NENHUMA;
        emergenciaRemotaAtiva = false;
        fimCursoRemotoAtivo   = false;

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

        emergenciaRemotaAtiva = (pkt.emergencia == 1);
        fimCursoRemotoAtivo   = (pkt.fim_curso_descida == 1);

        uint8_t cmd       = pkt.comando;
        bool    botaoHold = (pkt.botao_hold == 1);
        holdRemotoAtivo   = botaoHold && (cmd == CMD_SUBIR || cmd == CMD_DESCER);

        if (holdRemotoAtivo) {
            direcaoRemotaAtual = (cmd == CMD_SUBIR) ? DIRECAO_REMOTA_SUBIR : DIRECAO_REMOTA_DESCER;
        } else {
            direcaoRemotaAtual = DIRECAO_REMOTA_NENHUMA;
        }

        if (emergenciaRemotaAtiva) {
            LOG_WARN("CLP", "Emergencia ATIVA — sinal enviado ao CLP");
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

    bool operacaoRemotaPermitida = !watchdogExpirado &&
                                   !emergenciaRemotaAtiva &&
                                   !fbEmergenciaAtiva &&
                                   !microFreioAtiva &&
                                   fbMotorAtivo;
    bool demandaRemotaSubir  = holdRemotoAtivo && (direcaoRemotaAtual == DIRECAO_REMOTA_SUBIR);
    bool demandaRemotaDescer = holdRemotoAtivo && (direcaoRemotaAtual == DIRECAO_REMOTA_DESCER);
    bool bloqueioRemoto = (demandaRemotaSubir || demandaRemotaDescer) && !operacaoRemotaPermitida;

    if (bloqueioRemoto && !bloqueioRemotoAnterior) {
        LOG_WARN("CLP", "Comando remoto bloqueado por link, emergencia ou feedbacks de operacao");
    } else if (!bloqueioRemoto && bloqueioRemotoAnterior) {
        LOG_INFO("CLP", "Comando remoto liberado");
    }
    bloqueioRemotoAnterior = bloqueioRemoto;

    bool subirAtivo  = false;
    bool descerAtivo = false;

    if (operacaoRemotaPermitida && demandaRemotaSubir) {
        subirAtivo = true;
    } else if (operacaoRemotaPermitida && demandaRemotaDescer) {
        descerAtivo = true;
    } else if (!holdRemotoAtivo && !watchdogExpirado) {
        if (btnSubirLocal) {
            subirAtivo = true;
            if (!testeSubirAnterior) {
                LOG_INFO("TESTE", "Botao SUBIR local pressionado — sinal enviado ao CLP");
            }
        } else if (btnDescerLocal) {
            descerAtivo = true;
            if (!testeDescerAnterior) {
                LOG_INFO("TESTE", "Botao DESCER local pressionado — sinal enviado ao CLP");
            }
        }
    }

    aplicarMovimento(subirAtivo, descerAtivo);
    registrarMudancaSaidaMovimento(subirAtivo, descerAtivo);
    testeSubirAnterior  = btnSubirLocal;
    testeDescerAnterior = btnDescerLocal;

    // Encerrar pulsos vencidos
    atualizarPulsos();

    // Sinais sustentados para o CLP
    if (!watchdogExpirado) {
        setEmergencia(emergenciaRemotaAtiva);
    }
    digitalWrite(PIN_CLP_FIM_CURSO, fimCursoRemotoAtivo ? LOW : HIGH);

    // 3. Enviar PacoteStatus ao Remote (heartbeat 200ms + imediato em mudança)
    PacoteStatus status = {};
    status.link_ok           = watchdogExpirado ? 0 : 1;
    status.motor_ativo       = fbMotorAtivo ? 1 : 0;
    status.emergencia_ativa  = fbEmergenciaAtiva ? 1 : 0;
    status.vel1_ativa        = fbVel1Ativa ? 1 : 0;
    status.vel2_ativa        = fbVel2Ativa ? 1 : 0;
    status.micro_freio_ativa = microFreioAtiva ? 1 : 0;

    bool envioPeriodicoStatus = (agora - ultimoEnvioStatusMs >= STATUS_INTERVALO_MS);
    bool envioImediatoStatus  = statusMudou(status, ultimoStatusEnviado);
    if (envioPeriodicoStatus || envioImediatoStatus) {
        comunicacao.enviarStatus(status);
        ultimoEnvioStatusMs = agora;
        ultimoStatusEnviado = status;
    }

    // 4. Atualizar LED LINK
    if (!watchdogExpirado) {
        ledLink.ligar();
    } else {
        ledLink.piscar(250);  // 2 Hz quando sem link
    }
    ledLink.atualizar();
}
