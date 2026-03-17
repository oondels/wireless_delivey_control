/**
 * motor.cpp — Implementação do controle de direção do motor
 *
 * Dead-time não-bloqueante via millis(): ao inverter direção,
 * ambos os relés ficam OFF por 100ms antes de ligar o novo.
 * Nunca usa delay().
 *
 * Ref: motor/SPEC.md §2
 */

#include <Arduino.h>
#include "pinout.h"
#include "motor.h"

static Direcao direcao_atual     = DIR_NENHUMA;
static bool    em_dead_time      = false;
static uint32_t dead_time_inicio = 0;
static Direcao direcao_pendente  = DIR_NENHUMA;

void motor_init() {
    pinMode(PIN_RELE_DIRECAO_A, OUTPUT);
    pinMode(PIN_RELE_DIRECAO_B, OUTPUT);
    digitalWrite(PIN_RELE_DIRECAO_A, LOW);
    digitalWrite(PIN_RELE_DIRECAO_B, LOW);
    direcao_atual    = DIR_NENHUMA;
    em_dead_time     = false;
    direcao_pendente = DIR_NENHUMA;
}

static void reles_off() {
    digitalWrite(PIN_RELE_DIRECAO_A, LOW);
    digitalWrite(PIN_RELE_DIRECAO_B, LOW);
}

static void ativar_direcao(Direcao dir) {
    if (dir == DIR_SUBIR) {
        digitalWrite(PIN_RELE_DIRECAO_A, HIGH);
        digitalWrite(PIN_RELE_DIRECAO_B, LOW);
    } else if (dir == DIR_DESCER) {
        digitalWrite(PIN_RELE_DIRECAO_A, LOW);
        digitalWrite(PIN_RELE_DIRECAO_B, HIGH);
    }
    direcao_atual = dir;
}

bool acionar_motor(Direcao dir) {
    if (dir == DIR_NENHUMA) {
        desligar_motor();
        return false;
    }

    // Se em dead-time, verificar se expirou
    if (em_dead_time) {
        if ((millis() - dead_time_inicio) >= DEAD_TIME_MS) {
            em_dead_time = false;
            if (direcao_pendente != DIR_NENHUMA) {
                ativar_direcao(direcao_pendente);
                direcao_pendente = DIR_NENHUMA;
                return true;
            }
        }
        // Ainda em dead-time — atualizar direção pendente
        direcao_pendente = dir;
        return false;
    }

    // Mesma direção — manter
    if (dir == direcao_atual) {
        return true;
    }

    // Direção diferente com motor ativo — iniciar dead-time
    if (direcao_atual != DIR_NENHUMA) {
        reles_off();
        direcao_atual    = DIR_NENHUMA;
        em_dead_time     = true;
        dead_time_inicio = millis();
        direcao_pendente = dir;
        return false;
    }

    // Motor desligado, sem dead-time — ligar direto
    ativar_direcao(dir);
    return true;
}

void desligar_motor() {
    reles_off();
    direcao_atual    = DIR_NENHUMA;
    em_dead_time     = false;
    direcao_pendente = DIR_NENHUMA;
}

Direcao motor_direcao_atual() {
    return direcao_atual;
}

bool motor_em_dead_time() {
    // Atualizar estado do dead-time se expirou
    if (em_dead_time && (millis() - dead_time_inicio) >= DEAD_TIME_MS) {
        em_dead_time = false;
        if (direcao_pendente != DIR_NENHUMA) {
            ativar_direcao(direcao_pendente);
            direcao_pendente = DIR_NENHUMA;
        }
    }
    return em_dead_time;
}
