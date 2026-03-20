/**
 * freio.h — Controle do freio solenoide de dupla bobina
 *
 * FREIO_ON  (GPIO 19): bobina de aplicação — cilindro avança, freio trava. LED aceso quando ativo.
 * FREIO_OFF (GPIO 22): bobina de liberação — cilindro recua, freio libera. Sem LED.
 *
 * Invariante: FREIO_ON e FREIO_OFF nunca ficam HIGH simultaneamente.
 * A troca segue: desaciona lado ativo → dead-time ~10 ms → aciona lado oposto.
 *
 * Sem leitura de sensor — a microchave do freio atua diretamente no circuito (hardware externo).
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
    void acionar();    // FREIO_OFF → LOW, delay 10ms, FREIO_ON → HIGH (freio aplicado + LED aceso)
    void liberar();    // FREIO_ON  → LOW, delay 10ms, FREIO_OFF → HIGH (freio liberado + LED apagado)
};

#endif // FREIO_H
