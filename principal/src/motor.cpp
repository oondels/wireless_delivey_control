/**
 * motor.cpp — Implementação do controle de direção do motor
 *
 * Dead-time não-bloqueante via millis(): ao inverter direção,
 * ambos os relés ficam OFF por 100ms antes de ligar o novo.
 * Nunca usa delay().
 *
 * Ref: motor/SPEC.md §2
 */

#include "motor.h"
#include "logger.h"

void Motor::init() {
    pinMode(PIN_RELE_DIRECAO_A, OUTPUT);
    pinMode(PIN_RELE_DIRECAO_B, OUTPUT);
    relesOff();
    _direcao         = DIR_NENHUMA;
    _emDeadTime      = false;
    _direcaoPendente = DIR_NENHUMA;
}

void Motor::relesOff() {
    digitalWrite(PIN_RELE_DIRECAO_A, LOW);
    digitalWrite(PIN_RELE_DIRECAO_B, LOW);
}

void Motor::ativarDirecao(Direcao dir) {
    if (dir == DIR_SUBIR) {
        LOG_INFO("MOTOR", "Motor ativado — direcao SUBIR");
        digitalWrite(PIN_RELE_DIRECAO_A, HIGH);
        digitalWrite(PIN_RELE_DIRECAO_B, LOW);
    } else if (dir == DIR_DESCER) {
        LOG_INFO("MOTOR", "Motor ativado — direcao DESCER");
        digitalWrite(PIN_RELE_DIRECAO_A, LOW);
        digitalWrite(PIN_RELE_DIRECAO_B, HIGH);
    }
    _direcao = dir;
}

bool Motor::acionar(Direcao dir) {
    if (dir == DIR_NENHUMA) {
        desligar();
        return false;
    }

    // Se em dead-time, verificar se expirou
    if (_emDeadTime) {
        if ((millis() - _deadTimeInicio) >= DEAD_TIME_MS) {
            _emDeadTime = false;
            if (_direcaoPendente != DIR_NENHUMA) {
                ativarDirecao(_direcaoPendente);
                _direcaoPendente = DIR_NENHUMA;
                return true;
            }
        }
        // Ainda em dead-time — atualizar direção pendente
        _direcaoPendente = dir;
        return false;
    }

    // Mesma direção — manter
    if (dir == _direcao) {
        return true;
    }

    // Direção diferente com motor ativo — iniciar dead-time
    if (_direcao != DIR_NENHUMA) {
        LOG_INFO("MOTOR", "Inversao de direcao — iniciando dead-time 100ms");
        relesOff();
        _direcao         = DIR_NENHUMA;
        _emDeadTime      = true;
        _deadTimeInicio  = millis();
        _direcaoPendente = dir;
        return false;
    }

    // Motor desligado, sem dead-time — ligar direto
    ativarDirecao(dir);
    return true;
}

void Motor::desligar() {
    if (_direcao != DIR_NENHUMA) {
        LOG_INFO("MOTOR", "Motor desligado");
    }
    relesOff();
    _direcao         = DIR_NENHUMA;
    _emDeadTime      = false;
    _direcaoPendente = DIR_NENHUMA;
}

bool Motor::emDeadTime() {
    if (_emDeadTime && (millis() - _deadTimeInicio) >= DEAD_TIME_MS) {
        _emDeadTime = false;
        if (_direcaoPendente != DIR_NENHUMA) {
            ativarDirecao(_direcaoPendente);
            _direcaoPendente = DIR_NENHUMA;
        }
    }
    return _emDeadTime;
}
