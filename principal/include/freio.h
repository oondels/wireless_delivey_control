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

void freio_init();
void acionar_freio();   // GPIO HIGH → relé ON → freio aplicado + LED aceso
void liberar_freio();   // GPIO LOW  → relé OFF → freio liberado + LED apagado

#endif // FREIO_H
