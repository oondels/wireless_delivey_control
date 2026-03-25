# Especificação de Controle de Motor e Freio

**Versão:** 2.0
**Data:** 2026-03-22
**Referência:** DESIGN_SPEC.md v3.3, README.md v3.4

---

## 1. Visão Geral

O sistema controla um guincho motorizado que movimenta um carrinho de transporte de jet skis ao longo de um trilho entre o depósito e a margem do rio. O controle é feito por relés de 5V acionados pelo Módulo Principal (ESP32).

---

## 2. Relés de Direção

### 2.1 Configuração

| Relé | Função | GPIO | Acionamento |
|---|---|---|---|
| DIREÇÃO A | Motor sentido SUBIR | 1 GPIO compartilhado (relé + LED) | HIGH = ativo |
| DIREÇÃO B | Motor sentido DESCER | 1 GPIO compartilhado (relé + LED) | HIGH = ativo |

### 2.2 Regras de Operação

- **Exclusividade:** Apenas **um** relé de direção pode estar ativo por vez.
- **Dead-time:** Intervalo mínimo de **100 ms** entre desligar um relé de direção e ligar o oposto.
- O dead-time protege o motor contra curto-circuito por inversão instantânea.

### 2.3 Regra Homem-Morto

O motor **só permanece em operação enquanto o botão SUBIR ou DESCER estiver fisicamente mantido pressionado**.

**Ao soltar o botão, executa imediatamente:**
1. Cortar alimentação do motor (desacionar relés de direção) → `GPIO LOW`
2. Acionar freio mecânico (acionar relé de freio) → `GPIO HIGH`

Esta regra aplica-se tanto ao Painel Central quanto ao Remote.

### 2.4 Prioridade de Comando

- Comandos do Painel Central têm **prioridade** sobre comandos do Remote.
- Se ambos estiverem pressionando botões de direção simultaneamente, o Painel Central prevalece.

---

## 3. Controle de Velocidade

### 3.1 Arquitetura

A velocidade é controlada por **hardware externo**: potenciômetros físicos pré-ajustados, selecionados por relés. O software apenas seleciona **qual relé ativar**.

| Relé | Nível | GPIO | Acionamento |
|---|---|---|---|
| VEL1 | Velocidade 1 (baixa) | 1 GPIO compartilhado (relé + LED) | HIGH = ativo |
| VEL2 | Velocidade 2 (média) | 1 GPIO compartilhado (relé + LED) | HIGH = ativo |
| VEL3 | Velocidade 3 (alta) | 1 GPIO compartilhado (relé + LED) | HIGH = ativo |

### 3.2 Regras de Operação

- Seleção por botões de **pulso** (não precisam ser mantidos pressionados).
- A velocidade selecionada é armazenada em memória de estado (`velocidade_atual`).
- Apenas **um** relé de velocidade pode estar ativo por vez.
- Ao selecionar novo nível: **desacionar relé anterior → acionar novo relé**.
- O LED de cada nível acende/apaga automaticamente junto com o relé (GPIO compartilhado).

### 3.3 Sincronização com Remote

- O valor de `velocidade_atual` é incluído no `PacoteStatus` enviado ao Remote a cada 200 ms.
- O Remote atualiza seus LEDs de velocidade com base nesse valor.
- Ambos os painéis exibem sempre a mesma indicação de velocidade.

### 3.4 Persistência de Velocidade

- A velocidade selecionada persiste entre ciclos de movimentação.
- Ao parar e retomar o motor, o último nível selecionado permanece ativo.
- A velocidade padrão na inicialização é **VEL1**.

---

## 4. Relé de Freio

### 4.1 Configuração

O módulo de relés utilizado é **ativo em LOW**: `GPIO LOW` = relé acionado; `GPIO HIGH` = relé desacionado.

| Relé | Função | GPIO | Acionamento |
|---|---|---|---|
| FREIO_ON | Bobina de aplicação — cilindro avança, freio trava | 19 | GPIO LOW = bobina energizada |
| FREIO_OFF | Bobina de liberação — cilindro recua, freio libera | 22 | GPIO LOW = bobina energizada |

**Microchave do freio (GPIO 27, NA + pull-up interno):**

| GPIO 27 | Microchave | Estado do cilindro | Estado do freio |
|---|---|---|---|
| HIGH | Aberta (não pressionada) | Avançado | **Engatado** (ativo) — motor bloqueado |
| LOW | Pressionada (acionada) | Retraído | **Liberado** (inativo) — motor permitido |

> **Modo padrão:** freio engatado (GPIO 27 = HIGH). Fail-safe: cabo partido → GPIO HIGH → motor bloqueado.

### 4.2 Máquina de Estados do Freio

O freio possui uma máquina de estados interna (classe `Freio`) com 4 estados:

