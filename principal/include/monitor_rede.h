/**
 * monitor_rede.h — Monitoramento da presença da rede elétrica (GPIO 13)
 *
 * GPIO 13: HIGH = rede presente, LOW = rede ausente.
 * Debounce de 50 ms para filtrar transitórios (brownout, micro-quedas).
 * Leitura não-bloqueante via millis().
 *
 * Circuito recomendado: divisor resistivo 5V→2,5V (R1=R2=10kΩ) ou
 * optoacoplador (PC817) com pull-down externo de 10kΩ no lado do ESP32.
 *
 * Ref: hardware_io/SPEC.md §4.2, seguranca/SPEC.md §2
 */

#ifndef MONITOR_REDE_H
#define MONITOR_REDE_H

#include <Arduino.h>
#include "pinout.h"

class MonitorRede {
public:
    static constexpr uint32_t DEBOUNCE_MS = 50;

    void init();

    // Atualizar debounce — chamar a cada ciclo do loop
    void atualizar();

    // Retorna true se a rede está presente (estado estável HIGH)
    #ifdef BYPASS_MONITOR_REDE
    bool redePresente() const { return true; }
    #else
    bool redePresente() const { return _ultimoEstavel; }
    #endif

private:
    bool     _ultimoEstavel    = true;   // estado estável após debounce
    bool     _leituraAnterior  = true;
    uint32_t _tempoMudanca     = 0;
};

#endif // MONITOR_REDE_H
