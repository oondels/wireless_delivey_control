# Design Specification — Controle Remoto para Carrinho de Jet Ski

**Versão:** 3.1  
**Data:** 2026-03-16  
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

### 3.1 Módulo Principal (Painel Central)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Painel fixo no estacionamento/depósito |
| Alimentação | Fonte derivada da rede elétrica 110/220V |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA (c/ trava), REARME; microchave do freio; fim de curso do estacionamento |
| Saídas relés | Relés 5V: direção do motor (2x), velocidade (3x), freio (1x) |
| Saídas LEDs | 1 GPIO por LED: VEL1, VEL2, VEL3, EMERGÊNCIA, LINK REMOTE — cor definida pelo LED físico instalado |
| Comunicação | ESP-NOW — recebe pacotes do Remote, envia status de retorno |

### 3.2 Módulo Remote (Carrinho)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Embarcado no carrinho de transporte |
| Alimentação | Bateria recarregável (ex: Li-Ion 18650 + regulador 3.3V) |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA (c/ trava) |
| Saídas LEDs | 1 GPIO por LED: LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME — cor definida pelo LED físico instalado |
| Comunicação | ESP-NOW — transmite comandos e heartbeat para o Principal |

> **Nota sobre LEDs:** todos os LEDs são componentes discretos de 3V (padrão Arduino) com cor física pré-definida externamente. O firmware controla apenas o estado lógico de cada GPIO (HIGH/LOW e frequência de piscar). Não há controle de cor por software.

---

## 4. Sensores de Hardware

### 4.1 Microchave do Freio

Conectada ao Módulo Principal. Indica o estado mecânico do freio. Sinal HIGH = freio liberado; sinal LOW = freio engatado. Condição obrigatória para acionamento do motor (ver seção 5.3).

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

- O sistema possui **3 níveis de velocidade**, selecionados por botões de **pulso** (não precisam ser mantidos pressionados).
- A velocidade selecionada é armazenada em memória de estado no firmware do Principal.
- A atuação ocorre via relé: cada nível ativa um relé correspondente, que comuta a tensão para um potenciômetro físico externo pré-ajustado. **A definição real da velocidade é hardware; o software apenas seleciona qual relé ativar.**
- Apenas **um** relé de velocidade pode estar ativo por vez. Ao selecionar um novo nível, o relé anterior é desacionado antes de acionar o novo.
- **Sincronização de LEDs:** o estado de velocidade é incluído no `PacoteStatus` enviado pelo Principal ao Remote a cada 200 ms. O Remote atualiza seus LEDs de velocidade com base nesse valor, garantindo que ambos os painéis exibam sempre a mesma indicação.

### 5.2 Acionamento do Motor — Regra "Homem-Morto"

- O motor **só permanece em operação enquanto o botão SUBIR ou DESCER estiver fisicamente mantido pressionado.**
- Ao soltar o botão, o sistema executa imediatamente, nesta sequência:
  1. Corta a alimentação do motor (desaciona os relés de direção).
  2. Aciona o freio mecânico (aciona o relé de freio).
- Esta regra aplica-se tanto ao Painel Central quanto ao Remote.
- O Remote transmite o estado do botão (pressionado / solto) continuamente. O Principal executa a lógica localmente com base no estado recebido.

### 5.3 Trava Lógica e Microchave do Freio

- O Principal monitora continuamente o estado da microchave conectada ao freio mecânico.
- O motor **só pode ser energizado** se a microchave indicar que o freio está **liberado** (sinal HIGH).
- Se a microchave indicar freio engatado (sinal LOW), qualquer comando de movimentação é **sumariamente ignorado**.
- Quando a trava lógica está ativa no software (emergência ou outra condição de bloqueio), os comandos de movimentação do Remote são ignorados independentemente do estado da microchave.

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

Se o Painel Central acionar o REARME enquanto o botão de emergência do Remote ainda estiver travado, o Principal aceita o rearme e limpa `EMERGENCIA_ATIVA`. Para evitar inconsistência visual, o campo `rearme_ativo = 1` é incluído no `PacoteStatus` enviado ao Remote. O Remote acende o **LED de ALARME** ao receber esse sinal com o botão local ainda ativo, alertando o operador de que o sistema foi rearmaado externamente e que o botão local ainda está travado.

