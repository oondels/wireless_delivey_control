/**
 * repeater.cpp — Loop do Módulo Repeater
 *
 * Ponte ESP-NOW autenticada entre Remote e Principal.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "comunicacao.h"
#include "logger.h"

Comunicacao comunicacao;

static uint32_t ultimoHeartbeatLogMs = 0;

void setup() {
    Serial.begin(115200);
    LOG_ALWAYS("BOOT", "=== Modulo Repeater - Inicializando ===");

    comunicacao.init();

    LOG_ALWAYS_VAL("BOOT", "MAC local: ", WiFi.macAddress());
    LOG_ALWAYS("BOOT", "=== Modulo Repeater - Pronto ===");
}

void loop() {
    uint32_t agora = millis();
    if (agora - ultimoHeartbeatLogMs >= 5000) {
        ultimoHeartbeatLogMs = agora;
        LOG_INFO("REPEATER", "Heartbeat operacional");
    }
}
