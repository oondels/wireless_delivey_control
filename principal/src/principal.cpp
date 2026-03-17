/**
 * Módulo Principal — Firmware
 * Painel central (mestre) do sistema de controle remoto.
 *
 * Responsabilidades:
 * - Leitura de botões locais (SUBIR, DESCER, VEL1-3, EMERGÊNCIA, REARME)
 * - Leitura do sensor fim de curso
 * - Controle de relés (direção, velocidade, freio) + LEDs compartilhados
 * - Máquina de estados com prioridade de segurança
 * - Comunicação ESP-NOW com Módulo Remote
 * - Watchdog de comunicação
 */

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Principal — Inicializando ===");
}

void loop() {
    // Implementação nas próximas tarefas
}
