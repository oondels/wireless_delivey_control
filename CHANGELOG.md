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
