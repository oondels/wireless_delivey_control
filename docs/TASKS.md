# TASKS.md

Referência: `docs/IMPLEMENTATION_PLAN.md` v3.2

## Formato

- `[ ]` = pendente (status: false)
- `[x]` = completo (status: true)
- `[BLOQUEADO]` = bloqueado após 3 tentativas consecutivas

---

## Fase 1 — Preparação do Ambiente

- [x] T-001: Criar projeto PlatformIO para o Módulo Principal (`principal/`) — `platformio.ini` configurado para ESP32, estrutura de diretórios conforme IMPLEMENTATION_PLAN §2.1
- [x] T-002: Criar projeto PlatformIO para o Módulo Remote (`remote/`) — `platformio.ini` configurado para ESP32, estrutura de diretórios conforme IMPLEMENTATION_PLAN §2.1
- [x] T-003: Definir `pinout.h` do Módulo Principal — mapear 15 GPIOs (8 entradas + 7 saídas), respeitando restrições de strapping pins (evitar GPIO 0, 2, 12, 15 para entradas críticas). Ref: `hardware_io/SPEC.md` §8
- [x] T-004: Definir `pinout.h` do Módulo Remote — mapear 13 GPIOs (6 entradas + 7 saídas), mesmas restrições de strapping pins. Ref: `hardware_io/SPEC.md` §8

## Fase 2 — Firmware: Protocolo Compartilhado

- [x] T-005: Criar `protocolo.h` com structs `PacoteRemote` (8 bytes) e `PacoteStatus` (5 bytes), enums `Comando` (0–5) e `EstadoSistema` (0–4), e função `calcular_checksum()` (XOR). Copiar arquivo idêntico em `principal/` e `remote/`. Ref: `comunicacao/SPEC.md` §4–6

## Fase 2 — Firmware: Principal — Segurança (implementar primeiro)

- [x] T-006: Implementar `freio.h` / `freio.cpp` — funções `acionar_freio()` (GPIO HIGH) e `liberar_freio()` (GPIO LOW). Sem leitura de sensor — microchave é hardware externo. Ref: `motor/SPEC.md` §4
- [x] T-007: Implementar `sensores.h` / `sensores.cpp` — leitura do fim de curso com debounce 20 ms via `millis()`. Função `fim_de_curso_acionado()` retorna estado filtrado. Ref: `motor/SPEC.md` §5
- [x] T-008: Implementar lógica de emergência no Principal — flag `emergencia_ativa`, leitura do botão EMERGÊNCIA (trava mecânica, nível contínuo), ativação imediata (motor OFF → freio ON → flag true). Ref: `seguranca/SPEC.md` §3
- [x] T-009: Implementar lógica de rearme no Principal — botão REARME (pulso) limpa `emergencia_ativa` e `FALHA_COMUNICACAO`; caso especial: se `pacote_remote.emergencia == 1` no momento do rearme, setar `rearme_ativo = 1` no PacoteStatus. Ref: `seguranca/SPEC.md` §3.4–3.5
- [x] T-010: Implementar watchdog de comunicação no Principal — constante `WATCHDOG_TIMEOUT_MS = 500`, verificação a cada ciclo do loop (`millis() - ultimo_pacote_remote > timeout`), transição para `FALHA_COMUNICACAO`. Ref: `seguranca/SPEC.md` §4

## Fase 2 — Firmware: Principal — Comunicação

- [x] T-011: Implementar comunicação ESP-NOW no Principal — inicialização WiFi + esp_now, registro de peer (MAC do Remote), callback `OnDataRecv` (validar checksum, resetar watchdog, processar emergência imediata se `emergencia == 1`), envio de `PacoteStatus` a cada 200 ms e imediato em mudança de estado. Ref: `comunicacao/SPEC.md` §7–9

## Fase 2 — Firmware: Principal — Motor e Velocidade

- [x] T-012: Implementar `motor.h` / `motor.cpp` — funções `acionar_motor(direcao)` e `desligar_motor()`, exclusividade mútua dos relés DIREÇÃO A/B, dead-time 100 ms na inversão via `millis()` (nunca `delay()`). Ref: `motor/SPEC.md` §2
- [x] T-013: Implementar controle de velocidade no Principal — relés VEL1/2/3 com exclusividade mútua (desacionar anterior → acionar novo), variável `velocidade_atual` (padrão VEL1 na inicialização), incluir no campo `velocidade` do PacoteStatus. Ref: `motor/SPEC.md` §3
- [x] T-014: Implementar leitura de botões do Principal — debounce 50 ms para todos, SUBIR/DESCER lidos como hold (nível), VEL1/2/3/REARME como pulso (borda de descida), EMERGÊNCIA como nível contínuo (trava). Ref: `hardware_io/SPEC.md` §4

