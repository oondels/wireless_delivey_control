/**
 * protocolo.h — Protocolo de comunicação ESP-NOW
 *
 * Arquivo compartilhado entre Módulo Principal e Módulo Remote.
 * Qualquer alteração deve ser replicada em ambos os projetos.
 *
 * Ref: docs/specs/comunicacao/SPEC.md §4-6
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>

// ============================================================
// Enumerações
// ============================================================

typedef enum {
    CMD_HEARTBEAT = 0,
    CMD_SUBIR     = 1,
    CMD_DESCER    = 2,
    CMD_VEL1      = 3,
    CMD_VEL2      = 4,
    CMD_VEL3      = 5
} Comando;

typedef enum {
    ESTADO_PARADO            = 0,
    ESTADO_SUBINDO           = 1,
    ESTADO_DESCENDO          = 2,
    ESTADO_EMERGENCIA        = 3,
    ESTADO_FALHA_COMUNICACAO = 4,
    ESTADO_FALHA_ENERGIA     = 5   // queda de energia da rede elétrica (GPIO 13)
} EstadoSistema;

// ============================================================
// Structs de pacotes
// ============================================================

// Remote → Principal (9 bytes)
typedef struct {
    uint8_t  comando;            // Comando enum (0-5)
    uint8_t  botao_hold;         // 1 = SUBIR ou DESCER pressionado (Homem-Morto)
    uint8_t  emergencia;         // 1 = botão emergência com trava ativo no Remote
    uint8_t  fim_curso_descida;  // 1 = carrinho na posição final de descida (GPIO 13)
    uint32_t timestamp;          // millis() do Remote
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} __attribute__((packed)) PacoteRemote;

// Principal → Remote (5 bytes)
typedef struct {
    uint8_t  estado_sistema; // EstadoSistema enum (0-5)
    uint8_t  velocidade;     // 1, 2 ou 3
    uint8_t  trava_logica;   // 1 = movimentação bloqueada
    uint8_t  rearme_ativo;   // 1 = rearme feito com emergência Remote ainda travada
    uint8_t  checksum;       // XOR de todos os bytes anteriores
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

// ============================================================
// Constantes de timing (ms)
// ============================================================

#define HEARTBEAT_INTERVALO_MS   100
#define WATCHDOG_TIMEOUT_MS      500
#define STATUS_INTERVALO_MS      200

#endif // PROTOCOLO_H
