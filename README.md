# Design Specification — Controle Remoto para Carrinho de Jet Ski

**Versão:** 3.3
**Data:** 2026-03-17
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
│  - Fim de curso estacionamento   │                                 │  - Enclosure IP54               │
│  - LEDs compartilhados c/ relés  │                                 │                                 │
│  - Alimentação rede elétrica     │                                 │                                 │
└──────────────────────────────────┘                                 └─────────────────────────────────┘
                │
                │ Circuito elétrico direto (sem ESP32)
                ▼
    ┌───────────────────────┐
    │  MICROCHAVE DO FREIO  │
    │  Acionamento hardware │
    │  direto no circuito   │
    │  do freio mecânico    │
    └───────────────────────┘
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
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA (c/ trava), REARME; fim de curso do estacionamento |
| Saídas relés/LEDs | 1 GPIO por canal — aciona relé **e** LED simultaneamente via ligação física: DIREÇÃO A, DIREÇÃO B, VEL1, VEL2, VEL3, FREIO |
| LEDs exclusivos | LINK REMOTE (sem relé associado — GPIO dedicado) |
| Comunicação | ESP-NOW — recebe pacotes do Remote, envia status de retorno |

### 3.2 Módulo Remote (Carrinho)

| Item | Descrição |
|---|---|
| Microcontrolador | ESP32 |
| Localização | Embarcado no carrinho de transporte |
| Alimentação | Bateria recarregável (ex: Li-Ion 18650 + regulador 3.3V) |
| Entradas | Botões: SUBIR (hold), DESCER (hold), VEL1, VEL2, VEL3, EMERGÊNCIA (c/ trava) |
| Saídas LEDs | GPIOs dedicados: LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME |
| Comunicação | ESP-NOW — transmite comandos e heartbeat para o Principal |

> O Remote não possui relés. Todos os seus LEDs são GPIOs dedicados.

---

## 4. Arquitetura de Hardware — GPIOs e Ligações Físicas

### 4.1 Compartilhamento GPIO: Relé + LED (Módulo Principal)

No Módulo Principal, cada saída de relé possui um LED indicativo conectado **em paralelo** na mesma saída GPIO. Quando o ESP32 aciona o GPIO (HIGH), ele energiza simultaneamente o relé e o LED. Quando desaciona (LOW), ambos apagam.

```
GPIO ESP32
    │
    ├──► Bobina do relé (via transistor/driver)
    │
    └──► LED 3V (com resistor em série 220Ω)
```

> **Atenção de hardware:** a corrente total do GPIO deve suportar o LED (tipicamente ~10 mA) mais a corrente de controle do driver do relé. Recomenda-se usar um transistor ou driver de relé (ex: ULN2003) para não sobrecarregar o GPIO diretamente. O LED pode ser ligado entre o coletor do transistor e VCC, ou diretamente no GPIO se a corrente total for segura.

**Canais com GPIO compartilhado (relé + LED) no Principal:**

| Canal | Relé aciona | LED indica |
|---|---|---|
| DIREÇÃO A | Motor sentido SUBIDA | Motor ativo — sentido subida |
| DIREÇÃO B | Motor sentido DESCIDA | Motor ativo — sentido descida |
| VEL1 | Potenciômetro velocidade 1 | Velocidade 1 selecionada |
| VEL2 | Potenciômetro velocidade 2 | Velocidade 2 selecionada |
| VEL3 | Potenciômetro velocidade 3 | Velocidade 3 selecionada |
| FREIO | Relé de freio | Freio acionado pelo sistema |

**Canal com GPIO exclusivo (sem relé) no Principal:**

| Canal | Função |
|---|---|
| LINK REMOTE | LED indicando comunicação ativa com o Remote |

### 4.2 Microchave do Freio — Acionamento Direto por Hardware

A microchave do freio **não está conectada ao ESP32**. Ela atua diretamente no circuito elétrico do freio mecânico, interrompendo ou permitindo a alimentação do freio de forma independente do firmware.

