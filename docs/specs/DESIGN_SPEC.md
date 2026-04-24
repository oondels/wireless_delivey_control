# Design Specification — Controle Remoto para Carrinho de Jet Ski

**Versão:** 3.3
**Data:** 2026-03-22
**Status:** Em execução

---

## 1. Visão Geral

O sistema moderniza o controle de um carrinho de transporte de jet skis movido por guincho motorizado. A operação, antes restrita a um painel fixo no depósito, passa a ser realizada por controle remoto sem fio, permitindo que o operador acompanhe o equipamento ao longo de todo o trajeto entre o depósito e a margem do rio.

A **prioridade absoluta do sistema é a segurança (Fail-Safe):** qualquer falha de comunicação, queda de energia ou acionamento de emergência resulta no freio mecânico sendo aplicado imediatamente.

---

## 2. Arquitetura do Sistema

O sistema adota uma arquitetura **Mestre-Escravo** com dois ESP32 comunicando-se via **ESP-NOW** (peer-to-peer, sem roteador).

```
┌──────────────────────────────────┐     ESP-NOW (bidirecional)     ┌─────────────────────────────────┐
│     MÓDULO PRINCIPAL             │  ────────────────────────────► │     MÓDULO REMOTE               │
│  (Painel Central / Mestre)       │  ◄────────────────────────────  │  (Carrinho / Escravo)           │
│                                  │   Cmds ←→ Heartbeat / Status   │                                 │
│  - ESP32                         │                                 │  - ESP32                        │
│  - Botões do painel fixo         │                                 │  - Botões do operador           │
│  - Botão de emergência c/ trava  │                                 │  - Botão de emergência c/ trava │
│  - Relés: direção, velocidade,   │                                 │  - LEDs de status (cor física)  │
│    freio                         │                                 │  - Bateria                      │
│  - Leitura: microchave freio,    │                                 │  - Enclosure IP54               │
│    fim de curso estacionamento   │                                 │                                 │
│  - LEDs de status (cor física)   │                                 │                                 │
│  - Alimentação rede elétrica     │                                 │                                 │
└──────────────────────────────────┘                                 └─────────────────────────────────┘
```

### 2.1 Hierarquia de Controle

O Módulo Principal possui **autoridade máxima** sobre o sistema:

- Comandos de direção e velocidade do Painel Central têm prioridade sobre os do Remote.
- O Painel Central pode ativar **e desativar** o estado de emergência, incluindo emergências originadas no Remote.
- O Remote **nunca** pode desativar uma emergência por conta própria; apenas solicitar seu acionamento.

---

## 3. Descrição dos Módulos

### 3.1 Módulo Principal (Bridge ESP↔CLP)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Painel fixo no estacionamento/depósito |
| Alimentação | Fonte derivada da rede elétrica 110/220V |
| Entradas | 2 botões de teste local, 4 feedbacks do CLP e 1 micro do freio |
| Saídas | 7 sinais GPIO para entradas do CLP + 1 LED LINK |
| Comunicação | ESP-NOW — recebe `PacoteRemote`, envia `PacoteStatus` com feedbacks |

### 3.2 Módulo Remote (Carrinho)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Embarcado no carrinho de transporte |
| Alimentação | Bateria recarregável (ex: Li-Ion 18650 + regulador 3.3V) |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, EMERGÊNCIA (c/ trava); fim de curso descida |
| Saídas LEDs | 1 GPIO por LED: LINK, MOTOR, VEL1, VEL2, EMERGÊNCIA — cor definida pelo LED físico instalado |
| Comunicação | ESP-NOW — transmite comandos e heartbeat para o Principal |

> **Nota sobre LEDs:** todos os LEDs são componentes discretos de 3V (padrão Arduino) com cor física pré-definida externamente. O firmware controla apenas o estado lógico de cada GPIO (HIGH/LOW e frequência de piscar). Não há controle de cor por software.

---

## 4. Sensores de Hardware

### 4.1 Micro do Freio

Conectada ao `GPIO 14` do Módulo Principal como contato **NC** com `INPUT_PULLUP`.

- **GPIO 14 LOW** = condição normal
- **GPIO 14 HIGH** = micro aberta/acionada ou cabo rompido

O Principal não usa esse sinal para controlar o freio diretamente; ele apenas o retransmite ao Remote via `PacoteStatus.micro_freio_ativa`.

### 4.2 Fim de Curso do Estacionamento

Sensor instalado no estacionamento/depósito que é acionado quando o carrinho chega à posição final de subida.

