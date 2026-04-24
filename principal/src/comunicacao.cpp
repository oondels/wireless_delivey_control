/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Principal
 *
 * OnDataRecv: valida checksum e reseta watchdog.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#include "comunicacao.h"
#include <esp_system.h>

#ifndef SEC_REMOTE_MAC_STR
#error "SEC_REMOTE_MAC_STR nao definido. Configure via .env"
#endif
#ifndef SEC_ESPNOW_PMK_STR
#error "SEC_ESPNOW_PMK_STR nao definido. Configure via .env"
#endif
#ifndef SEC_ESPNOW_LMK_STR
#error "SEC_ESPNOW_LMK_STR nao definido. Configure via .env"
#endif

// Definição dos membros estáticos
WatchdogComm*          Comunicacao::_pWatchdog    = nullptr;
volatile PacoteRemote  Comunicacao::_ultimoPacote = {};
volatile bool          Comunicacao::_novoPacote   = false;

static uint8_t MAC_REMOTE_ESPERADO[6] = {};
static uint8_t ESPNOW_PMK[16] = {};
static uint8_t ESPNOW_LMK[16] = {};
static uint32_t sessaoLocalPrincipal = 0;
static uint32_t seqStatusEnvio = 0;
static bool sessaoRemoteConhecida = false;
static uint32_t sessaoRemoteAtual = 0;
static uint32_t ultimoSeqRemote = 0;
static uint32_t ultimoPacoteRemoteMs = 0;

static bool aceitarPacoteRemote(uint32_t sessionId, uint32_t seq, uint32_t agora) {
    if (!sessaoRemoteConhecida || (agora - ultimoPacoteRemoteMs) > WATCHDOG_TIMEOUT_MS) {
        sessaoRemoteConhecida = true;
        sessaoRemoteAtual = sessionId;
        ultimoSeqRemote = seq;
        ultimoPacoteRemoteMs = agora;
        return true;
    }

    if (sessionId != sessaoRemoteAtual) {
        return false;
    }

    if (seq <= ultimoSeqRemote) {
        return false;
    }

    ultimoSeqRemote = seq;
    ultimoPacoteRemoteMs = agora;
    return true;
}

static bool registrarPeerRemoto() {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MAC_REMOTE_ESPERADO, 6);
    memcpy(peerInfo.lmk, ESPNOW_LMK, 16);
    peerInfo.channel = 0;
    peerInfo.encrypt = true;

    if (esp_now_is_peer_exist(MAC_REMOTE_ESPERADO)) {
        return true;
    }

    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacao::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len != sizeof(PacoteRemote)) {
        return;
    }

    if (info == nullptr || info->src_addr == nullptr || memcmp(info->src_addr, MAC_REMOTE_ESPERADO, 6) != 0) {
        return;
    }

    PacoteRemote pacote;
    memcpy(&pacote, data, sizeof(PacoteRemote));

    // Validar checksum — pacotes inválidos são descartados sem resetar watchdog
    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteRemote) - 1);
    if (cs != pacote.checksum) {
        return;
    }
    if (calcular_auth_tag(pacote, ESPNOW_LMK) != pacote.auth_tag) {
        return;
    }
    if (!aceitarPacoteRemote(pacote.session_id, pacote.seq, millis())) {
        return;
    }

    // Pacote válido — resetar watchdog
    if (_pWatchdog) {
        _pWatchdog->resetar();
    }

    // Copiar pacote para variável estática
    memcpy((void*)&_ultimoPacote, &pacote, sizeof(PacoteRemote));
    _novoPacote = true;
}
#else
void Comunicacao::onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (len != sizeof(PacoteRemote)) {
        return;
    }

    if (mac_addr == nullptr || memcmp(mac_addr, MAC_REMOTE_ESPERADO, 6) != 0) {
        return;
    }

    PacoteRemote pacote;
    memcpy(&pacote, data, sizeof(PacoteRemote));

    // Validar checksum — pacotes inválidos são descartados sem resetar watchdog
    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteRemote) - 1);
    if (cs != pacote.checksum) {
        return;
    }
    if (calcular_auth_tag(pacote, ESPNOW_LMK) != pacote.auth_tag) {
        return;
    }
    if (!aceitarPacoteRemote(pacote.session_id, pacote.seq, millis())) {
        return;
    }

    // Pacote válido — resetar watchdog
    if (_pWatchdog) {
        _pWatchdog->resetar();
    }

    // Copiar pacote para variável estática
    memcpy((void*)&_ultimoPacote, &pacote, sizeof(PacoteRemote));
    _novoPacote = true;
}
#endif

void Comunicacao::init(WatchdogComm& watchdog) {
    _pWatchdog   = &watchdog;

    if (!parseMacString(SEC_REMOTE_MAC_STR, MAC_REMOTE_ESPERADO) ||
        !parseHexKey16(SEC_ESPNOW_PMK_STR, ESPNOW_PMK) ||
        !parseHexKey16(SEC_ESPNOW_LMK_STR, ESPNOW_LMK)) {
        Serial.println("[ERRO] Configuracao de seguranca invalida");
        return;
    }

    do {
        sessaoLocalPrincipal = esp_random();
    } while (sessaoLocalPrincipal == 0);
    seqStatusEnvio = 0;
    sessaoRemoteConhecida = false;
    ultimoSeqRemote = 0;
    ultimoPacoteRemoteMs = 0;

    // WiFi em modo station (necessário para ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERRO] Falha ao inicializar ESP-NOW");
        return;
    }
    if (esp_now_set_pmk(ESPNOW_PMK) != ESP_OK) {
        Serial.println("[ERRO] Falha ao configurar PMK do ESP-NOW");
        return;
    }

    // Registrar callback de recepção
    esp_now_register_recv_cb(onDataRecv);

    if (!registrarPeerRemoto()) {
        Serial.println("[ERRO] Falha ao registrar peer remoto criptografado");
        return;
    }

    Serial.printf(
        "[OK] ESP-NOW inicializado — Principal pareado com %02X:%02X:%02X:%02X:%02X:%02X\n",
        MAC_REMOTE_ESPERADO[0], MAC_REMOTE_ESPERADO[1], MAC_REMOTE_ESPERADO[2],
        MAC_REMOTE_ESPERADO[3], MAC_REMOTE_ESPERADO[4], MAC_REMOTE_ESPERADO[5]
    );
}

void Comunicacao::enviarStatus(const PacoteStatus& status) {
    PacoteStatus pacote = status;
    pacote.seq = seqStatusEnvio++;
    pacote.session_id = sessaoLocalPrincipal;
    pacote.auth_tag = calcular_auth_tag(pacote, ESPNOW_LMK);
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteStatus) - 1);
    esp_now_send(MAC_REMOTE_ESPERADO, (const uint8_t*)&pacote, sizeof(PacoteStatus));
}
