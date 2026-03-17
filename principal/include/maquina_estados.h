/**
 * maquina_estados.h — Máquina de estados finita do Módulo Principal
 *
 * Avalia condições em ordem estrita de prioridade a cada ciclo:
 * 1) Emergência, 2) Watchdog, 3) Fim de curso, 4) Movimentação, 5) PARADO.
 *
 * Ref: maquina_estados/SPEC.md §4–6
 */

#ifndef MAQUINA_ESTADOS_H
#define MAQUINA_ESTADOS_H

#include <Arduino.h>
#include "protocolo.h"
#include "emergencia.h"
#include "watchdog_comm.h"
#include "sensores.h"
#include "motor.h"
#include "freio.h"
#include "botoes.h"

class MaquinaEstados {
public:
    void init();

    /**
     * Avalia a máquina de estados com prioridade sequencial.
     * Recebe estado dos botões locais e o último pacote do Remote.
     * Aciona/desliga motor e freio conforme transição.
     * Retorna o estado resultante.
     */
    EstadoSistema atualizar(
        Emergencia&        emergencia,
        WatchdogComm&      watchdog,
        Sensores&          sensores,
        Motor&             motor,
        Freio&             freio,
        const EstadoBotoes& botoesLocal,
        const volatile PacoteRemote& pacoteRemote
    );

    EstadoSistema estadoAtual() const { return _estado; }

private:
    EstadoSistema _estado = ESTADO_PARADO;
};

#endif // MAQUINA_ESTADOS_H
