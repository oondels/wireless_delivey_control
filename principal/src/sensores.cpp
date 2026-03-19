/**
 * sensores.cpp — Implementação da leitura do fim de curso
 *
 * Debounce via millis(): o estado só muda após permanecer
 * estável por DEBOUNCE_FIM_CURSO_MS (20 ms).
 *
 * Bloqueio pós-acionamento: após o sensor ser confirmado como acionado,
 * o bloqueio permanece por BLOQUEIO_POS_FIM_CURSO_MS (10 s) mesmo que
 * o sensor físico já tenha sido liberado.
 *
 * Ref: motor/SPEC.md §5, seguranca/SPEC.md §6.2
 */

#include "sensores.h"
#include "logger.h"

void Sensores::init() {
    pinMode(PIN_FIM_DE_CURSO, INPUT_PULLUP);  // GPIO 26 — pull-up interno
    _estadoFiltrado      = false;
    _ultimaLeitura       = HIGH;
    _ultimoCambioMs      = 0;
    _ultimoAcionamentoMs = 0;
}

bool Sensores::fimDeCursoAcionado() {
    bool leitura = digitalRead(PIN_FIM_DE_CURSO);

    if (leitura != _ultimaLeitura) {
        _ultimoCambioMs = millis();
        _ultimaLeitura = leitura;
    }

    if ((millis() - _ultimoCambioMs) >= DEBOUNCE_FIM_CURSO_MS) {
        // LOW = acionado (pull-up interno)
        bool novoEstado = (leitura == LOW);
        if (novoEstado != _estadoFiltrado) {
            if (novoEstado) {
                _ultimoAcionamentoMs = millis();
                LOG_WARN("SENSOR", "Fim de curso ACIONADO — bloqueio ativo por 10 s");
            } else {
                LOG_INFO("SENSOR", "Fim de curso liberado — aguardando fim do bloqueio de 10 s");
            }
        }
        _estadoFiltrado = novoEstado;
    }

    // Sensor fisicamente acionado → bloqueio imediato
    if (_estadoFiltrado) {
        return true;
    }

    // Sensor liberado, mas ainda dentro do periodo de segurança pos-acionamento
    if (_ultimoAcionamentoMs > 0 &&
        (millis() - _ultimoAcionamentoMs) < BLOQUEIO_POS_FIM_CURSO_MS) {
        return true;
    }

    return false;
}
