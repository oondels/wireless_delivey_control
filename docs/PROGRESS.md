# PROGRESS.md — Registro de Execução

## 2026-03-16 - T-001 - Criar projeto PlatformIO do Módulo Principal

- Outcome: Projeto PlatformIO criado em `principal/` com `platformio.ini` configurado para ESP32 (board: esp32dev, framework: arduino). Estrutura de diretórios conforme IMPLEMENTATION_PLAN §2.1 (`src/`, `include/`, `lib/`, `test/`). Arquivo `principal.cpp` com setup/loop mínimos.
- Files changed: `principal/platformio.ini`, `principal/src/principal.cpp`, `.gitignore`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-002 segue o mesmo padrão para o Módulo Remote. Reutilizar a mesma configuração de `platformio.ini` com ajustes de `build_flags` (`-DREMOTE`).

## 2026-03-16 - T-002 - Criar projeto PlatformIO do Módulo Remote

- Outcome: Projeto PlatformIO criado em `remote/` com `platformio.ini` configurado para ESP32 (board: esp32dev, framework: arduino, flag `-DREMOTE`). Estrutura de diretórios (`src/`, `include/`, `lib/`, `test/`). Arquivo `remote.cpp` com setup/loop mínimos.
- Files changed: `remote/platformio.ini`, `remote/src/remote.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-003 define pinout.h do Principal — 15 GPIOs (8 entradas + 7 saídas). Consultar `hardware_io/SPEC.md` §8 para restrições de strapping pins.

## 2026-03-16 - T-003 - Definir pinout.h do Módulo Principal

- Outcome: Mapeamento de 15 GPIOs definido em `pinout.h`. Entradas: SUBIR(36), DESCER(39), VEL1(34), VEL2(35), VEL3(32), EMERGÊNCIA(33), REARME(25), FIM_DE_CURSO(26). Saídas: DIR_A(4), DIR_B(16), VEL1(17), VEL2(5), VEL3(18), FREIO(19), LED_LINK(21). Strapping pins evitados para entradas críticas. Pinos input-only (34-39) usados apenas para botões.
- Files changed: `principal/include/pinout.h`, `principal/src/principal.cpp` (include adicionado)
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-004 define pinout.h do Remote — 13 GPIOs (6 entradas + 7 saídas). Mesmas restrições de strapping pins. Evitar conflito com pinos já usados no Principal (módulos separados, mas manter consistência).

## 2026-03-16 - T-004 - Definir pinout.h do Módulo Remote

- Outcome: Mapeamento de 13 GPIOs definido em `pinout.h`. Entradas consistentes com Principal: SUBIR(36), DESCER(39), VEL1(34), VEL2(35), VEL3(32), EMERGÊNCIA(33). Saídas (LEDs dedicados): LINK(4), MOTOR(16), VEL1(17), VEL2(5), VEL3(18), EMERGÊNCIA(19), ALARME(21). Documentação atualizada em README.md §5.2, hardware_io/SPEC.md §6-7 e DESIGN_SPEC.md §9.1.
- Files changed: `remote/include/pinout.h`, `remote/src/remote.cpp`, `README.md`, `docs/specs/hardware_io/SPEC.md`, `docs/specs/DESIGN_SPEC.md`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: README.md §5.2, hardware_io/SPEC.md §6-7, DESIGN_SPEC.md §9.1.
- Notes for next task: T-005 cria protocolo.h compartilhado com structs PacoteRemote/PacoteStatus, enums Comando/EstadoSistema e função calcular_checksum(). Arquivo idêntico em principal/ e remote/. Ref: comunicacao/SPEC.md §4-6.

## 2026-03-16 - T-005 - Criar protocolo.h compartilhado

- Outcome: protocolo.h criado com structs PacoteRemote (8 bytes, packed) e PacoteStatus (5 bytes, packed), enums Comando (0-5) e EstadoSistema (0-4), função inline calcular_checksum() (XOR), e constantes de timing (HEARTBEAT 200ms, WATCHDOG 500ms, STATUS 200ms). Arquivo idêntico em ambos os módulos.
- Files changed: `principal/include/protocolo.h`, `remote/include/protocolo.h`
- Validations: `platformio run` em ambos — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-006 implementa freio.h/freio.cpp no Principal — funções acionar_freio() e liberar_freio() usando PIN_RELE_FREIO (GPIO 19). Ref: motor/SPEC.md §4.

## 2026-03-16 - T-006 - Implementar freio.h / freio.cpp

- Outcome: Módulo de freio criado com freio_init() (configura GPIO + aciona freio como estado padrão), acionar_freio() (HIGH) e liberar_freio() (LOW). Sem leitura de sensor conforme spec.
- Files changed: `principal/include/freio.h`, `principal/src/freio.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-007 implementa sensores.h/sensores.cpp — leitura do fim de curso com debounce 20ms via millis(). Ref: motor/SPEC.md §5.

## 2026-03-16 - T-007 - Implementar sensores.h / sensores.cpp

- Outcome: Módulo de sensores com debounce 20ms via millis(). fim_de_curso_acionado() retorna true quando GPIO LOW estável por 20ms (pull-up externo). Não-bloqueante.
- Files changed: `principal/include/sensores.h`, `principal/src/sensores.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-008 implementa lógica de emergência — flag emergencia_ativa, leitura do botão EMERGÊNCIA (nível contínuo), ativação imediata. Ref: seguranca/SPEC.md §3.

## 2026-03-16 - T-008 - Implementar lógica de emergência

- Outcome: Módulo emergencia.h/cpp com flag volatile emergencia_ativa (global), emergencia_verificar() que checa botão local (LOW=ativo) e emergência do Remote, emergencia_botao_local_ativo(). Flag nunca limpa automaticamente — invariante §9.3.
- Files changed: `principal/include/emergencia.h`, `principal/src/emergencia.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-009 implementa rearme — botão REARME (pulso) limpa emergencia_ativa e FALHA_COMUNICACAO. Caso especial: se pacote_remote.emergencia == 1, seta rearme_ativo = 1. Ref: seguranca/SPEC.md §3.4-3.5.

## 2026-03-16 - T-009 - Implementar lógica de rearme

- Outcome: Módulo rearme.h/cpp com detecção de borda de descida (pulso, debounce 50ms). Rearme bloqueado se botão EMERGÊNCIA local ativo. Limpa emergencia_ativa, retorna PARADO. Caso especial: seta rearme_ativo se emergência Remote ainda travada.
- Files changed: `principal/include/rearme.h`, `principal/src/rearme.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-010 implementa watchdog — verificação millis() - ultimo_pacote_remote > 500ms, transição para FALHA_COMUNICACAO. Ref: seguranca/SPEC.md §4.

## 2026-03-16 - T-010 - Implementar watchdog de comunicação

- Outcome: Módulo watchdog_comm.h/cpp com timestamp volatile ultimo_pacote_remote_ms, watchdog_comm_resetar() e watchdog_comm_expirado() usando WATCHDOG_TIMEOUT_MS (500ms de protocolo.h).
- Files changed: `principal/include/watchdog_comm.h`, `principal/src/watchdog_comm.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-011 implementa comunicação ESP-NOW no Principal — WiFi + esp_now init, peer register, OnDataRecv (checksum + watchdog reset + emergência), envio de PacoteStatus. Ref: comunicacao/SPEC.md §7-9.
