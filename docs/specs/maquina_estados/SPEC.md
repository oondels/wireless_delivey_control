# Especificação da Máquina de Estados

**Versão:** 1.0
**Data:** 2026-03-16
**Referência:** DESIGN_SPEC.md v3.1

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
    ESTADO_FALHA_COMUNICACAO = 4
} EstadoSistema;
```

| Estado | Motor | Freio | Descrição |
|---|---|---|---|
| `PARADO` | OFF | ON | Estado seguro padrão. Motor desligado, freio acionado. |
| `SUBINDO` | ON (direção A) | OFF | Motor ativo no sentido SUBIR. Freio liberado. |
| `DESCENDO` | ON (direção B) | OFF | Motor ativo no sentido DESCER. Freio liberado. |
| `EMERGENCIA_ATIVA` | OFF | ON | Emergência. Motor desligado, freio acionado. Requer rearme. |
| `FALHA_COMUNICACAO` | OFF | ON | Perda de link. Motor desligado, freio acionado. Requer rearme. |

---

## 3. Diagrama de Transições

```
                    ┌──────────────────────────────────────────┐
                    │           EMERGENCIA_ATIVA               │◄─── Emergência (Painel ou Remote)
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
| 2 | `watchdog.expirado()` (> 500ms sem pacote) | → `FALHA_COMUNICACAO` |
| 3 | Fim de curso acionado | → `PARADO` |
| 4 | Botão hold ativo + direção válida | → `SUBINDO` ou `DESCENDO` |
| 5 (padrão) | Nenhuma condição acima | → `PARADO` |

---

## 5. Transições Detalhadas

### 5.1 Transições Globais (de Qualquer Estado)

| De | Para | Gatilho |
|---|---|---|
| Qualquer | `EMERGENCIA_ATIVA` | Botão emergência Painel ativo OU `emergencia == 1` no pacote Remote |
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
| `FALHA_COMUNICACAO` | `PARADO` | Rearme manual no Painel Central |

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

    // Prioridade 2: watchdog
    if (watchdog.expirado()) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_FALHA_COMUNICACAO;
        return;
    }

    // Prioridade 3: fim de curso
    if (sensores.fimDeCursoAcionado()) {
        motor.desligar();
        freio.acionar();
        estado = ESTADO_PARADO;
        return;
    }

    // Prioridade 4: movimentação
    bool hold = botao_hold_local || pacoteRemote.botao_hold;
    Direcao dir = obter_direcao_ativa();

    if (hold && dir != DIR_NENHUMA) {
        freio.liberar();
        motor.acionar(dir);
        estado = (dir == DIR_SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
    } else {
        motor.desligar();
        freio.acionar();
        estado = ESTADO_PARADO;
    }
}
```

---

## 7. Rearme (Transição de Recuperação)

### 7.1 Condições para Rearme

O botão REARME no Painel Central (`rearme.verificar()`) limpa os estados de erro quando:

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

---

## 8. Invariantes da Máquina de Estados

1. O estado `EMERGENCIA_ATIVA` **nunca** transiciona diretamente para `SUBINDO` ou `DESCENDO`.
2. O estado `FALHA_COMUNICACAO` **nunca** transiciona diretamente para `SUBINDO` ou `DESCENDO`.
3. Os estados `SUBINDO` e `DESCENDO` **nunca** coexistem (dois relés de direção simultâneos).
4. Todo estado com Motor OFF implica Freio ON.
5. Todo estado com Motor ON implica Freio OFF.
6. A transição de recuperação **sempre** passa por `PARADO`.
