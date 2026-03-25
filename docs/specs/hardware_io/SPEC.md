# Especificação de Hardware e I/O

**Versão:** 1.4
**Data:** 2026-03-22
**Referência:** DESIGN_SPEC.md v3.3, README.md v3.4

---

## 1. Visão Geral

O sistema utiliza dois microcontroladores ESP32 WROOM-32U com I/O digital para botões, relés e LEDs. Este documento detalha todos os componentes de hardware, pinout e especificações elétricas.

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
| Proteção | Enclosure mínimo IP54 (respingos e umidade) |

---

## 4. Entradas — Módulo Principal

### 4.1 Botões

| Botão | GPIO | Tipo | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|
| SUBIR | 36 | Táctil | 50 ms | Hold (nível) | Motor sentido subir — mantido pressionado |
| DESCER | 39 | Táctil | 50 ms | Hold (nível) | Motor sentido descer — mantido pressionado |
| VEL1 | 34 | Táctil | 50 ms | Pulso (borda) | Selecionar velocidade 1 |
| VEL2 | 35 | Táctil | 50 ms | Pulso (borda) | Selecionar velocidade 2 |
| VEL3 | 32 | Táctil | 50 ms | Pulso (borda) | Selecionar velocidade 3 |
| EMERGÊNCIA | 33 | NC c/ trava | — | Nível contínuo | Emergência — NC: repouso LOW, pressionado HIGH |
| REARME | 25 | Táctil | 50 ms | Pulso (borda) | Desativar emergência |

> GPIOs 34, 35, 36, 39 são input-only (sem pull-up interno) — usar pull-up externo 10kΩ.

**Total: 7 entradas digitais**

### 4.2 Sensores

| Sensor | GPIO | Tipo | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|
| Fim de curso | 26 | Microswitch | 20 ms | Nível (LOW=acionado) | Posição final de subida (estacionamento) — bloqueio pós-acionamento de 10 s |
| Microchave freio | 27 | Microswitch NA | — | Nível (HIGH=engatado) | Estado mecânico do freio |
| Monitor rede | 13 | Entrada digital | 50 ms | Nível (HIGH=presente) | Presença da rede elétrica — pull-down externo (divisor resistivo 5V→2,5V ou optoacoplador) |

**Total: 3 entradas digitais**

**Total de entradas no Principal: 10 GPIOs**

---

## 5. Saídas — Módulo Principal

### 5.1 Relés (com LED Compartilhado)

O módulo de relés utilizado é **ativo em LOW**: `GPIO LOW` = relé acionado; `GPIO HIGH` = relé desacionado. Cada GPIO aciona o relé e o LED em paralelo — ambos seguem o mesmo estado lógico.

| Relé | GPIO | Função | LED Associado | Acionamento |
|---|---|---|---|---|
| DIREÇÃO A | 4 | Motor sentido SUBIR | LED SUBIR — aceso quando motor sobe | GPIO LOW = ativo |
| DIREÇÃO B | 16 | Motor sentido DESCER | LED DESCER — aceso quando motor desce | GPIO LOW = ativo |
| VEL1 | 17 | Velocidade 1 (potenciômetro baixo) | LED VEL1 | GPIO LOW = ativo |
| VEL2 | 5 | Velocidade 2 (potenciômetro médio) | LED VEL2 | GPIO LOW = ativo |
| VEL3 | 18 | Velocidade 3 (potenciômetro alto) | LED VEL3 | GPIO LOW = ativo |
| FREIO_ON | 19 | Bobina de aplicação (cilindro avança, freio trava) | LED FREIO — aceso quando bobina pulsa | GPIO LOW = energizada |
| FREIO_OFF | 22 | Bobina de liberação (cilindro retrai, freio libera) | Nenhum | GPIO LOW = energizada |

> **Nota sobre o LED FREIO_ON:** o relé permanece ativo apenas durante o pulso de acionamento (~10s até microchave confirmar). Após confirmação, o relé é desativado (GPIO HIGH) para não manter a bobina energizada continuamente.

**Total: 7 GPIOs de saída (6 c/ LED + 1 sem LED)**

### 5.2 LEDs Exclusivos

| LED | GPIO | Função |
|---|---|---|
| LINK REMOTE | 21 | Indica comunicação ativa com Remote |

