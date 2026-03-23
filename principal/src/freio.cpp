/**
 * freio.cpp — Implementação do controle do freio solenoide de dupla bobina
 *
 * O cilindro leva ~10s para completar o curso em cada direção.
 * Os relés funcionam como pulso: são desativados após a microchave
 * confirmar que o cilindro chegou à posição final.
 *
 * Ref: motor/SPEC.md §4
 */

#include "freio.h"
#include "logger.h"

void Freio::init() {
    pinMode(PIN_RELE_FREIO_ON, OUTPUT);
    pinMode(PIN_RELE_FREIO_OFF, OUTPUT);
    pinMode(PIN_MICROCHAVE_FREIO, INPUT_PULLUP);

    // Ambos os relés desativados inicialmente
    digitalWrite(PIN_RELE_FREIO_ON, HIGH);
    digitalWrite(PIN_RELE_FREIO_OFF, HIGH);

    // Determinar estado inicial pela microchave
    if (digitalRead(PIN_MICROCHAVE_FREIO) == HIGH) {
        // Freio já engatado — não precisa pulsar
        _estado = FREIO_ENGATADO;
        LOG_INFO("FREIO", "Init — freio ja engatado (microchave HIGH)");
    } else {
        // Freio liberado — acionar para estado seguro
        LOG_INFO("FREIO", "Init — freio liberado, acionando para estado seguro");
        digitalWrite(PIN_RELE_FREIO_ON, LOW);
        _estado = FREIO_ENGATANDO;
    }
}

void Freio::acionar() {
    if (_estado == FREIO_ENGATADO || _estado == FREIO_ENGATANDO) {
        return; // Já engatado ou em processo — idempotente
    }

    LOG_INFO("FREIO", "Iniciando engate do freio");
    digitalWrite(PIN_RELE_FREIO_OFF, HIGH);  // Desativa liberação
    delay(10);                                // Dead-time entre bobinas
    digitalWrite(PIN_RELE_FREIO_ON, LOW);    // Ativa aplicação
    _estado = FREIO_ENGATANDO;
}

void Freio::liberar() {
    if (_estado == FREIO_LIBERADO || _estado == FREIO_LIBERANDO) {
        return; // Já liberado ou em processo — idempotente
    }

    LOG_INFO("FREIO", "Iniciando liberacao do freio");
    digitalWrite(PIN_RELE_FREIO_ON, HIGH);   // Desativa aplicação
    delay(10);                                // Dead-time entre bobinas
    digitalWrite(PIN_RELE_FREIO_OFF, LOW);   // Ativa liberação
    _estado = FREIO_LIBERANDO;
}

void Freio::atualizar() {
    // Em modo manual, não faz transições automáticas
    if (_modoManual) return;

    bool microchave = digitalRead(PIN_MICROCHAVE_FREIO);

    // Transições de confirmação: relé ativo → microchave confirma → desativa relé
    if (_estado == FREIO_ENGATANDO && microchave == HIGH) {
        LOG_INFO("FREIO", "Microchave confirmou freio engatado — desativando rele FREIO_ON");
        digitalWrite(PIN_RELE_FREIO_ON, HIGH);
        _estado = FREIO_ENGATADO;
    }

    if (_estado == FREIO_LIBERANDO && microchave == LOW) {
        LOG_INFO("FREIO", "Microchave confirmou freio liberado — desativando rele FREIO_OFF");
        digitalWrite(PIN_RELE_FREIO_OFF, HIGH);
        _estado = FREIO_LIBERADO;
    }

    // Guarda de segurança: estado deve refletir a microchave real
    // Se a microchave contradiz o estado, corrige imediatamente
    if (_estado == FREIO_LIBERADO && microchave == HIGH) {
        LOG_WARN("FREIO", "Microchave indica freio engatado mas estado era LIBERADO — corrigindo");
        _estado = FREIO_ENGATADO;
    }
}

// === Controle manual do cilindro (modo de ajuste/recuperação) ===

void Freio::manualAcionar() {
    if (!_modoManual) {
        LOG_WARN("FREIO", "Modo MANUAL ativado — acionando freio (cilindro avanca)");
        _modoManual = true;
    }
    digitalWrite(PIN_RELE_FREIO_OFF, HIGH);
    digitalWrite(PIN_RELE_FREIO_ON, LOW);
}

void Freio::manualLiberar() {
    if (!_modoManual) {
        LOG_WARN("FREIO", "Modo MANUAL ativado — liberando freio (cilindro retrai)");
        _modoManual = true;
    }
    digitalWrite(PIN_RELE_FREIO_ON, HIGH);
    digitalWrite(PIN_RELE_FREIO_OFF, LOW);
}

void Freio::manualParar() {
    if (!_modoManual) return;

    // Desliga ambos os relés
    digitalWrite(PIN_RELE_FREIO_ON, HIGH);
    digitalWrite(PIN_RELE_FREIO_OFF, HIGH);

    // Sincroniza estado pela microchave
    if (digitalRead(PIN_MICROCHAVE_FREIO) == HIGH) {
        _estado = FREIO_ENGATADO;
    } else if (digitalRead(PIN_MICROCHAVE_FREIO) == LOW) {
        _estado = FREIO_LIBERADO;
    } else {
        // Cilindro no meio do curso — estado indefinido, manter como ENGATANDO
        _estado = FREIO_ENGATANDO;
    }

    _modoManual = false;
    LOG_WARN("FREIO", "Modo MANUAL desativado — estado sincronizado pela microchave");
}
