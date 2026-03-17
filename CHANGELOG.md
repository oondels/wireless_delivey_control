# Changelog

Todas as mudanças relevantes do projeto são documentadas neste arquivo.

## [Unreleased]

### Adicionado
- Projeto PlatformIO do Módulo Principal (`principal/`) com estrutura de diretórios e build funcional para ESP32
- Projeto PlatformIO do Módulo Remote (`remote/`) com estrutura de diretórios e build funcional para ESP32
- Módulos do Principal: Freio, Sensores, Emergencia, Rearme, WatchdogComm, Motor, Velocidade, Botoes, Comunicacao

### Refatorado
- Todos os módulos do Principal convertidos de C procedural para C++ orientado a objetos (classes encapsuladas, sem variáveis globais/static de módulo)
