/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Repeater
 *
 * Valida MAC físico, direção lógica, checksum, auth_tag e anti-replay antes
 * de encaminhar comandos/status. Probes locais do Remote recebem ACK local.
 */

#include "comunicacao.h"
#include "logger.h"
#include <esp_system.h>

#ifndef SEC_PRINCIPAL_MAC_STR
#error "SEC_PRINCIPAL_MAC_STR nao definido. Configure via .env"
#endif
#ifndef SEC_REMOTE_MAC_STR
#error "SEC_REMOTE_MAC_STR nao definido. Configure via .env"
#endif
#ifndef SEC_REPEATER_MAC_STR
#error "SEC_REPEATER_MAC_STR nao definido. Configure REPEATER_MAC no .env"
#endif
#ifndef SEC_ESPNOW_PMK_STR
#error "SEC_ESPNOW_PMK_STR nao definido. Configure via .env"
#endif
#ifndef SEC_ESPNOW_LMK_STR
#error "SEC_ESPNOW_LMK_STR nao definido. Configure via .env"
#endif

struct ReplayState {
    bool conhecida = false;
    uint32_t session = 0;
    uint32_t seq = 0;
    uint32_t ultimoMs = 0;
};

static uint8_t MAC_PRINCIPAL_ESPERADO[6] = {};
static uint8_t MAC_REMOTE_ESPERADO[6] = {};
static uint8_t MAC_REPEATER_LOCAL[6] = {};
static uint8_t ESPNOW_PMK[16] = {};
static uint8_t ESPNOW_LMK[16] = {};
static uint32_t sessaoLocalRepeater = 0;
static uint32_t seqLinkAckEnvio = 0;
static ReplayState replayCmdRemote;
static ReplayState replayStatusPrincipal;
static ReplayState replayProbeRemote;

static bool macIgual(const uint8_t* a, const uint8_t* b) {
    return a != nullptr && b != nullptr && memcmp(a, b, 6) == 0;
}

static bool aceitarSequencia(ReplayState& replay, uint32_t sessionId, uint32_t seq, uint32_t agora) {
    if (!replay.conhecida || (agora - replay.ultimoMs) > LINK_QUALITY_TIMEOUT_MS) {
        replay.conhecida = true;
        replay.session = sessionId;
        replay.seq = seq;
        replay.ultimoMs = agora;
        return true;
    }

    if (sessionId != replay.session || seq <= replay.seq) {
        return false;
    }

    replay.seq = seq;
    replay.ultimoMs = agora;
    return true;
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

static bool validarComandoViaRepeater(const PacoteRemote& pacote, const uint8_t* macFisico) {
    return macIgual(macFisico, MAC_REMOTE_ESPERADO) &&
           pacote.header.tipo == PKT_REMOTE_CMD &&
           pacote.header.origem == NODE_REMOTE &&
           pacote.header.destino == NODE_PRINCIPAL &&
           pacote.header.rota == ROUTE_VIA_REPEATER &&
           pacote.header.hop_count == 1;
}

static bool validarStatusViaRepeater(const PacoteStatus& status, const uint8_t* macFisico) {
    return macIgual(macFisico, MAC_PRINCIPAL_ESPERADO) &&
           status.header.tipo == PKT_STATUS &&
           status.header.origem == NODE_PRINCIPAL &&
           status.header.destino == NODE_REMOTE &&
           status.header.rota == ROUTE_VIA_REPEATER &&
           status.header.hop_count == 1;
}

static bool validarProbeLocal(const PacoteLink& probe, const uint8_t* macFisico) {
    return macIgual(macFisico, MAC_REMOTE_ESPERADO) &&
           probe.header.tipo == PKT_LINK_PROBE &&
           probe.header.origem == NODE_REMOTE &&
           probe.header.destino == NODE_REPEATER &&
           probe.header.rota == ROUTE_DIRECT &&
           probe.header.hop_count == 0;
}

static void enviarAckLocal() {
    PacoteLink ack = {};
    ack.header.tipo = PKT_LINK_ACK;
    ack.header.origem = NODE_REPEATER;
    ack.header.destino = NODE_REMOTE;
    ack.header.rota = ROUTE_DIRECT;
    ack.header.hop_count = 0;
    ack.timestamp = millis();
    ack.seq = seqLinkAckEnvio++;
    ack.session_id = sessaoLocalRepeater;
    ack.auth_tag = calcular_auth_tag(ack, ESPNOW_LMK);
    ack.checksum = calcular_checksum((const uint8_t*)&ack, sizeof(PacoteLink) - 1);

    esp_err_t resultado = esp_now_send(MAC_REMOTE_ESPERADO, (const uint8_t*)&ack, sizeof(PacoteLink));
    if (resultado != ESP_OK) {
        LOG_WARN("REPEATER", "Falha ao enviar LINK_ACK para Remote");
    } else {
        LOG_INFO("REPEATER", "LINK_ACK encaminhado para Remote");
    }
}

static void processarComandoRemote(const uint8_t* macFisico, const uint8_t* data) {
    PacoteRemote pacote;
    memcpy(&pacote, data, sizeof(PacoteRemote));

    if (!validarComandoViaRepeater(pacote, macFisico)) {
        LOG_WARN("REPEATER", "Comando rejeitado: MAC ou direcao invalida");
        return;
    }
    uint8_t cs = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteRemote) - 1);
    if (cs != pacote.checksum) {
        LOG_WARN("REPEATER", "Comando rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(pacote, ESPNOW_LMK) != pacote.auth_tag) {
        LOG_WARN("REPEATER", "Comando rejeitado: auth_tag invalido");
        return;
    }
    if (!aceitarSequencia(replayCmdRemote, pacote.session_id, pacote.seq, millis())) {
        LOG_INFO("REPEATER", "Comando rejeitado: replay/duplicado");
        return;
    }

    LOG_INFO_VAL("REPEATER", "Pacote recebido do Remote: ", comandoParaString(pacote.comando));
    esp_err_t resultado = esp_now_send(MAC_PRINCIPAL_ESPERADO, data, sizeof(PacoteRemote));
    if (resultado != ESP_OK) {
        LOG_WARN("REPEATER", "Falha ao encaminhar comando para Principal");
    } else {
        LOG_INFO("REPEATER", "Comando encaminhado para Principal");
    }
}

