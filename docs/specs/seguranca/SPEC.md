# Especificação de Segurança e Emergência (Fail-Safe)

**Versão:** 1.5  
**Data:** 2026-04-24  
**Referência:** README.md v4.0

---

## 1. Princípio Fundamental

Na arquitetura atual, o CLP executa a lógica de potência e segurança. Os ESP32 atuam como ponte de comunicação e devem falhar para um estado seguro:

- perda curta de comunicação Remote → Principal bloqueia movimento; perda prolongada pode acionar `PIN_CLP_EMERGENCIA` conforme configuração
- comandos de movimento são interrompidos imediatamente quando condições de operação deixam de ser válidas
- o Remote nunca assume que pode mover apenas pelo botão local; ele depende do status válido vindo do Principal
- o Repeater opcional não tem autoridade sobre movimento, emergência, freio ou fim de curso

Com `ONLY_EMERGENCY_MODE=true`, movimento e velocidade pertencem a outro dispositivo. Nesse modo, perda de link apenas reporta link inativo; emergência já ativa permanece travada no Principal até chegada de pacote válido com o botão liberado.

---

## 2. Prioridades de Bloqueio

### 2.1 No Principal

Para aceitar movimento remoto (`SUBIR`/`DESCER`), o firmware exige simultaneamente:

1. watchdog de comunicação **não expirado**
2. `emergencia` remota **inativa**
3. feedback `EMERGENCIA_ATIVA` do CLP **inativo**
4. `micro_freio_ativa == 0`

O feedback `MOTOR_ATIVO` é telemetria para LED/diagnóstico e não bloqueia o acionamento remoto, para evitar oscilação quando o próprio comando remoto faz o CLP ativar esse feedback.

Se qualquer uma dessas condições falhar:

- `PIN_CLP_SUBIR` e `PIN_CLP_DESCER` vão para HIGH
- o hold remoto é bloqueado
- a perda curta de watchdog força apenas parada de movimento; emergência por perda de sinal depende de timeout configurável

### 2.2 No Remote

O Remote bloqueia localmente `SUBIR` e `DESCER` quando qualquer condição abaixo ocorre:

1. status do Principal expirado
2. `link_ok == 0`
3. nenhuma rota ESP-NOW está operacional
4. emergência local ativa
5. `emergencia_ativa == 1` no `PacoteStatus`

Os comandos de pulso (`VEL1`, `VEL2`, `RESET`) continuam podendo ser transmitidos mesmo com bloqueio de movimento.

---

## 3. Emergência

### 3.0 Modo Somente Emergência

Quando `ONLY_EMERGENCY_MODE=true`, os ESP32 atuam apenas como ponte sem fio de emergência:

- o Remote lê somente o botão de emergência NC e transmite heartbeat com `emergencia`
- o Principal ignora comandos de movimento, velocidade, reset e fim de curso
- todos os sinais de controle ao CLP permanecem em HIGH, exceto `PIN_CLP_EMERGENCIA`
- perda de link não cria nova emergência; apenas reporta `link_ok = 0`
- emergência já ativa permanece em LOW se o link cair, até chegar pacote válido liberando o botão

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
- LOW quando a perda de sinal excede `SIGNAL_LOSS_EMERGENCY_TIMEOUT_MS` e `ENABLE_SIGNAL_LOSS_EMERGENCY=true`
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
| Timeout de emergência por perda de sinal | `SIGNAL_LOSS_EMERGENCY_TIMEOUT_MS` (padrão 5000 ms) |
| Probe de qualidade de rota | 250 ms |

Comportamento:

- o callback do Principal reseta o watchdog apenas ao receber pacote válido
- pacote inválido por MAC, checksum, autenticação ou replay **não** reseta watchdog
- `PKT_LINK_PROBE`, `PKT_LINK_ACK` e `PacoteStatus` não resetam watchdog
- ao expirar:
  - `PIN_CLP_SUBIR` e `PIN_CLP_DESCER` vão para HIGH
  - o estado remoto persistente é limpo
  - `link_ok` passa a 0 no `PacoteStatus`
- se a perda persistir além de `SIGNAL_LOSS_EMERGENCY_TIMEOUT_MS` e `ENABLE_SIGNAL_LOSS_EMERGENCY=true`:
  - `PIN_CLP_EMERGENCIA` vai para LOW
- em `ONLY_EMERGENCY_MODE=true`:
  - perda de link não cria nova emergência
  - `PIN_CLP_EMERGENCIA` preserva o último estado de emergência remota recebido
- ao recuperar comunicação:
  - `PIN_CLP_EMERGENCIA` volta para HIGH se não houver emergência remota ativa
  - o LED LINK do Principal volta ao estado fixo

Com Repeater habilitado:

- se a rota direta degradar, o Remote pode trocar preventivamente para `ROUTE_VIA_REPEATER`
- perda de `Principal -> Remote` direto não é falha global se comandos válidos continuarem chegando por `Remote -> Repeater -> Principal` e status válido continuar chegando por `Principal -> Repeater -> Remote`
- quando a rota ativa está via Repeater, o Principal mantém o status operacional via Repeater e testa a rota direta apenas no intervalo configurado
- se o Repeater cair, o Remote volta para direto se essa rota estiver operacional
- se nenhuma rota estiver operacional, o Remote bloqueia `SUBIR`/`DESCER`
- o Principal continua entrando em fail-safe se não receber `PKT_REMOTE_CMD` válido dentro de 500 ms

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

- funcionalidade temporariamente desabilitada nesta versão
- o campo `fim_curso_descida` permanece reservado no protocolo
- o Remote envia `fim_curso_descida = 0` em todos os pacotes
- o Principal ignora esse campo e mantém `PIN_CLP_FIM_CURSO` em HIGH
- a implementação de debounce e retenção pós-liberação fica preservada para reativação futura

---

## 7. Invariantes

Estas condições devem permanecer verdadeiras no firmware atual:

1. watchdog expirado implica `PIN_CLP_SUBIR = HIGH` e `PIN_CLP_DESCER = HIGH`
2. perda prolongada de sinal implica `PIN_CLP_EMERGENCIA = LOW` apenas se `ENABLE_SIGNAL_LOSS_EMERGENCY=true`
3. `SUBIR` e `DESCER` nunca ficam ativos ao mesmo tempo
4. emergência local do Remote impede envio de `SUBIR` e `DESCER`
5. `EMERGENCIA_ATIVA` reportada pelo CLP impede movimento remoto
6. `micro_freio_ativa == 1` impede movimento remoto no Principal
7. `MOTOR_ATIVO` não interfere no acionamento remoto no Principal
8. pacotes inválidos não atualizam estado de link nem resetam watchdog
9. Repeater nunca gera comando, nunca mantém último comando e nunca reseta watchdog do Principal por conta própria
