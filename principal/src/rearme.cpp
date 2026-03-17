/**
 * rearme.cpp — Implementação da lógica de rearme
 *
 * Detecta borda de descida do botão REARME (pulso, debounce 50ms).
 * Ao detectar pulso:
 * - Se botão EMERGÊNCIA local estiver ativo → rearme bloqueado
 * - Limpa emergência e estado FALHA_COMUNICACAO
 * - Se emergência Remote ainda travada → _rearmeAtivo = true
 * - Estado retorna a PARADO
 *
 * Ref: seguranca/SPEC.md §3.4-3.5
 */

#include "rearme.h"
#include "logger.h"

void Rearme::init() {
    pinMode(PIN_BTN_REARME, INPUT_PULLUP);  // GPIO 25 — pull-up interno
    _rearmeAtivo    = false;
    _ultimoEstado   = HIGH;
    _estadoFiltrado = HIGH;
    _ultimoCambioMs = 0;
}

void Rearme::verificar(EstadoSistema* estadoAtual, bool emergenciaRemote, Emergencia& emergencia) {
    // Debounce do botão REARME
    bool leitura = digitalRead(PIN_BTN_REARME);

    if (leitura != _ultimoEstado) {
        _ultimoCambioMs = millis();
        _ultimoEstado = leitura;
    }

    bool estadoAnterior = _estadoFiltrado;

    if ((millis() - _ultimoCambioMs) >= DEBOUNCE_MS) {
        _estadoFiltrado = _ultimoEstado;
    }

    // Detectar borda de descida (HIGH → LOW = botão pressionado)
    bool pulsoRearme = (estadoAnterior == HIGH && _estadoFiltrado == LOW);

    if (!pulsoRearme) {
        return;
    }

    // Só rearma se estiver em EMERGENCIA ou FALHA_COMUNICACAO
    if (*estadoAtual != ESTADO_EMERGENCIA && *estadoAtual != ESTADO_FALHA_COMUNICACAO) {
        return;
    }

    // Botão EMERGÊNCIA local ainda ativo → rearme bloqueado
    if (emergencia.botaoLocalAtivo()) {
        LOG_WARN("REARM", "Rearme BLOQUEADO — botao emergencia local ainda ativo");
        return;
    }

    // Executa rearme
    LOG_INFO("REARM", "Rearme executado — emergencia limpa, estado -> PARADO");
    emergencia.ativa() = false;
    *estadoAtual = ESTADO_PARADO;

    // Caso especial: emergência Remote ainda travada
    if (emergenciaRemote) {
        LOG_WARN("REARM", "Rearme com emergencia Remote ainda travada — rearme_ativo = 1");
    }
    _rearmeAtivo = emergenciaRemote;
}
