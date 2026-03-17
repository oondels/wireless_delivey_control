/**
 * botoes.h — Leitura de botões do Módulo Principal
 *
 * Debounce 50ms para todos os botões.
 * SUBIR/DESCER: hold (nível) — true enquanto pressionado.
 * VEL1/VEL2/VEL3: pulso (borda descida) — true apenas no momento da pressão.
 * EMERGÊNCIA: nível contínuo (trava mecânica) — tratado em emergencia.h.
 * REARME: pulso (borda descida) — tratado em rearme.h.
 *
 * Ref: hardware_io/SPEC.md §4
 */

#ifndef BOTOES_H
#define BOTOES_H

#include <stdbool.h>

typedef struct {
    bool subir_hold;    // true = SUBIR pressionado agora
    bool descer_hold;   // true = DESCER pressionado agora
    bool vel1_pulso;    // true = VEL1 acabou de ser pressionado (borda)
    bool vel2_pulso;    // true = VEL2 acabou de ser pressionado (borda)
    bool vel3_pulso;    // true = VEL3 acabou de ser pressionado (borda)
} EstadoBotoes;

void botoes_init();

// Atualiza debounce e retorna estado atual. Chamar uma vez por ciclo do loop.
EstadoBotoes botoes_ler();

#endif // BOTOES_H
