# Especificação de Hardware e I/O

**Versão:** 1.6
**Data:** 2026-04-24
**Referência:** README.md v4.0

---

## 1. Visão Geral

O sistema utiliza dois ESP32 WROOM-32U.

- O **Remote** lê botões e sensor de fim de curso e envia comandos via ESP-NOW.
- O **Principal** recebe esses comandos, aciona entradas digitais do CLP por GPIO e lê feedbacks do CLP e da micro do freio para retransmiti-los ao Remote.

---

## 2. Microcontroladores

| Módulo | Microcontrolador | Localização |
|---|---|---|
| Principal | ESP32 WROOM-32U (DevKit) | Painel fixo no depósito |
| Remote | ESP32 WROOM-32U (DevKit) | Embarcado no carrinho |

---

## 3. Alimentação

### 3.1 Módulo Principal

| Parâmetro | Valor |
|---|---|
| Fonte | Rede elétrica 110/220V |
| Conversão | Fonte chaveada → 5V → regulador 3.3V do ESP32 |
| Isolação | Galvânica obrigatória entre rede e GPIOs |

### 3.2 Módulo Remote

| Parâmetro | Valor |
|---|---|
| Bateria | Li-Ion 18650 recarregável |
| Carregador | Módulo TP4056 |
| Regulador | LM2596 step-down → 3.3V para ESP32 |
| Proteção | Enclosure mínimo IP54 |

---

## 4. Entradas — Módulo Principal

### 4.1 Botões de Teste Local

Usados para acionar o CLP diretamente durante testes, sem necessidade do Remote conectado. Quando pressionados, resetam o watchdog interno para evitar emergência por timeout.

| Botão | GPIO | Tipo | Pull-up | Leitura | Descrição |
|---|---|---|---|---|---|
| TESTE SUBIR | 32 | Táctil | Interno (`INPUT_PULLUP`) | LOW = ativo | Ativa `PIN_CLP_SUBIR` LOW enquanto pressionado |
| TESTE DESCER | 33 | Táctil | Interno (`INPUT_PULLUP`) | LOW = ativo | Ativa `PIN_CLP_DESCER` LOW enquanto pressionado |

### 4.2 Feedbacks do CLP

Todos configurados com `INPUT_PULLUP`.

| Sinal | GPIO | Origem | Leitura | Descrição |
|---|---|---|---|---|
| MOTOR_ATIVO | 23 | CLP | LOW = ativo | Informa motor em operação |
| EMERGÊNCIA_ATIVA | 25 | CLP | LOW = ativo | Informa emergência ativa |
| VEL1_ATIVA | 26 | CLP | LOW = ativo | Informa velocidade 1 ativa |
| VEL2_ATIVA | 27 | CLP | LOW = ativo | Informa velocidade 2 ativa |

### 4.3 Micro do Freio

| Sinal | GPIO | Tipo | Pull-up | Leitura | Descrição |
|---|---|---|---|---|---|
| MICRO_FREIO | 14 | NC | Interno (`INPUT_PULLUP`) | LOW = freio liberado, HIGH = freio ativo | Micro do freio indica freio aplicado; abertura do circuito também resulta em HIGH |

**Total de entradas no Principal: 7 GPIOs**

---

## 5. Saídas — Módulo Principal

### 5.1 Saídas para o CLP

Todas as saídas para o CLP operam em **ativo LOW** e passam antes por um **módulo de relé 5V comandado pelo ESP32**:
- `LOW` = sinal ativo para o CLP
- `HIGH` = sinal inativo

| Sinal | GPIO | Tipo | Descrição |
|---|---|---|---|
| SUBIR | 4 | Saída | Nível LOW estável enquanto comando remoto de subida permanecer válido |
| DESCER | 16 | Saída | Nível LOW estável enquanto comando remoto de descida permanecer válido |
| VEL1 | 17 | Saída | Pulso LOW de 50 ms |
| VEL2 | 5 | Saída | Pulso LOW de 50 ms |
| EMERGÊNCIA | 18 | Saída | LOW quando emergência remota ou watchdog expirado |
| RESET | 19 | Saída | Pulso LOW de 50 ms |
| FIM_CURSO | 22 | Saída | LOW quando fim de curso de descida está ativo |

### 5.2 LED Exclusivo

| LED | GPIO | Função |
|---|---|---|
| LINK REMOTE | 21 | Indica comunicação ativa com o Remote |

> Os canais de saída para o CLP podem acender LEDs físicos do módulo de relé ou da instalação externa, mas o firmware não controla esses indicadores separadamente.

**Total de saídas no Principal: 8 GPIOs**

**Total de GPIOs no Principal: 15** (7 entradas + 8 saídas)

---

## 6. Entradas — Módulo Remote

### 6.1 Botões

| Botão | GPIO | Tipo | Pull-up | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|---|
| SUBIR | 32 | Táctil | Interno (`INPUT_PULLUP`) | 50 ms | LOW = pressionado | Hold |
| DESCER | 33 | Táctil | Interno (`INPUT_PULLUP`) | 50 ms | LOW = pressionado | Hold |
| VEL1 | 39 | Táctil | Externo obrigatório | 50 ms | LOW = pressionado | Pulso |
| VEL2 | 34 | Táctil | Externo obrigatório | 50 ms | LOW = pressionado | Pulso |
| RESET | 255 | Desabilitado | — | — | — | Não utilizado nesta versão |
| EMERGÊNCIA | 13 | NC com trava | Interno (`INPUT_PULLUP`) | — | HIGH = ativo | Emergência local |

