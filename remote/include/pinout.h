/**
 * pinout.h — Mapeamento de GPIOs do Módulo Remote
 *
 * Total: 11 GPIOs (6 entradas + 5 saídas)
 *
 * Restrições respeitadas (hardware_io/SPEC.md §8):
 * - GPIO 0, 2, 12, 15 evitados para entradas críticas (strapping pins)
 * - GPIO 34, 35, 36, 39 usados apenas como entrada (input-only, sem pull-up interno)
 * - GPIO 1 (TX) e GPIO 3 (RX) reservados para Serial
 * - GPIOs 34, 35, 36, 39: pull-up externo 10kΩ obrigatório (input-only, sem pull-up interno)
 * - GPIOs 13, 32, 33: pull-up interno via INPUT_PULLUP (sem resistor externo)
 * - Lógica botões NO: HIGH = solto, LOW = pressionado
 * - Lógica botão NC (emergência): LOW = repouso (fechado), HIGH = contato aberto/ativo
 * - Saídas: HIGH = LED aceso
 *
 * Mapeamento consistente com o Módulo Principal onde as funções coincidem.
 */

#ifndef PINOUT_H
#define PINOUT_H

// ============================================================
// ENTRADAS — Botões
// GPIOs 34-39: pull-up externo obrigatório (input-only)
// GPIOs 32, 33: pull-up interno (INPUT_PULLUP)
// NO (normalmente aberto): LOW = pressionado
// NC (normalmente fechado, emergência): HIGH = contato aberto/ativo
// ============================================================

#define PIN_BTN_SUBIR       32  // Hold (nível) — pull-up interno (INPUT_PULLUP)
#define PIN_BTN_DESCER      33  // Hold (nível) — pull-up interno (INPUT_PULLUP)
#define PIN_BTN_VEL1        39  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_VEL2        34  // Pulso (borda) — input-only, pull-up externo obrigatório
#define PIN_BTN_RESET       255 // Desabilitado nesta versão do controle remoto
#define PIN_BTN_EMERGENCIA  13  // NC: repouso LOW, ativo HIGH — pull-up interno (INPUT_PULLUP)

// ============================================================
// ENTRADAS — Sensores
// ============================================================

#define PIN_FIM_CURSO_DESCIDA  36  // LOW = carrinho na posição final de descida — pull-up externo obrigatório

// ============================================================
// SAÍDAS — LEDs dedicados (HIGH = aceso)
// ============================================================

#define PIN_LED_LINK        4   // Status de comunicação com Principal
#define PIN_LED_MOTOR       16  // Motor em operação (SUBIR ou DESCER hold ativo)
#define PIN_LED_VEL1        17  // Velocidade 1 selecionada
#define PIN_LED_VEL2        5   // Velocidade 2 selecionada
// GPIO 18 (antes VEL3): não utilizado nesta arquitetura
#define PIN_LED_EMERGENCIA  19  // Emergência ativa ou link perdido

#endif // PINOUT_H