**Comportamento ao acionar:**
- O Principal detecta o sinal do fim de curso (com debounce de 20 ms).
- O motor é cortado imediatamente.
- O freio mecânico é acionado imediatamente.
- O sistema entra no estado `PARADO` — não é um estado de emergência e não requer rearme manual.
- O operador pode retomar a operação normalmente após o acionamento.

**Diferença em relação à emergência:** o fim de curso representa uma condição operacional esperada (chegada ao destino), não uma falha. Por isso o sistema retorna a `PARADO` sem exigir rearme.

---

## 5. Regras de Negócio

### 5.1 Controle de Velocidade

- O sistema possui **2 níveis de velocidade** (`VEL1`, `VEL2`), selecionados por botões de pulso no Remote.
- O Principal apenas gera pulsos digitais para o CLP; a lógica de velocidade fica no Ladder.
- Os LEDs `VEL1` e `VEL2` do Remote refletem o feedback do CLP recebido no `PacoteStatus`.

### 5.2 Acionamento do Motor — Regra "Homem-Morto"

- O motor **só permanece em operação enquanto o botão SUBIR ou DESCER estiver fisicamente mantido pressionado.**
- Ao soltar o botão, o sistema executa imediatamente, nesta sequência:
  1. Corta a alimentação do motor (desaciona os relés de direção).
  2. Aciona o freio mecânico (aciona o relé de freio).
- Esta regra aplica-se tanto ao Painel Central quanto ao Remote.
- O Remote transmite o estado do botão (pressionado / solto) continuamente. O Principal executa a lógica localmente com base no estado recebido.

### 5.3 Sequência de Acionamento do Motor com Freio

O CLP executa toda a sequência de aplicação e liberação do freio. O papel do firmware ESP32 é:

1. O Remote lê os botões locais.
2. O Remote só envia `SUBIR` ou `DESCER` quando o status do Principal é válido e `emergencia_ativa == 0`.
3. O Principal replica os sinais ao CLP.
4. O CLP aciona motor, freio e velocidades.
5. O Principal lê os feedbacks do CLP e os retransmite ao Remote.

---

## 6. Protocolos de Segurança e Emergência (Fail-Safe)

### 6.1 Condições de Acionamento Automático do Freio

| # | Condição | Origem | Estado resultante |
|---|---|---|---|
| 1 | Perda de heartbeat do Remote (watchdog timeout) | Comunicação | `FALHA_COMUNICACAO` |
| 2 | Queda de energia ou desligamento do Remote | Hardware | `FALHA_COMUNICACAO` |
| 3 | Botão de EMERGÊNCIA acionado no Painel Central | Operador | `EMERGENCIA_ATIVA` |
| 4 | Botão de EMERGÊNCIA acionado no Remote | Operador | `EMERGENCIA_ATIVA` |
| 5 | Soltura do botão de acionamento (regra Homem-Morto) | Operador | `PARADO` |
| 6 | Microchave indicando freio engatado com motor ativo | Hardware | `PARADO` |
| 7 | Fim de curso do estacionamento acionado | Hardware | `PARADO` |

### 6.2 Botão de Emergência com Trava Mecânica

Os botões de emergência (Painel e Remote) são do tipo **com trava**: uma vez pressionados, o sinal permanece ativo continuamente até que o botão seja manualmente destrancado. O firmware lê o estado do pino como nível contínuo (não como borda).

**O sistema só pode sair de `EMERGENCIA_ATIVA` quando:**

1. O botão de emergência que originou o estado estiver fisicamente solto (sinal inativo); **e**
2. O operador do Painel Central pressionar o botão de REARME.

**Caso especial — Rearme com emergência do Remote ainda travada:**

Na arquitetura atual do firmware ESP32, não existe campo `rearme_ativo` no `PacoteStatus`. O ESP32 apenas retransmite o feedback atual do CLP e do hardware.

### 6.3 Prioridade da Emergência

- Comandos de emergência têm **prioridade máxima** no firmware, sobrepondo qualquer outro comando.
- Ao entrar em emergência: corte do motor → acionamento do freio → `EMERGENCIA_ATIVA = true`.
- Com `EMERGENCIA_ATIVA = true`, todos os comandos de movimentação do Remote são ignorados.

### 6.4 Desativação de Emergência (Rearme)

- Desativação **exclusivamente manual** via botão REARME no Painel Central.
- O sistema **jamais** rearma automaticamente.
- Após o rearme: sistema retorna a `PARADO` (freio acionado, motor desligado).

