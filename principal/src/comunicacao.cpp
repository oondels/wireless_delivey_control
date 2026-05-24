/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Principal
 *
 * Recebe comandos do Remote por rota direta ou via Repeater. Apenas pacotes
 * PKT_REMOTE_CMD válidos resetam o watchdog e chegam ao loop operacional.
 *
 * Ref: comunicacao/SPEC.md
 */

#include "comunicacao.h"
#include "logger.h"
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
#ifndef SEC_ENABLE_REPEATER_ROUTE
#define SEC_ENABLE_REPEATER_ROUTE 0
#endif
#if SEC_ENABLE_REPEATER_ROUTE && !defined(SEC_REPEATER_MAC_STR)
#error "SEC_REPEATER_MAC_STR nao definido. Configure REPEATER_MAC no .env"
#endif

// Definição dos membros estáticos
WatchdogComm*          Comunicacao::_pWatchdog    = nullptr;
volatile PacoteRemote  Comunicacao::_ultimoPacote = {};
volatile bool          Comunicacao::_novoPacote   = false;

static uint8_t MAC_REMOTE_ESPERADO[6] = {};
#if SEC_ENABLE_REPEATER_ROUTE
static uint8_t MAC_REPEATER_ESPERADO[6] = {};
#endif
static uint8_t ESPNOW_PMK[16] = {};
static uint8_t ESPNOW_LMK[16] = {};
static uint32_t sessaoLocalPrincipal = 0;
static uint32_t seqStatusEnvio = 0;
static uint32_t seqLinkAckEnvio = 0;
static bool sessaoRemoteConhecida = false;
static uint32_t sessaoRemoteAtual = 0;
static uint32_t ultimoSeqRemote = 0;
static uint32_t ultimoPacoteRemoteMs = 0;
static bool sessaoProbeRemoteConhecida = false;
static uint32_t sessaoProbeRemoteAtual = 0;
static uint32_t ultimoSeqProbeRemote = 0;

static bool macIgual(const uint8_t* a, const uint8_t* b) {
    return a != nullptr && b != nullptr && memcmp(a, b, 6) == 0;
}

static bool aceitarSequencia(
    uint32_t sessionId,
    uint32_t seq,
    uint32_t agora,
    bool& sessaoConhecida,
    uint32_t& sessaoAtual,
    uint32_t& ultimoSeq,
    uint32_t* ultimoPacoteMs
) {
    if (!sessaoConhecida || (ultimoPacoteMs != nullptr && (agora - *ultimoPacoteMs) > WATCHDOG_TIMEOUT_MS)) {
        sessaoConhecida = true;
        sessaoAtual = sessionId;
        ultimoSeq = seq;
        if (ultimoPacoteMs != nullptr) {
            *ultimoPacoteMs = agora;
        }
        return true;
    }

    if (sessionId != sessaoAtual || seq <= ultimoSeq) {
        return false;
    }

    ultimoSeq = seq;
    if (ultimoPacoteMs != nullptr) {
        *ultimoPacoteMs = agora;
    }
    return true;
}

static bool aceitarPacoteRemote(uint32_t sessionId, uint32_t seq, uint32_t agora) {
    return aceitarSequencia(
        sessionId,
        seq,
        agora,
        sessaoRemoteConhecida,
        sessaoRemoteAtual,
        ultimoSeqRemote,
        &ultimoPacoteRemoteMs
    );
}

static bool aceitarProbeRemote(uint32_t sessionId, uint32_t seq, uint32_t agora) {
    return aceitarSequencia(
        sessionId,
        seq,
        agora,
        sessaoProbeRemoteConhecida,
        sessaoProbeRemoteAtual,
        ultimoSeqProbeRemote,
        nullptr
    );
}

static bool registrarPeer(const uint8_t mac[6]) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    memcpy(peerInfo.lmk, ESPNOW_LMK, 16);
    peerInfo.channel = 0;
    peerInfo.encrypt = true;

    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

static bool headerComandoValido(const PacoteRemote& pacote, const uint8_t* macFisico) {
    if (pacote.header.tipo != PKT_REMOTE_CMD ||
        pacote.header.origem != NODE_REMOTE ||
        pacote.header.destino != NODE_PRINCIPAL) {
        return false;
    }

    if (macIgual(macFisico, MAC_REMOTE_ESPERADO)) {
        return pacote.header.rota == ROUTE_DIRECT && pacote.header.hop_count == 0;
    }

#if SEC_ENABLE_REPEATER_ROUTE
    if (macIgual(macFisico, MAC_REPEATER_ESPERADO)) {
        return pacote.header.rota == ROUTE_VIA_REPEATER && pacote.header.hop_count == 1;
    }
#endif

    return false;
}

