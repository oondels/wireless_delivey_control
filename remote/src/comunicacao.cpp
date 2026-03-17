/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Remote
 *
 * OnDataRecv: valida checksum do PacoteStatus e atualiza estado local.
 * enviarPacote: calcula checksum e envia PacoteRemote via ESP-NOW.
 *
 * Ref: comunicacao/SPEC.md §7, §9.1
 */

#include "comunicacao.h"

// Definição dos membros estáticos
volatile PacoteStatus Comunicacao::_ultimoStatus  = {};
volatile uint32_t     Comunicacao::_ultimoStatusMs = 0;

void Comunicacao::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(PacoteStatus)) {
        return;
    }

    PacoteStatus status;
    memcpy(&status, data, sizeof(PacoteStatus));

    // Validar checksum
    uint8_t cs = calcular_checksum((const uint8_t*)&status, sizeof(PacoteStatus) - 1);
    if (cs != status.checksum) {
        return;
    }

    // Pacote válido — atualizar estado local
    memcpy((void*)&_ultimoStatus, &status, sizeof(PacoteStatus));
    _ultimoStatusMs = millis();
}

void Comunicacao::init() {
    // WiFi em modo station (necessário para ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERRO] Falha ao inicializar ESP-NOW");
        return;
    }

    // Registrar callback de recepção
    esp_now_register_recv_cb(onDataRecv);

    // Registrar peer (Principal)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, _macPrincipal, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERRO] Falha ao registrar peer Principal");
        return;
    }

    Serial.println("[OK] ESP-NOW inicializado — Remote");
}

void Comunicacao::enviarPacote(const PacoteRemote& pacote) {
    PacoteRemote p = pacote;
    p.checksum = calcular_checksum((const uint8_t*)&p, sizeof(PacoteRemote) - 1);
    esp_now_send(_macPrincipal, (const uint8_t*)&p, sizeof(PacoteRemote));
}