## Fase 2 — Firmware: Principal — Máquina de Estados e Integração

- [x] T-015: Implementar classe `MaquinaEstados` (`maquina_estados.h` / `maquina_estados.cpp`) — método `atualizar()` recebe referências das dependências (`Emergencia&`, `WatchdogComm&`, `Sensores&`, `Motor&`, `Freio&`, `Botoes&`) e avalia prioridade sequencial: 1) `emergencia.verificar()`, 2) `watchdog.expirado()`, 3) `sensores.fimDeCursoAcionado()`, 4) movimentação (hold + `motor.acionar(dir)`), 5) padrão → PARADO. Retorna `EstadoSistema`. Ref: `maquina_estados/SPEC.md` §4–6
- [x] T-016: Implementar classe `Led` (`leds.h` / `leds.cpp`) do Principal — construtor `Led(uint8_t gpio)`, métodos `ligar()`, `desligar()`, `piscar(uint16_t intervalo_ms)`, `atualizar()`. Piscar não-bloqueante via `millis()`. No Principal, instanciar apenas `Led ledLink(PIN_LED_LINK)` (aceso = watchdog OK, apagado = watchdog expirado). Ref: `leds/SPEC.md` §4.2, §5
- [x] T-017: Implementar `principal.cpp` — loop principal integrando todas as classes: instanciar objetos (`Freio`, `Sensores`, `Emergencia`, `Rearme`, `WatchdogComm`, `Motor`, `Velocidade`, `Botoes`, `Comunicacao`, `Led`, `MaquinaEstados`), `setup()` chama `init()` de cada módulo, `loop()` executa: ler botões → atualizar máquina de estados → processar velocidade → enviar status → atualizar LEDs. Estado inicial PARADO, freio acionado. Ref: IMPLEMENTATION_PLAN §2.6

## Fase 2 — Firmware: Remote

- [x] T-018: Implementar classe `Led` (`leds.h` / `leds.cpp`) do Remote — mesma classe `Led` do Principal (copiar ou compartilhar), instanciar 7 objetos: `Led ledLink(PIN_LED_LINK)`, `Led ledMotor(PIN_LED_MOTOR)`, `Led ledVel1(PIN_LED_VEL1)`, `Led ledVel2(PIN_LED_VEL2)`, `Led ledVel3(PIN_LED_VEL3)`, `Led ledEmergencia(PIN_LED_EMERGENCIA)`, `Led ledAlarme(PIN_LED_ALARME)`. Frequências: LINK 1 Hz (500 ms), ALARME 2 Hz (250 ms), EMERGÊNCIA 4 Hz (125 ms). Ref: `leds/SPEC.md` §3, §5–6
- [x] T-019: Implementar classe `Comunicacao` do Remote (`comunicacao.h` / `comunicacao.cpp`) — inicialização WiFi + esp_now, registro de peer (MAC do Principal fixo), método `enviarPacote(const PacoteRemote&)` (heartbeat 200 ms + imediato em mudança), callback estático `onDataRecv()` para atualizar estado local com `PacoteStatus`. Ref: `comunicacao/SPEC.md` §7, §9.1
- [x] T-020: Implementar classe `Botoes` do Remote (`botoes.h` / `botoes.cpp`) — debounce 50 ms, SUBIR/DESCER (hold), VEL1/2/3 (pulso), EMERGÊNCIA (nível contínuo — trava mecânica). Método `ler()` retorna `EstadoBotoes`. Montagem do `PacoteRemote` com campos `comando`, `botao_hold`, `emergencia` feita no loop principal. Ref: `hardware_io/SPEC.md` §6
- [x] T-021: Implementar função `atualizarLeds()` do Remote — recebe `PacoteStatus` e atualiza os 7 objetos `Led`: `ledLink` (fixo/piscar por timeout 1000 ms), `ledMotor` (SUBINDO/DESCENDO), `ledVel1/2/3` (campo velocidade), `ledEmergencia` (piscar 4 Hz se EMERGENCIA / fixo se FALHA_COMUNICACAO), `ledAlarme` (piscar 2 Hz se `rearme_ativo == 1` E `digitalRead(PIN_BTN_EMERGENCIA) == HIGH`). Chamar `atualizar()` de cada Led. Ref: `leds/SPEC.md` §3.2
- [x] T-022: Implementar `remote.cpp` — loop principal integrando classes: instanciar objetos (`Botoes`, `Comunicacao`, 7x `Led`), `setup()` inicializa GPIOs, ESP-NOW e LEDs desligados, `loop()` executa: `botoes.ler()` → montar `PacoteRemote` → `comunicacao.enviarPacote()` se necessário → `atualizarLeds(ultimoStatus)`. Ref: IMPLEMENTATION_PLAN §2.5
