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
