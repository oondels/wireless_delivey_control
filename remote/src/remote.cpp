/**
 * Módulo Remote — Firmware
 * Painel embarcado no carrinho (escravo) do sistema de controle remoto.
 *
 * Responsabilidades:
 * - Leitura de botões locais (SUBIR, DESCER, VEL1-3, EMERGÊNCIA)
 * - Envio de PacoteRemote via ESP-NOW (heartbeat 200 ms + imediato)
 * - Recepção de PacoteStatus do Principal
 * - Atualização de 7 LEDs de status (LINK, MOTOR, VEL1-3, EMERGÊNCIA, ALARME)
 */

#include <Arduino.h>
#include "pinout.h"

void setup() {
    Serial.begin(115200);
    Serial.println("=== Módulo Remote — Inicializando ===");
}

void loop() {
    // Implementação nas próximas tarefas
}