### 6.2 Sensores

| Sensor | GPIO | Tipo | Pull-up | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|---|
| Fim de curso descida | 36 | Microswitch | Externo obrigatório | 20 ms | LOW = acionado | Posição final de descida |

**Total de entradas no Remote: 6 GPIOs ativas + 1 desabilitada**

---

## 7. Saídas — Módulo Remote

| LED | GPIO | Função |
|---|---|---|
| LINK | 4 | Status de comunicação |
| MOTOR | 16 | Pisca enquanto aguarda partida; fixo com `motor_ativo` |
| VEL1 | 17 | Reflete `vel1_ativa` recebida |
| VEL2 | 5 | Reflete `vel2_ativa` recebida |
| EMERGÊNCIA | 19 | Emergência local ou emergência do CLP |

**Total de saídas no Remote: 5 GPIOs**

**Total de GPIOs no Remote: 11** (6 entradas ativas + 5 saídas)

---

## 8. Restrições de Pinout do ESP32

### 8.1 Pinos a Evitar para Entradas Críticas

| Pino | Motivo |
|---|---|
| GPIO 0 | Modo boot (strapping pin) |
| GPIO 2 | Modo boot (strapping pin) |
| GPIO 12 | Modo boot — afeta tensão do flash |
| GPIO 15 | Modo boot — afeta log de inicialização |

### 8.2 Pinos Somente Entrada (sem pull-up interno)

| Pinos | Motivo |
|---|---|
| GPIO 34, 35, 36, 39 | Somente entrada; sem pull-up/pull-down interno |

### 8.3 Recomendação

- Registrar o mapa final de GPIOs em `pinout.h` de cada módulo.
- Priorizar GPIOs com `INPUT_PULLUP` para sinais NC e feedbacks críticos.
- Evitar pinos strapping em entradas críticas.

---

## 9. Configuração de Pull-Up

| Componente | Pull-Up |
|---|---|
| Botões tácteis em GPIO 32/33 | Interno (`INPUT_PULLUP`) |
| Botões tácteis em GPIO 34/39/36 | Externo obrigatório |
| Botão emergência NC do Remote | Interno (`INPUT_PULLUP`) |
| Feedbacks do CLP no Principal | Interno (`INPUT_PULLUP`) |
| Micro do freio NC no Principal | Interno (`INPUT_PULLUP`) |

Lógicas importantes:

- botão NO: repouso = HIGH, pressionado = LOW
- botão NC de emergência: repouso = LOW, pressionado = HIGH
- feedback do CLP: LOW = ativo
- micro do freio NC: LOW = normal, HIGH = aberta/acionada

---

## 10. Especificações Elétricas dos LEDs

| Parâmetro | Valor |
|---|---|
| Tensão do LED | 3V |
| Resistor limitador | 220Ω por LED |
| Corrente máxima por GPIO | 12 mA |
| Cor | Definida pelo componente físico |

---

## 11. Módulo de Relés / Interface com CLP

Na arquitetura atual, o Principal não aciona diretamente o motor nem o freio. Ele apenas:

- envia sinais digitais para um módulo de relé 5V por GPIO
- usa os contatos desse módulo para acionar as entradas do CLP
- lê feedbacks digitais do CLP por GPIO

Os relés e a lógica de potência ficam sob responsabilidade do CLP e do circuito externo.

---

## 12. Sensor de Fim de Curso de Descida

| Parâmetro | Valor |
|---|---|
| Tipo | Microswitch |
| Localização | Posição final de descida |
| Debounce | 20 ms |
| Conexão | GPIO 36 do Remote |

---

## 13. Micro do Freio

| Parâmetro | Valor |
|---|---|
| Tipo | Microswitch NC |
| Conexão | GPIO 14 do Principal |
| Pull-up | Interno (`INPUT_PULLUP`) |
| Lógica | LOW = normal; HIGH = aberta/acionada |
| Uso no firmware | Propagada ao `PacoteStatus` como `micro_freio_ativa` |

> Fail-safe: cabo partido ou abertura da micro resultam em HIGH.

---

## 14. Lista de Materiais

| Qtd | Componente | Uso |
|---|---|---|
| 2 | ESP32 DevKit (WROOM-32U) | Principal + Remote |
| 1 | CLP | Lógica central do sistema |
| 1 | Bateria Li-Ion 18650 | Remote |
| 1 | Módulo TP4056 | Carregador do Remote |
| 1 | LM2596 step-down | Regulador do Remote |
| 2 | Botões com trava | Emergência principal + emergência remote |
| 4 | Botões tácteis / teste | Remote e testes locais do Principal |
| 1 | Sensor fim de curso | Descida |
| 1 | Micro do freio NC | Feedback do freio no Principal |
| 6 | LEDs discretos | 1 no Principal + 5 no Remote |
| Resistores | 220Ω / 10kΩ | LEDs e pull-ups externos necessários |
