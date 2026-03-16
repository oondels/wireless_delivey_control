# Design Specification — Controle Remoto para Carrinho de Jet Ski

**Versão:** 2.0  
**Data:** 2026-03-16  
**Status:** Rascunho

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
│  - Botão de emergência (painel)  │                                 │  - Botão de emergência          │
│  - Relés: direção, velocidade,   │                                 │  - LEDs de status               │
│    freio                         │                                 │  - Bateria                      │
│  - Leitura da microchave         │                                 │  - Enclosure IP54               │
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

### 3.1 Módulo Principal (Painel Central)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Painel fixo no estacionamento/depósito |
| Alimentação | Fonte derivada da rede elétrica 110/220V |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA, REARME; microchave do freio |
| Saídas | Relés 5V: direção do motor (2x), velocidade (3x), freio (1x) |
| Comunicação | ESP-NOW — recebe pacotes do Remote, envia status de retorno |

### 3.2 Módulo Remote (Carrinho)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Embarcado no carrinho de transporte |
| Alimentação | Bateria recarregável (ex: Li-Ion 18650 + regulador 3.3V) |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA |
| Saídas | LEDs: LINK, MOTOR ATIVO, EMERGÊNCIA/FALHA |
| Comunicação | ESP-NOW — transmite comandos e heartbeat para o Principal |

---

## 4. Regras de Negócio

### 4.1 Controle de Velocidade

- O sistema possui **3 níveis de velocidade**, selecionados por botões de **pulso** (não precisam ser mantidos pressionados).
- A velocidade selecionada é armazenada em memória de estado no firmware do Principal.
- A atuação ocorre via relé: cada nível ativa um relé correspondente, que comuta a tensão para um potenciômetro físico externo pré-ajustado. **A definição real da velocidade é hardware; o software apenas seleciona qual relé ativar.**
- Ao selecionar uma velocidade, o LED indicativo correspondente no painel é acionado.
- Apenas **um** relé de velocidade pode estar ativo por vez. Ao selecionar um novo nível, o relé anterior é desacionado antes de acionar o novo.

### 4.2 Acionamento do Motor — Regra "Homem-Morto"

- O motor **só permanece em operação enquanto o botão SUBIR ou DESCER estiver fisicamente mantido pressionado.**
- Ao soltar o botão, o sistema executa imediatamente, nesta sequência:
  1. Corta a alimentação do motor (desaciona os relés de direção).
  2. Aciona o freio mecânico (aciona o relé de freio).
- Esta regra aplica-se tanto ao Painel Central quanto ao Remote.
- O Remote transmite o estado do botão (pressionado / solto) continuamente. O Principal executa a lógica localmente com base no estado recebido.

### 4.3 Trava Lógica e Microchave do Freio

- O Principal monitora continuamente o estado da microchave conectada ao freio mecânico.
- O motor **só pode ser energizado** se a microchave indicar que o freio está **liberado** (sinal HIGH).
- Se a microchave indicar freio engatado (sinal LOW), qualquer comando de movimentação é **sumariamente ignorado**.
- Quando a trava lógica está ativa no software (emergência ou outra condição de bloqueio), os comandos de movimentação do Remote são ignorados independentemente do estado da microchave.

---

## 5. Protocolos de Segurança e Emergência (Fail-Safe)

### 5.1 Condições de Acionamento Automático do Freio

O freio é ativado de forma automática e imediata em qualquer das seguintes condições:

| # | Condição | Origem |
|---|---|---|
| 1 | Perda de heartbeat do Remote (watchdog timeout) | Comunicação |
| 2 | Queda de energia ou desligamento do Remote | Hardware |
| 3 | Botão de EMERGÊNCIA acionado no Painel Central | Operador |
| 4 | Botão de EMERGÊNCIA acionado no Remote | Operador |
| 5 | Soltura do botão de acionamento (regra Homem-Morto) | Operador |
| 6 | Microchave indicando freio engatado com motor ativo | Hardware |

