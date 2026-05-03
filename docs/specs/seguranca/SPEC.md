# Especificação de Segurança e Emergência (Fail-Safe)

**Versão:** 1.5  
**Data:** 2026-04-24  
**Referência:** README.md v4.0

---

## 1. Princípio Fundamental

Na arquitetura atual, o CLP executa a lógica de potência e segurança. Os ESP32 atuam como ponte de comunicação e devem falhar para um estado seguro:

- perda de comunicação Remote → Principal aciona `PIN_CLP_EMERGENCIA` em LOW
- comandos de movimento são interrompidos imediatamente quando condições de operação deixam de ser válidas
- o Remote nunca assume que pode mover apenas pelo botão local; ele depende do status válido vindo do Principal

---

## 2. Prioridades de Bloqueio

### 2.1 No Principal

Para aceitar movimento remoto (`SUBIR`/`DESCER`), o firmware exige simultaneamente:

1. watchdog de comunicação **não expirado**
2. `emergencia` remota **inativa**
3. feedback `EMERGENCIA_ATIVA` do CLP **inativo**
4. `micro_freio_ativa == 0`
5. feedback `MOTOR_ATIVO == 0`

Se qualquer uma dessas condições falhar:

- `PIN_CLP_SUBIR` e `PIN_CLP_DESCER` vão para HIGH
- o hold remoto é bloqueado
- a perda de watchdog também força `PIN_CLP_EMERGENCIA` para LOW

### 2.2 No Remote

O Remote bloqueia localmente `SUBIR` e `DESCER` quando qualquer condição abaixo ocorre:

1. status do Principal expirado
2. `link_ok == 0`
3. emergência local ativa
4. `emergencia_ativa == 1` no `PacoteStatus`

Os comandos de pulso (`VEL1`, `VEL2`, `RESET`) continuam podendo ser transmitidos mesmo com bloqueio de movimento.

---

## 3. Emergência

### 3.1 Emergência Local do Remote

O botão de emergência do Remote é NC com trava:

- repouso: LOW
- ativo: HIGH
- cabo rompido ou contato aberto também resultam em HIGH

Quando ativo:

- o Remote envia `emergencia = 1` no `PacoteRemote`
- o LED de emergência do Remote pisca em 4 Hz
- `SUBIR` e `DESCER` ficam bloqueados localmente

### 3.2 Emergência no Principal / CLP

O Principal propaga emergência ao CLP por `PIN_CLP_EMERGENCIA`:

- LOW quando a emergência remota está ativa
- LOW quando o watchdog do Remote expira
- HIGH quando a comunicação é restaurada e não há emergência remota ativa

O feedback de emergência efetiva do sistema vem do CLP por `EMERGENCIA_ATIVA`:

- LOW no GPIO do Principal = emergência ativa
- esse estado é retransmitido ao Remote em `PacoteStatus.emergencia_ativa`
- no Remote, o LED de emergência também pisca em 4 Hz quando esse feedback está ativo

> O firmware ESP32 não implementa lógica própria de rearme da emergência do CLP. O eventual rearme final pertence ao Ladder e ao circuito externo.

---

## 4. Watchdog de Comunicação

| Parâmetro | Valor |
|---|---|
| Heartbeat Remote → Principal | 100 ms |
| Status Principal → Remote | 200 ms |
| Timeout de watchdog | 500 ms |

Comportamento:

- o callback do Principal reseta o watchdog apenas ao receber pacote válido
- pacote inválido por MAC, checksum, autenticação ou replay **não** reseta watchdog
- ao expirar:
  - `PIN_CLP_EMERGENCIA` vai para LOW
  - `PIN_CLP_SUBIR` e `PIN_CLP_DESCER` vão para HIGH
  - o estado remoto persistente é limpo
  - `link_ok` passa a 0 no `PacoteStatus`
- ao recuperar comunicação:
  - `PIN_CLP_EMERGENCIA` volta para HIGH se não houver emergência remota ativa
  - o LED LINK do Principal volta ao estado fixo

---

## 5. Homem-Morto e Movimento

### 5.1 Remote

- `SUBIR` e `DESCER` são botões de hold
- o Remote reenviará o comando imediatamente em mudança de estado e depois a cada 100 ms
- ao soltar o botão, o próximo pacote deixa de carregar hold ativo

### 5.2 Principal

- `SUBIR` e `DESCER` são saídas sustentadas, não pulsos
- enquanto o hold remoto continuar válido e as condições de segurança estiverem satisfeitas, a saída correspondente permanece em LOW
- ao cessar o hold ou surgir bloqueio, a saída volta imediatamente para HIGH

### 5.3 Principal

O Principal não possui botões de teste local. Os sinais `SUBIR` e `DESCER` enviados ao CLP derivam apenas do hold recebido do Remote e das condições de segurança locais.

---

## 6. Proteções de Hardware Supervisionadas pelos ESP32

### 6.1 Micro do Freio

- conexão: `GPIO 14` no Principal com `INPUT_PULLUP`
- LOW = freio liberado
- HIGH = freio ativo, micro aberta ou cabo rompido
- o Principal retransmite esse estado em `micro_freio_ativa`
- `SUBIR`/`DESCER` remotos são bloqueados no Principal enquanto `micro_freio_ativa == 1`

### 6.2 Fim de Curso de Descida

- conexão: `GPIO 36` no Remote
- LOW = sensor acionado
- debounce: 20 ms
- após a liberação física, o firmware mantém bloqueio lógico por 10 s
- durante esse período, `fim_curso_descida = 1` continua sendo enviado ao Principal
- o Principal replica esse estado em `PIN_CLP_FIM_CURSO`

---

## 7. Invariantes

Estas condições devem permanecer verdadeiras no firmware atual:

1. watchdog expirado implica `PIN_CLP_EMERGENCIA = LOW`
2. watchdog expirado implica `PIN_CLP_SUBIR = HIGH` e `PIN_CLP_DESCER = HIGH`
3. `SUBIR` e `DESCER` nunca ficam ativos ao mesmo tempo
4. emergência local do Remote impede envio de `SUBIR` e `DESCER`
5. `EMERGENCIA_ATIVA` reportada pelo CLP impede movimento remoto
6. `micro_freio_ativa == 1` impede movimento remoto no Principal
7. `MOTOR_ATIVO == 1` impede movimento remoto no Principal
8. pacotes inválidos não atualizam estado de link nem resetam watchdog
