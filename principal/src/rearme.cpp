/**
 * rearme.cpp — Implementação da lógica de rearme
 *
 * Detecta borda de descida do botão REARME (pulso, debounce 50ms).
 * Ao detectar pulso:
 * - Se botão EMERGÊNCIA local estiver ativo → rearme bloqueado
 * - Limpa emergencia_ativa e estado FALHA_COMUNICACAO
 * - Se emergência Remote ainda travada → rearme_ativo = true
 * - Estado retorna a PARADO
 *
 * Ref: seguranca/SPEC.md §3.4-3.5
 */

#include <Arduino.h>
#include "pinout.h"
#include "rearme.h"
#include "emergencia.h"

#define DEBOUNCE_REARME_MS 50

bool rearme_ativo = false;

static bool     ultimo_estado_btn = HIGH;
static uint32_t ultimo_cambio_ms  = 0;
static bool     estado_filtrado   = HIGH;

void rearme_init() {
    pinMode(PIN_BTN_REARME, INPUT);
    rearme_ativo      = false;
    ultimo_estado_btn = HIGH;
    estado_filtrado   = HIGH;
    ultimo_cambio_ms  = 0;
}

void rearme_verificar(EstadoSistema* estado_atual, bool emergencia_remote) {
    // Debounce do botão REARME
    bool leitura = digitalRead(PIN_BTN_REARME);

    if (leitura != ultimo_estado_btn) {
        ultimo_cambio_ms = millis();
        ultimo_estado_btn = leitura;
    }

    bool estado_anterior = estado_filtrado;

    if ((millis() - ultimo_cambio_ms) >= DEBOUNCE_REARME_MS) {
        estado_filtrado = ultimo_estado_btn;
    }

    // Detectar borda de descida (HIGH → LOW = botão pressionado)
    bool pulso_rearme = (estado_anterior == HIGH && estado_filtrado == LOW);

    if (!pulso_rearme) {
        return;
    }

    // Só rearma se estiver em EMERGENCIA ou FALHA_COMUNICACAO
    if (*estado_atual != ESTADO_EMERGENCIA && *estado_atual != ESTADO_FALHA_COMUNICACAO) {
        return;
    }

    // Botão EMERGÊNCIA local ainda ativo → rearme bloqueado
    if (emergencia_botao_local_ativo()) {
        return;
    }

    // Executa rearme
    emergencia_ativa = false;
    *estado_atual = ESTADO_PARADO;

    // Caso especial: emergência Remote ainda travada
    if (emergencia_remote) {
        rearme_ativo = true;
    } else {
        rearme_ativo = false;
    }
}
