/**
 * monitor_rede.cpp — Implementação do monitor de rede elétrica
 *
 * Debounce de 50 ms para filtrar transitórios de rede.
 * Logging apenas em transições de estado (não repete a cada ciclo).
 *
 * Ref: hardware_io/SPEC.md §4.2, seguranca/SPEC.md §2
 */

#include "monitor_rede.h"
#include "logger.h"

void MonitorRede::init() {
    pinMode(PIN_MONITOR_REDE, INPUT);  // pull-down externo — sem pull interno
    _leituraAnterior = (digitalRead(PIN_MONITOR_REDE) == HIGH);
    _ultimoEstavel   = _leituraAnterior;
    _tempoMudanca    = millis();
}

void MonitorRede::atualizar() {
    bool leituraAtual = (digitalRead(PIN_MONITOR_REDE) == HIGH);

    if (leituraAtual != _leituraAnterior) {
        _tempoMudanca    = millis();
        _leituraAnterior = leituraAtual;
    }

    if ((millis() - _tempoMudanca) >= DEBOUNCE_MS) {
        if (leituraAtual != _ultimoEstavel) {
            _ultimoEstavel = leituraAtual;
            if (_ultimoEstavel) {
                LOG_INFO("REDE", "Rede eletrica RESTAURADA — GPIO 13 HIGH");
            } else {
                LOG_WARN("REDE", "Rede eletrica AUSENTE — GPIO 13 LOW");
            }
        }
    }
}
