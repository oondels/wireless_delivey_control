# Especificação de Controle de Motor e Freio

**Versão:** 2.0
**Data:** 2026-03-22
**Referência:** DESIGN_SPEC.md v3.3, README.md v3.4

---

## 1. Visão Geral

Na arquitetura atual, o controle de motor e freio é executado pelo **CLP**. O firmware ESP32:

- no **Remote** lê os comandos do operador
- no **Principal** replica esses comandos ao CLP
- no **Principal** lê os feedbacks do CLP e envia o status de volta ao Remote

---

## 2. Sinais de Direção

### 2.1 Configuração

| Sinal | Função | GPIO no Principal | Acionamento |
|---|---|---|---|
| SUBIR | Movimento no sentido SUBIR | 4 | LOW = ativo |
| DESCER | Movimento no sentido DESCER | 16 | LOW = ativo |

### 2.2 Regras de Operação

- O Principal nunca mantém `SUBIR` e `DESCER` ativos ao mesmo tempo.
- O dead-time e qualquer outra proteção de inversão são responsabilidade do CLP.

### 2.3 Regra Homem-Morto

O motor só permanece em operação enquanto o Remote continuar enviando hold de `SUBIR` ou `DESCER`. Ao soltar o botão, o Principal volta os sinais de direção a HIGH e o CLP executa a parada.

### 2.4 Prioridade de Comando

- O firmware atual não implementa botões operacionais no Principal; apenas botões de teste local.
- Quando o status do Principal é inválido ou `emergencia_ativa == 1`, o Remote bloqueia `SUBIR` e `DESCER`.

---

## 3. Controle de Velocidade

### 3.1 Arquitetura

A velocidade é controlada por **hardware externo / CLP**. O firmware apenas gera pulsos de seleção e lê o feedback correspondente do CLP.

| Sinal | Nível | GPIO no Principal | Acionamento |
|---|---|---|---|
| VEL1 | Velocidade 1 | 17 | Pulso LOW de 50 ms |
| VEL2 | Velocidade 2 | 5 | Pulso LOW de 50 ms |

### 3.2 Regras de Operação

- Seleção por botões de pulso no Remote.
- O Principal gera um pulso LOW de 50 ms no sinal correspondente.
- O CLP decide qual nível de velocidade permanece ativo.

### 3.3 Sincronização com Remote

- O Principal lê `VEL1_ATIVA` e `VEL2_ATIVA` do CLP.
- O Remote atualiza seus LEDs com base nesses feedbacks.

### 3.4 Persistência de Velocidade

- A persistência de velocidade é responsabilidade do CLP.

---

## 4. Interface de Freio e Feedback

### 4.1 Configuração

Na arquitetura atual, o freio é controlado pelo **CLP**. O ESP32 Principal não energiza bobinas do freio; ele apenas:

- envia comandos digitais ao CLP
- lê feedbacks digitais do CLP
- lê a micro do freio no `GPIO 14`

### 4.2 Micro do Freio

| Sinal | GPIO | Tipo | Leitura | Uso |
|---|---|---|---|---|
| MICRO_FREIO | 14 | NC | LOW = normal, HIGH = aberta/acionada | Telemetria via `PacoteStatus` |

### 4.3 Regras de Operação

- A lógica de aplicação/liberação do freio é externa ao firmware ESP32 e fica sob responsabilidade do CLP.
- O Principal repassa a telemetria da micro do freio ao Remote, mas não a usa para bloquear `SUBIR`/`DESCER`.
- O Remote registra transições de `micro_freio_ativa` apenas para diagnóstico.

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
  ├─ Remote verifica: status do Principal válido
  ├─ Remote verifica: emergencia_ativa == false no PacoteStatus
  ├─ Se permitido: envia SUBIR ou DESCER via ESP-NOW
  ├─ Principal replica o comando ao CLP
  └─ CLP executa a lógica de freio e motor
```

### 6.2 Parada (Homem-Morto)

```
Operador solta SUBIR ou DESCER
  │
  ├─ Remote deixa de enviar hold
  ├─ Principal volta GPIO de direção a HIGH
  └─ CLP executa a lógica de parada e freio
```

### 6.3 Inversão de Direção

```
Operador solta SUBIR → pressiona DESCER (ou vice-versa)
  │
  ├─ Remote troca o comando enviado
  ├─ Principal troca o sinal de direção entregue ao CLP
  └─ CLP aplica a sequência de dead-time e freio
```

---

## 7. Tabela de Condições de Acionamento do Motor

| Emergência | Falha Comun. | Fim de Curso | Status do Principal | Botão Hold | Resultado |
|---|---|---|---|---|---|
| Não | Não | Não acionado | Válido | Pressionado | Comando enviado ao CLP |
| Não | Não | Não acionado | Inválido | Pressionado | Comando bloqueado no Remote |
| Sim | Qualquer | Qualquer | Qualquer | Pressionado | Comando bloqueado no Remote |
| Não | Não | Não acionado | Qualquer | Solto | Hold removido; CLP para o sistema |
| Não | Não | Acionado | Qualquer | Qualquer | Motor OFF → Freio ON → `PARADO` |
| Não | Sim | Qualquer | Qualquer | Qualquer | Watchdog do Principal aciona emergência no CLP |

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