```
Emergência Remote ativa (botão travado)
        │
        ├─ Operador Remote solta botão ──► emergencia = 0 no pacote
        │                                  Principal aceita REARME normalmente
        │
        └─ Operador Painel pressiona REARME (botão Remote ainda travado)
                   │
                   ├─ Principal limpa EMERGENCIA_ATIVA
                   ├─ Principal seta rearme_ativo = 1 no PacoteStatus
                   └─ Remote recebe → acende LED ALARME (pisca)
                      Operador deve soltar botão de emergência local
```

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
- Remote envia heartbeat a cada **200 ms** mesmo sem botão pressionado.
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

MAC do Principal fixado em firmware no Remote (ou configurado na inicialização).

### 8.2 Pacote Remote → Principal

```c
typedef struct {
    uint8_t  comando;       // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                            // 3=VEL1, 4=VEL2, 5=VEL3
    uint8_t  botao_hold;    // 1=SUBIR ou DESCER pressionado (Homem-Morto)
    uint8_t  emergencia;    // 1=botão de emergência com trava ativo no Remote
    uint32_t timestamp;     // millis() do Remote
    uint8_t  checksum;      // XOR de todos os bytes anteriores
} PacoteRemote;
```

### 8.3 Pacote Principal → Remote (Status)

```c
typedef struct {
    uint8_t  estado_sistema; // 0=PARADO, 1=SUBINDO, 2=DESCENDO,
                             // 3=EMERGENCIA_ATIVA, 4=FALHA_COMUNICACAO
    uint8_t  estado_freio;   // 0=engatado, 1=liberado
    uint8_t  velocidade;     // 1, 2 ou 3 — sincroniza LEDs de velocidade no Remote
    uint8_t  trava_logica;   // 1=trava ativa
    uint8_t  rearme_ativo;   // 1=Painel fez rearme com botão emergência Remote ainda travado
    uint8_t  checksum;
} PacoteStatus;
```

### 8.4 Frequência de Envio

| Direção | Condição | Frequência |
|---|---|---|
| Remote → Principal | Heartbeat (sem botão) | A cada 200 ms |
| Remote → Principal | Mudança de estado de botão | Imediato + repetir a cada 200 ms enquanto ativo |
| Principal → Remote | Status de retorno | A cada 200 ms ou imediato após mudança de estado |

---

## 9. Indicadores Visuais (LEDs)

Todos os LEDs são componentes discretos de **3V (padrão Arduino)** com cor física definida externamente no momento da montagem. O firmware controla apenas o estado lógico de cada GPIO: ligado, desligado ou piscando em uma frequência específica. Não há controle de cor por software.

Cada LED corresponde a **exatamente 1 GPIO de saída** no ESP32.

### 9.1 LEDs no Módulo Remote

| LED | GPIO | Comportamento | Condição |
|---|---|---|---|
| LINK | 1 | Piscando 1 Hz | Sem comunicação com o Principal (> 1000 ms sem status) |
| LINK | 1 | Ligado fixo | Comunicação ativa |
| MOTOR | 1 | Ligado fixo | `estado_sistema == SUBINDO` ou `DESCENDO` |
| VEL1 | 1 | Ligado fixo | `velocidade == 1` (recebido no PacoteStatus) |
| VEL2 | 1 | Ligado fixo | `velocidade == 2` (recebido no PacoteStatus) |
| VEL3 | 1 | Ligado fixo | `velocidade == 3` (recebido no PacoteStatus) |
| EMERGÊNCIA | 1 | Piscando 4 Hz | `estado_sistema == EMERGENCIA_ATIVA` |
| EMERGÊNCIA | 1 | Ligado fixo | `estado_sistema == FALHA_COMUNICACAO` |
| ALARME | 1 | Piscando 2 Hz | `rearme_ativo == 1` E botão emergência local ainda travado |

**Total: 7 GPIOs de saída** (LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME)

### 9.2 LEDs no Painel Central

| LED | GPIO | Comportamento | Condição |
|---|---|---|---|
| VEL1 | 1 | Ligado fixo | `velocidade_atual == 1` |
| VEL2 | 1 | Ligado fixo | `velocidade_atual == 2` |
| VEL3 | 1 | Ligado fixo | `velocidade_atual == 3` |
| EMERGÊNCIA | 1 | Ligado fixo | `emergencia_ativa == true` |
| LINK REMOTE | 1 | Ligado fixo | Comunicação com Remote ativa (watchdog OK) |

**Total: 5 GPIOs de saída**

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

## 11. Fora de Escopo (v1.0)

- Fim de curso na posição inferior (margem do rio).
- Display LCD/OLED.
- Controle por aplicativo mobile.
- Registro de logs de operação.
- Múltiplos remotes simultâneos.

---

## 12. Glossário

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
| LED ALARME | Indicador no Remote que sinaliza inconsistência entre estado do botão local e estado do sistema |