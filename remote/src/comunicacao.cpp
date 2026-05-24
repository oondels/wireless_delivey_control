/**
 * comunicacao.cpp — Implementação ESP-NOW do Módulo Remote
 *
 * Mantém rota direta e rota via Repeater, mede qualidade de link e troca
 * preventivamente para a rota mais estável sem esperar timeout.
 *
 * Ref: comunicacao/SPEC.md
 */

#include "comunicacao.h"
#include "logger.h"
#include <esp_system.h>
#include <esp_wifi_types.h>

#ifndef SEC_PRINCIPAL_MAC_STR
#error "SEC_PRINCIPAL_MAC_STR nao definido. Configure via .env"
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
#ifndef SEC_PREFER_DIRECT_ROUTE
#define SEC_PREFER_DIRECT_ROUTE 0
#endif
#if SEC_ENABLE_REPEATER_ROUTE && !defined(SEC_REPEATER_MAC_STR)
#error "SEC_REPEATER_MAC_STR nao definido. Configure REPEATER_MAC no .env"
#endif

struct ReplayState {
    bool conhecida = false;
    uint32_t session = 0;
    uint32_t seq = 0;
    uint32_t ultimoMs = 0;
};

struct QualidadeRota {
    uint32_t ultimoStatusMs = 0;
    uint32_t ultimoLinkAckMs = 0;
    uint32_t ultimoEnvioOkMs = 0;
    uint32_t ultimaFalhaEnvioMs = 0;
    int16_t rssiEwma = RSSI_INVALIDO_DBM;
    uint8_t falhasRecentes = 0;
};

// Definição dos membros estáticos
volatile PacoteStatus Comunicacao::_ultimoStatus   = {};
volatile uint32_t     Comunicacao::_ultimoStatusMs = 0;
volatile uint8_t      Comunicacao::_rotaAtual      = ROUTE_DIRECT;

static uint8_t MAC_PRINCIPAL_ESPERADO[6] = {};
#if SEC_ENABLE_REPEATER_ROUTE
static uint8_t MAC_REPEATER_ESPERADO[6] = {};
#endif
static uint8_t ESPNOW_PMK[16] = {};
static uint8_t ESPNOW_LMK[16] = {};
static uint32_t sessaoLocalRemote = 0;
static uint32_t seqPacoteEnvio = 0;
static uint32_t seqProbeEnvio = 0;
static uint32_t ultimoProbeMs = 0;
static uint32_t ultimaAvaliacaoRotaMs = 0;
static uint8_t candidatoRota = ROUTE_DIRECT;
static uint8_t amostrasCandidato = 0;
static ReplayState replayStatusDirect;
static ReplayState replayStatusRepeater;
static ReplayState replayAckPrincipal;
static ReplayState replayAckRepeater;
static QualidadeRota qualidadeDirect;
static QualidadeRota qualidadeRepeater;

static bool macIgual(const uint8_t* a, const uint8_t* b) {
    return a != nullptr && b != nullptr && memcmp(a, b, 6) == 0;
}

static QualidadeRota& qualidadeDaRota(uint8_t rota) {
    return (rota == ROUTE_VIA_REPEATER) ? qualidadeRepeater : qualidadeDirect;
}