**Implicação para o firmware:** o ESP32 Principal não lê o estado do freio. A trava física é garantida pela microchave em hardware. O firmware controla o relé de freio (acionar/liberar), mas a microchave pode sobrepor esse estado diretamente no circuito elétrico, funcionando como uma camada de segurança adicional independente do software.

Esta abordagem é mais segura pois garante o acionamento do freio mesmo em caso de falha total do firmware.

### 4.3 Fim de Curso do Estacionamento

Sensor instalado no estacionamento que é acionado quando o carrinho chega à posição final de subida. Este sensor **está conectado ao ESP32 Principal** como entrada digital.

**Comportamento ao acionar:**
- O Principal detecta o sinal do fim de curso (com debounce de 20 ms).
- O motor é cortado imediatamente (desaciona relés de direção).
- O relé de freio é acionado.
- O sistema entra no estado `PARADO` — não é emergência, não requer rearme.
- O operador pode retomar a operação normalmente.

---

## 5. Pinout Resumido

### 5.1 Módulo Principal

| Função | Tipo | GPIO | Observação |
|---|---|---|---|
| Botão SUBIR | Entrada | 36 (VP) | Input-only, pull-up externo obrigatório |
| Botão DESCER | Entrada | 39 (VN) | Input-only, pull-up externo obrigatório |
| Botão VEL1 | Entrada | 34 | Input-only, pull-up externo obrigatório |
| Botão VEL2 | Entrada | 35 | Input-only, pull-up externo obrigatório |
| Botão VEL3 | Entrada | 32 | Pull-up interno (INPUT_PULLUP) |
| Botão EMERGÊNCIA (trava) | Entrada | 33 | Pull-up interno (INPUT_PULLUP) — não usa strapping pin |
| Botão REARME | Entrada | 25 | Pull-up interno (INPUT_PULLUP) |
| Fim de curso | Entrada | 26 | Pull-up interno (INPUT_PULLUP) — não usa strapping pin |
| Relé + LED DIREÇÃO A | Saída | 4 | HIGH = motor sentido SUBIR |
| Relé + LED DIREÇÃO B | Saída | 16 | HIGH = motor sentido DESCER |
| Relé + LED VEL1 | Saída | 17 | HIGH = velocidade 1 |
| Relé + LED VEL2 | Saída | 5 | HIGH = velocidade 2 |
| Relé + LED VEL3 | Saída | 18 | HIGH = velocidade 3 |
| Relé + LED FREIO | Saída | 19 | HIGH = freio aplicado |
| LED LINK REMOTE | Saída | 21 | Comunicação ativa com Remote |
| **Total** | | **15** | **8 entradas + 7 saídas** |

> GPIOs confirmados fisicamente na placa ESP32 WROOM-32U utilizada.
> GPIOs 34, 35, 36 e 39 requerem pull-up externo obrigatório (10kΩ para 3.3V) — não suportam INPUT_PULLUP.
> GPIOs 32, 33, 25 e 26 usam pull-up interno ativado via INPUT_PULLUP no firmware — sem resistor externo necessário.
> GPIOs 0, 2, 12 e 15 evitados (strapping pins de boot).
> Pinos de flash SPI interna (D0, D1, D2, D3, CLK, CMD) não utilizados.

### 5.2 Módulo Remote

| Função | Tipo | GPIO | Observação |
|---|---|---|---|
| Botão SUBIR | Entrada | 36 | Input-only, pull-up externo obrigatório |
| Botão DESCER | Entrada | 39 | Input-only, pull-up externo obrigatório |
| Botão VEL1 | Entrada | 34 | Input-only, pull-up externo obrigatório |
| Botão VEL2 | Entrada | 35 | Input-only, pull-up externo obrigatório |
| Botão VEL3 | Entrada | 32 | Pull-up interno (INPUT_PULLUP) |
| Botão EMERGÊNCIA (trava) | Entrada | 33 | Pull-up interno (INPUT_PULLUP) — não usa strapping pin |
| LED LINK | Saída | 4 | Comunicação com Principal |
| LED MOTOR | Saída | 16 | Motor em operação |
| LED VEL1 | Saída | 17 | Velocidade 1 ativa |
| LED VEL2 | Saída | 5 | Velocidade 2 ativa |
| LED VEL3 | Saída | 18 | Velocidade 3 ativa |
| LED EMERGÊNCIA | Saída | 19 | Emergência ou falha de comunicação |
| LED ALARME | Saída | 21 | Rearme com botão local ainda travado |
| **Total** | | **13** | **6 entradas + 7 saídas** |

