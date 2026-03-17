/**
 * botoes.cpp — Implementação da leitura de botões do Principal
 *
 * Debounce genérico via millis(). SUBIR/DESCER retornam nível (hold).
 * VEL1/2/3 retornam pulso (borda HIGH→LOW, consumido após leitura).
 * EMERGÊNCIA e REARME são tratados em seus módulos específicos.
 *
 * Ref: hardware_io/SPEC.md §4
 */

#include <Arduino.h>
#include "pinout.h"
#include "botoes.h"

#define DEBOUNCE_MS 50
#define NUM_BOTOES  5

// Índices internos
enum { IDX_SUBIR = 0, IDX_DESCER, IDX_VEL1, IDX_VEL2, IDX_VEL3 };

static const uint8_t pinos[NUM_BOTOES] = {
    PIN_BTN_SUBIR, PIN_BTN_DESCER,
    PIN_BTN_VEL1, PIN_BTN_VEL2, PIN_BTN_VEL3
};

static bool     ultima_leitura[NUM_BOTOES];
static bool     estado_filtrado[NUM_BOTOES];
static uint32_t ultimo_cambio[NUM_BOTOES];

void botoes_init() {
    for (int i = 0; i < NUM_BOTOES; i++) {
        pinMode(pinos[i], INPUT);
        ultima_leitura[i]  = HIGH;
        estado_filtrado[i] = HIGH;
        ultimo_cambio[i]   = 0;
    }
}

EstadoBotoes botoes_ler() {
    uint32_t agora = millis();
    bool estado_anterior[NUM_BOTOES];

    // Salvar estado anterior e atualizar debounce
    for (int i = 0; i < NUM_BOTOES; i++) {
        estado_anterior[i] = estado_filtrado[i];

        bool leitura = digitalRead(pinos[i]);
        if (leitura != ultima_leitura[i]) {
            ultimo_cambio[i] = agora;
            ultima_leitura[i] = leitura;
        }
        if ((agora - ultimo_cambio[i]) >= DEBOUNCE_MS) {
            estado_filtrado[i] = ultima_leitura[i];
        }
    }

    EstadoBotoes resultado = {0};

    // SUBIR/DESCER: hold (LOW = pressionado)
    resultado.subir_hold  = (estado_filtrado[IDX_SUBIR]  == LOW);
    resultado.descer_hold = (estado_filtrado[IDX_DESCER] == LOW);

    // VEL1/2/3: pulso — borda de descida (HIGH → LOW)
    resultado.vel1_pulso = (estado_anterior[IDX_VEL1] == HIGH && estado_filtrado[IDX_VEL1] == LOW);
    resultado.vel2_pulso = (estado_anterior[IDX_VEL2] == HIGH && estado_filtrado[IDX_VEL2] == LOW);
    resultado.vel3_pulso = (estado_anterior[IDX_VEL3] == HIGH && estado_filtrado[IDX_VEL3] == LOW);

    return resultado;
}