static const QualidadeRota& qualidadeDaRotaConst(uint8_t rota) {
    return (rota == ROUTE_VIA_REPEATER) ? qualidadeRepeater : qualidadeDirect;
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

static void registrarRssi(QualidadeRota& qualidade, int rssi) {
    if (rssi == RSSI_INVALIDO_DBM) {
        return;
    }
    if (qualidade.rssiEwma == RSSI_INVALIDO_DBM) {
        qualidade.rssiEwma = rssi;
        return;
    }
    qualidade.rssiEwma = static_cast<int16_t>(((qualidade.rssiEwma * 3) + rssi) / 4);
}

static int limitar(int valor, int minimo, int maximo) {
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

static bool statusRecente(const QualidadeRota& qualidade, uint32_t agora) {
    return qualidade.ultimoStatusMs > 0 && (agora - qualidade.ultimoStatusMs <= LINK_QUALITY_TIMEOUT_MS);
}

static bool uplinkRecente(const QualidadeRota& qualidade, uint32_t agora) {
    const bool ackRecente = qualidade.ultimoLinkAckMs > 0 &&
                            (agora - qualidade.ultimoLinkAckMs <= LINK_QUALITY_TIMEOUT_MS);
    const bool envioRecente = qualidade.ultimoEnvioOkMs > 0 &&
                              (agora - qualidade.ultimoEnvioOkMs <= LINK_QUALITY_TIMEOUT_MS);
    return ackRecente || envioRecente;
}

static bool rotaOperacional(uint8_t rota, uint32_t agora) {
    const QualidadeRota& qualidade = qualidadeDaRotaConst(rota);
#if SEC_ENABLE_REPEATER_ROUTE
    return statusRecente(qualidade, agora) && uplinkRecente(qualidade, agora);
#else
    return rota == ROUTE_DIRECT && statusRecente(qualidade, agora);
#endif
}

static int calcularScore(uint8_t rota, uint32_t agora) {
    const QualidadeRota& qualidade = qualidadeDaRotaConst(rota);
    int score = 0;

    if (statusRecente(qualidade, agora)) {
        score += 50;
        const uint32_t idade = agora - qualidade.ultimoStatusMs;
        score += limitar(20 - static_cast<int>(idade / 25), 0, 20);
    }
    if (uplinkRecente(qualidade, agora)) {
        score += 30;
    }
    if (qualidade.rssiEwma != RSSI_INVALIDO_DBM) {
        score += limitar(qualidade.rssiEwma - LINK_RSSI_MIN_DBM, -20, 30);
    }
    score -= static_cast<int>(qualidade.falhasRecentes) * 8;

    return score;
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

static int rssiDoCallback(const esp_now_recv_info_t* info) {
    if (info == nullptr || info->rx_ctrl == nullptr) {
        return RSSI_INVALIDO_DBM;
    }
    return info->rx_ctrl->rssi;
}

static bool headerStatusValido(const PacoteStatus& status, const uint8_t* macFisico, uint8_t& rota) {
    if (status.header.tipo != PKT_STATUS ||
        status.header.origem != NODE_PRINCIPAL ||
        status.header.destino != NODE_REMOTE) {
        return false;
    }

    if (macIgual(macFisico, MAC_PRINCIPAL_ESPERADO)) {
        rota = ROUTE_DIRECT;
        return status.header.rota == ROUTE_DIRECT && status.header.hop_count == 0;
    }

#if SEC_ENABLE_REPEATER_ROUTE
    if (macIgual(macFisico, MAC_REPEATER_ESPERADO)) {
        rota = ROUTE_VIA_REPEATER;
        return status.header.rota == ROUTE_VIA_REPEATER && status.header.hop_count == 1;
    }
#endif

    return false;
}

static bool headerAckValido(const PacoteLink& ack, const uint8_t* macFisico, uint8_t& rota) {
    if (ack.header.tipo != PKT_LINK_ACK || ack.header.destino != NODE_REMOTE) {
        return false;
    }

    if (macIgual(macFisico, MAC_PRINCIPAL_ESPERADO)) {
        rota = ROUTE_DIRECT;
        return ack.header.origem == NODE_PRINCIPAL &&
               ack.header.rota == ROUTE_DIRECT &&
               ack.header.hop_count == 0;
    }

#if SEC_ENABLE_REPEATER_ROUTE
    if (macIgual(macFisico, MAC_REPEATER_ESPERADO)) {
        rota = ROUTE_VIA_REPEATER;
        return ack.header.origem == NODE_REPEATER &&
               ack.header.rota == ROUTE_DIRECT &&
               ack.header.hop_count == 0;
    }
#endif

    return false;
}

static void processarStatus(const uint8_t* macFisico, const uint8_t* data, int rssi) {
    PacoteStatus status;
    memcpy(&status, data, sizeof(PacoteStatus));

    uint8_t rota = ROUTE_DIRECT;
    if (!headerStatusValido(status, macFisico, rota)) {
        LOG_WARN("ESP-NOW", "Status rejeitado: origem/rota invalida");
        return;
    }

    uint8_t cs = calcular_checksum((const uint8_t*)&status, sizeof(PacoteStatus) - 1);
    if (cs != status.checksum) {
        LOG_WARN("ESP-NOW", "Status rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(status, ESPNOW_LMK) != status.auth_tag) {
        LOG_WARN("ESP-NOW", "Status rejeitado: auth_tag invalido");
        return;
    }

    ReplayState& replay = (rota == ROUTE_VIA_REPEATER) ? replayStatusRepeater : replayStatusDirect;
    uint32_t agora = millis();
    if (!aceitarSequencia(replay, status.session_id, status.seq, agora)) {
        LOG_INFO("ESP-NOW", "Status rejeitado: replay/duplicado");
        return;
    }

    QualidadeRota& qualidade = qualidadeDaRota(rota);
    qualidade.ultimoStatusMs = agora;
    registrarRssi(qualidade, rssi);

    memcpy((void*)&Comunicacao::_ultimoStatus, &status, sizeof(PacoteStatus));
    Comunicacao::_ultimoStatusMs = agora;
}

static void processarLinkAck(const uint8_t* macFisico, const uint8_t* data, int rssi) {
    PacoteLink ack;
    memcpy(&ack, data, sizeof(PacoteLink));

    uint8_t rota = ROUTE_DIRECT;
    if (!headerAckValido(ack, macFisico, rota)) {
        LOG_WARN("ESP-NOW", "Link ACK rejeitado: origem/rota invalida");
        return;
    }

    uint8_t cs = calcular_checksum((const uint8_t*)&ack, sizeof(PacoteLink) - 1);
    if (cs != ack.checksum) {
        LOG_WARN("ESP-NOW", "Link ACK rejeitado: checksum invalido");
        return;
    }
    if (calcular_auth_tag(ack, ESPNOW_LMK) != ack.auth_tag) {
        LOG_WARN("ESP-NOW", "Link ACK rejeitado: auth_tag invalido");
        return;
    }

    ReplayState& replay = (rota == ROUTE_VIA_REPEATER) ? replayAckRepeater : replayAckPrincipal;
    uint32_t agora = millis();
    if (!aceitarSequencia(replay, ack.session_id, ack.seq, agora)) {
        LOG_INFO("ESP-NOW", "Link ACK rejeitado: replay/duplicado");
        return;
    }

    QualidadeRota& qualidade = qualidadeDaRota(rota);
    qualidade.ultimoLinkAckMs = agora;
    registrarRssi(qualidade, rssi);
}

static void processarRecebimento(const uint8_t* macFisico, const uint8_t* data, int len, int rssi) {
    if (macFisico == nullptr || data == nullptr) {
        return;
    }

    const bool macAutorizado =
        macIgual(macFisico, MAC_PRINCIPAL_ESPERADO)
#if SEC_ENABLE_REPEATER_ROUTE
        || macIgual(macFisico, MAC_REPEATER_ESPERADO)
#endif
    ;
    if (!macAutorizado) {
        LOG_WARN("ESP-NOW", "Pacote rejeitado: MAC fisico nao autorizado");
        return;
    }

    if (len == sizeof(PacoteStatus)) {
        processarStatus(macFisico, data, rssi);
        return;
    }
    if (len == sizeof(PacoteLink)) {
        processarLinkAck(macFisico, data, rssi);
        return;
    }

    LOG_WARN("ESP-NOW", "Pacote rejeitado: tamanho/tipo invalido");
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void Comunicacao::onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info == nullptr) {
        return;
    }
    processarRecebimento(info->src_addr, data, len, rssiDoCallback(info));
}
#else
void Comunicacao::onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    processarRecebimento(mac, data, len, RSSI_INVALIDO_DBM);
}
#endif

void Comunicacao::onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (mac == nullptr) {
        return;
    }

    uint8_t rota = ROUTE_DIRECT;
    if (macIgual(mac, MAC_PRINCIPAL_ESPERADO)) {
        rota = ROUTE_DIRECT;
#if SEC_ENABLE_REPEATER_ROUTE
    } else if (macIgual(mac, MAC_REPEATER_ESPERADO)) {
        rota = ROUTE_VIA_REPEATER;
#endif
    } else {
        return;
    }

    QualidadeRota& qualidade = qualidadeDaRota(rota);
    if (status == ESP_NOW_SEND_SUCCESS) {
        qualidade.ultimoEnvioOkMs = millis();
        if (qualidade.falhasRecentes > 0) {
            qualidade.falhasRecentes--;
        }
    } else {
        qualidade.ultimaFalhaEnvioMs = millis();
        if (qualidade.falhasRecentes < 10) {
            qualidade.falhasRecentes++;
        }
    }
}

void Comunicacao::init() {
    if (!parseMacString(SEC_PRINCIPAL_MAC_STR, MAC_PRINCIPAL_ESPERADO) ||
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
        sessaoLocalRemote = esp_random();
    } while (sessaoLocalRemote == 0);
    seqPacoteEnvio = 0;
    seqProbeEnvio = 0;
    ultimoProbeMs = 0;
    ultimaAvaliacaoRotaMs = 0;
    candidatoRota = ROUTE_DIRECT;
    amostrasCandidato = 0;
    replayStatusDirect = ReplayState();
    replayStatusRepeater = ReplayState();
    replayAckPrincipal = ReplayState();
    replayAckRepeater = ReplayState();
    qualidadeDirect = QualidadeRota();
    qualidadeRepeater = QualidadeRota();
    _rotaAtual = ROUTE_DIRECT;

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

    if (!registrarPeer(MAC_PRINCIPAL_ESPERADO)) {
        LOG_ERROR("ESP-NOW", "Falha ao registrar peer principal criptografado");
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
        MAC_PRINCIPAL_ESPERADO[0], MAC_PRINCIPAL_ESPERADO[1], MAC_PRINCIPAL_ESPERADO[2],
        MAC_PRINCIPAL_ESPERADO[3], MAC_PRINCIPAL_ESPERADO[4], MAC_PRINCIPAL_ESPERADO[5]
    );
    LOG_ALWAYS_VAL("ESP-NOW", "Peer principal configurado: ", macPareado);

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

static void assinarLink(PacoteLink& pacote) {
    pacote.auth_tag = calcular_auth_tag(pacote, ESPNOW_LMK);
    pacote.checksum = calcular_checksum((const uint8_t*)&pacote, sizeof(PacoteLink) - 1);
}

static void enviarProbePara(const uint8_t macDestino[6], uint8_t destinoLogico) {
    PacoteLink probe = {};
    probe.header.tipo = PKT_LINK_PROBE;
    probe.header.origem = NODE_REMOTE;
    probe.header.destino = destinoLogico;
    probe.header.rota = ROUTE_DIRECT;
    probe.header.hop_count = 0;
    probe.timestamp = millis();
    probe.seq = seqProbeEnvio++;
    probe.session_id = sessaoLocalRemote;
    assinarLink(probe);
    esp_now_send(macDestino, (const uint8_t*)&probe, sizeof(PacoteLink));
}

void Comunicacao::enviarProbes() {
    const uint32_t agora = millis();
    if (agora - ultimoProbeMs < LINK_PROBE_INTERVALO_MS) {
        return;
    }
    ultimoProbeMs = agora;

    enviarProbePara(MAC_PRINCIPAL_ESPERADO, NODE_PRINCIPAL);
#if SEC_ENABLE_REPEATER_ROUTE
    enviarProbePara(MAC_REPEATER_ESPERADO, NODE_REPEATER);
#endif
}

void Comunicacao::atualizarRota() {
#if !SEC_ENABLE_REPEATER_ROUTE
    _rotaAtual = ROUTE_DIRECT;
    return;
#else
    const uint32_t agora = millis();
    if (agora - ultimaAvaliacaoRotaMs < HEARTBEAT_INTERVALO_MS) {
        return;
    }
    ultimaAvaliacaoRotaMs = agora;

    const uint8_t atual = _rotaAtual;
    const uint8_t alternativa = (atual == ROUTE_DIRECT) ? ROUTE_VIA_REPEATER : ROUTE_DIRECT;
    const bool atualOk = rotaOperacional(atual, agora);
    const bool alternativaOk = rotaOperacional(alternativa, agora);
    const int scoreAtual = calcularScore(atual, agora);
    const int scoreAlternativa = calcularScore(alternativa, agora);
    const int16_t rssiAtual = qualidadeDaRotaConst(atual).rssiEwma;
    const int16_t rssiAlternativa = qualidadeDaRotaConst(alternativa).rssiEwma;
    const bool rssiAlternativaMelhor =
        rssiAtual != RSSI_INVALIDO_DBM &&
        rssiAlternativa != RSSI_INVALIDO_DBM &&
        rssiAlternativa >= rssiAtual + LINK_SWITCH_MARGIN_DB;

    bool deveTrocar = false;
    if (!atualOk && alternativaOk) {
        deveTrocar = true;
    } else if (alternativaOk &&
               (scoreAlternativa >= scoreAtual + ROUTE_SWITCH_SCORE_MARGIN || rssiAlternativaMelhor)) {
        if (candidatoRota != alternativa) {
            candidatoRota = alternativa;
            amostrasCandidato = 1;
        } else if (amostrasCandidato < ROUTE_SWITCH_CONSECUTIVE_SAMPLES) {
            amostrasCandidato++;
        }
        deveTrocar = amostrasCandidato >= ROUTE_SWITCH_CONSECUTIVE_SAMPLES;
    } else {
        candidatoRota = atual;
        amostrasCandidato = 0;
    }

#if SEC_PREFER_DIRECT_ROUTE
    if (atual == ROUTE_VIA_REPEATER && alternativa == ROUTE_DIRECT &&
        alternativaOk && scoreAlternativa + ROUTE_SWITCH_SCORE_MARGIN >= scoreAtual) {
        deveTrocar = true;
    }
#endif

    if (deveTrocar) {
        _rotaAtual = alternativa;
        candidatoRota = alternativa;
        amostrasCandidato = 0;
        LOG_WARN_VAL("ROTA", "Rota ativa alterada para ", rotaAtualNome());
    }
#endif
}

uint8_t Comunicacao::rotaAtual() const {
    return _rotaAtual;
}

bool Comunicacao::rotaAtualOperacional() const {
    return rotaOperacional(_rotaAtual, millis());
}

const char* Comunicacao::rotaAtualNome() const {
    return (_rotaAtual == ROUTE_VIA_REPEATER) ? "via_repeater" : "direta";
}

int Comunicacao::scoreRota(uint8_t rota) const {
    return calcularScore(rota, millis());
}

void Comunicacao::enviarPacote(const PacoteRemote& pacote) {
    PacoteRemote p = pacote;
    const uint8_t rota = _rotaAtual;
    const uint8_t* macDestino = MAC_PRINCIPAL_ESPERADO;

#if SEC_ENABLE_REPEATER_ROUTE
    if (rota == ROUTE_VIA_REPEATER) {
        macDestino = MAC_REPEATER_ESPERADO;
    }
#endif

    p.header.tipo = PKT_REMOTE_CMD;
    p.header.origem = NODE_REMOTE;
    p.header.destino = NODE_PRINCIPAL;
    p.header.rota = rota;
    p.header.hop_count = (rota == ROUTE_VIA_REPEATER) ? 1 : 0;
    p.seq = seqPacoteEnvio++;
    p.session_id = sessaoLocalRemote;
    p.auth_tag = calcular_auth_tag(p, ESPNOW_LMK);
    p.checksum = calcular_checksum((const uint8_t*)&p, sizeof(PacoteRemote) - 1);
    esp_now_send(macDestino, (const uint8_t*)&p, sizeof(PacoteRemote));
}