> GPIOs de ambos os módulos definidos. Mapeamento de entradas consistente entre Principal e Remote (mesmos GPIOs para botões com funções idênticas).
> GPIOs 34, 35, 36 e 39 requerem pull-up externo obrigatório (10kΩ para 3.3V) — não suportam INPUT_PULLUP.
> GPIOs 32 e 33 usam pull-up interno ativado via INPUT_PULLUP no firmware — sem resistor externo necessário.
> Restrições de boot do ESP32 respeitadas: GPIOs 0, 2, 12 e 15 evitados para entradas críticas.

---

## 6. Regras de Negócio

### 6.1 Controle de Velocidade

- O sistema possui **3 níveis de velocidade**, selecionados por botões de **pulso**.
- A velocidade selecionada é armazenada em memória de estado no firmware do Principal.
- Cada nível aciona um relé correspondente (e seu LED em paralelo), que comuta a tensão para um potenciômetro físico externo pré-ajustado.
- Apenas **um** relé de velocidade pode estar ativo por vez. Ao selecionar um novo nível, o relé anterior é desacionado antes de acionar o novo.
- **Sincronização de LEDs:** o campo `velocidade` do `PacoteStatus` permite ao Remote atualizar seus próprios LEDs de velocidade, espelhando o estado do Painel.

### 6.2 Acionamento do Motor — Regra "Homem-Morto"

- O motor **só permanece em operação enquanto SUBIR ou DESCER estiver mantido pressionado.**
- Ao soltar: corte do motor (relés de direção) → acionamento do freio (relé de freio).
- Aplica-se ao Painel Central e ao Remote.
- O Remote transmite o estado do botão continuamente; o Principal executa a lógica.

### 6.3 Trava Lógica de Software

- Quando a trava lógica está ativa no firmware (emergência ou falha de comunicação), os comandos de movimentação do Remote são ignorados.
- A microchave do freio atua em paralelo como camada de segurança de hardware, independente da trava lógica.

---

## 7. Protocolos de Segurança e Emergência (Fail-Safe)

### 7.1 Condições de Acionamento do Freio

| # | Condição | Camada | Estado resultante |
|---|---|---|---|
| 1 | Microchave do freio (circuito direto) | **Hardware** | Freio acionado independente do firmware |
| 2 | Perda de heartbeat do Remote (watchdog) | Firmware | `FALHA_COMUNICACAO` |
| 3 | Queda de energia / desligamento do Remote | Firmware | `FALHA_COMUNICACAO` |
| 4 | Botão EMERGÊNCIA no Painel Central | Firmware | `EMERGENCIA_ATIVA` |
| 5 | Botão EMERGÊNCIA no Remote | Firmware | `EMERGENCIA_ATIVA` |
| 6 | Soltura do botão de acionamento (Homem-Morto) | Firmware | `PARADO` |
| 7 | Fim de curso do estacionamento | Firmware | `PARADO` |

### 7.2 Botão de Emergência com Trava Mecânica

Botões de emergência são do tipo **com trava**: sinal permanece ativo até destravar manualmente. O firmware lê o nível contínuo do pino.

**Saída de `EMERGENCIA_ATIVA` requer:**
1. Botão de emergência fisicamente solto (sinal inativo); **e**
2. Operador pressionar REARME no Painel Central.

**Rearme com botão Remote ainda travado:** o Principal aceita o rearme, limpa `EMERGENCIA_ATIVA` e seta `rearme_ativo = 1` no `PacoteStatus`. O Remote acende o LED ALARME ao receber esse flag com botão local ainda ativo.