static void processarStatusPrincipal(const uint8_t* macFisico, const uint8_t* data) {
    PacoteStatus status;
    memcpy(&status, data, sizeof(PacoteStatus));

    if (!validarStatusViaRepeater(status, macFisico)) {
        LOG_WARN("REPEATER", "Status rejeitado: MAC ou direcao invalida");
        return;
    }
    uint8_t cs = calcular_checksum((const uint8_t*)&status, sizeof(PacoteStatus) - 1);
    if (cs != status.checksum) {
        LOG_WARN("REPEATER", "Status rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(status, ESPNOW_LMK) != status.auth_tag) {
        LOG_WARN("REPEATER", "Status rejeitado: auth_tag invalido");
        return;
    }
    if (!aceitarSequencia(replayStatusPrincipal, status.session_id, status.seq, millis())) {
        LOG_INFO("REPEATER", "Status rejeitado: replay/duplicado");
        return;
    }

    LOG_INFO("REPEATER", "Status recebido do Principal");
    esp_err_t resultado = esp_now_send(MAC_REMOTE_ESPERADO, data, sizeof(PacoteStatus));
    if (resultado != ESP_OK) {
        LOG_WARN("REPEATER", "Falha ao encaminhar status para Remote");
    } else {
        LOG_INFO("REPEATER", "Status encaminhado para Remote");
    }
}

static void processarProbeRemote(const uint8_t* macFisico, const uint8_t* data) {
    PacoteLink probe;
    memcpy(&probe, data, sizeof(PacoteLink));

    if (!validarProbeLocal(probe, macFisico)) {
        LOG_WARN("REPEATER", "Probe rejeitado: MAC ou direcao invalida");
        return;
    }
    uint8_t cs = calcular_checksum((const uint8_t*)&probe, sizeof(PacoteLink) - 1);
    if (cs != probe.checksum) {
        LOG_WARN("REPEATER", "Probe rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(probe, ESPNOW_LMK) != probe.auth_tag) {
        LOG_WARN("REPEATER", "Probe rejeitado: auth_tag invalido");
        return;
    }
    if (!aceitarSequencia(replayProbeRemote, probe.session_id, probe.seq, millis())) {
        LOG_INFO("REPEATER", "Probe rejeitado: replay/duplicado");
        return;
    }

    LOG_INFO("REPEATER", "Probe recebido do Remote");
    enviarAckLocal();
}

static void processarRecebimento(const uint8_t* macFisico, const uint8_t* data, int len) {
    if (macFisico == nullptr || data == nullptr) {
        return;
    }

    if (!macIgual(macFisico, MAC_REMOTE_ESPERADO) && !macIgual(macFisico, MAC_PRINCIPAL_ESPERADO)) {
        LOG_WARN("REPEATER", "Pacote rejeitado: MAC fisico nao autorizado");
        return;
    }

    if (len == sizeof(PacoteRemote)) {
        processarComandoRemote(macFisico, data);
        return;
    }
    if (len == sizeof(PacoteStatus)) {
        processarStatusPrincipal(macFisico, data);
        return;
    }
    if (len == sizeof(PacoteLink)) {
        processarProbeRemote(macFisico, data);
        return;
    }

    LOG_WARN("REPEATER", "Pacote rejeitado: tamanho/tipo invalido");
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacao::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info == nullptr) {
        return;
    }
    processarRecebimento(info->src_addr, data, len);
}
#else
void Comunicacao::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    processarRecebimento(mac, data, len);
}
#endif

void Comunicacao::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        return;
    }

    if (macIgual(mac, MAC_PRINCIPAL_ESPERADO)) {
        LOG_WARN("REPEATER", "Falha confirmada no envio para Principal");
    } else if (macIgual(mac, MAC_REMOTE_ESPERADO)) {
        LOG_WARN("REPEATER", "Falha confirmada no envio para Remote");
    }
}

void Comunicacao::init() {
    if (!parseMacString(SEC_PRINCIPAL_MAC_STR, MAC_PRINCIPAL_ESPERADO) ||
        !parseMacString(SEC_REMOTE_MAC_STR, MAC_REMOTE_ESPERADO) ||
        !parseMacString(SEC_REPEATER_MAC_STR, MAC_REPEATER_LOCAL) ||
        !parseHexKey16(SEC_ESPNOW_PMK_STR, ESPNOW_PMK) ||
        !parseHexKey16(SEC_ESPNOW_LMK_STR, ESPNOW_LMK)) {
        LOG_ERROR("ESP-NOW", "Configuracao de seguranca invalida");
        return;
    }

    do {
        sessaoLocalRepeater = esp_random();
    } while (sessaoLocalRepeater == 0);
    seqLinkAckEnvio = 0;
    replayCmdRemote = ReplayState();
    replayStatusPrincipal = ReplayState();
    replayProbeRemote = ReplayState();

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
    esp_now_register_send_cb(onDataSent);

    if (!registrarPeer(MAC_REMOTE_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer Remote criptografado");
        return;
    }
    if (!registrarPeer(MAC_PRINCIPAL_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer Principal criptografado");
        return;
    }

    LOG_ALWAYS("ESP-NOW", "Peers Remote e Principal configurados");
}