### 6.5 Watchdog de Comunicação

- Timeout: **500 ms** (configurável em firmware).
- Se nenhum pacote for recebido dentro do timeout: freio acionado, motor cortado, estado `FALHA_COMUNICACAO`.
- Remote envia heartbeat a cada **100 ms** mesmo sem botão pressionado.
- `FALHA_COMUNICACAO` exige rearme manual pelo Painel Central.

---

## 7. Máquina de Estados do Sistema (Módulo Principal)

```
                    ┌──────────────────────────────────────────┐
                    │           EMERGENCIA_ATIVA               │◄─── Emergência (Painel ou Remote)
                    │  Motor: OFF | Freio: ON                  │
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
                    ┌──────────────────────────────────────────┐
                    │           FALHA_COMUNICACAO              │◄─── Watchdog timeout / Remote off
                    │  Motor: OFF | Freio: ON                  │
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

Transições globais → EMERGENCIA_ATIVA (qualquer estado):
  - Botão EMERGÊNCIA ativo (Painel ou Remote)

Transições globais → FALHA_COMUNICACAO (qualquer estado operacional):
  - Watchdog timeout / queda de energia do Remote
```

### Tabela de Condições de Acionamento do Motor

| Emergência Ativa | Falha Comun. | Fim de Curso | Microchave | Botão Hold | Resultado |
|---|---|---|---|---|---|
| Não | Não | Não acionado | Liberado | Pressionado | Motor ON |
| Não | Não | Não acionado | Liberado | Solto | Motor OFF → Freio ON → PARADO |
| Não | Não | Não acionado | Engatado | Qualquer | Motor BLOQUEADO |
| Não | Não | Acionado | Qualquer | Qualquer | Motor OFF → Freio ON → PARADO |
| Não | Sim | Qualquer | Qualquer | Qualquer | FALHA_COMUNICACAO → Freio ON |
| Sim | Qualquer | Qualquer | Qualquer | Qualquer | EMERGENCIA_ATIVA → Freio ON |

---

## 8. Protocolo de Comunicação (ESP-NOW)

### 8.1 Emparelhamento

Ambos os módulos iniciam em modo de descoberta usando **broadcast** como peer inicial. O MAC real do peer é detectado dinamicamente a partir do primeiro pacote válido recebido, e o peer é registrado automaticamente via `esp_now_add_peer()`.

### 8.2 Pacote Remote → Principal

```c
typedef struct {
    uint8_t  comando;            // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                                 // 3=VEL1, 4=VEL2, 5=RESET
    uint8_t  botao_hold;         // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;         // 1=botão de emergência com trava ativo
    uint8_t  fim_curso_descida;  // 1=carrinho na posição final de descida
    uint32_t timestamp;          // millis() do Remote
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} PacoteRemote;
```

### 8.3 Pacote Principal → Remote (Status)

```c
typedef struct {
    uint8_t  link_ok;             // 1=Principal recebendo pacotes válidos do Remote
    uint8_t  motor_ativo;         // 1=CLP reporta motor ativo
    uint8_t  emergencia_ativa;    // 1=CLP reporta emergência ativa
    uint8_t  vel1_ativa;          // 1=CLP reporta velocidade 1 ativa
    uint8_t  vel2_ativa;          // 1=CLP reporta velocidade 2 ativa
    uint8_t  micro_freio_ativa;   // 1=micro do freio NC abriu
    uint8_t  checksum;
} PacoteStatus;
```

### 8.4 Frequência de Envio

| Direção | Condição | Frequência |
|---|---|---|
| Remote → Principal | Heartbeat (sem botão) | A cada 100 ms |
| Remote → Principal | Mudança de estado de botão | Imediato + repetir a cada 100 ms enquanto ativo |
| Principal → Remote | Status de retorno | A cada 200 ms ou imediato após mudança de estado |

---

## 9. Indicadores Visuais (LEDs)

Todos os LEDs são componentes discretos de **3V (padrão Arduino)** com cor física definida externamente no momento da montagem. O firmware controla apenas o estado lógico de cada GPIO: ligado, desligado ou piscando em uma frequência específica. Não há controle de cor por software.

Cada LED corresponde a **exatamente 1 GPIO de saída** no ESP32.

### 9.1 LEDs no Módulo Remote

