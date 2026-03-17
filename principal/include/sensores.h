/**
 * sensores.h — Leitura do sensor fim de curso com debounce
 *
 * Debounce de 20 ms via millis() (não-bloqueante).
 * Lógica: LOW = sensor acionado (pull-up externo).
 *
 * Ref: motor/SPEC.md §5
 */

#ifndef SENSORES_H
#define SENSORES_H

#include <stdbool.h>

#define DEBOUNCE_FIM_CURSO_MS 20

void sensores_init();
bool fim_de_curso_acionado();

#endif // SENSORES_H
