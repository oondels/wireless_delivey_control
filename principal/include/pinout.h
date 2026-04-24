/**
 * pinout.h — Mapeamento de GPIOs do Módulo Principal
 *
 * O Principal recebe comandos via ESP-NOW do Remote, repassa esses
 * sinais para as entradas digitais do CLP via saídas GPIO e lê
 * feedbacks do CLP e da micro do freio.
 *
 * Total: 8 saídas GPIO + 7 entradas GPIO
 *
 * Lógica de comunicação com CLP: ativo em LOW (GND)
 * - LOW (GND) = sinal ativo → CLP lê entrada como acionada
 * - HIGH       = sinal inativo (repouso)
 *
 * Restrições de boot respeitadas: GPIOs 0, 2, 12, 15 não utilizados.
 * GPIO 1 (TX) e GPIO 3 (RX) reservados para Serial.
 */

#ifndef PINOUT_H
#define PINOUT_H

// ============================================================
// SAÍDAS — Sinais para entradas digitais do CLP
// Lógica ativa em LOW (GND): LOW = ativo, HIGH = inativo
// Inicialização: todos HIGH (inativo)
// ============================================================

#define PIN_CLP_SUBIR       4   // SUBIR hold ativo enquanto botão pressionado no Remote
#define PIN_CLP_DESCER      16  // DESCER hold ativo enquanto botão pressionado no Remote
#define PIN_CLP_VEL1        17  // VEL1 selecionada (pulso PULSO_CLP_MS ms)
#define PIN_CLP_VEL2        5   // VEL2 selecionada (pulso PULSO_CLP_MS ms)
#define PIN_CLP_EMERGENCIA  18  // Emergência: botão Remote travado OU watchdog expirado
#define PIN_CLP_RESET       19  // RESET: pulso de PULSO_CLP_MS ms
#define PIN_CLP_FIM_CURSO   22  // Fim de curso de descida (Remote GPIO 13)

// ============================================================
// SAÍDAS — LED exclusivo
// ============================================================

#define PIN_LED_LINK        21  // Comunicação ativa com Remote (aceso = link OK)

// ============================================================
// ENTRADAS — Botões de teste local (sem Remote conectado)
// INPUT_PULLUP: LOW = pressionado, HIGH = solto
// ============================================================

#define PIN_BTN_TESTE_SUBIR    32
#define PIN_BTN_TESTE_DESCER   33

// ============================================================
// ENTRADAS — Feedbacks do CLP e micro do freio
// INPUT_PULLUP: LOW = feedback ativo do CLP
// Micro do freio NC: LOW = normal, HIGH = aberta/acionada
// ============================================================

#define PIN_FB_MOTOR_ATIVO        23
#define PIN_FB_EMERGENCIA_ATIVA   25
#define PIN_FB_VEL1_ATIVA         26
#define PIN_FB_VEL2_ATIVA         27
#define PIN_MICRO_FREIO           14

#endif // PINOUT_H
