# PROGRESS.md — Registro de Execução

## 2026-03-16 - T-001 - Criar projeto PlatformIO do Módulo Principal

- Outcome: Projeto PlatformIO criado em `principal/` com `platformio.ini` configurado para ESP32 (board: esp32dev, framework: arduino). Estrutura de diretórios conforme IMPLEMENTATION_PLAN §2.1 (`src/`, `include/`, `lib/`, `test/`). Arquivo `principal.cpp` com setup/loop mínimos.
- Files changed: `principal/platformio.ini`, `principal/src/principal.cpp`, `.gitignore`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-002 segue o mesmo padrão para o Módulo Remote. Reutilizar a mesma configuração de `platformio.ini` com ajustes de `build_flags` (`-DREMOTE`).
