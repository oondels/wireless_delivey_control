/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Principal
 *
 * OnDataRecv: valida checksum e reseta watchdog.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#include "comunicacao.h"

constexpr uint8_t Comunicacao::MAC_BROADCAST[6];

// Definição dos membros estáticos
WatchdogComm*          Comunicacao::_pWatchdog   = nullptr;
volatile PacoteRemote  Comunicacao::_ultimoPacote = {};
volatile bool          Comunicacao::_novoPacote   = false;
uint8_t                Comunicacao::_macRemoteAtual[6] = {
    Comunicacao::MAC_BROADCAST[0],
    Comunicacao::MAC_BROADCAST[1],
    Comunicacao::MAC_BROADCAST[2],
    Comunicacao::MAC_BROADCAST[3],
    Comunicacao::MAC_BROADCAST[4],
    Comunicacao::MAC_BROADCAST[5]
};
bool                   Comunicacao::_peerRemotoConhecido = false;

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

void Comunicacao::atualizarPeerRemoto(const uint8_t* mac) {
    if (mac == nullptr) {
        return;
    }

    if (_peerRemotoConhecido && memcmp(_macRemoteAtual, mac, 6) == 0) {
        return;
    }

    if (!registrarPeer(mac)) {
        return;
    }

    memcpy(_macRemoteAtual, mac, 6);
    _peerRemotoConhecido = true;

    Serial.printf(
        "[INFO] Peer MAC Remote detectado: %02X:%02X:%02X:%02X:%02X:%02X\n",
        _macRemoteAtual[0],
        _macRemoteAtual[1],
        _macRemoteAtual[2],
        _macRemoteAtual[3],
        _macRemoteAtual[4],
        _macRemoteAtual[5]
    );
}

void Comunicacao::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
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

    atualizarPeerRemoto(mac);

    // Pacote válido — resetar watchdog
    if (_pWatchdog) {
        _pWatchdog->resetar();
    }

    // Copiar pacote para variável estática
    memcpy((void*)&_ultimoPacote, &pacote, sizeof(PacoteRemote));
    _novoPacote = true;
}

void Comunicacao::init(WatchdogComm& watchdog) {
    _pWatchdog   = &watchdog;

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

    Serial.println("[INFO] Peer Remote ainda nao detectado — modo descoberta em broadcast");

    Serial.println("[OK] ESP-NOW inicializado — Principal");
}

void Comunicacao::enviarStatus(const PacoteStatus& status) {
    PacoteStatus pacote = status;
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteStatus) - 1);
    const uint8_t* destino = _peerRemotoConhecido ? _macRemoteAtual : MAC_BROADCAST;
    esp_now_send(destino, (const uint8_t*)&pacote, sizeof(PacoteStatus));
}
