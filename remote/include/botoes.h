/**
 * botoes.h — Leitura de botões do Módulo Remote
 *
 * Debounce 50ms para todos os botões.
 * SUBIR/DESCER: hold (nível) — true enquanto pressionado.
 * VEL1/VEL2/VEL3: pulso (borda descida) — true apenas no momento da pressão.
 * EMERGÊNCIA: nível contínuo (trava mecânica) — true enquanto travado.
 *
 * Ref: hardware_io/SPEC.md §6
 */

#ifndef BOTOES_H
#define BOTOES_H

#include <Arduino.h>
#include "pinout.h"

struct EstadoBotoes {
    bool subir_hold;       // true = SUBIR pressionado agora
    bool descer_hold;      // true = DESCER pressionado agora
    bool vel1_pulso;       // true = VEL1 acabou de ser pressionado (borda)
    bool vel2_pulso;       // true = VEL2 acabou de ser pressionado (borda)
    bool vel3_pulso;       // true = VEL3 acabou de ser pressionado (borda)
    bool emergencia;       // true = botão emergência com trava ativo (nível)
};

class Botoes {
public:
    static constexpr uint32_t DEBOUNCE_MS = 50;
    static constexpr int NUM_BOTOES = 6;

    void init();

    // Atualiza debounce e retorna estado atual. Chamar uma vez por ciclo do loop.
    EstadoBotoes ler();

private:
    enum { IDX_SUBIR = 0, IDX_DESCER, IDX_VEL1, IDX_VEL2, IDX_VEL3, IDX_EMERGENCIA };

    static const uint8_t _pinos[NUM_BOTOES];
    bool     _ultimaLeitura[NUM_BOTOES];
    bool     _estadoFiltrado[NUM_BOTOES];
    uint32_t _ultimoCambio[NUM_BOTOES];
};

#endif // BOTOES_H