```
FREIO_ENGATADO   — Freio aplicado, confirmado pela microchave (GPIO 27 = HIGH)
FREIO_ENGATANDO  — Relé FREIO_ON ativo, cilindro avançando (~10s), aguardando GPIO 27 → HIGH
FREIO_LIBERADO   — Freio liberado, confirmado pela microchave (GPIO 27 = LOW)
FREIO_LIBERANDO  — Relé FREIO_OFF ativo, cilindro retraindo (~10s), aguardando GPIO 27 → LOW
```

Os relés operam de forma assimétrica:
- **FREIO_ON (GPIO 19 — aplicação):** permanece **ativo continuamente** enquanto o freio está engatado. O cilindro possui fim de curso mecânico próprio; o relé não é desativado após a microchave confirmar o engate.
- **FREIO_OFF (GPIO 22 — liberação):** funciona como **pulso** — é ativado para retrair o cilindro e desativado automaticamente quando a microchave confirma o freio liberado (GPIO 27 = LOW).

### 4.3 API do Firmware (classe `Freio`)

```cpp
Freio freio;
freio.init();  // Configura GPIOs, lê microchave para determinar estado inicial

// Inicia engate do freio (idempotente se já ENGATADO ou ENGATANDO)
freio.acionar();
//   GPIO 22 (FREIO_OFF) → HIGH  (garante bobina de liberação inativa)
//   delay 10 ms
//   GPIO 19 (FREIO_ON)  → LOW   (ativa bobina de aplicação — permanece ativa)
//   _estado = FREIO_ENGATANDO
//   atualizar() confirma GPIO 27 → HIGH (~10s) e atualiza _estado = FREIO_ENGATADO
//   FREIO_ON permanece LOW continuamente (não é desativado após confirmação)

// Inicia liberação do freio (idempotente se já LIBERADO ou LIBERANDO)
freio.liberar();
//   GPIO 19 (FREIO_ON)  → HIGH  (garante bobina de aplicação inativa)
//   delay 10 ms
//   GPIO 22 (FREIO_OFF) → LOW   (pulsa bobina de liberação)
//   _estado = FREIO_LIBERANDO
//   atualizar() desativa FREIO_OFF quando GPIO 27 → LOW (~10s)

// Monitorar microchave e transicionar estados (chamar no loop principal)
freio.atualizar();

// Consultar estado
freio.isLiberado();   // true somente após GPIO 27 = LOW confirmado
freio.isEngatado();   // true somente após GPIO 27 = HIGH confirmado
freio.isTransicao();  // true durante ENGATANDO ou LIBERANDO
```

> **Invariante:** `FREIO_ON` e `FREIO_OFF` nunca ficam LOW (ativos) simultaneamente. A troca sempre segue: desativa lado ativo → dead-time ~10 ms → ativa lado oposto.

### 4.4 Regras de Operação

- O freio é o **estado padrão** — sistema inicializa em FREIO_ENGATADO (ou FREIO_ENGATANDO se GPIO 27 = LOW na inicialização).
- O motor **nunca** aciona sem `freio.isLiberado() == true` **E** `digitalRead(GPIO 27) == LOW` (dupla verificação).
- Durante a transição (~10s), o motor permanece bloqueado e o estado do sistema é `PARADO`.
- Guarda de segurança em `atualizar()`: se estado é LIBERADO mas GPIO 27 = HIGH (freio engata externamente), estado é corrigido imediatamente para ENGATADO.

### 4.5 Modo Manual de Recuperação (REARME + SUBIR/DESCER)

Para situações onde o cilindro para no meio do curso (microchave não ativada, sistema travado):

| Combinação | Ação |
|---|---|
| REARME segurado + SUBIR hold | `freio.manualAcionar()` — força cilindro avançar (freio engata) |
| REARME segurado + DESCER hold | `freio.manualLiberar()` — força cilindro retrair (freio libera) |

- Motor permanece **sempre desligado** durante o modo manual.
- Máquina de estados principal é **ignorada**.
- Ao soltar REARME ou direção, `freio.manualParar()` desliga ambos os relés e ressincroniza o estado pela microchave.
- **Emergência tem prioridade máxima:** se qualquer fonte de emergência estiver ativa (botão local, botão remote, ou sinal ESP-NOW), o modo manual é **completamente bloqueado**, independentemente de REARME estar pressionado.

### 4.6 Microchave do Freio

- Conectada ao **GPIO 27** do ESP32 Principal (NA, pull-up interno `INPUT_PULLUP`).
- Lida pelo firmware para controle da máquina de estados do freio.
- Também pode estar conectada diretamente ao circuito elétrico do freio como camada de hardware independente.
- O Remote **não recebe** o estado do freio — o operador percebe o bloqueio pela ausência de resposta do motor durante a transição.

---

## 5. Fim de Curso do Estacionamento

