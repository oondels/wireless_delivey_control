/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Principal
 *
 * OnDataRecv: valida checksum, reseta watchdog, processa emergência
 * imediata se emergencia == 1.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "protocolo.h"
#include "comunicacao.h"
#include "watchdog_comm.h"
#include "emergencia.h"

// MAC do Remote — deve ser atualizado com o MAC real do dispositivo
static uint8_t mac_remote[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

volatile PacoteRemote ultimo_pacote_remote = {0};
volatile bool novo_pacote_recebido = false;

// Callback de recepção (executado no contexto da task WiFi)
static void on_data_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(PacoteRemote)) {
        return;
    }

    PacoteRemote pacote;
    memcpy(&pacote, data, sizeof(PacoteRemote));

    // Validar checksum — pacotes inválidos são descartados sem resetar watchdog
    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteRemote) - 1);
    if (cs != pacote.checksum) {
        return;
    }

    // Pacote válido — resetar watchdog
    watchdog_comm_resetar();

    // Processar emergência imediatamente (prioridade máxima)
    if (pacote.emergencia == 1) {
        emergencia_ativa = true;
    }

    // Copiar pacote para variável global
    memcpy((void*)&ultimo_pacote_remote, &pacote, sizeof(PacoteRemote));
    novo_pacote_recebido = true;
}

void comunicacao_init() {
    // WiFi em modo station (necessário para ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERRO] Falha ao inicializar ESP-NOW");
        return;
    }

    // Registrar callback de recepção
    esp_now_register_recv_cb(on_data_recv);

    // Registrar peer (Remote)
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, mac_remote, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        Serial.println("[ERRO] Falha ao registrar peer Remote");
        return;
    }

    Serial.println("[OK] ESP-NOW inicializado — Principal");
}

void comunicacao_enviar_status(const PacoteStatus* status) {
    // Calcular checksum antes de enviar
    PacoteStatus pacote = *status;
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteStatus) - 1);

    esp_now_send(mac_remote, (const uint8_t*)&pacote, sizeof(PacoteStatus));
}
