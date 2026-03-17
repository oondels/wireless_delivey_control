/**
 * freio.h — Controle do relé de freio mecânico
 *
 * Apenas controle do GPIO do relé. Sem leitura de sensor —
 * a microchave do freio atua diretamente no circuito (hardware externo).
 *
 * Ref: motor/SPEC.md §4
 */

#ifndef FREIO_H
#define FREIO_H

#include <Arduino.h>
#include "pinout.h"

class Freio {
public:
    void init();
    void acionar();    // GPIO HIGH → relé ON → freio aplicado + LED aceso
    void liberar();    // GPIO LOW  → relé OFF → freio liberado + LED apagado
};

#endif // FREIO_H