static bool headerProbeValido(const PacoteLink& pacote, const uint8_t* macFisico) {
    return macIgual(macFisico, MAC_REMOTE_ESPERADO) &&
           pacote.header.tipo == PKT_LINK_PROBE &&
           pacote.header.origem == NODE_REMOTE &&
           pacote.header.destino == NODE_PRINCIPAL &&
           pacote.header.rota == ROUTE_DIRECT &&
           pacote.header.hop_count == 0;
}

static void enviarLinkAckDireto() {
    PacoteLink ack = {};
    ack.header.tipo = PKT_LINK_ACK;
    ack.header.origem = NODE_PRINCIPAL;
    ack.header.destino = NODE_REMOTE;
    ack.header.rota = ROUTE_DIRECT;
    ack.header.hop_count = 0;
    ack.timestamp = millis();
    ack.seq = seqLinkAckEnvio++;
    ack.session_id = sessaoLocalPrincipal;
    ack.auth_tag = calcular_auth_tag(ack, ESPNOW_LMK);
    ack.checksum = calcular_checksum((const uint8_t*)&ack, sizeof(PacoteLink) - 1);
    esp_now_send(MAC_REMOTE_ESPERADO, (const uint8_t*)&ack, sizeof(PacoteLink));
}

static void processarPacoteRemote(const uint8_t* macFisico, const uint8_t* data) {
    PacoteRemote pacote;
    memcpy(&pacote, data, sizeof(PacoteRemote));

    if (!headerComandoValido(pacote, macFisico)) {
        LOG_WARN("ESP-NOW", "Pacote Remote rejeitado: origem/rota invalida");
        return;
    }

    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteRemote) - 1);
    if (cs != pacote.checksum) {
        LOG_WARN("ESP-NOW", "Pacote Remote rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(pacote, ESPNOW_LMK) != pacote.auth_tag) {
        LOG_WARN("ESP-NOW", "Pacote Remote rejeitado: auth_tag invalido");
        return;
    }
    if (!aceitarPacoteRemote(pacote.session_id, pacote.seq, millis())) {
        LOG_INFO("ESP-NOW", "Pacote Remote rejeitado: replay/duplicado");
        return;
    }

    if (Comunicacao::_pWatchdog) {
        Comunicacao::_pWatchdog->resetar();
    }

    memcpy((void*)&Comunicacao::_ultimoPacote, &pacote, sizeof(PacoteRemote));
    Comunicacao::_novoPacote = true;
}

static void processarLinkProbe(const uint8_t* macFisico, const uint8_t* data) {
    PacoteLink pacote;
    memcpy(&pacote, data, sizeof(PacoteLink));

    if (!headerProbeValido(pacote, macFisico)) {
        LOG_WARN("ESP-NOW", "Probe rejeitado: origem/rota invalida");
        return;
    }

    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteLink) - 1);
    if (cs != pacote.checksum) {
        LOG_WARN("ESP-NOW", "Probe rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(pacote, ESPNOW_LMK) != pacote.auth_tag) {
        LOG_WARN("ESP-NOW", "Probe rejeitado: auth_tag invalido");
        return;
    }
    if (!aceitarProbeRemote(pacote.session_id, pacote.seq, millis())) {
        LOG_INFO("ESP-NOW", "Probe rejeitado: replay/duplicado");
        return;
    }

    enviarLinkAckDireto();
}

static void processarRecebimento(const uint8_t* macFisico, const uint8_t* data, int len) {
    if (macFisico == nullptr || data == nullptr) {
        return;
    }

    const bool macAutorizado =
        macIgual(macFisico, MAC_REMOTE_ESPERADO)
#if SEC_ENABLE_REPEATER_ROUTE
        || macIgual(macFisico, MAC_REPEATER_ESPERADO)
#endif
    ;
    if (!macAutorizado) {
        LOG_WARN("ESP-NOW", "Pacote rejeitado: MAC fisico nao autorizado");
        return;
    }

    if (len == sizeof(PacoteRemote)) {
        processarPacoteRemote(macFisico, data);
        return;
    }

    if (len == sizeof(PacoteLink)) {
        processarLinkProbe(macFisico, data);
        return;
    }

    LOG_WARN("ESP-NOW", "Pacote rejeitado: tamanho/tipo invalido");
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacao::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info == nullptr) {
        return;
    }
    processarRecebimento(info->src_addr, data, len);
}
#else
void Comunicacao::onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    processarRecebimento(mac_addr, data, len);
}
#endif

