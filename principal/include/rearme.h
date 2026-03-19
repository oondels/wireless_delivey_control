/**
 * rearme.h — Lógica de rearme (desativação de emergência)
 *
 * Botão REARME é do tipo pulso (borda de descida, debounce 50ms).
 * Limpa emergência, FALHA_ENERGIA e FALHA_COMUNICACAO.
 * Caso especial: se emergência Remote ainda travada, seta _rearmeAtivo.
 *
 * Ref: seguranca/SPEC.md §3.4-3.5
 */

#ifndef REARME_H
#define REARME_H

#include <Arduino.h>
#include "pinout.h"
#include "protocolo.h"
#include "emergencia.h"

class Rearme {
public:
    static constexpr uint32_t DEBOUNCE_MS = 50;

    void init();

    // Verifica botão REARME e executa rearme se condições atendidas.
    void verificar(EstadoSistema* estadoAtual, bool emergenciaRemote, Emergencia& emergencia);

    bool isRearmeAtivo() const { return _rearmeAtivo; }
    void limparRearmeAtivo() { _rearmeAtivo = false; }

private:
    bool     _rearmeAtivo    = false;
    bool     _ultimoEstado   = HIGH;
    bool     _estadoFiltrado = HIGH;
    uint32_t _ultimoCambioMs = 0;
};

#endif // REARME_H