**Total: 1 GPIO de saída exclusivo**

**Total de saídas no Principal: 8 GPIOs**

**Total de GPIOs no Principal: 18** (10 entradas + 8 saídas)

---

## 6. Entradas — Módulo Remote

### 6.1 Botões

| Botão | GPIO | Tipo | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|
| SUBIR | 36 | Táctil (capa borracha) | 50 ms | Hold (nível) | Motor sentido subir — mantido pressionado |
| DESCER | 39 | Táctil (capa borracha) | 50 ms | Hold (nível) | Motor sentido descer — mantido pressionado |
| VEL1 | 34 | Táctil (capa borracha) | 50 ms | Pulso (borda) | Selecionar velocidade 1 |
| VEL2 | 35 | Táctil (capa borracha) | 50 ms | Pulso (borda) | Selecionar velocidade 2 |
| VEL3 | 32 | Táctil (capa borracha) | 50 ms | Pulso (borda) | Selecionar velocidade 3 |
| EMERGÊNCIA | 33 | NC c/ trava | — | Nível contínuo | Emergência — NC: repouso LOW, pressionado HIGH |

> GPIOs de entrada consistentes com o Módulo Principal. GPIOs 34, 35, 36, 39 são input-only — usar pull-up externo 10kΩ.

### 6.2 Sensores

| Sensor | GPIO | Tipo | Debounce | Leitura | Descrição |
|---|---|---|---|---|---|
| Fim de curso descida | 13 | Microswitch | 20 ms | Nível (LOW=acionado) | Posição final de descida (margem do rio) — bloqueio pós-liberação de 10 s (bloqueia apenas DESCER) |

**Total de entradas no Remote: 7 GPIOs** (6 botões + 1 sensor)

---

## 7. Saídas — Módulo Remote

| LED | GPIO | Função |
|---|---|---|
| LINK | 4 | Status de comunicação |
| MOTOR | 16 | Motor em operação |
| VEL1 | 17 | Velocidade 1 ativa |
| VEL2 | 5 | Velocidade 2 ativa |
| VEL3 | 18 | Velocidade 3 ativa |
| EMERGÊNCIA | 19 | Emergência ou falha de comunicação |
| ALARME | 21 | Rearme com botão local ainda travado |

**Total de saídas no Remote: 7 GPIOs**

**Total de GPIOs no Remote: 14** (7 entradas + 7 saídas)

---

## 8. Restrições de Pinout do ESP32

### 8.1 Pinos a Evitar para Entradas Críticas

| Pino | Motivo |
|---|---|
| GPIO 0 | Modo boot (strapping pin) |
| GPIO 2 | Modo boot (strapping pin) |
| GPIO 12 | Modo boot — afeta tensão do flash (strapping pin) |
| GPIO 15 | Modo boot — afeta log de inicialização |

### 8.2 Pinos Somente Entrada (sem pull-up interno)

| Pinos | Motivo |
|---|---|
| GPIO 34, 35, 36, 39 | Somente entrada; sem pull-up/pull-down interno |

### 8.3 Recomendação

- Registrar o mapa final de GPIOs no arquivo `pinout.h` de cada módulo.
- Botões de emergência e fim de curso **não** devem usar pinos strapping.
- Preferir resistores de pull-up **externos** para entradas críticas.

---

## 9. Configuração de Pull-Up

| Componente | Pull-Up |
|---|---|
| Botões tácteis (NO) | Resistor pull-up externo (10kΩ recomendado) |
| Botão emergência NC (trava) | Pull-up interno (INPUT_PULLUP) — GPIO 33 |
| Fim de curso | Resistor pull-up externo |

Lógica botões NO: **não** pressionado = HIGH; pressionado = LOW.

Lógica botão emergência NC: **não** pressionado (repouso) = LOW (contato fechado drena para GND); pressionado = HIGH (contato abre, pull-up puxa para HIGH).

> **Fail-safe:** com botão NC e pull-up, um cabo partido resulta em pino HIGH → emergência ativada. Comportamento seguro por projeto.

---

## 10. Especificações Elétricas dos LEDs