### 5.1 Configuração

- Sensor tipo microswitch instalado na posição final de subida.
- Conectado ao ESP32 do Módulo Principal como entrada digital.
- Debounce: **20 ms** mínimo.

### 5.2 Comportamento ao Acionar

1. Motor cortado imediatamente (relés de direção OFF).
2. Freio mecânico acionado imediatamente (relé de freio ON).
3. Estado do sistema → `PARADO`.

### 5.3 Diferença em Relação à Emergência

| Aspecto | Fim de Curso | Emergência |
|---|---|---|
| Natureza | Condição operacional esperada | Falha ou ação de segurança |
| Estado resultante | `PARADO` | `EMERGENCIA_ATIVA` |
| Requer rearme | **Não** | **Sim** |
| Retomar operação | Imediato (ex: DESCER) | Somente após rearme manual |

---

## 6. Sequência de Acionamento do Motor

### 6.1 Partida

```
Operador pressiona e segura SUBIR ou DESCER
  │
  ├─ Verificar: emergencia_ativa == false
  ├─ Verificar: falha_comunicacao == false
  ├─ Verificar: fim_de_curso não acionado (se SUBIR)
  │
  ├─ freio.liberar() → pulso FREIO_OFF (GPIO 22 LOW)
  │   [cilindro retrai durante ~10 segundos]
  │   [atualizar() monitora GPIO 27]
  │
  ├─ Aguardar: freio.isLiberado() == true E GPIO 27 == LOW
  │   (durante a espera: motor desligado, estado PARADO)
  │
  ├─ freio.atualizar() confirma GPIO 27 LOW → FREIO_OFF desativado
  ├─ Acionar relé de direção correspondente (GPIO LOW = ativo)
  └─ Estado → SUBINDO ou DESCENDO
```

### 6.2 Parada (Homem-Morto)

```
Operador solta SUBIR ou DESCER
  │
  ├─ Desacionar relés de direção (GPIO HIGH = inativo)
  ├─ freio.acionar() → pulso FREIO_ON (GPIO 19 LOW)
  │   [cilindro avança durante ~10 segundos]
  │   [atualizar() monitora GPIO 27]
  │
  ├─ freio.atualizar() confirma GPIO 27 HIGH → _estado = FREIO_ENGATADO (FREIO_ON permanece LOW)
  └─ Estado → PARADO
```

### 6.3 Inversão de Direção

```
Operador solta SUBIR → pressiona DESCER (ou vice-versa)
  │
  ├─ Desacionar relé de direção atual (GPIO HIGH)
  ├─ freio.acionar() → pulso FREIO_ON
  ├─ Dead-time 100 ms no motor (classe Motor)
  ├─ freio.liberar() → pulso FREIO_OFF
  ├─ Aguardar freio.isLiberado() == true (GPIO 27 LOW)
  ├─ Acionar relé de direção oposta (GPIO LOW)
  └─ Estado → DESCENDO (ou SUBINDO)
```

---

## 7. Tabela de Condições de Acionamento do Motor

| Emergência | Falha Comun. | Fim de Curso | GPIO 27 | Botão Hold | Resultado |
|---|---|---|---|---|---|
| Não | Não | Não acionado | LOW (freio liberado) | Pressionado | Motor ON |
| Não | Não | Não acionado | HIGH (freio engatado/transição) | Pressionado | Motor AGUARDA (FREIO_OFF pulsando) |
| Não | Não | Não acionado | Qualquer | Solto | Motor OFF → Freio ON → `PARADO` |
| Não | Não | Acionado | Qualquer | Qualquer | Motor OFF → Freio ON → `PARADO` |
| Não | Sim | Qualquer | Qualquer | Qualquer | `FALHA_COMUNICACAO` → Freio ON |
| Sim | Qualquer | Qualquer | Qualquer | Qualquer | `EMERGENCIA_ATIVA` → Freio ON |

> Módulo relé **ativo em LOW**: "Freio acionado (FREIO_ON)" = GPIO 19 LOW + GPIO 22 HIGH. "Freio liberado (FREIO_OFF)" = GPIO 22 LOW + GPIO 19 HIGH. Após confirmação de liberação (GPIO 27 = LOW), FREIO_OFF vai para HIGH (pulso encerrado). FREIO_ON **permanece LOW** continuamente enquanto o freio está engatado.

---

## 8. Dimensionamento dos Relés

- Relés dimensionados para corrente de **partida** do motor (não corrente nominal).
- Fator de segurança: **2x** sobre a corrente de partida medida.
- Medição obrigatória na Fase 3 (Integração).

---

## 9. Requisitos de Latência

| Métrica | Requisito |
|---|---|
| Botão → resposta do motor | < **100 ms** |
| Dead-time na inversão de direção | **100 ms** (mínimo) |