```
Botão Remote travado + Painel pressiona REARME:
  → Principal: EMERGENCIA_ATIVA = false, rearme_ativo = 1
  → Remote recebe status: LED ALARME pisca (2 Hz)
  → Operador solta botão no Remote: LED ALARME apaga
```

### 7.3 Prioridade da Emergência

Prioridade máxima no firmware. Ao entrar: corte do motor → acionamento do relé de freio → `EMERGENCIA_ATIVA = true`. Com flag ativa, todos os comandos de movimentação do Remote são ignorados.

### 7.4 Desativação de Emergência (Rearme)

Exclusivamente manual via botão REARME no Painel Central. O sistema **jamais** rearma automaticamente. Após rearme: estado `PARADO`.

### 7.5 Watchdog de Comunicação

- Timeout: **500 ms** (configurável).
- Sem pacote no timeout: freio acionado, motor cortado, `FALHA_COMUNICACAO`.
- Remote envia heartbeat a cada **200 ms**.
- `FALHA_COMUNICACAO` exige rearme manual.

---

## 8. Máquina de Estados do Sistema (Módulo Principal)

```
                    ┌──────────────────────────────────────────┐
                    │           EMERGENCIA_ATIVA               │◄─── Emergência (Painel ou Remote)
                    │  Relés direção: OFF                      │
                    │  Relé freio: ON                          │
                    │  Remote: ignorado                        │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
                    ┌──────────────────────────────────────────┐
                    │           FALHA_COMUNICACAO              │◄─── Watchdog timeout
                    │  Relés direção: OFF                      │
                    │  Relé freio: ON                          │
                    └────────────────┬─────────────────────────┘
                                     │ Rearme MANUAL (Painel Central)
                                     ▼
          ┌──────────┐  hold SUBIR  ┌───────────┐  hold DESCER  ┌──────────┐
          │  SUBINDO │◄─────────── │  PARADO   │──────────────►│ DESCENDO │
          │ Dir A:ON │             │ Dir:OFF   │               │ Dir B:ON │
          │ Freio:OFF│────────────►│ Freio:ON  │◄──────────────│ Freio:OFF│
          └──────────┘  solto      └─────▲─────┘  solto        └──────────┘
                                         │
                              Fim de curso → PARADO (sem rearme)

* Microchave do freio: camada de hardware paralela, independente dos estados acima.
```

### Tabela de Condições de Acionamento do Motor (camada firmware)

| Emergência Ativa | Falha Comun. | Fim de Curso | Botão Hold | Resultado |
|---|---|---|---|---|
| Não | Não | Não acionado | Pressionado | Motor ON |
| Não | Não | Não acionado | Solto | Motor OFF → Freio ON → PARADO |
| Não | Não | Acionado | Qualquer | Motor OFF → Freio ON → PARADO |
| Não | Sim | Qualquer | Qualquer | FALHA_COMUNICACAO → Freio ON |
| Sim | Qualquer | Qualquer | Qualquer | EMERGENCIA_ATIVA → Freio ON |

> A microchave do freio não aparece nesta tabela por atuar em nível de hardware, fora do controle do firmware.

---

## 9. Protocolo de Comunicação (ESP-NOW)

### 9.1 Emparelhamento

MAC do Principal fixado em firmware no Remote.

### 9.2 Pacote Remote → Principal

```c
typedef struct {
    uint8_t  comando;       // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                            // 3=VEL1, 4=VEL2, 5=VEL3
    uint8_t  botao_hold;    // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;    // 1=botão com trava ativo no Remote
    uint32_t timestamp;     // millis() do Remote
    uint8_t  checksum;      // XOR de todos os bytes anteriores
} PacoteRemote;
```

### 9.3 Pacote Principal → Remote (Status)

```c
typedef struct {
    uint8_t  estado_sistema; // 0=PARADO, 1=SUBINDO, 2=DESCENDO,
                             // 3=EMERGENCIA_ATIVA, 4=FALHA_COMUNICACAO
    uint8_t  velocidade;     // 1, 2 ou 3 — sincroniza LEDs de velocidade no Remote
    uint8_t  trava_logica;   // 1=trava ativa
    uint8_t  rearme_ativo;   // 1=Painel fez rearme com botão Remote ainda travado
    uint8_t  checksum;
} PacoteStatus;
```

