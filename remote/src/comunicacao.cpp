/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Remote
 *
 * OnDataRecv: valida checksum do PacoteStatus e atualiza estado local.
 * enviarPacote: calcula checksum e envia PacoteRemote via ESP-NOW.
 *
 * Ref: comunicacao/SPEC.md §7, §9.1
 */

#include "comunicacao.h"
#include "logger.h"
#include <esp_system.h>

#ifndef SEC_PRINCIPAL_MAC_STR
#error "SEC_PRINCIPAL_MAC_STR nao definido. Configure via .env"
#endif
#ifndef SEC_ESPNOW_PMK_STR
#error "SEC_ESPNOW_PMK_STR nao definido. Configure via .env"
#endif
#ifndef SEC_ESPNOW_LMK_STR
#error "SEC_ESPNOW_LMK_STR nao definido. Configure via .env"
#endif

// Definição dos membros estáticos
volatile PacoteStatus Comunicacao::_ultimoStatus   = {};
volatile uint32_t     Comunicacao::_ultimoStatusMs = 0;

static uint8_t MAC_PRINCIPAL_ESPERADO[6] = {};
static uint8_t ESPNOW_PMK[16] = {};
static uint8_t ESPNOW_LMK[16] = {};
static uint32_t sessaoLocalRemote = 0;
static uint32_t seqPacoteEnvio = 0;
static bool sessaoPrincipalConhecida = false;
static uint32_t sessaoPrincipalAtual = 0;
static uint32_t ultimoSeqPrincipal = 0;
static uint32_t ultimoStatusValidoMs = 0;

static bool aceitarStatusPrincipal(uint32_t sessionId, uint32_t seq, uint32_t agora) {
    if (!sessaoPrincipalConhecida || (agora - ultimoStatusValidoMs) > WATCHDOG_TIMEOUT_MS) {
        sessaoPrincipalConhecida = true;
        sessaoPrincipalAtual = sessionId;
        ultimoSeqPrincipal = seq;
        ultimoStatusValidoMs = agora;
        return true;
    }

    if (sessionId != sessaoPrincipalAtual) {
        return false;
    }

    if (seq <= ultimoSeqPrincipal) {
        return false;
    }

    ultimoSeqPrincipal = seq;
    ultimoStatusValidoMs = agora;
    return true;
}

static bool registrarPeerPrincipal() {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MAC_PRINCIPAL_ESPERADO, 6);
    memcpy(peerInfo.lmk, ESPNOW_LMK, 16);
    peerInfo.channel = 0;
    peerInfo.encrypt = true;

    if (esp_now_is_peer_exist(MAC_PRINCIPAL_ESPERADO)) {
        return true;
    }

    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacao::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len != sizeof(PacoteStatus)) {
        return;
    }

    if (info == nullptr || info->src_addr == nullptr || memcmp(info->src_addr, MAC_PRINCIPAL_ESPERADO, 6) != 0) {
        return;
    }

    PacoteStatus status;
    memcpy(&status, data, sizeof(PacoteStatus));

    // Validar checksum
    uint8_t cs = calcular_checksum((const uint8_t*)&status, sizeof(PacoteStatus) - 1);
    if (cs != status.checksum) {
        return;
    }
    if (calcular_auth_tag(status, ESPNOW_LMK) != status.auth_tag) {
        return;
    }
    if (!aceitarStatusPrincipal(status.session_id, status.seq, millis())) {
        return;
    }

    // Pacote válido — atualizar estado local
    memcpy((void*)&_ultimoStatus, &status, sizeof(PacoteStatus));
    _ultimoStatusMs = millis();
}
#else
void Comunicacao::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(PacoteStatus)) {
        return;
    }

    if (mac == nullptr || memcmp(mac, MAC_PRINCIPAL_ESPERADO, 6) != 0) {
        return;
    }

    PacoteStatus status;
    memcpy(&status, data, sizeof(PacoteStatus));

    // Validar checksum
    uint8_t cs = calcular_checksum((const uint8_t*)&status, sizeof(PacoteStatus) - 1);
    if (cs != status.checksum) {
        return;
    }
    if (calcular_auth_tag(status, ESPNOW_LMK) != status.auth_tag) {
        return;
    }
    if (!aceitarStatusPrincipal(status.session_id, status.seq, millis())) {
        return;
    }

    // Pacote válido — atualizar estado local
    memcpy((void*)&_ultimoStatus, &status, sizeof(PacoteStatus));
    _ultimoStatusMs = millis();
}
#endif

void Comunicacao::init() {
    if (!parseMacString(SEC_PRINCIPAL_MAC_STR, MAC_PRINCIPAL_ESPERADO) ||
        !parseHexKey16(SEC_ESPNOW_PMK_STR, ESPNOW_PMK) ||
        !parseHexKey16(SEC_ESPNOW_LMK_STR, ESPNOW_LMK)) {
        LOG_ERROR("ESP-NOW", "Configuracao de seguranca invalida");
        return;
    }

    do {
        sessaoLocalRemote = esp_random();
    } while (sessaoLocalRemote == 0);
    seqPacoteEnvio = 0;
    sessaoPrincipalConhecida = false;
    ultimoSeqPrincipal = 0;
    ultimoStatusValidoMs = 0;

    // WiFi em modo station (necessário para ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        LOG_ERROR("ESP-NOW", "Falha ao inicializar ESP-NOW");
        return;
    }
    if (esp_now_set_pmk(ESPNOW_PMK) != ESP_OK) {
        LOG_ERROR("ESP-NOW", "Falha ao configurar PMK do ESP-NOW");
        return;
    }

    // Registrar callback de recepção
    esp_now_register_recv_cb(onDataRecv);

    if (!registrarPeerPrincipal()) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer principal criptografado");
        return;
    }

    char macPareado[18];
    snprintf(
        macPareado,
        sizeof(macPareado),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        MAC_PRINCIPAL_ESPERADO[0], MAC_PRINCIPAL_ESPERADO[1], MAC_PRINCIPAL_ESPERADO[2],
        MAC_PRINCIPAL_ESPERADO[3], MAC_PRINCIPAL_ESPERADO[4], MAC_PRINCIPAL_ESPERADO[5]
    );
    LOG_ALWAYS_VAL("ESP-NOW", "Peer principal configurado: ", macPareado);
}

void Comunicacao::enviarPacote(const PacoteRemote& pacote) {
    PacoteRemote p = pacote;
    p.seq = seqPacoteEnvio++;
    p.session_id = sessaoLocalRemote;
    p.auth_tag = calcular_auth_tag(p, ESPNOW_LMK);
    p.checksum = calcular_checksum((const uint8_t*)&p, sizeof(PacoteRemote) - 1);
    esp_now_send(MAC_PRINCIPAL_ESPERADO, (const uint8_t*)&p, sizeof(PacoteRemote));
}
