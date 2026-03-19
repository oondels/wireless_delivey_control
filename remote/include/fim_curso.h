/**
 * fim_curso.h — Sensor de fim de curso com debounce e bloqueio pós-liberação
 *
 * Leitura não-bloqueante via millis(). Lógica: LOW = sensor acionado
 * (pull-up interno).
 *
 * Bloqueio pós-liberação: após o sensor físico ser liberado, o método
 * acionado() continua retornando true por BLOQUEIO_POS_LIBERACAO_MS (10 s).
 * O timer inicia na liberação — independente do tempo que o sensor ficou ativo.
 *
 * Ref: hardware_io/SPEC.md §6.1, seguranca/SPEC.md §6.2
 */

#ifndef FIM_CURSO_H
#define FIM_CURSO_H

#include <Arduino.h>

class FimCurso {
public:
    static constexpr uint32_t DEBOUNCE_MS               = 20;
    static constexpr uint32_t BLOQUEIO_POS_LIBERACAO_MS  = 10000;  // 10 s de segurança

    explicit FimCurso(uint8_t gpio) : _gpio(gpio) {}

    void init() {
        pinMode(_gpio, INPUT_PULLUP);
        _estadoFiltrado    = false;
        _ultimaLeitura     = HIGH;
        _ultimoCambioMs    = 0;
        _ultimaLiberacaoMs = 0;
    }

    /**
     * Atualiza debounce e timers. Chamar uma vez por ciclo do loop
     * antes de consultar acionado().
     */
    void atualizar();

    /**
     * Retorna true se o sensor está acionado OU se está dentro do
     * período de bloqueio pós-liberação (10 s).
     */
    bool acionado() const;

private:
    uint8_t  _gpio;
    bool     _estadoFiltrado    = false;
    bool     _ultimaLeitura     = HIGH;
    uint32_t _ultimoCambioMs    = 0;
    uint32_t _ultimaLiberacaoMs = 0;
};

#endif // FIM_CURSO_H
