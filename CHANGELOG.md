# Changelog

Todas as mudanças relevantes do projeto são documentadas neste arquivo.

## [Unreleased]

### Adicionado
- Projeto PlatformIO do Módulo Principal (`principal/`) com estrutura de diretórios e build funcional para ESP32
- Projeto PlatformIO do Módulo Remote (`remote/`) com estrutura de diretórios e build funcional para ESP32
- Módulos do Principal: Freio, Sensores, Emergencia, Rearme, WatchdogComm, Motor, Velocidade, Botoes, Comunicacao
- Modo degradado local no Principal após REARME em `FALHA_COMUNICACAO`, permitindo `SUBIR`/`DESCER` local sem link com Remote

### Refatorado
- Todos os módulos do Principal convertidos de C procedural para C++ orientado a objetos (classes encapsuladas, sem variáveis globais/static de módulo)

### Alterado
- Em perda de comunicação, fail-safe imediato mantido (motor OFF + freio ON), com desbloqueio apenas por REARME manual
- Comandos do Remote passam a ser ignorados durante watchdog expirado; controle local permanece disponível em modo degradado até recuperação do link
- Logs do Principal refinados para reduzir spam e registrar transições de comandos remotos/bloqueios por estado
