/**
 * maquina_estados.cpp — Implementação da máquina de estados do Principal
 *
 * Avaliação sequencial por prioridade. A primeira condição verdadeira
 * determina o estado e a função retorna imediatamente.
 *
 * Invariantes (SPEC §8):
 * - Motor OFF implica Freio ON
 * - Motor ON implica Freio OFF
 * - EMERGENCIA/FALHA_ENERGIA/FALHA_COMUNICACAO nunca transitiona direto para SUBINDO/DESCENDO
 *
 * Ref: maquina_estados/SPEC.md §4–6
 */

#include "maquina_estados.h"
#include "logger.h"

void MaquinaEstados::init() {
    _estado = ESTADO_PARADO;
}

EstadoSistema MaquinaEstados::atualizar(
    Emergencia&        emergencia,
    MonitorRede&       monitorRede,
    WatchdogComm&      watchdog,
    Sensores&          sensores,
    Motor&             motor,
    Freio&             freio,
    const EstadoBotoes& botoesLocal,
    const volatile PacoteRemote& pacoteRemote,
    bool               rearmeAtivo
) {
    const bool tentativaRemotaSubir = (pacoteRemote.botao_hold == 1) && (pacoteRemote.comando == CMD_SUBIR);
    const bool tentativaRemotaDescer = (pacoteRemote.botao_hold == 1) && (pacoteRemote.comando == CMD_DESCER);
    const bool watchdogExpirado = watchdog.expirado();

    // Leitura da microchave do freio (NA + pull-up interno: HIGH = freio engatado)
    const bool freio_engatado = (digitalRead(PIN_MICROCHAVE_FREIO) == HIGH);

    // Prioridade 1: emergência (botão local OU flag já ativa OU Remote)
    // Se rearmeAtivo=true, sinal de emergência do remote é ignorado (sobreposto pelo REARME)
    const bool emRemote = (pacoteRemote.emergencia == 1) && !rearmeAtivo;
    if (emergencia.verificar(emRemote)) {
        if (_estado != ESTADO_EMERGENCIA) {
            LOG_WARN("MAQEST", "Movimentacao BLOQUEADA — emergencia ativa");
        }

        if (tentativaRemotaSubir && !_logBloqueioRemotoSubir) {
            LOG_WARN("REMOTO", "Comando SUBIR bloqueado - emergencia ativa/freio acionado");
            _logBloqueioRemotoSubir = true;
        }
        if (tentativaRemotaDescer && !_logBloqueioRemotoDescer) {
            LOG_WARN("REMOTO", "Comando DESCER bloqueado - emergencia ativa/freio acionado");
            _logBloqueioRemotoDescer = true;
        }

        motor.desligar();
        freio.acionar();
        _estado = ESTADO_EMERGENCIA;
        return _estado;
    }

    if (!tentativaRemotaSubir) {
        _logBloqueioRemotoSubir = false;
    }
    if (!tentativaRemotaDescer) {
        _logBloqueioRemotoDescer = false;
    }

    // Prioridade 2: falha de energia da rede elétrica
    if (!monitorRede.redePresente()) {
        if (_estado != ESTADO_FALHA_ENERGIA) {
            LOG_WARN("MAQEST", "Movimentacao BLOQUEADA — falha de energia da rede");
        }
        motor.desligar();
        freio.acionar();
        _estado = ESTADO_FALHA_ENERGIA;
        return _estado;
    }

    // Prioridade 3: watchdog de comunicação
    if (watchdogExpirado && !_controleLocalSemRemote) {
        if (_estado != ESTADO_FALHA_COMUNICACAO) {
            LOG_WARN("MAQEST", "Movimentacao BLOQUEADA — falha de comunicacao");
        }
        motor.desligar();
        freio.acionar();
        _estado = ESTADO_FALHA_COMUNICACAO;
        return _estado;
    }

    if (!watchdogExpirado && _controleLocalSemRemote) {
        _controleLocalSemRemote = false;
        LOG_INFO("MAQEST", "Comunicacao restabelecida — modo degradado local desativado");
    }

    // Prioridade 4: fim de curso (bloqueia SUBIR e DESCER)
    if (sensores.fimDeCursoAcionado()) {
        if (tentativaRemotaSubir && !_logBloqueioRemotoSubir) {
            LOG_WARN("REMOTO", "Comando SUBIR bloqueado - fim de curso acionado");
            _logBloqueioRemotoSubir = true;
        }
        if (tentativaRemotaDescer && !_logBloqueioRemotoDescer) {
            LOG_WARN("REMOTO", "Comando DESCER bloqueado - fim de curso acionado");
            _logBloqueioRemotoDescer = true;
        }
        motor.desligar();
        freio.acionar();
        _estado = ESTADO_PARADO;
        return _estado;
    }

    // Prioridade 5: movimentação (Homem-Morto)
    // Determinar direção: botão local tem prioridade sobre Remote
    Direcao dir = DIR_NENHUMA;
    bool holdLocal  = botoesLocal.subir_hold || botoesLocal.descer_hold;
    bool holdRemote = (pacoteRemote.botao_hold == 1) && !watchdogExpirado;

    if (holdLocal) {
        // Prioridade do Painel Central
        if (botoesLocal.subir_hold) {
            dir = DIR_SUBIR;
        } else if (botoesLocal.descer_hold) {
            dir = DIR_DESCER;
        }
    } else if (holdRemote) {
        // Comando do Remote
        if (pacoteRemote.comando == CMD_SUBIR) {
            dir = DIR_SUBIR;
        } else if (pacoteRemote.comando == CMD_DESCER) {
            dir = DIR_DESCER;
        }
    }

    // Fim de curso bloqueia apenas SUBIR — DESCER continua permitido
    if (dir == DIR_SUBIR && sensores.fimDeCursoAcionado()) {
        dir = DIR_NENHUMA;
    }

    if (dir != DIR_NENHUMA) {
        if (!freio_engatado) {
            freio.liberar();
            motor.acionar(dir);
            _estado = (dir == DIR_SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
        } else {
            // Operador tenta mover mas freio está engatado por hardware — bloqueia silenciosamente
            motor.desligar();
            freio.acionar();
            _estado = ESTADO_PARADO;
        }
        return _estado;
    }

    // Prioridade 6: padrão — PARADO
    motor.desligar();
    freio.acionar();
    _estado = ESTADO_PARADO;
    return _estado;
}
