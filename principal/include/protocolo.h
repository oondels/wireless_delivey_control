/**
 * protocolo.h — Protocolo de comunicação ESP-NOW
 *
 * Arquivo compartilhado entre Módulo Principal e Módulo Remote.
 * Qualquer alteração deve ser replicada em ambos os projetos.
 *
 * Arquitetura: ESP32s são pontes de comunicação.
 * O CLP gerencia toda a lógica de controle (motor, freio, estados, segurança).
 *
 * Ref: docs/specs/comunicacao/SPEC.md §4-6
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/sha256.h>

// ============================================================
// Enumerações
// ============================================================

typedef enum {
    CMD_HEARTBEAT = 0,
    CMD_SUBIR     = 1,
    CMD_DESCER    = 2,
    CMD_VEL1      = 3,
    CMD_VEL2      = 4,
    CMD_RESET     = 5  // Botão RESET no Remote (era CMD_VEL3)
} Comando;

// ============================================================
// Structs de pacotes
// ============================================================

// Remote → Principal (21 bytes)
typedef struct {
    uint8_t  comando;            // Comando enum (0-5)
    uint8_t  botao_hold;         // 1 = SUBIR ou DESCER pressionado
    uint8_t  emergencia;         // 1 = botão emergência com trava ativo no Remote
    uint8_t  fim_curso_descida;  // 1 = carrinho na posição final de descida (GPIO 36)
    uint32_t timestamp;          // millis() do Remote
    uint32_t seq;                // contador monotônico Remote -> Principal
    uint32_t session_id;         // sessão do Remote para anti-replay
    uint32_t auth_tag;           // autenticação do pacote com chave secreta
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} __attribute__((packed)) PacoteRemote;

// Principal → Remote (19 bytes)
// Principal propaga ao Remote o status atual do CLP e da micro do freio.
typedef struct {
    uint8_t  link_ok;             // 1 = Principal ativo e recebendo pacotes do Remote
    uint8_t  motor_ativo;         // 1 = CLP reporta motor ativo
    uint8_t  emergencia_ativa;    // 1 = CLP reporta emergencia ativa
    uint8_t  vel1_ativa;          // 1 = CLP reporta velocidade 1 ativa
    uint8_t  vel2_ativa;          // 1 = CLP reporta velocidade 2 ativa
    uint8_t  micro_freio_ativa;   // 1 = freio ativo; 0 = freio liberado
    uint32_t seq;                 // contador monotônico Principal -> Remote
    uint32_t session_id;          // sessão do Principal para anti-replay
    uint32_t auth_tag;            // autenticação do pacote com chave secreta
    uint8_t  checksum;            // XOR de todos os bytes anteriores
} __attribute__((packed)) PacoteStatus;

// ============================================================
// Checksum — XOR simples
// ============================================================

/**
 * Calcula checksum XOR de um buffer de bytes.
 * Passar o pacote SEM o campo checksum (len = sizeof(pacote) - 1).
 */
inline uint8_t calcular_checksum(const uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

inline bool parseHexByte(char hi, char lo, uint8_t* out) {
    int h = hexNibble(hi);
    int l = hexNibble(lo);
    if (h < 0 || l < 0) {
        return false;
    }
    *out = static_cast<uint8_t>((h << 4) | l);
    return true;
}

inline bool parseMacString(const char* mac, uint8_t out[6]) {
    if (mac == nullptr) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        const int pos = i * 3;
        if (!parseHexByte(mac[pos], mac[pos + 1], &out[i])) {
            return false;
        }
        if (i < 5 && mac[pos + 2] != ':') {
            return false;
        }
    }
    return strlen(mac) == 17;
}

inline bool parseHexKey16(const char* value, uint8_t out[16]) {
    if (value == nullptr || strlen(value) != 32) {
        return false;
    }
    for (int i = 0; i < 16; ++i) {
        if (!parseHexByte(value[i * 2], value[i * 2 + 1], &out[i])) {
            return false;
        }
    }
    return true;
}

inline uint32_t calcular_auth_tag_bytes(const uint8_t key[16], const uint8_t* data, size_t len) {
    uint8_t digest[32] = {};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, key, 16);
    mbedtls_sha256_update_ret(&ctx, data, len);
    mbedtls_sha256_update_ret(&ctx, key, 16);
    mbedtls_sha256_finish_ret(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    return (static_cast<uint32_t>(digest[0]) << 24) |
           (static_cast<uint32_t>(digest[1]) << 16) |
           (static_cast<uint32_t>(digest[2]) << 8) |
           static_cast<uint32_t>(digest[3]);
}

inline uint32_t calcular_auth_tag(const PacoteRemote& pacote, const uint8_t key[16]) {
    PacoteRemote copia = pacote;
    copia.auth_tag = 0;
    copia.checksum = 0;
    return calcular_auth_tag_bytes(key, reinterpret_cast<const uint8_t*>(&copia), sizeof(PacoteRemote));
}

inline uint32_t calcular_auth_tag(const PacoteStatus& pacote, const uint8_t key[16]) {
    PacoteStatus copia = pacote;
    copia.auth_tag = 0;
    copia.checksum = 0;
    return calcular_auth_tag_bytes(key, reinterpret_cast<const uint8_t*>(&copia), sizeof(PacoteStatus));
}

// ============================================================
// Constantes de timing (ms)
// ============================================================

#define HEARTBEAT_INTERVALO_MS   100
#define WATCHDOG_TIMEOUT_MS      500
#define STATUS_INTERVALO_MS      200
#define PULSO_CLP_MS              50  // Duração do pulso enviado ao CLP (VEL1/VEL2/RESET)

#endif // PROTOCOLO_H
