# Especificação de Segurança e Emergência (Fail-Safe)

**Versão:** 1.4
**Data:** 2026-03-22
**Referência:** DESIGN_SPEC.md v3.3, README.md v3.4

---

## 1. Princípio Fundamental

O sistema opera sob o princípio **Fail-Safe**: qualquer falha de comunicação, queda de energia, anomalia de hardware ou acionamento de emergência resulta no **freio mecânico sendo aplicado imediatamente** e no **motor sendo desligado**.

O sistema **nunca** assume um estado seguro por omissão — ele **força** ativamente o estado seguro acionando o relé de freio.

---

## 2. Hierarquia de Prioridade de Segurança

As condições de segurança são avaliadas em ordem estrita de prioridade no loop principal do firmware. Uma condição de prioridade superior **sempre** sobrepõe qualquer condição inferior.

| Prioridade | Condição | Estado Resultante |
|---|---|---|
| 1 (máxima) | Emergência ativa (Painel ou Remote) | `EMERGENCIA_ATIVA` |
| 2 | Queda de energia da rede elétrica (GPIO 13 LOW) | `FALHA_ENERGIA` |
| 3 | Falha de comunicação (watchdog timeout) | `FALHA_COMUNICACAO` |
| 4 | Fim de curso acionado | `PARADO` |
| 5 | Microchave do freio engatada | Motor bloqueado |
| 6 | Botão de acionamento solto (Homem-Morto) | `PARADO` |

---

## 3. Sistema de Emergência

### 3.1 Botões de Emergência

Ambos os módulos (Painel Central e Remote) possuem botões de emergência do tipo **NC (normalmente fechado) com trava mecânica**:

- Em repouso: contato fechado → pino drenado para GND → lê **LOW** (sem emergência).
- Ao pressionar: contato abre → pull-up puxa para **HIGH** → lê **HIGH** (emergência ativa).
- Com trava acionada, o pino permanece HIGH continuamente até o botão ser **manualmente destrancado**.
- O firmware lê o estado do pino como **nível contínuo** (não como borda).
- **Fail-safe por projeto:** cabo partido ou desconexão → pino flutua HIGH → emergência ativada automaticamente.

### 3.2 Acionamento de Emergência

| Origem | Ação no Firmware |
|---|---|
| Botão EMERGÊNCIA no Painel Central | `emergencia.ativa() = true` imediatamente; motor OFF; freio ON |
| Botão EMERGÊNCIA no Remote (via pacote) | Campo `emergencia == 1` no `PacoteRemote`; Principal processa e ativa `emergencia.ativa() = true` |

**Ao entrar em emergência, a sequência é:**
1. Cortar alimentação do motor (`motor.desligar()`)
2. Acionar freio mecânico (`freio.acionar()`)
3. Setar flag `emergencia.ativa() = true`

### 3.3 Comportamento Durante Emergência

- **Todos** os comandos de movimentação do Remote são ignorados.
- **Todos** os comandos de movimentação do Painel Central são ignorados.
- O sistema permanece neste estado até que **todas as fontes de emergência sejam inativas** (auto-liberação) ou até rearme manual pelo REARME.

### 3.4 Liberação de Emergência

**Auto-liberação (caso normal):**

A emergência é limpa automaticamente quando **todas** as fontes ficam inativas simultaneamente:
- Botão local do Painel Central está **fisicamente solto** (destravado); **E**
- Remote **não** está sinalizando emergência (`emergencia == 0` no pacote).

Não é necessário pressionar REARME neste caso.

**Após auto-liberação:**
- `emergencia.ativa() = false`
- Estado retorna a `PARADO` no próximo ciclo
- Freio permanece acionado; motor permanece desligado
- O operador deve iniciar nova movimentação manualmente

**Rearme manual (caso especial — remote travado):**

Usado quando o botão de emergência do Remote está **travado mecanicamente** (ou o Remote está inacessível) e o operador precisa retomar a operação pelo Painel Central:

1. O botão de emergência **local** (Painel Central) deve estar **fisicamente solto**; **e**
2. O operador pressiona **REARME** no Painel Central.

### 3.5 Rearme com Botão Remote Ainda Travado (Caso Especial)

Se o operador do Painel Central pressionar REARME enquanto o botão de emergência do Remote ainda estiver travado, a liberação continua dependendo da lógica do CLP e do estado físico do botão. O firmware ESP32 atual **não** envia um campo dedicado de `rearme_ativo` ao Remote.

---

## 4. Watchdog de Comunicação

### 4.1 Parâmetros

| Parâmetro | Valor | Configurável |
|---|---|---|
| Timeout | 500 ms | Sim (constante `WATCHDOG_TIMEOUT_MS` em firmware) |
| Frequência de heartbeat do Remote | 100 ms | Sim (constante `HEARTBEAT_INTERVALO_MS` em firmware) |

### 4.2 Funcionamento

- O Remote envia heartbeat a cada **100 ms**, mesmo sem nenhum botão pressionado.
- O Principal mantém um timestamp do último pacote recebido (internamente na classe `WatchdogComm`).
- A cada ciclo do loop principal, verifica: `watchdog.expirado()` (equivale a `millis() - _ultimoPacoteMs > WATCHDOG_TIMEOUT_MS`).
- Se o timeout for excedido: motor OFF → freio ON → estado `FALHA_COMUNICACAO`.

### 4.3 Causas de Timeout

- Remote desligado ou sem bateria.
- Remote fora do alcance de comunicação.
- Interferência RF prolongada.
- Falha de hardware no Remote.

### 4.4 Recuperação de Falha de Comunicação