### 5.2 Prioridade da Emergência

- Comandos de emergência têm **prioridade máxima** no firmware, sobrepondo qualquer outro comando em execução.
- Ao entrar em estado de emergência, o sistema executa imediatamente: corte do motor → acionamento do freio → flag `EMERGENCIA_ATIVA = true`.
- Com `EMERGENCIA_ATIVA = true`, **todos** os comandos de movimentação do Remote são ignorados.

### 5.3 Desativação de Emergência (Rearme)

- A desativação do estado de emergência é **exclusivamente manual**, realizada por ação deliberada no **Painel Central** (botão REARME dedicado).
- O sistema **jamais** rearma automaticamente sob nenhuma circunstância.
- Se a emergência foi originada no Remote, o operador do Painel Central tem autoridade para desativar esse estado.
- Após o rearme, o sistema retorna ao estado `PARADO` (freio acionado, motor desligado), aguardando novo comando.

### 5.4 Watchdog de Comunicação

- O Principal monitora o intervalo entre pacotes recebidos do Remote.
- **Timeout:** 500 ms (configurável em firmware).
- Se nenhum pacote for recebido dentro do timeout: freio acionado, motor cortado, sistema entra em `FALHA_COMUNICACAO`.
- O Remote envia heartbeat a cada **200 ms**, mesmo sem botão pressionado.
- O estado `FALHA_COMUNICACAO` **não é** resolvido automaticamente. Exige rearme manual pelo Painel Central.

---

## 6. Máquina de Estados do Sistema (Módulo Principal)

```
                    ┌──────────────────────────────────────────┐
                    │           EMERGENCIA_ATIVA               │◄─── Qualquer emergência
                    │  Motor: OFF | Freio: ON                  │◄─── Watchdog timeout
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
          ┌──────────┐  hold SUBIR  ┌───────────┐  hold DESCER  ┌──────────┐
          │  SUBINDO │◄─────────── │  PARADO   │──────────────►│ DESCENDO │
          │ Motor:ON │             │ Motor:OFF │               │ Motor:ON │
          │ Freio:OFF│─────────── ►│ Freio:ON  │◄──────────── -│ Freio:OFF│
          └──────────┘  solto      └───────────┘  solto        └──────────┘

Regras globais (qualquer estado → EMERGENCIA_ATIVA):
  - botão EMERGÊNCIA (Painel ou Remote)
  - watchdog timeout
  - queda de energia do Remote
  - microchave: freio engatado com motor ativo
```

### Tabela de Condições de Acionamento do Motor

| Emergência Ativa | Trava Lógica | Microchave | Botão Hold | Watchdog OK | Resultado |
|---|---|---|---|---|---|
| Não | Não | Liberado | Pressionado | Sim | Motor ON |
| Não | Não | Liberado | Solto | Qualquer | Motor OFF → Freio ON |
| Não | Não | Engatado | Qualquer | Qualquer | Motor BLOQUEADO |
| Não | Sim | Qualquer | Qualquer | Qualquer | Motor BLOQUEADO, Remote ignorado |
| Sim | Qualquer | Qualquer | Qualquer | Qualquer | Motor BLOQUEADO, Remote ignorado |
| Qualquer | Qualquer | Qualquer | Qualquer | Não | FALHA_COMUNICACAO → Freio ON |

---

## 7. Protocolo de Comunicação (ESP-NOW)

### 7.1 Emparelhamento

Os dois módulos são emparelhados pelo endereço MAC. O MAC do Principal é fixado em firmware no Remote (ou configurado na inicialização).

### 7.2 Pacote Remote → Principal

```c
typedef struct {
    uint8_t  comando;        // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                             // 3=VEL1, 4=VEL2, 5=VEL3
    uint8_t  botao_hold;     // 1=pressionado, 0=solto (Homem-Morto)
    uint8_t  emergencia;     // 1=emergência acionada no Remote
    uint32_t timestamp;      // millis() do Remote
    uint8_t  checksum;       // XOR de todos os bytes anteriores
} PacoteRemote;
```

