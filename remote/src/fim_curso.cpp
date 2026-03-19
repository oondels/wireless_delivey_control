/**
 * fim_curso.cpp — Implementação do sensor de fim de curso
 *
 * Debounce de 20 ms + bloqueio de 10 s após a liberação física.
 * O timer de 10 s começa quando o sensor solta, não quando aciona.
 *
 * Ref: hardware_io/SPEC.md §6.1, seguranca/SPEC.md §6.2
 */

#include "fim_curso.h"
#include "logger.h"

void FimCurso::atualizar() {
    bool leitura = digitalRead(_gpio);

    if (leitura != _ultimaLeitura) {
        _ultimoCambioMs = millis();
        _ultimaLeitura  = leitura;
    }

    if ((millis() - _ultimoCambioMs) >= DEBOUNCE_MS) {
        bool novoEstado = (leitura == LOW);  // LOW = acionado (pull-up interno)
        if (novoEstado != _estadoFiltrado) {
            if (novoEstado) {
                LOG_WARN("SENSOR", "Fim de curso descida ACIONADO");
            } else {
                _ultimaLiberacaoMs = millis();
                LOG_INFO("SENSOR", "Fim de curso descida liberado — bloqueio de 10 s iniciado");
            }
            _estadoFiltrado = novoEstado;
        }
    }
}

bool FimCurso::acionado() const {
    if (_estadoFiltrado) {
        return true;
    }
    if (_ultimaLiberacaoMs > 0 &&
        (millis() - _ultimaLiberacaoMs) < BLOQUEIO_POS_LIBERACAO_MS) {
        return true;
    }
    return false;
}