void Comunicacao::init(WatchdogComm& watchdog) {
    _pWatchdog = &watchdog;

    if (!parseMacString(SEC_REMOTE_MAC_STR, MAC_REMOTE_ESPERADO) ||
        !parseHexKey16(SEC_ESPNOW_PMK_STR, ESPNOW_PMK) ||
        !parseHexKey16(SEC_ESPNOW_LMK_STR, ESPNOW_LMK)) {
        LOG_ERROR("ESP-NOW", "Configuracao de seguranca invalida");
        return;
    }

#if SEC_ENABLE_REPEATER_ROUTE
    if (!parseMacString(SEC_REPEATER_MAC_STR, MAC_REPEATER_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "MAC do Repeater invalido");
        return;
    }
#endif

    do {
        sessaoLocalPrincipal = esp_random();
    } while (sessaoLocalPrincipal == 0);
    seqStatusEnvio = 0;
    seqLinkAckEnvio = 0;
    sessaoRemoteConhecida = false;
    ultimoSeqRemote = 0;
    ultimoPacoteRemoteMs = 0;
    sessaoProbeRemoteConhecida = false;
    ultimoSeqProbeRemote = 0;

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

    esp_now_register_recv_cb(onDataRecv);

    if (!registrarPeer(MAC_REMOTE_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer remoto criptografado");
        return;
    }

#if SEC_ENABLE_REPEATER_ROUTE
    if (!registrarPeer(MAC_REPEATER_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer repeater criptografado");
        return;
    }
#endif

    char macPareado[18];
    snprintf(
        macPareado,
        sizeof(macPareado),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        MAC_REMOTE_ESPERADO[0], MAC_REMOTE_ESPERADO[1], MAC_REMOTE_ESPERADO[2],
        MAC_REMOTE_ESPERADO[3], MAC_REMOTE_ESPERADO[4], MAC_REMOTE_ESPERADO[5]
    );
    LOG_ALWAYS_VAL("ESP-NOW", "Peer remoto configurado: ", macPareado);

#if SEC_ENABLE_REPEATER_ROUTE
    snprintf(
        macPareado,
        sizeof(macPareado),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        MAC_REPEATER_ESPERADO[0], MAC_REPEATER_ESPERADO[1], MAC_REPEATER_ESPERADO[2],
        MAC_REPEATER_ESPERADO[3], MAC_REPEATER_ESPERADO[4], MAC_REPEATER_ESPERADO[5]
    );
    LOG_ALWAYS_VAL("ESP-NOW", "Peer repeater configurado: ", macPareado);
#endif
}

static void enviarStatusRota(const PacoteStatus& status, uint8_t rota, uint32_t seq, const uint8_t macDestino[6]) {
    PacoteStatus pacote = status;
    pacote.header.tipo = PKT_STATUS;
    pacote.header.origem = NODE_PRINCIPAL;
    pacote.header.destino = NODE_REMOTE;
    pacote.header.rota = rota;
    pacote.header.hop_count = (rota == ROUTE_VIA_REPEATER) ? 1 : 0;
    pacote.seq = seq;
    pacote.session_id = sessaoLocalPrincipal;
    pacote.auth_tag = calcular_auth_tag(pacote, ESPNOW_LMK);
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteStatus) - 1);
    esp_now_send(macDestino, (const uint8_t*)&pacote, sizeof(PacoteStatus));
}

void Comunicacao::enviarStatus(const PacoteStatus& status) {
    const uint32_t seq = seqStatusEnvio++;
    enviarStatusRota(status, ROUTE_DIRECT, seq, MAC_REMOTE_ESPERADO);

#if SEC_ENABLE_REPEATER_ROUTE
    enviarStatusRota(status, ROUTE_VIA_REPEATER, seq, MAC_REPEATER_ESPERADO);
#endif
}
