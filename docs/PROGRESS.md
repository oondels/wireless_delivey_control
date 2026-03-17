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

## 2026-03-16 - T-011 - Implementar comunicação ESP-NOW no Principal

- Outcome: Módulo comunicacao.h/cpp com WiFi STA + esp_now_init, registro de peer (MAC placeholder), callback on_data_recv (valida checksum, reseta watchdog, ativa emergência imediata se emergencia==1), comunicacao_enviar_status() com checksum calculado antes do envio. Variáveis voláteis: ultimo_pacote_remote, novo_pacote_recebido.
- Files changed: `principal/include/comunicacao.h`, `principal/src/comunicacao.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-012 implementa motor.h/cpp — acionar_motor(direcao) e desligar_motor(), exclusividade mútua dos relés DIREÇÃO A/B, dead-time 100ms via millis(). Ref: motor/SPEC.md §2. NOTA: MAC do Remote é placeholder (0xFF) — será definido no deploy.

## 2026-03-16 - T-012 - Implementar motor.h / motor.cpp

- Outcome: Módulo motor com acionar_motor(dir), desligar_motor(), motor_direcao_atual(), motor_em_dead_time(). Dead-time 100ms não-bloqueante via millis() com direção pendente. Exclusividade mútua garantida. Enum Direcao (NENHUMA, SUBIR, DESCER).
- Files changed: `principal/include/motor.h`, `principal/src/motor.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-013 implementa velocidade — relés VEL1/2/3 com exclusividade mútua, velocidade_atual (padrão VEL1), incluir no PacoteStatus. Ref: motor/SPEC.md §3.

## 2026-03-16 - T-013 - Implementar controle de velocidade

- Outcome: Módulo velocidade.h/cpp com velocidade_selecionar(nivel), velocidade_atual(). Exclusividade mútua: desaciona anterior antes de acionar novo. Padrão VEL1 na init.
- Files changed: `principal/include/velocidade.h`, `principal/src/velocidade.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-014 implementa leitura de botões do Principal — debounce 50ms, SUBIR/DESCER hold (nível), VEL1/2/3/REARME pulso (borda descida), EMERGÊNCIA nível contínuo. Ref: hardware_io/SPEC.md §4.

## 2026-03-16 - T-014 - Implementar leitura de botões do Principal

- Outcome: Módulo botoes.h/cpp com struct EstadoBotoes e botoes_ler(). Debounce 50ms genérico para 5 botões. SUBIR/DESCER retornam hold (nível LOW=pressionado). VEL1/2/3 retornam pulso (borda HIGH→LOW). EMERGÊNCIA e REARME tratados em módulos dedicados.
- Files changed: `principal/include/botoes.h`, `principal/src/botoes.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-015 implementa máquina de estados — atualizar_maquina_estados() com prioridade: emergência > watchdog > fim de curso > movimentação > PARADO. Ref: maquina_estados/SPEC.md §4-6.

## 2026-03-16 - T-015 - Implementar classe MaquinaEstados

- Outcome: Classe MaquinaEstados com método atualizar() que recebe referências de todas as dependências (Emergencia, WatchdogComm, Sensores, Motor, Freio) e EstadoBotoes/PacoteRemote. Avaliação sequencial por prioridade: 1) emergencia.verificar(), 2) watchdog.expirado(), 3) sensores.fimDeCursoAcionado(), 4) movimentação com hold local (prioridade) ou Remote, 5) PARADO. Fim de curso bloqueia apenas SUBIR. Invariantes §8 respeitadas (Motor OFF → Freio ON, Motor ON → Freio OFF).
- Files changed: `principal/include/maquina_estados.h`, `principal/src/maquina_estados.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-016 implementa classe Led para o Principal — construtor com GPIO, métodos ligar/desligar/piscar/atualizar, piscar não-bloqueante via millis(). Apenas LED LINK REMOTE no Principal. Ref: leds/SPEC.md §4.2, §5.

## 2026-03-17 - T-016 - Implementar classe Led do Principal

- Outcome: Classe Led implementada com construtor Led(uint8_t gpio), métodos ligar() (HIGH, piscando=false), desligar() (LOW, piscando=false), piscar(uint16_t intervalo_ms) (não-bloqueante via millis()), atualizar() (toggle no intervalo). No Principal, apenas LED LINK REMOTE usa esta abstração — LEDs compartilhados com relés não utilizam.
- Files changed: `principal/include/leds.h`, `principal/src/leds.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-017 implementa principal.cpp — loop principal integrando todas as classes. Instanciar todos os objetos, setup() chama init() de cada módulo, loop() executa: ler botões → atualizar máquina de estados → processar velocidade → enviar status → atualizar LEDs. Estado inicial PARADO, freio acionado. Ref: IMPLEMENTATION_PLAN §2.6.

