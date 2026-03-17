/**
 * pinout.h — Mapeamento de GPIOs do Módulo Remote
 *
 * Total: 13 GPIOs (6 entradas + 7 saídas)
 *
 * Restrições respeitadas (hardware_io/SPEC.md §8):
 * - GPIO 0, 2, 12, 15 evitados para entradas críticas (strapping pins)
 * - GPIO 34, 35, 36, 39 usados apenas como entrada (input-only, sem pull-up interno)
 * - GPIO 1 (TX) e GPIO 3 (RX) reservados para Serial
 * - GPIOs 34, 35, 36, 39: pull-up externo 10kΩ obrigatório (input-only, sem pull-up interno)
 * - GPIOs 32, 33: pull-up interno via INPUT_PULLUP (sem resistor externo)
 * - Lógica de todos os botões: HIGH = solto, LOW = pressionado
 * - Saídas: HIGH = LED aceso
 *
 * Mapeamento consistente com o Módulo Principal onde as funções coincidem.
 */

#ifndef PINOUT_H
#define PINOUT_H

// ============================================================
// ENTRADAS — Botões (LOW = pressionado)
// GPIOs 34-39: pull-up externo obrigatório (input-only)
// GPIOs 32, 33: pull-up interno (INPUT_PULLUP)
// ============================================================

#define PIN_BTN_SUBIR       36  // Hold (nível) — input-only, pull-up externo obrigatório
#define PIN_BTN_DESCER      39  // Hold (nível) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL1        34  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL2        35  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL3        32  // Pulso (borda) — pull-up interno (INPUT_PULLUP)
#define PIN_BTN_EMERGENCIA  33  // Nível contínuo (trava mecânica) — pull-up interno (INPUT_PULLUP)

// ============================================================
// SAÍDAS — LEDs dedicados (HIGH = aceso)
// ============================================================

#define PIN_LED_LINK        4   // Status de comunicação com Principal
#define PIN_LED_MOTOR       16  // Motor em operação (SUBINDO ou DESCENDO)
#define PIN_LED_VEL1        17  // Velocidade 1 ativa
#define PIN_LED_VEL2        5   // Velocidade 2 ativa
#define PIN_LED_VEL3        18  // Velocidade 3 ativa
#define PIN_LED_EMERGENCIA  19  // Emergência ou falha de comunicação
#define PIN_LED_ALARME      21  // Rearme com botão local ainda travado

#endif // PINOUT_H
