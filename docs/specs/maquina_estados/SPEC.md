# Especificação da Máquina de Estados

**Versão:** 1.4
**Data:** 2026-04-24
**Referência:** README.md v4.0

---

## 1. Visão Geral

Na arquitetura atual, a máquina de estados operacional do sistema fica no **CLP em Ladder**.

O firmware dos ESP32 **não** mantém mais uma máquina de estados própria para motor/freio. Ele apenas:

- no **Remote**: lê botões, decide se `SUBIR`/`DESCER` podem ser enviados e atualiza LEDs
- no **Principal**: recebe comandos, replica sinais ao CLP, monitora watchdog e retransmite feedbacks

---

## 2. Estado Observado pelo Firmware

O estado observado pelo firmware ESP32 é derivado de:

- validade do link (`link_ok`)
- feedback do CLP (`motor_ativo`, `emergencia_ativa`, `vel1_ativa`, `vel2_ativa`)
- micro do freio (`micro_freio_ativa`)

Não existe enum `EstadoSistema` ativo no código atual do firmware.

---

## 3. Regras de Decisão no Remote

### 3.1 Bloqueio de Movimento

O Remote bloqueia `SUBIR` e `DESCER` quando qualquer condição abaixo for verdadeira:

1. `link_ok == 0`, ou
2. o último `PacoteStatus` tem mais de `500 ms`, ou
3. `emergencia_ativa == 1`

Quando bloqueado:

- `SUBIR` e `DESCER` não são enviados
- heartbeat, `VEL1`, `VEL2`, `RESET`, `EMERGÊNCIA` e `fim_curso_descida` continuam podendo ser enviados

### 3.2 LEDs do Remote

- `LINK` reflete validade do status
- `MOTOR` reflete `motor_ativo`
- `VEL1` reflete `vel1_ativa`
- `VEL2` reflete `vel2_ativa`
- `EMERGÊNCIA` reflete botão local de emergência, emergência do CLP ou perda de link

---

## 4. Regras de Decisão no Principal

### 4.1 Watchdog

Se nenhum `PacoteRemote` válido for recebido por mais de `500 ms`:

1. `PIN_CLP_EMERGENCIA` vai para LOW
2. `PIN_CLP_SUBIR` e `PIN_CLP_DESCER` vão para HIGH
3. `PacoteStatus.link_ok` passa a `0`

### 4.2 Replicação para o CLP

- `SUBIR` e `DESCER` são sinais de nível
- `VEL1`, `VEL2` e `RESET` são pulsos de `50 ms`
- `EMERGÊNCIA` e `FIM_CURSO` são sinais de nível

### 4.3 Feedback para o Remote

O Principal retransmite ao Remote:

- `motor_ativo`
- `emergencia_ativa`
- `vel1_ativa`
- `vel2_ativa`
- `micro_freio_ativa`

---

## 5. Fonte de Verdade

Para comportamento operacional do sistema, a fonte de verdade é:

1. **CLP** para estados de motor, freio, emergência e velocidade
2. **Principal** para validade do link com o Remote
3. **Remote** apenas para entrada do operador e bloqueio preventivo local
