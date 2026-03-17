/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Remote
 *
 * OnDataRecv: valida checksum do PacoteStatus e atualiza estado local.
 * enviarPacote: calcula checksum e envia PacoteRemote via ESP-NOW.
 *
 * Ref: comunicacao/SPEC.md §7, §9.1
 */

#include "comunicacao.h"

constexpr uint8_t Comunicacao::MAC_BROADCAST[6];

// Definição dos membros estáticos
volatile PacoteStatus Comunicacao::_ultimoStatus  = {};
volatile uint32_t     Comunicacao::_ultimoStatusMs = 0;
uint8_t               Comunicacao::_macPrincipalAtual[6] = {
    Comunicacao::MAC_BROADCAST[0],
    Comunicacao::MAC_BROADCAST[1],
    Comunicacao::MAC_BROADCAST[2],
    Comunicacao::MAC_BROADCAST[3],
    Comunicacao::MAC_BROADCAST[4],
    Comunicacao::MAC_BROADCAST[5]
};
bool                  Comunicacao::_peerPrincipalConhecido = false;

bool Comunicacao::registrarPeer(const uint8_t* mac) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

void Comunicacao::atualizarPeerPrincipal(const uint8_t* mac) {
    if (mac == nullptr) {
        return;
    }

    if (_peerPrincipalConhecido && memcmp(_macPrincipalAtual, mac, 6) == 0) {
        return;
    }

    if (!registrarPeer(mac)) {
        return;
    }

    memcpy(_macPrincipalAtual, mac, 6);
    _peerPrincipalConhecido = true;

    Serial.printf(
        "[INFO] Peer MAC Principal detectado: %02X:%02X:%02X:%02X:%02X:%02X\n",
        _macPrincipalAtual[0],
        _macPrincipalAtual[1],
        _macPrincipalAtual[2],
        _macPrincipalAtual[3],
        _macPrincipalAtual[4],
        _macPrincipalAtual[5]
    );
}

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

    atualizarPeerPrincipal(mac);

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

    // Registrar broadcast para descoberta inicial do peer
    if (!registrarPeer(MAC_BROADCAST)) {
        Serial.println("[ERRO] Falha ao registrar peer broadcast");
        return;
    }

    Serial.println("[INFO] Peer Principal ainda nao detectado — modo descoberta em broadcast");

    Serial.println("[OK] ESP-NOW inicializado — Remote");
}

void Comunicacao::enviarPacote(const PacoteRemote& pacote) {
    PacoteRemote p = pacote;
    p.checksum = calcular_checksum((const uint8_t*)&p, sizeof(PacoteRemote) - 1);
    const uint8_t* destino = _peerPrincipalConhecido ? _macPrincipalAtual : MAC_BROADCAST;
    esp_now_send(destino, (const uint8_t*)&p, sizeof(PacoteRemote));
}