> O campo `estado_freio` foi removido do `PacoteStatus` em relação à versão anterior, pois o firmware do Principal não lê mais a microchave.

### 9.4 Frequência de Envio

| Direção | Condição | Frequência |
|---|---|---|
| Remote → Principal | Heartbeat | A cada 200 ms |
| Remote → Principal | Mudança de estado | Imediato + repetir a cada 200 ms |
| Principal → Remote | Status | A cada 200 ms ou imediato em mudança de estado |

---

## 10. Indicadores Visuais (LEDs)

Todos os LEDs são componentes discretos de **3V (padrão Arduino)**, cor definida fisicamente. O firmware controla apenas o estado lógico do GPIO.

### 10.1 LEDs no Módulo Principal (compartilhados com relés)

| LED | GPIO compartilhado com | Aceso quando |
|---|---|---|
| DIREÇÃO A | Relé DIREÇÃO A | Motor ativo no sentido subida |
| DIREÇÃO B | Relé DIREÇÃO B | Motor ativo no sentido descida |
| VEL1 | Relé VEL1 | Velocidade 1 selecionada |
| VEL2 | Relé VEL2 | Velocidade 2 selecionada |
| VEL3 | Relé VEL3 | Velocidade 3 selecionada |
| FREIO | Relé FREIO | Relé de freio acionado pelo firmware |
| LINK REMOTE | — (GPIO exclusivo) | Comunicação ativa com o Remote |

### 10.2 LEDs no Módulo Remote (GPIOs dedicados)

| LED | Comportamento | Condição |
|---|---|---|
| LINK | Piscando 1 Hz | Sem status recebido há > 1000 ms |
| LINK | Ligado fixo | Comunicação ativa |
| MOTOR | Ligado fixo | `estado_sistema == SUBINDO` ou `DESCENDO` |
| VEL1 | Ligado fixo | `velocidade == 1` |
| VEL2 | Ligado fixo | `velocidade == 2` |
| VEL3 | Ligado fixo | `velocidade == 3` |
| EMERGÊNCIA | Piscando 4 Hz | `estado_sistema == EMERGENCIA_ATIVA` |
| EMERGÊNCIA | Ligado fixo | `estado_sistema == FALHA_COMUNICACAO` |
| ALARME | Piscando 2 Hz | `rearme_ativo == 1` e botão emergência local travado |

---

## 11. Sistema de Logging (Debug/Testes)

O firmware inclui um sistema de logging via **Serial** (115200 baud) para facilitar testes e depuração antes do deploy em produção. O sistema registra transições de estado e ações relevantes, **não** estados repetidos a cada ciclo do loop.

### 11.1 Formato das Mensagens

```
[timestamp_ms] [NIVEL] [MODULO] mensagem
```

Níveis: `INFO` (operação normal), `WARN` (alerta/bloqueio), `ERRO` (falha).

### 11.2 Módulos Monitorados — Principal

| Tag | Eventos logados |
|---|---|
| `BOTAO` | Pressionar/soltar SUBIR, DESCER; pulsos VEL1/2/3 |
| `EMERG` | Emergência ativada (botão local ou sinal Remote) |
| `FREIO` | Acionamento e liberação do freio (transição) |
| `MOTOR` | Ativação, desligamento, inversão com dead-time |
| `VELOC` | Alteração de velocidade (nível 1/2/3) |
| `REARM` | Rearme executado, bloqueado, ou com Remote travado |
| `SENSOR` | Fim de curso acionado/liberado |
| `MAQEST` | Bloqueio de movimentação (emergência/falha comunicação) |
| `ESTADO` | Transição de estado (ex: PARADO → SUBINDO) |
| `WDOG` | Watchdog expirado/recuperado |
| `REMOTO` | Comando de velocidade recebido do Remote |

### 11.3 Módulos Monitorados — Remote