| Parâmetro | Valor |
|---|---|
| Tensão do LED | 3V (padrão Arduino) |
| Resistor limitador | 220Ω por LED |
| Corrente máxima por GPIO | 12 mA |
| Cor | Definida pelo componente físico |

### 10.1 LEDs Compartilhados com Relés (Principal)

Quando um LED e o driver do módulo relé compartilham o mesmo GPIO:
- Medir corrente total na protoboard durante a Fase 1.
- Se corrente > 12 mA: usar transistor NPN (ex: BC547) ou buffer (ULN2003) para isolar.

---

## 11. Módulo de Relés

| Parâmetro | Valor |
|---|---|
| Tensão de operação | 5V |
| Número de canais (Principal) | 7 (direção A, direção B, VEL1, VEL2, VEL3, FREIO_ON, FREIO_OFF) — de 8 disponíveis |
| Acionamento | **Ativo LOW** — GPIO LOW = relé acionado; GPIO HIGH = relé desacionado |
| Dimensionamento | Corrente de partida do motor × fator 2x |

---

## 12. Sensor de Fim de Curso

| Parâmetro | Valor |
|---|---|
| Tipo | Microswitch (chave de limite) |
| Localização | Posição final de subida (estacionamento/depósito) |
| Debounce | 20 ms (firmware) |
| Conexão | Entrada digital do ESP32 Principal |

---

## 13. Microchave do Freio

| Parâmetro | Valor |
|---|---|
| Tipo | Microswitch NA (normalmente aberto) |
| Localização | Acoplada ao cilindro do freio — acionada quando cilindro está retraído (freio liberado) |
| Conexão | GPIO 27 do ESP32 Principal |
| Pull-up | Interno (INPUT_PULLUP) |
| Lógica | HIGH = freio engatado (cilindro avançado, micro aberta); LOW = freio liberado (cilindro retraído, micro pressionada) |
| Função firmware | Controla máquina de estados do freio (`Freio::atualizar()`); bloqueia motor enquanto GPIO 27 ≠ LOW |

**Comportamento por estado:**

| Condição | GPIO 27 | Microchave | Estado do cilindro | Motor |
|---|---|---|---|---|
| Freio engatado (padrão) | HIGH | Aberta | Avançado (brake pads travados) | Bloqueado |
| Freio em transição (FREIO_OFF pulsando) | HIGH → LOW | Abre → Fecha | Retraindo (~10s) | Bloqueado durante transição |
| Freio liberado (confirmado) | LOW | Pressionada | Retraído (brake pads livres) | Permitido |
| Freio em transição (FREIO_ON pulsando) | LOW → HIGH | Fecha → Abre | Avançando (~10s) | Bloqueado |

> Fail-safe: cabo partido → GPIO flutua HIGH → interpretado como freio engatado → motor bloqueado.
>
> O Remote não recebe o estado do freio — o operador percebe o bloqueio pela ausência de resposta do motor durante a transição de ~10s.

---

## 14. Lista de Materiais

| Qtd | Componente | Uso |
|---|---|---|
| 2 | ESP32 DevKit (WROOM-32U) | Principal + Remote |
| 1 | Módulo relé 6 canais (5V) | Principal |
| 1 | Bateria Li-Ion 18650 | Remote |
| 1 | Módulo TP4056 | Carregador de bateria Remote |
| 1 | LM2596 step-down | Regulador de tensão Remote |
| 2 | Botões com trava (emergência) | Painel + Remote |
| 6 | Botões tácteis | Painel (SUBIR, DESCER, VEL1, VEL2, VEL3, REARME) |
| 5 | Botões tácteis com capa borracha | Remote (SUBIR, DESCER, VEL1, VEL2, VEL3) |
| 1 | Sensor fim de curso (microswitch) | Estacionamento |
| 13 | LEDs 3V (cor a definir) | 7 no Principal + 7 no Remote (nota: LED EMERGÊNCIA do Principal usa LED exclusivo e 6 compartilhados c/ relés) |
| 13 | Resistores 220Ω | 1 por LED |
| ~15 | Resistores 10kΩ | Pull-ups de botões e sensores |
| — | Transistores NPN (BC547) / ULN2003 | Se necessário para driver dos relés |
| 2 | Enclosures | Painel (caixa elétrica) + Remote (IP54) |
| — | Cabos, conectores, terminais | Montagem geral |
