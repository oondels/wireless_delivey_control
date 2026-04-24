# Changelog

Todas as mudanças relevantes do projeto são documentadas neste arquivo.

## [Unreleased]

### feat(comunicacao): adiciona feedback do CLP e micro do freio ao status do remote

- `PacoteStatus` ampliado para carregar `link_ok`, `motor_ativo`, `emergencia_ativa`, `vel1_ativa`, `vel2_ativa` e `micro_freio_ativa`
- Módulo Principal passa a ler 4 feedbacks do CLP via `INPUT_PULLUP`: `GPIO 23` (motor), `25` (emergência), `26` (VEL1) e `27` (VEL2)
- Módulo Principal passa a ler a micro do freio NC no `GPIO 14`, com `HIGH` representando micro aberta/acionada
- Status do Principal agora é enviado ao Remote a cada `200 ms` e também imediatamente quando algum feedback muda
- Remote passa a bloquear `SUBIR` e `DESCER` quando o status do Principal expira ou quando o CLP reporta emergência ativa
- LEDs `MOTOR`, `VEL1` e `VEL2` do Remote passam a refletir o feedback do CLP, não mais o estado local
- `README.md` atualizado com diagrama Mermaid do fluxo de comunicação, novo protocolo e novo pinout do Principal

### feat(principal): botões de teste local para acionar CLP sem Remote

- Adicionados dois botões de teste físico no Módulo Principal (GPIO 32 = TESTE SUBIR, GPIO 33 = TESTE DESCER), configurados com `INPUT_PULLUP`
- Quando pressionados, resetam o watchdog interno para evitar emergência por timeout
- Acionam diretamente `PIN_CLP_SUBIR` / `PIN_CLP_DESCER` LOW enquanto mantidos pressionados
- Pacote do Remote tem prioridade — botões de teste só atuam quando nenhum pacote foi processado no ciclo
- Log via Serial com tag `[TESTE]` apenas na borda de pressionar (anti-spam)
- Documentação atualizada: `README.md` (§4.3 e §5.1), `docs/specs/hardware_io/SPEC.md` (§4, v1.5)



### Refatoração de Arquitetura — ESP32 como Bridge para CLP

**Motivação:** interferência eletromagnética do motor e do inversor (VFD) comprometia a operação
dos ESP32. Um CLP programado em Ladder passou a gerenciar toda a lógica de controle
(motor, freio, estados, segurança). Os ESP32 tornaram-se pontes de comunicação.

#### Módulo Principal — alterações

**Removido:**
- Toda a lógica de controle direto: classes `Motor`, `Freio`, `Velocidade`, `MaquinaEstados`,
  `Sensores`, `MonitorRede`, `Emergencia`, `Rearme`, `Botoes`
- Todas as entradas de GPIO: botões (SUBIR, DESCER, VEL1-3, EMERGÊNCIA, REARME)
  e sensores (fim de curso, microchave freio, monitor rede)

**Adicionado:**
- Bridge ESP→CLP: recebe `PacoteRemote` via ESP-NOW e replica sinais para entradas digitais do CLP
- Sete saídas GPIO para o CLP (lógica ativa em LOW/GND): `PIN_CLP_SUBIR` (GPIO 4),
  `PIN_CLP_DESCER` (GPIO 16), `PIN_CLP_VEL1` (GPIO 17), `PIN_CLP_VEL2` (GPIO 5),
  `PIN_CLP_EMERGENCIA` (GPIO 18), `PIN_CLP_RESET` (GPIO 19), `PIN_CLP_FIM_CURSO` (GPIO 22)
- Pulsos de `PULSO_CLP_MS` (50 ms) para sinais VEL1, VEL2 e RESET

**Alterado:**
- Fail-safe: watchdog timeout → `PIN_CLP_EMERGENCIA` LOW imediato (emergência enviada ao CLP);
  todos os sinais de movimento vão a HIGH (inativo)
