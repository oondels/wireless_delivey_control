/**
 * sensores.h — Leitura do sensor fim de curso com debounce e bloqueio pós-acionamento
 *
 * Debounce de 20 ms via millis() (não-bloqueante).
 * Lógica: LOW = sensor acionado (pull-up externo).
 *
 * Segurança: após o sensor ser acionado, o bloqueio de movimento
 * permanece ativo por BLOQUEIO_POS_FIM_CURSO_MS (10 s) mesmo que
 * o sensor físico já tenha sido liberado.
 *
 * Ref: motor/SPEC.md §5, seguranca/SPEC.md §6.2
 */

#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>
#include "pinout.h"

class Sensores {
public:
    static constexpr uint32_t DEBOUNCE_FIM_CURSO_MS      = 20;
    static constexpr uint32_t BLOQUEIO_POS_FIM_CURSO_MS  = 10000;  // 10 s de segurança

    void init();

    /**
     * Retorna true se o movimento deve ser bloqueado por fim de curso:
     * - sensor fisicamente acionado (debounce 20 ms); OU
     * - dentro do periodo de bloqueio pos-acionamento (10 s).
     */
    bool fimDeCursoAcionado();

private:
    bool     _estadoFiltrado      = false;
    bool     _ultimaLeitura       = HIGH;
    uint32_t _ultimoCambioMs      = 0;
    uint32_t _ultimoAcionamentoMs = 0;   // timestamp do ultimo acionamento confirmado
};

#endif // SENSORES_H
