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

- [ ] T-012: Implementar `motor.h` / `motor.cpp` — funções `acionar_motor(direcao)` e `desligar_motor()`, exclusividade mútua dos relés DIREÇÃO A/B, dead-time 100 ms na inversão via `millis()` (nunca `delay()`). Ref: `motor/SPEC.md` §2
- [ ] T-013: Implementar controle de velocidade no Principal — relés VEL1/2/3 com exclusividade mútua (desacionar anterior → acionar novo), variável `velocidade_atual` (padrão VEL1 na inicialização), incluir no campo `velocidade` do PacoteStatus. Ref: `motor/SPEC.md` §3
- [ ] T-014: Implementar leitura de botões do Principal — debounce 50 ms para todos, SUBIR/DESCER lidos como hold (nível), VEL1/2/3/REARME como pulso (borda de descida), EMERGÊNCIA como nível contínuo (trava). Ref: `hardware_io/SPEC.md` §4

## Fase 2 — Firmware: Principal — Máquina de Estados e Integração

- [ ] T-015: Implementar `maquina_estados.h` / `maquina_estados.cpp` — função `atualizar_maquina_estados()` com avaliação sequencial por prioridade: 1) emergência, 2) watchdog, 3) fim de curso, 4) movimentação (hold + direção), 5) padrão → PARADO. Ref: `maquina_estados/SPEC.md` §4–6
- [ ] T-016: Implementar `leds.h` / `leds.cpp` do Principal — struct `Led`, API (`led_ligar`, `led_desligar`, `led_piscar`, `led_atualizar`), piscar não-bloqueante via `millis()`. No Principal, usado apenas para LED LINK REMOTE (aceso = watchdog OK, apagado = watchdog expirado). Ref: `leds/SPEC.md` §4.2, §5
- [ ] T-017: Implementar `principal.ino` — loop principal integrando todos os módulos: ler botões → atualizar máquina de estados → enviar status → atualizar LEDs. Setup: inicializar GPIOs, ESP-NOW, estado inicial PARADO, freio acionado. Ref: IMPLEMENTATION_PLAN §2.6

## Fase 2 — Firmware: Remote

- [ ] T-018: Implementar `leds.h` / `leds.cpp` do Remote — mesma abstração struct `Led` e API do Principal, instanciar 7 LEDs (LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME). Frequências: LINK 1 Hz (500 ms), ALARME 2 Hz (250 ms), EMERGÊNCIA 4 Hz (125 ms). Ref: `leds/SPEC.md` §3, §5–6
- [ ] T-019: Implementar comunicação ESP-NOW no Remote — inicialização WiFi + esp_now, registro de peer (MAC do Principal fixo), envio de `PacoteRemote` (heartbeat 200 ms + imediato em mudança), callback `OnDataRecv` para atualizar estado local com `PacoteStatus`. Ref: `comunicacao/SPEC.md` §7, §9.1
- [ ] T-020: Implementar leitura de botões do Remote — debounce 50 ms, SUBIR/DESCER (hold), VEL1/2/3 (pulso), EMERGÊNCIA (nível contínuo — trava mecânica). Montar `PacoteRemote` com campos `comando`, `botao_hold`, `emergencia`. Ref: `hardware_io/SPEC.md` §6
- [ ] T-021: Implementar lógica de atualização de LEDs do Remote — baseada em campos do `PacoteStatus` recebido: LINK (fixo/piscar por timeout), MOTOR (SUBINDO/DESCENDO), VEL1/2/3 (campo velocidade), EMERGÊNCIA (piscar 4 Hz / fixo por falha), ALARME (piscar 2 Hz se rearme_ativo E botão local travado). Ref: `leds/SPEC.md` §3.2
- [ ] T-022: Implementar `remote.ino` — loop principal: ler botões → montar pacote → enviar se necessário → atualizar LEDs com base no último status recebido. Setup: inicializar GPIOs, ESP-NOW, LEDs desligados. Ref: IMPLEMENTATION_PLAN §2.5