### 7.3 Pacote Principal → Remote (Status)

```c
typedef struct {
    uint8_t  estado_sistema; // 0=PARADO, 1=SUBINDO, 2=DESCENDO,
                             // 3=EMERGENCIA_ATIVA, 4=FALHA_COMUNICACAO
    uint8_t  estado_freio;   // 0=engatado, 1=liberado
    uint8_t  velocidade;     // 1, 2 ou 3 (nível ativo)
    uint8_t  trava_logica;   // 1=trava ativa
    uint8_t  checksum;
} PacoteStatus;
```

### 7.4 Frequência de Envio

| Direção | Condição | Frequência |
|---|---|---|
| Remote → Principal | Heartbeat (sem botão) | A cada 200 ms |
| Remote → Principal | Mudança de estado de botão | Imediato + repetir a cada 200 ms enquanto pressionado |
| Principal → Remote | Status de retorno | A cada 200 ms ou imediato após mudança de estado |

---

## 8. Indicadores Visuais

### 8.1 LEDs no Módulo Remote

| LED | Cor | Padrão | Significado |
|---|---|---|---|
| LINK | Verde | Piscando 1 Hz | Sem comunicação com o Principal |
| LINK | Verde | Fixo | Comunicação ativa |
| MOTOR | Azul | Fixo | Motor em operação |
| EMERGÊNCIA | Vermelho | Piscando 4 Hz | Estado de emergência ativo |
| EMERGÊNCIA | Vermelho | Fixo | Falha de comunicação ou freio engatado |

### 8.2 LEDs no Painel Central

| LED | Cor | Significado |
|---|---|---|
| VEL 1 | Amarelo | Velocidade nível 1 selecionada |
| VEL 2 | Amarelo | Velocidade nível 2 selecionada |
| VEL 3 | Amarelo | Velocidade nível 3 selecionada |
| EMERGÊNCIA | Vermelho | Estado de emergência ativo |
| LINK REMOTE | Verde | Comunicação com o Remote ativa |

---

## 9. Requisitos Não-Funcionais

- **Latência:** Tempo entre acionamento do botão e resposta do motor < **100 ms**.
- **Watchdog:** Timeout padrão **500 ms**, configurável via constante em firmware.
- **Alcance:** Operação confiável em linha de visada de pelo menos **50 metros**.
- **Robustez:** Enclosure do Remote mínimo IP54 (respingos, umidade de ambiente de rio).
- **Segurança elétrica:** Relés dimensionados para corrente de partida do motor com fator de segurança 2x. Isolação galvânica obrigatória entre rede elétrica e GPIOs do ESP32.
- **Anti-colisão de direção:** Dead-time mínimo de **100 ms** ao inverter sentido do motor.
- **Rearme:** O sistema jamais rearma emergência automaticamente.

---

## 10. Fora de Escopo (v1.0)

- Sensores de fim de curso (limites de posição do carrinho).
- Display LCD/OLED.
- Controle por aplicativo mobile.
- Registro de logs de operação.
- Múltiplos remotes simultâneos.

---

## 11. Glossário

| Termo | Definição |
|---|---|
| ESP-NOW | Protocolo de comunicação sem fio da Espressif, direto entre dispositivos, sem roteador |
| Microchave | Microswitch que indica o estado mecânico do freio |
| Watchdog | Timer de supervisão que aciona ação de segurança se comunicação é perdida |
| Dead-time | Intervalo obrigatório entre desligar um relé de direção e ligar o oposto |
| Homem-Morto | Regra que exige o botão mantido pressionado para o motor permanecer ativo |
| Fail-Safe | Princípio onde qualquer falha leva o sistema ao estado seguro (freio aplicado) |
| Rearme | Ato manual de desativar o estado de emergência e retornar à operação normal |
| Trava Lógica | Flag de software que bloqueia movimentação independentemente de entradas físicas |