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

void Rearme::init() {
    pinMode(PIN_BTN_REARME, INPUT);
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
        return;
    }

    // Executa rearme
    emergencia.ativa() = false;
    *estadoAtual = ESTADO_PARADO;

    // Caso especial: emergência Remote ainda travada
    _rearmeAtivo = emergenciaRemote;
}