| LED | GPIO | Comportamento | Condição |
|---|---|---|---|
| LINK | 4 | Piscando 1 Hz | Sem status válido do Principal |
| LINK | 4 | Ligado fixo | `link_ok == 1` e último status <= 500 ms |
| MOTOR | 16 | Ligado fixo | `motor_ativo == 1` |
| VEL1 | 17 | Ligado fixo | `vel1_ativa == 1` |
| VEL2 | 5 | Ligado fixo | `vel2_ativa == 1` |
| EMERGÊNCIA | 19 | Piscando 4 Hz | Botão local de emergência ativo |
| EMERGÊNCIA | 19 | Ligado fixo | `emergencia_ativa == 1` ou sem status válido |

**Total: 5 GPIOs de saída** (LINK, MOTOR, VEL1, VEL2, EMERGÊNCIA)

### 9.2 LEDs no Painel Central

> No Módulo Principal, os LEDs de relé (DIREÇÃO A/B, VEL1/2/3, FREIO) são controlados pelo mesmo GPIO que aciona o relé correspondente — ver pinout em `README.md` §5.1. O único LED com GPIO exclusivo é o LINK REMOTE.

| LED | GPIO | Comportamento | Condição |
|---|---|---|---|
| DIREÇÃO A | 4 | Ligado fixo | Motor ativo sentido subida (compartilhado c/ relé) |
| DIREÇÃO B | 16 | Ligado fixo | Motor ativo sentido descida (compartilhado c/ relé) |
| VEL1 | 17 | Ligado fixo | `velocidade_atual == 1` (compartilhado c/ relé) |
| VEL2 | 5 | Ligado fixo | `velocidade_atual == 2` (compartilhado c/ relé) |
| LINK REMOTE | 21 | Ligado fixo | Comunicação com Remote ativa (watchdog OK) |

**Total: 1 GPIO de saída exclusivo** (LINK REMOTE)

---

## 10. Requisitos Não-Funcionais

- **Latência:** Tempo entre acionamento do botão e resposta do motor < **100 ms**.
- **Watchdog:** Timeout padrão **500 ms**, configurável via constante em firmware.
- **Alcance:** Operação confiável em linha de visada de pelo menos **50 metros**.
- **Robustez:** Enclosure do Remote mínimo IP54 (respingos, umidade de ambiente de rio).
- **Segurança elétrica:** Relés dimensionados para corrente de partida do motor com fator de segurança 2x. Isolação galvânica obrigatória entre rede elétrica e GPIOs do ESP32.
- **Anti-colisão de direção:** Dead-time mínimo de **100 ms** ao inverter sentido do motor.
- **Rearme:** O sistema jamais rearma emergência automaticamente.
- **Fim de curso:** Debounce mínimo de 20 ms no sinal do sensor.

---

## 11. Sistema de Logging (Debug/Testes)

O firmware inclui logging via Serial (115200 baud) para depuração pré-deploy. Implementado como header-only `logger.h` com macros que compilam como no-op quando desabilitadas (`-DLOG_DISABLED`). Registra apenas transições de estado e ações (não polled states). Formato: `[timestamp_ms] [NIVEL] [MODULO] mensagem`. Ver `README.md` §11 para detalhes completos.

---

## 12. Fora de Escopo (v1.0)

- ~~Fim de curso na posição inferior (margem do rio).~~ — **implementado** (Remote GPIO 13)
- Display LCD/OLED.
- Controle por aplicativo mobile.
- Registro persistente de logs de operação (logs via Serial para debug estão disponíveis — ver §11).
- Múltiplos remotes simultâneos.

---

## 13. Glossário

| Termo | Definição |
|---|---|
| ESP-NOW | Protocolo de comunicação sem fio da Espressif, direto entre dispositivos, sem roteador |
| Microchave | Microswitch que indica o estado mecânico do freio |
| Fim de Curso | Sensor de posição que detecta a chegada do carrinho ao estacionamento |
| Watchdog | Timer de supervisão que aciona ação de segurança se comunicação é perdida |
| Dead-time | Intervalo obrigatório entre desligar um relé de direção e ligar o oposto |
| Homem-Morto | Regra que exige o botão mantido pressionado para o motor permanecer ativo |
| Fail-Safe | Princípio onde qualquer falha leva o sistema ao estado seguro (freio aplicado) |
| Rearme | Ato manual de desativar o estado de emergência e retornar à operação normal |
| Trava Lógica | Flag de software que bloqueia movimentação independentemente de entradas físicas |
| Botão com Trava | Botão que mantém o sinal ativo após pressionado, até ser manualmente destrancado |
