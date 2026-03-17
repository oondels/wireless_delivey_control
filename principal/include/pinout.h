/**
 * pinout.h — Mapeamento de GPIOs do Módulo Principal
 *
 * Total: 15 GPIOs (8 entradas + 7 saídas)
 *
 * Restrições respeitadas (hardware_io/SPEC.md §8):
 * - GPIO 0, 2, 12, 15 evitados para entradas críticas (strapping pins)
 * - GPIO 34, 35, 36, 39 usados apenas como entrada (input-only, sem pull-up interno)
 * - GPIO 1 (TX) e GPIO 3 (RX) reservados para Serial
 * - Todos os botões usam pull-up externo 10kΩ (lógica: HIGH = solto, LOW = pressionado)
 * - Saídas: HIGH = ativo (relé energizado / LED aceso)
 */

#ifndef PINOUT_H
#define PINOUT_H

// ============================================================
// ENTRADAS — Botões (pull-up externo, LOW = pressionado)
// ============================================================

#define PIN_BTN_SUBIR       36  // Hold (nível) — input-only, pull-up externo
#define PIN_BTN_DESCER      39  // Hold (nível) — input-only, pull-up externo
#define PIN_BTN_VEL1        34  // Pulso (borda) — input-only, pull-up externo
#define PIN_BTN_VEL2        35  // Pulso (borda) — input-only, pull-up externo
#define PIN_BTN_VEL3        32  // Pulso (borda) — pull-up externo
#define PIN_BTN_EMERGENCIA  33  // Nível contínuo (trava mecânica) — pull-up externo
#define PIN_BTN_REARME      25  // Pulso (borda) — pull-up externo

// ============================================================
// ENTRADAS — Sensores
// ============================================================

#define PIN_FIM_DE_CURSO    26  // Nível — debounce 20 ms, pull-up externo

// ============================================================
// SAÍDAS — Relés com LED compartilhado (HIGH = ativo)
// ============================================================

#define PIN_RELE_DIRECAO_A  4   // Motor sentido SUBIR + LED
#define PIN_RELE_DIRECAO_B  16  // Motor sentido DESCER + LED
#define PIN_RELE_VEL1       17  // Velocidade 1 + LED
#define PIN_RELE_VEL2       5   // Velocidade 2 + LED
#define PIN_RELE_VEL3       18  // Velocidade 3 + LED
#define PIN_RELE_FREIO      19  // Freio mecânico + LED

// ============================================================
// SAÍDAS — LEDs exclusivos (sem relé associado)
// ============================================================

#define PIN_LED_LINK        21  // Comunicação ativa com Remote

#endif // PINOUT_H
