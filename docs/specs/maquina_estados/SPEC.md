# Especificação da Máquina de Estados

**Versão:** 1.3
**Data:** 2026-03-22
**Referência:** DESIGN_SPEC.md v3.3, README.md v3.4

---

## 1. Visão Geral

O Módulo Principal (Painel Central) opera sob uma máquina de estados finita que governa todo o comportamento do sistema. A máquina é avaliada a **cada ciclo do loop principal**, seguindo uma ordem estrita de prioridade.

---

## 2. Estados do Sistema

```c
typedef enum {
    ESTADO_PARADO            = 0,
    ESTADO_SUBINDO           = 1,
    ESTADO_DESCENDO          = 2,
    ESTADO_EMERGENCIA        = 3,
    ESTADO_FALHA_COMUNICACAO = 4,
    ESTADO_FALHA_ENERGIA     = 5   // queda de energia da rede elétrica (GPIO 13)
} EstadoSistema;
```

| Estado | Motor | Freio | Descrição |
|---|---|---|---|
| `PARADO` | OFF | ON | Estado seguro padrão. Motor desligado, freio acionado. |
| `SUBINDO` | ON (direção A) | OFF | Motor ativo no sentido SUBIR. Freio liberado. |
| `DESCENDO` | ON (direção B) | OFF | Motor ativo no sentido DESCER. Freio liberado. |
| `EMERGENCIA_ATIVA` | OFF | ON | Emergência. Motor desligado, freio acionado. Auto-libera ao soltar todos os botões; REARME apenas se remote travado. |
| `FALHA_COMUNICACAO` | OFF | ON | Perda de link detectada. Motor desligado e freio acionado imediatamente. Requer REARME para liberar controle local em modo degradado. |
| `FALHA_ENERGIA` | OFF | ON | Queda de energia da rede elétrica (GPIO 13 LOW). Motor desligado e freio acionado imediatamente. Requer REARME manual. |

---

## 3. Diagrama de Transições

```
                    ┌──────────────────────────────────────────┐
                    │           EMERGENCIA_ATIVA               │◄─── Emergência (Painel ou Remote)
                    │  Motor: OFF | Freio: ON                  │     (de qualquer estado)
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Auto-libera (fontes inativas) OU
                                     │ Rearme MANUAL (se remote travado)
                                     ▼
                    ┌──────────────────────────────────────────┐
                    │           FALHA_ENERGIA                  │◄─── GPIO 13 LOW + debounce 50 ms
                    │  Motor: OFF | Freio: ON                  │     (de qualquer estado)
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
                    ┌──────────────────────────────────────────┐
                    │           FALHA_COMUNICACAO              │◄─── Watchdog timeout / Remote off
                    │  Motor: OFF | Freio: ON                  │     (de qualquer estado operacional)
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
          ┌──────────┐  hold SUBIR  ┌───────────┐  hold DESCER  ┌──────────┐
          │  SUBINDO │◄─────────── │  PARADO   │──────────────►│ DESCENDO │
          │ Motor:ON │             │ Motor:OFF │               │ Motor:ON │
          │ Freio:OFF│────────────►│ Freio:ON  │◄──────────────│ Freio:OFF│
          └──────────┘  solto      └─────▲─────┘  solto        └──────────┘
                                         │
                              Fim de curso acionado
                              Motor OFF → Freio ON → PARADO (sem rearme)
```

---

## 4. Prioridade de Avaliação

A máquina é avaliada sequencialmente a cada ciclo. A **primeira condição verdadeira** determina o estado e a função retorna imediatamente.

| Prioridade | Condição | Transição |
|---|---|---|
| 1 (máxima) | `emergencia.verificar()` retorna true (botão local OU flag ativa) | → `EMERGENCIA_ATIVA` |
| 2 | `!monitorRede.redePresente()` (GPIO 13 LOW com debounce 50 ms) | → `FALHA_ENERGIA` |
| 3 | `watchdog.expirado()` (> 500ms sem pacote) e modo degradado local inativo | → `FALHA_COMUNICACAO` |
| 4 | Fim de curso local acionado (bloqueia SUBIR e DESCER) | → `PARADO` |
| 5 | Botão hold ativo + direção válida (DESCER bloqueado se `fim_curso_descida == 1`) | → `SUBINDO` ou `DESCENDO` |
| 6 (padrão) | Nenhuma condição acima | → `PARADO` |

---

## 5. Transições Detalhadas

### 5.1 Transições Globais (de Qualquer Estado)

| De | Para | Gatilho |
|---|---|---|
| Qualquer | `EMERGENCIA_ATIVA` | Botão emergência Painel ativo OU `emergencia == 1` no pacote Remote |
| Qualquer | `FALHA_ENERGIA` | GPIO 13 LOW com debounce 50 ms (rede elétrica ausente) |
| Qualquer (operacional) | `FALHA_COMUNICACAO` | Watchdog timeout (500 ms sem pacote) |

### 5.2 Transições Operacionais

| De | Para | Gatilho |
|---|---|---|
| `PARADO` | `SUBINDO` | Botão SUBIR hold + sem bloqueios |
| `PARADO` | `DESCENDO` | Botão DESCER hold + sem bloqueios |
| `SUBINDO` | `PARADO` | Botão SUBIR solto (Homem-Morto) |
| `SUBINDO` | `PARADO` | Fim de curso acionado |
| `DESCENDO` | `PARADO` | Botão DESCER solto (Homem-Morto) |
| `SUBINDO` | `DESCENDO` | Trocar de SUBIR para DESCER (via dead-time 100 ms) |
| `DESCENDO` | `SUBINDO` | Trocar de DESCER para SUBIR (via dead-time 100 ms) |

