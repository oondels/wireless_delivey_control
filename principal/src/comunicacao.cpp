/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Principal
 *
 * OnDataRecv: valida checksum, reseta watchdog, processa emergência
 * imediata se emergencia == 1.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#include "comunicacao.h"

// Definição dos membros estáticos
WatchdogComm*          Comunicacao::_pWatchdog   = nullptr;
Emergencia*            Comunicacao::_pEmergencia = nullptr;
volatile PacoteRemote  Comunicacao::_ultimoPacote = {};
volatile bool          Comunicacao::_novoPacote   = false;

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

    // Pacote válido — resetar watchdog
    if (_pWatchdog) {
        _pWatchdog->resetar();
    }

    // Processar emergência imediatamente (prioridade máxima)
    if (pacote.emergencia == 1 && _pEmergencia) {
        _pEmergencia->ativa() = true;
        // LOG de emergencia Remote é feito em emergencia.cpp
    }

    // Copiar pacote para variável estática
    memcpy((void*)&_ultimoPacote, &pacote, sizeof(PacoteRemote));
    _novoPacote = true;
}

void Comunicacao::init(WatchdogComm& watchdog, Emergencia& emergencia) {
    _pWatchdog   = &watchdog;
    _pEmergencia = &emergencia;

    // WiFi em modo station (necessário para ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERRO] Falha ao inicializar ESP-NOW");
        return;
    }

    // Registrar callback de recepção
    esp_now_register_recv_cb(onDataRecv);

    // Registrar peer (Remote)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, _macRemote, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERRO] Falha ao registrar peer Remote");
        return;
    }

    Serial.println("[OK] ESP-NOW inicializado — Principal");
}

void Comunicacao::enviarStatus(const PacoteStatus& status) {
    PacoteStatus pacote = status;
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteStatus) - 1);
    esp_now_send(_macRemote, (const uint8_t*)&pacote, sizeof(PacoteStatus));
}
