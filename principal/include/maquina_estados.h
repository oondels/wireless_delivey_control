/**
 * maquina_estados.h — Máquina de estados finita do Módulo Principal
 *
 * Avalia condições em ordem estrita de prioridade a cada ciclo:
 * 1) Emergência, 2) Falha energia, 3) Watchdog, 4) Fim de curso, 5) Movimentação, 6) PARADO.
 *
 * Ref: maquina_estados/SPEC.md §4–6
 */

#ifndef MAQUINA_ESTADOS_H
#define MAQUINA_ESTADOS_H

#include <Arduino.h>
#include "protocolo.h"
#include "emergencia.h"
#include "monitor_rede.h"
#include "watchdog_comm.h"
#include "sensores.h"
#include "motor.h"
#include "freio.h"
#include "botoes.h"

class MaquinaEstados {
public:
    void init();

    // Habilita operacao local no Principal mesmo com watchdog expirado,
    // exigindo rearme manual previo em FALHA_COMUNICACAO.
    void habilitarControleLocalSemRemote() { _controleLocalSemRemote = true; }
    void desabilitarControleLocalSemRemote() { _controleLocalSemRemote = false; }
    bool controleLocalSemRemoteAtivo() const { return _controleLocalSemRemote; }
    void definirEstado(EstadoSistema novoEstado) { _estado = novoEstado; }

    /**
     * Avalia a máquina de estados com prioridade sequencial.
     * Recebe estado dos botões locais e o último pacote do Remote.
     * Aciona/desliga motor e freio conforme transição.
     * Retorna o estado resultante.
     */
    EstadoSistema atualizar(
        Emergencia&        emergencia,
        MonitorRede&       monitorRede,
        WatchdogComm&      watchdog,
        Sensores&          sensores,
        Motor&             motor,
        Freio&             freio,
        const EstadoBotoes& botoesLocal,
        const volatile PacoteRemote& pacoteRemote,
        bool               rearmeAtivo = false
    );

    EstadoSistema estadoAtual() const { return _estado; }

private:
    EstadoSistema _estado = ESTADO_PARADO;
    bool _logBloqueioRemotoSubir          = false;
    bool _logBloqueioRemotoDescer         = false;
    bool _logBloqueioFimCursoDescida      = false;
    bool _controleLocalSemRemote          = false;
};

#endif // MAQUINA_ESTADOS_H
