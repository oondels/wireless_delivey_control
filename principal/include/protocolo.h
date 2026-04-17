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

// Remote → Principal (9 bytes)
typedef struct {
    uint8_t  comando;            // Comando enum (0-5)
    uint8_t  botao_hold;         // 1 = SUBIR ou DESCER pressionado
    uint8_t  emergencia;         // 1 = botão emergência com trava ativo no Remote
    uint8_t  fim_curso_descida;  // 1 = carrinho na posição final de descida (GPIO 13)
    uint32_t timestamp;          // millis() do Remote
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} __attribute__((packed)) PacoteRemote;

// Principal → Remote (2 bytes)
// CLP não fornece feedback; Principal informa apenas se está ativo.
typedef struct {
    uint8_t  link_ok;   // 1 = Principal ativo e recebendo pacotes do Remote
    uint8_t  checksum;  // XOR de todos os bytes anteriores
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
#define PULSO_CLP_MS              50  // Duração do pulso enviado ao CLP (VEL1/VEL2/RESET)

#endif // PROTOCOLO_H
