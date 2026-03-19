/**
 * pinout.h — Mapeamento de GPIOs do Módulo Principal
 *
 * Total: 17 GPIOs (10 entradas + 7 saídas)
 *
 * Restrições respeitadas (hardware_io/SPEC.md §8):
 * - GPIO 0, 2, 12, 15 evitados para entradas críticas (strapping pins)
 * - GPIO 34, 35, 36, 39 usados apenas como entrada (input-only, sem pull-up interno)
 * - GPIO 1 (TX) e GPIO 3 (RX) reservados para Serial
 * - GPIOs 34, 35, 36, 39: pull-up externo 10kΩ obrigatório (input-only, sem pull-up interno)
 * - GPIOs 27, 32, 33, 25, 26: pull-up interno via INPUT_PULLUP (sem resistor externo)
 * - Lógica de todos os botões: HIGH = solto, LOW = pressionado
 * - Microchave freio (GPIO 27): NA, HIGH = freio engatado, LOW = freio liberado
 * - Saídas: HIGH = ativo (relé energizado / LED aceso)
 */

#ifndef PINOUT_H
#define PINOUT_H

// ============================================================
// ENTRADAS — Botões (LOW = pressionado)
// GPIOs 34-39: pull-up externo obrigatório (input-only)
// GPIOs 25, 26, 32, 33: pull-up interno (INPUT_PULLUP)
// ============================================================

#define PIN_BTN_SUBIR       36  // Hold (nível) — input-only, pull-up externo obrigatório
#define PIN_BTN_DESCER      39  // Hold (nível) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL1        34  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL2        35  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL3        32  // Pulso (borda) — pull-up interno (INPUT_PULLUP)
#define PIN_BTN_EMERGENCIA  33  // Nível contínuo (trava mecânica) — pull-up interno (INPUT_PULLUP)
#define PIN_BTN_REARME      25  // Pulso (borda) — pull-up interno (INPUT_PULLUP)

// ============================================================
// ENTRADAS — Sensores
// ============================================================

#define PIN_FIM_DE_CURSO      26  // Nível — debounce 20 ms, pull-up interno (INPUT_PULLUP)
#define PIN_MICROCHAVE_FREIO  27  // NA — pull-up interno; HIGH = freio engatado (fail-safe)
#define PIN_MONITOR_REDE      13  // HIGH = rede presente; LOW = rede ausente — pull-down externo, debounce 50 ms

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