- `FALHA_COMUNICACAO` **exige rearme manual** pelo Painel Central (via `rearme.verificar()`).
- A reconexão do Remote **não** limpa o estado automaticamente.
- Após rearme: sistema retorna a `PARADO` e habilita **modo degradado local**.

### 4.5 Modo Degradado Local (após REARME)

Quando o operador executa REARME em `FALHA_COMUNICACAO`:

1. O fail-safe imediato da perda de link continua válido (motor OFF + freio ON no evento de timeout).
2. O Painel Central passa a poder comandar `SUBIR`/`DESCER` localmente, mesmo com watchdog expirado.
3. Comandos do Remote permanecem bloqueados enquanto o watchdog estiver expirado.
4. Quando a comunicação retorna (watchdog recuperado), o modo degradado local é desativado automaticamente.

---

## 5. Regra Homem-Morto (Dead-Man Switch)

### 5.1 Princípio

O motor **só permanece em operação enquanto o botão SUBIR ou DESCER estiver fisicamente mantido pressionado**. Esta é uma medida de segurança fundamental.

### 5.2 Comportamento ao Soltar o Botão

Sequência executada imediatamente:
1. Cortar alimentação do motor (desacionar relés de direção).
2. Acionar freio mecânico (acionar relé de freio).
3. Estado retorna a `PARADO`.

### 5.3 Aplicação

- Aplica-se a **ambos** os módulos (Painel Central e Remote).
- O Remote transmite o estado do botão (`botao_hold`) continuamente.
- O Principal executa a lógica localmente com base no estado recebido.

---

## 6. Proteções de Hardware

### 6.1 Micro do Freio (GPIO 14)

- Conectada ao **GPIO 14** do ESP32 Principal como contato **NC** com `INPUT_PULLUP`.
- Em condição normal: contato fechado → GPIO LOW.
- Ao abrir, acionar ou romper cabo: GPIO HIGH.
- O firmware propaga esse estado ao Remote via `PacoteStatus.micro_freio_ativa`.
- Fail-safe: cabo partido → HIGH → `micro_freio_ativa = 1`.

### 6.2 Fim de Curso do Estacionamento

- Sensor instalado na posição final de subida.
- Ao acionar: motor OFF → freio ON → estado `PARADO`.
- **Não** é um estado de emergência — não requer rearme.
- Debounce mínimo de **20 ms** no firmware.
- **Bloqueio pós-acionamento:** após o sensor ser confirmado, o bloqueio de movimento permanece ativo por **10 s** mesmo que o sensor físico tenha sido liberado. Medida de segurança para evitar reacionamento imediato acidental.

### 6.3 Anti-Colisão de Direção

- Dead-time mínimo de **100 ms** ao inverter o sentido do motor.
- Impede que dois relés de direção estejam ativos simultaneamente.
- Protege o motor contra curto-circuito por inversão instantânea.

---

## 7. Isolação Elétrica

- Isolação galvânica **obrigatória** entre rede elétrica (110/220V) e GPIOs do ESP32.
- Relés dimensionados para corrente de partida do motor com fator de segurança **2x**.
- Enclosure do Remote com proteção mínima **IP54** (respingos e umidade).

---

## 8. Tabela Resumo — Condições de Acionamento do Freio

| # | Condição | Origem | Estado Resultante | Requer Rearme |
|---|---|---|---|---|
| 1 | Perda de heartbeat (watchdog timeout) | Comunicação | `FALHA_COMUNICACAO` | Sim (para liberar controle local em modo degradado) |
| 2 | Queda de energia / desligamento do Remote | Hardware | `FALHA_COMUNICACAO` | Sim (para liberar controle local em modo degradado) |
| 3 | Queda de energia da rede elétrica (GPIO 13 LOW) | Hardware | `FALHA_ENERGIA` | Sim |
| 4 | Botão EMERGÊNCIA no Painel Central | Operador | `EMERGENCIA_ATIVA` | Não — auto-libera ao soltar |
| 5 | Botão EMERGÊNCIA no Remote | Operador | `EMERGENCIA_ATIVA` | Não — auto-libera ao soltar; REARME apenas se remote travado |
| 6 | Soltura do botão de acionamento (Homem-Morto) | Operador | `PARADO` | Não |
| 7 | Micro do freio NC aberta/acionada | Hardware | Telemetria de anomalia no status | Não |
| 8 | Fim de curso do estacionamento | Hardware | `PARADO` | Não |

> Na arquitetura atual, o Principal não controla diretamente o freio. O estado do freio é tratado pelo CLP; o ESP32 apenas envia comandos, recebe feedbacks do CLP e telemetria da micro do freio.

---

## 9. Invariantes de Segurança

Estas condições devem ser **sempre verdadeiras** no firmware, independentemente do estado:

1. `emergencia.isAtiva() == true` → motor **sempre** OFF e freio **sempre** ON.
2. Ao detectar `FALHA_COMUNICACAO` (timeout), motor **sempre** OFF e freio **sempre** ON imediatamente.
3. Ao detectar `FALHA_ENERGIA` (GPIO 13 LOW com debounce), motor **sempre** OFF e freio **sempre** ON imediatamente. Requer rearme manual.
4. `emergencia.ativa()` é limpa automaticamente quando **nenhuma** fonte está ativa (botão local solto + remote com `emergencia==0`). REARME manual é necessário apenas quando o remote mantém `emergencia==1` e não é possível acessá-lo.
5. O rearme **nunca** liga o motor — apenas retorna a `PARADO`.
6. Dois relés de direção **nunca** estão ativos simultaneamente.
7. O motor **nunca** é ligado sem botão hold ativo (Homem-Morto).
