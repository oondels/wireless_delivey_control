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