- `PacoteStatus` simplificado: de 5 bytes para 2 bytes — campo único `link_ok`
  (CLP não fornece feedback ao ESP)

#### Módulo Remote — alterações

**Adicionado:**
- Botão RESET em GPIO 32 (substituiu VEL3); envia `CMD_RESET` ao Principal

**Removido:**
- Botão VEL3 e campo `vel3_pulso` em `EstadoBotoes`
- LED VEL3 (GPIO 18) — não utilizado nesta arquitetura

**Alterado:**
- LEDs agora refletem **estado local** (sem feedback do CLP):
  - MOTOR: aceso se SUBIR ou DESCER hold ativo + sem emergência local
  - VEL1/VEL2: velocidade selecionada localmente
  - EMERGÊNCIA: pisca 4 Hz se botão emergência ativo; fixo se link perdido > 500 ms
  - ALARME: pisca 2 Hz se link com Principal perdido
- `CMD_VEL3` renomeado para `CMD_RESET` (valor 5) no protocolo

---

### Adicionado
- Projeto PlatformIO do Módulo Principal (`principal/`) com estrutura de diretórios e build funcional para ESP32
- Projeto PlatformIO do Módulo Remote (`remote/`) com estrutura de diretórios e build funcional para ESP32
- Módulos do Principal: Freio, Sensores, Emergencia, Rearme, WatchdogComm, Motor, Velocidade, Botoes, Comunicacao
- Modo degradado local no Principal após REARME em `FALHA_COMUNICACAO`, permitindo `SUBIR`/`DESCER` local sem link com Remote

### Refatorado
- Todos os módulos do Principal convertidos de C procedural para C++ orientado a objetos (classes encapsuladas, sem variáveis globais/static de módulo)

### Corrigido
- Estado seguro do freio: `FREIO_ON` permanece ativo por padrão no boot e em qualquer estado de repouso; freio só é liberado ao acionar o motor ou entrar no modo manual
- Modo manual do freio — dois bugs de segurança:
  - Relé `FREIO_OFF` permanecia ativo mesmo após a microchave confirmar o fim de curso do cilindro, forçando contra o componente mecânico; agora o loop verifica o GPIO antes de acionar e chama `manualParar()` se o fim de curso já foi atingido
  - Emergência era ignorada durante o modo manual porque o bloco executava `return` antes da máquina de estados; adicionada verificação prévia que combina `emergencia.isAtiva()`, botão local e campo `emergencia` do último `PacoteRemote`
- Lógica de parada no modo manual ajustada; estado do freio sincronizado pela leitura da microchave ao sair do modo manual
- Compatibilidade do callback `OnDataRecv` com ESP-IDF < 5.0: adicionada guarda de pré-processador via `ESP_IDF_VERSION_MAJOR` para suportar as duas assinaturas do callback (corrige erros de build `esp_now_recv_info_t does not name a type`)

### Alterado
- Em perda de comunicação, fail-safe imediato mantido (motor OFF + freio ON), com desbloqueio apenas por REARME manual
- Comandos do Remote passam a ser ignorados durante watchdog expirado; controle local permanece disponível em modo degradado até recuperação do link
- Logs do Principal refinados para reduzir spam e registrar transições de comandos remotos/bloqueios por estado
- Freio migrado de relé único para solenoide de dupla bobina: canal `FREIO_ON` (GPIO 19, bobina de aplicação) e canal `FREIO_OFF` (GPIO 22, bobina de liberação); as duas bobinas nunca ficam ativas simultaneamente — troca sequencial com dead-time de ~10 ms garantida por firmware
- API da classe `Freio` atualizada: `acionar()` energiza `FREIO_ON` e desenergia `FREIO_OFF`; `liberar()` faz o inverso; LED de freio permanece associado apenas ao canal `FREIO_ON`
- Canais do módulo relé em uso no Principal: de 6 para 7 (de 8 disponíveis); GPIOs de saída: de 7 para 8; total de GPIOs do Principal: de 17 para 18
