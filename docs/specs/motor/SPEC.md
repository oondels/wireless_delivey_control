# Especificação de Controle de Motor e Freio

**Versão:** 1.0
**Data:** 2026-03-16
**Referência:** DESIGN_SPEC.md v3.1

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

| Relé | Função | GPIO | Acionamento |
|---|---|---|---|
| FREIO | Freio mecânico | 1 GPIO compartilhado (relé + LED) | HIGH = freio aplicado |

### 4.2 API do Firmware (classe `Freio`)

```cpp
Freio freio;
freio.init();
freio.acionar();   // GPIO HIGH → relé energizado → freio aplicado + LED aceso
freio.liberar();   // GPIO LOW  → relé desenergizado → freio liberado + LED apagado
```

### 4.3 Regras de Operação

- O freio é o **estado padrão** — sempre acionado quando o motor está desligado.
- O freio é liberado **somente** quando o motor está prestes a ser ligado.
- Não há leitura de sensor de freio pelo firmware — a microchave atua diretamente no circuito.

### 4.4 Microchave do Freio (Hardware Externo)

- Conectada **diretamente** ao circuito do freio mecânico, **não** ao ESP32.
- Atua como camada de segurança independente do firmware.
- Impede fisicamente o funcionamento do motor se o freio estiver engatado.

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
  ├─ Liberar freio (GPIO LOW → relé freio OFF)
  ├─ Acionar relé de direção correspondente (GPIO HIGH)
  └─ Estado → SUBINDO ou DESCENDO
```

### 6.2 Parada (Homem-Morto)

```
Operador solta SUBIR ou DESCER
  │
  ├─ Desacionar relés de direção (GPIO LOW)
  ├─ Acionar freio (GPIO HIGH → relé freio ON)
  └─ Estado → PARADO
```

### 6.3 Inversão de Direção

```
Operador solta SUBIR → pressiona DESCER (ou vice-versa)
  │
  ├─ Desacionar relé de direção atual (GPIO LOW)
  ├─ Acionar freio (GPIO HIGH)
  ├─ Aguardar dead-time (100 ms)
  ├─ Liberar freio (GPIO LOW)
  ├─ Acionar relé de direção oposta (GPIO HIGH)
  └─ Estado → DESCENDO (ou SUBINDO)
```

---

## 7. Tabela de Condições de Acionamento do Motor

| Emergência | Falha Comun. | Fim de Curso | Botão Hold | Resultado |
|---|---|---|---|---|
| Não | Não | Não acionado | Pressionado | Motor ON |
| Não | Não | Não acionado | Solto | Motor OFF → Freio ON → `PARADO` |
| Não | Não | Acionado | Qualquer | Motor OFF → Freio ON → `PARADO` |
| Não | Sim | Qualquer | Qualquer | `FALHA_COMUNICACAO` → Freio ON |
| Sim | Qualquer | Qualquer | Qualquer | `EMERGENCIA_ATIVA` → Freio ON |

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