| Tag | Eventos logados |
|---|---|
| `BOTAO` | Pressionar/soltar SUBIR, DESCER, VEL1/2/3, EMERGÊNCIA |
| `STATUS` | Estado recebido do Principal (transições) |
| `LINK` | Comunicação perdida/restabelecida |

### 11.4 Exemplo de Saída Serial

```
[1523] [INFO] [BOTAO] Botao SUBIR pressionado (hold)
[1523] [INFO] [FREIO] Liberando freio
[1523] [INFO] [MOTOR] Motor ativado — direcao SUBIR
[1523] [INFO] [ESTADO] Transicao: PARADO -> SUBINDO
[3891] [INFO] [BOTAO] Botao SUBIR solto
[3891] [INFO] [MOTOR] Motor desligado
[3891] [INFO] [FREIO] Acionando freio
[3891] [INFO] [ESTADO] Transicao: SUBINDO -> PARADO
[5200] [WARN] [EMERG] Emergencia ATIVADA — botao local pressionado
[5200] [WARN] [MAQEST] Movimentacao BLOQUEADA — emergencia ativa
[5200] [INFO] [ESTADO] Transicao: PARADO -> EMERGENCIA
```

### 11.5 Desabilitar em Produção

Adicionar no `platformio.ini` do módulo desejado:

```ini
build_flags = -DLOG_DISABLED
```

Com `LOG_DISABLED`, todas as macros de logging compilam como no-op (zero overhead em Flash e RAM).

### 11.6 Arquivo Compartilhado

O módulo de logging é implementado em `logger.h` (header-only), idêntico em `principal/include/` e `remote/include/`. Inclui macros de logging e funções auxiliares `estadoParaString()` e `comandoParaString()` para saída legível.

---

## 12. Requisitos Não-Funcionais

- **Latência:** < 100 ms entre botão e resposta do motor.
- **Watchdog:** Timeout padrão 500 ms, configurável.
- **Alcance:** Mínimo 50 metros em linha de visada.
- **Robustez:** Enclosure Remote mínimo IP54.
- **Segurança elétrica:** Relés com fator de segurança 2x sobre corrente de partida do motor. Isolação galvânica entre rede elétrica e GPIOs. Usar driver (transistor/ULN2003) entre GPIO e bobina do relé.
- **Anti-colisão de direção:** Dead-time mínimo de 100 ms ao inverter sentido.
- **Rearme:** Jamais automático.
- **Fim de curso:** Debounce mínimo 20 ms.

---

## 13. Fora de Escopo (v1.0)

- Fim de curso na posição inferior (margem do rio).
- Display LCD/OLED.
- Controle por aplicativo mobile.
- Registro persistente de logs de operação (logs via Serial para debug estão disponíveis — ver §11).
- Múltiplos remotes simultâneos.

---

## 14. Glossário

| Termo | Definição |
|---|---|
| ESP-NOW | Protocolo de comunicação sem fio da Espressif, direto entre dispositivos, sem roteador |
| Microchave do Freio | Chave mecânica que atua diretamente no circuito do freio, sem passar pelo ESP32 |
| Fim de Curso | Sensor de posição conectado ao ESP32 que detecta a chegada do carrinho ao estacionamento |
| Watchdog | Timer de supervisão que aciona segurança se comunicação é perdida |
| Dead-time | Intervalo obrigatório entre desligar um relé de direção e ligar o oposto |
| Homem-Morto | Regra que exige botão mantido pressionado para o motor permanecer ativo |
| Fail-Safe | Qualquer falha leva ao estado seguro (freio aplicado) |
| Rearme | Ato manual de desativar emergência e retornar à operação |
| Trava Lógica | Flag de software que bloqueia movimentação independente de entradas físicas |
| Botão com Trava | Botão que mantém sinal ativo após pressionado, até ser destrancado manualmente |
| LED ALARME | Indicador no Remote: Painel fez rearme mas botão local ainda travado |
| GPIO Compartilhado | GPIO que aciona relé e LED simultaneamente via ligação física em paralelo |