### 5.3 Transições de Recuperação

| De | Para | Gatilho |
|---|---|---|
| `EMERGENCIA_ATIVA` | `PARADO` | Rearme manual no Painel Central |
| `FALHA_ENERGIA` | `PARADO` | Rearme manual no Painel Central (rede deve ter sido restabelecida) |
| `FALHA_COMUNICACAO` | `PARADO` | Rearme manual no Painel Central |
| `PARADO` (modo degradado) | `SUBINDO`/`DESCENDO` | Botão hold local ativo mesmo com watchdog expirado |

---

## 6. Pseudocódigo da Máquina de Estados

```cpp
// Objetos instanciados no escopo global (principal.cpp):
// Emergencia emergencia; Motor motor; Freio freio; Sensores sensores;
// WatchdogComm watchdog; Botoes botoes; Velocidade velocidade;

void atualizar_maquina_estados() {
    // Prioridade 1: emergência
    if (emergencia.verificar(pacoteRemote.emergencia)) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_EMERGENCIA;
        return;
    }

    // Prioridade 2: falha de energia da rede
    if (!monitorRede.redePresente()) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_FALHA_ENERGIA;
        return;
    }

    // Prioridade 3: watchdog (bloqueio total enquanto nao houver rearme)
    if (watchdog.expirado() && !controleLocalSemRemote) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_FALHA_COMUNICACAO;
        return;
    }

    // Prioridade 4: fim de curso local (bloqueia SUBIR e DESCER)
    if (sensores.fimDeCursoAcionado()) {
        motor.desligar();
        freio.acionar();
        estado = ESTADO_PARADO;
        return;
    }

    // Prioridade 5: movimentação
    bool hold = botao_hold_local || (pacoteRemote.botao_hold && !watchdog.expirado());
    Direcao dir = obter_direcao_ativa();

    // Fim de curso de descida do Remote — bloqueia apenas DESCER, SUBIR permitido
    if (dir == DIR_DESCER && pacoteRemote.fim_curso_descida == 1) {
        dir = DIR_NENHUMA;
    }

    if (hold && dir != DIR_NENHUMA) {
        // Inicia liberação do freio (idempotente — só pulsa se não estiver já liberando/liberado)
        freio.liberar();

        // Motor só aciona após microchave confirmar freio liberado (GPIO 27 = LOW, ~10s de espera)
        // Dupla verificação: estado interno E leitura direta do GPIO
        bool freio_liberado = freio.isLiberado() && (digitalRead(PIN_MICROCHAVE_FREIO) == LOW);

        if (freio_liberado) {
            motor.acionar(dir);
            estado = (dir == DIR_SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
        } else {
            // Freio ainda em transição (~10s) — aguardar sem acionar motor
            motor.desligar();
            estado = ESTADO_PARADO;
        }
    } else {
        motor.desligar();
        freio.acionar();  // Inicia engate (idempotente)
        estado = ESTADO_PARADO;
    }
}
```

---

## 7. Rearme (Transição de Recuperação)

### 7.1 Condições para Rearme

O botão REARME no Painel Central (`rearme.verificar()`) limpa os estados `EMERGENCIA_ATIVA`, `FALHA_ENERGIA` e `FALHA_COMUNICACAO` quando:

1. O botão de emergência que originou o estado está fisicamente solto; **ou**
2. O Painel Central aceita o rearme mesmo com botão Remote travado (caso especial — ver seção 7.2).

### 7.2 Rearme com Botão Remote Travado

Se `pacote_remote.emergencia == 1` no momento do rearme:

1. Principal limpa `emergencia.ativa()`.
2. Principal seta `rearme_ativo = 1` no `PacoteStatus` (via `rearme.isRearmeAtivo()`).
3. Remote acende LED ALARME (pisca 2 Hz).
4. `rearme_ativo` é limpo quando `pacote_remote.emergencia` voltar a 0.

### 7.3 Pós-Rearme

- Estado → `PARADO`.
- Motor permanece OFF.
- Freio permanece ON.
- Operador deve iniciar nova movimentação manualmente.
- Se o estado anterior era `FALHA_COMUNICACAO`, o sistema habilita modo degradado local até o watchdog recuperar.
- Se o estado anterior era `FALHA_ENERGIA`, o sistema retorna a operação normal assim que a rede for detectada (GPIO 13 HIGH).

---

## 8. Invariantes da Máquina de Estados

1. O estado `EMERGENCIA_ATIVA` **nunca** transiciona diretamente para `SUBINDO` ou `DESCENDO`.
2. O estado `FALHA_ENERGIA` **nunca** transiciona diretamente para `SUBINDO` ou `DESCENDO`; a recuperação passa por `PARADO` com REARME.
3. O estado `FALHA_COMUNICACAO` **nunca** transiciona diretamente para `SUBINDO` ou `DESCENDO`; a recuperação passa por `PARADO` com REARME.
4. Os estados `SUBINDO` e `DESCENDO` **nunca** coexistem (dois relés de direção simultâneos).
5. Motor ON implica `freio.isLiberado() == true` E `GPIO 27 == LOW`.
6. Durante transição do freio (ENGATANDO/LIBERANDO), o motor permanece desligado e o estado é `PARADO`.
7. A transição de recuperação **sempre** passa por `PARADO`.
8. `emergencia.ativa()` é limpa automaticamente quando todas as fontes inativas. Nunca limpa automaticamente se Remote mantém `emergencia == 1` — exige REARME manual.