## 2026-03-17 - T-017 - Implementar principal.cpp

- Outcome: Loop principal completo integrando 11 módulos. setup() inicializa todos na ordem correta (freio primeiro para estado seguro). loop() executa: 1) botoes.ler(), 2) rearme.verificar(), 3) maquinaEstados.atualizar(), 4) processar velocidade (local + Remote), 5) montar/enviar PacoteStatus (200ms periódico + imediato em mudança), 6) atualizar LED LINK. Também limpa rearme_ativo quando Remote solta emergência.
- Files changed: `principal/src/principal.cpp`
- Validations: `platformio run` — build com sucesso, 0 erros, 0 warnings. RAM 13.3%, Flash 56.0%.
- Docs updated: nenhum.
- Notes for next task: T-018 implementa classe Led do Remote — mesma classe Led, instanciar 7 objetos (LINK, MOTOR, VEL1-3, EMERGÊNCIA, ALARME). Frequências: LINK 1Hz (500ms), ALARME 2Hz (250ms), EMERGÊNCIA 4Hz (125ms). Copiar leds.h/leds.cpp para remote/. Ref: leds/SPEC.md §3, §5-6.

## 2026-03-17 - T-018 - Implementar classe Led do Remote

- Outcome: Classe Led copiada identicamente de principal/ para remote/include/leds.h e remote/src/leds.cpp. Mesma API: ligar(), desligar(), piscar(intervalo_ms), atualizar(). As 7 instâncias (LINK, MOTOR, VEL1-3, EMERGÊNCIA, ALARME) serão criadas no remote.cpp (T-022).
- Files changed: `remote/include/leds.h`, `remote/src/leds.cpp`
- Validations: `platformio run` (remote) — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-019 implementa Comunicacao do Remote — WiFi + ESP-NOW init, peer MAC do Principal fixo, enviarPacote() (heartbeat 200ms + imediato), callback onDataRecv() para atualizar estado local com PacoteStatus. Ref: comunicacao/SPEC.md §7, §9.1.

## 2026-03-17 - T-019 - Implementar Comunicacao do Remote

- Outcome: Classe Comunicacao com init() (WiFi STA + ESP-NOW + peer MAC placeholder 0xFF), enviarPacote() (checksum XOR), callback onDataRecv() (valida checksum + copia PacoteStatus + registra timestamp). ultimoStatusRecebidoMs() expõe timestamp para timeout do LED LINK.
- Files changed: `remote/include/comunicacao.h`, `remote/src/comunicacao.cpp`
- Validations: `platformio run` (remote) — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-020 implementa Botoes do Remote — debounce 50ms, SUBIR/DESCER (hold), VEL1/2/3 (pulso), EMERGÊNCIA (nível contínuo). Método ler() retorna EstadoBotoes. Ref: hardware_io/SPEC.md §6.

## 2026-03-17 - T-020 - Implementar Botoes do Remote

- Outcome: Classe Botoes com 6 entradas (vs 5 do Principal). EstadoBotoes inclui campo emergencia (nível contínuo, LOW=ativo). Debounce 50ms genérico via millis(). Mesma lógica de hold/pulso do Principal.
- Files changed: `remote/include/botoes.h`, `remote/src/botoes.cpp`
- Validations: `platformio run` (remote) — build com sucesso, 0 erros, 0 warnings.
- Docs updated: nenhum.
- Notes for next task: T-021 implementa atualizarLeds() do Remote — recebe PacoteStatus, atualiza 7 LEDs conforme spec. LINK (fixo/piscar 1Hz por timeout 1000ms), MOTOR (SUBINDO/DESCENDO), VEL1-3 (velocidade), EMERGÊNCIA (piscar 4Hz / fixo), ALARME (piscar 2Hz se rearme_ativo + botão travado). Ref: leds/SPEC.md §3.2.
