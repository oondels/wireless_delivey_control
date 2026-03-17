# Plano de Implementação — Controle Remoto para Carrinho de Jet Ski

**Versão:** 3.1  
**Data:** 2026-03-16  
**Referência:** design_spec.md v3.1

---

## Visão Geral das Fases

```
Fase 1 → Fase 2 → Fase 3 → Fase 4 → Fase 5
Ambiente  Firmware  Integração  Testes    Deploy
  (3h)     (14h)     (4h)        (7h)      (2h)
```

Estimativa total: **~30 horas** de trabalho técnico.

---

## Fase 1 — Preparação do Ambiente de Desenvolvimento

**Objetivo:** Ambiente configurado e comunicação ESP-NOW validada antes de qualquer lógica de negócio.

**1.1 — Setup da IDE e SDK**
- Instalar PlatformIO (VS Code) ou Arduino IDE
- Adicionar suporte ao ESP32: Espressif ESP32 Arduino Core
- Bibliotecas necessárias: `esp_now.h` e `WiFi.h` (inclusas no core)

**1.2 — Teste de comunicação ESP-NOW básica**
- Gravar sketch mínimo de transmissão no ESP32-A (futuro Remote)
- Gravar sketch mínimo de recepção no ESP32-B (futuro Principal)
- Validar recebimento bidirecional e medir latência com `millis()`
- **Critério de aceite:** latência < 20 ms em 100 envios consecutivos

**1.3 — Definição e documentação do pinout**

Módulo Principal — GPIOs necessários:
- 7 entradas: SUBIR, DESCER, VEL1, VEL2, VEL3, EMERGÊNCIA (trava), REARME
- 2 entradas de sensores: microchave do freio, fim de curso do estacionamento
- 6 saídas para relés: direção A, direção B, VEL1, VEL2, VEL3, freio
- 5 saídas para LEDs: VEL1, VEL2, VEL3, EMERGÊNCIA, LINK REMOTE
- **Total: 9 entradas + 11 saídas = 20 GPIOs**

Módulo Remote — GPIOs necessários:
- 6 entradas: SUBIR, DESCER, VEL1, VEL2, VEL3, EMERGÊNCIA (trava)
- 7 saídas para LEDs: LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME
- **Total: 6 entradas + 7 saídas = 13 GPIOs**

> Verificar a disponibilidade de GPIOs no modelo de ESP32 escolhido. O ESP32 DevKit padrão expõe 30+ GPIOs, mas alguns têm restrições de boot e uso. Evitar GPIOs 0, 2, 12 e 15 para entradas críticas.

Criar arquivo `pinout.h` para cada módulo com todas as constantes nomeadas.

**1.4 — Montagem dos protótipos em protoboard**
- Montar Remote em protoboard: 6 botões (táctil normal para simular trava), 7 LEDs com resistores de 220Ω, bateria
- Montar Principal em protoboard: 7 botões, 2 chaves tácteis para microchave e fim de curso, 6 relés de teste, 5 LEDs com resistores de 220Ω

---

## Fase 2 — Desenvolvimento do Firmware

**Objetivo:** Implementar toda a lógica de negócio conforme design_spec.md v3.1, priorizando segurança antes de funcionalidades de operação.

### 2.1 — Estrutura de Arquivos dos Projetos

```
principal/
├── principal.ino         (ou main.cpp)
├── pinout.h              (todos os GPIOs nomeados)
├── protocolo.h           (structs e enums compartilhados)
├── maquina_estados.h     (enum EstadoSistema + função de transição)
├── motor.h               (relés de direção + velocidade)
├── freio.h               (relé de freio + leitura microchave)
├── sensores.h            (fim de curso com debounce)
└── leds.h                (controle de GPIO + lógica de piscar por timer)

remote/
├── remote.ino            (ou main.cpp)
├── pinout.h
├── protocolo.h           (cópia idêntica)
└── leds.h
```

### 2.2 — Módulo de Protocolo Compartilhado (`protocolo.h`)

```c
typedef enum {
    CMD_HEARTBEAT = 0,
    CMD_SUBIR     = 1,
    CMD_DESCER    = 2,
    CMD_VEL1      = 3,
    CMD_VEL2      = 4,
    CMD_VEL3      = 5
} Comando;

typedef enum {
    ESTADO_PARADO            = 0,
    ESTADO_SUBINDO           = 1,
    ESTADO_DESCENDO          = 2,
    ESTADO_EMERGENCIA        = 3,
    ESTADO_FALHA_COMUNICACAO = 4
} EstadoSistema;

typedef struct {
    uint8_t  comando;       // Comando ativo ou HEARTBEAT
    uint8_t  botao_hold;    // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;    // 1=botão com trava ativo no Remote
    uint32_t timestamp;     // millis() do Remote
    uint8_t  checksum;      // XOR de todos os bytes anteriores
} PacoteRemote;

typedef struct {
    uint8_t  estado_sistema; // EstadoSistema
    uint8_t  estado_freio;   // 0=engatado, 1=liberado
    uint8_t  velocidade;     // 1, 2 ou 3
    uint8_t  trava_logica;   // 1=trava ativa
    uint8_t  rearme_ativo;   // 1=Painel fez rearme com botão Remote ainda travado
    uint8_t  checksum;
} PacoteStatus;

uint8_t calcular_checksum(uint8_t* data, size_t len);
```

### 2.3 — Módulo de LEDs (`leds.h`)

Implementar uma abstração de LED que suporte dois modos de operação: ligado fixo e piscando em frequência configurável. A lógica de piscar deve ser **não-bloqueante** (baseada em `millis()`, nunca em `delay()`).

```c
typedef struct {
    uint8_t  gpio;
    bool     piscando;
    uint16_t intervalo_ms;   // período de cada semi-ciclo (ex: 125ms para 4Hz)
    uint32_t ultimo_toggle;
    bool     estado_atual;
} Led;

void led_ligar(Led* led);
void led_desligar(Led* led);
void led_piscar(Led* led, uint16_t intervalo_ms);
void led_atualizar(Led* led);  // chamar no loop principal
```

Frequências utilizadas no projeto:
- 1 Hz → intervalo 500 ms (LED LINK sem conexão)
- 2 Hz → intervalo 250 ms (LED ALARME)
- 4 Hz → intervalo 125 ms (LED EMERGÊNCIA ativo)

### 2.4 — Firmware do Módulo Remote

**Etapa A — Comunicação base**
- Inicialização do ESP-NOW com MAC do Principal fixado
- Callback `OnDataSent`: registrar flag de entrega confirmada
- Callback `OnDataRecv`: receber `PacoteStatus`; atualizar variáveis locais (`estado_sistema`, `velocidade`, `rearme_ativo`)

**Etapa B — Lógica dos botões**
- Leitura dos 6 botões com debounce por software (mínimo 50 ms)
- SUBIR/DESCER: ler nível contínuo (hold)
- VEL1/VEL2/VEL3: detectar borda de descida (pulso)
- EMERGÊNCIA (com trava): `digitalRead(PIN_EMERGENCIA)` — a trava mecânica mantém o sinal; sem latch por software

**Etapa C — Transmissão**
- Heartbeat a cada 200 ms sem botão ativo
- Envio imediato em qualquer mudança de estado + continuar a cada 200 ms enquanto ativo
- `botao_hold` = nível de SUBIR ou DESCER
- `emergencia` = leitura direta do pino do botão com trava

**Etapa D — Atualização de LEDs**

Todos os LEDs do Remote são controlados pelo status recebido do Principal:

| LED | Lógica de controle |
|---|---|
| LINK | Piscar 1 Hz se `millis() - ultimo_status > 1000`; ligado fixo caso contrário |
| MOTOR | Ligado se `estado_sistema == SUBINDO \|\| DESCENDO` |
| VEL1 | Ligado se `velocidade == 1` |
| VEL2 | Ligado se `velocidade == 2` |
| VEL3 | Ligado se `velocidade == 3` |
| EMERGÊNCIA | Piscar 4 Hz se `estado_sistema == EMERGENCIA`; ligado fixo se `FALHA_COMUNICACAO` |
| ALARME | Piscar 2 Hz se `rearme_ativo == 1 && digitalRead(PIN_EMERGENCIA) == HIGH`; desligado caso contrário |

**Pseudo-código do loop principal do Remote:**
```
loop():
  ler_botoes_com_debounce()
  emergencia_local = digitalRead(PIN_EMERGENCIA)

  if emergencia_local != emergencia_anterior
     OR direcao_mudou
     OR velocidade_pulsada:
    montar_pacote(comando, botao_hold, emergencia_local)
    enviar_imediato()

  if millis() - ultimo_envio > 200:
    montar_pacote(CMD_HEARTBEAT_ou_atual, botao_hold, emergencia_local)
    enviar()

  atualizar_todos_os_leds(status_recebido)
```

### 2.5 — Firmware do Módulo Principal

**Etapa A — Camada de segurança (implementar e testar ANTES de qualquer movimentação)**
- Leitura da microchave com debounce de 20 ms
- Leitura do fim de curso com debounce de 20 ms
- Relé do freio: `acionar_freio()` e `liberar_freio()` com proteção contra duplo acionamento
- Watchdog: `millis()` baseado, timeout `WATCHDOG_TIMEOUT_MS = 500`
- Flag `emergencia_ativa`: só é limpa pelo botão REARME
- Botão EMERGÊNCIA local (com trava): `digitalRead()` direto — nível alto ativa emergência imediatamente
- Botão REARME: ao ser pressionado, limpar `emergencia_ativa` e `falha_comunicacao`; se `ultimo_pacote_remote.emergencia == 1`, setar `rearme_ativo = true` no próximo status

**Etapa B — Comunicação**
- Inicialização do ESP-NOW
- Callback `OnDataRecv`: validar checksum; se válido, resetar watchdog e processar; se `emergencia == 1`, ativar `emergencia_ativa` imediatamente
- Envio de `PacoteStatus` a cada 200 ms; envio imediato em mudança de estado

**Etapa C — Lógica de fim de curso**
- Ao detectar fim de curso (com debounce):
  - `desligar_motor()`
  - `acionar_freio()`
  - `estado = ESTADO_PARADO`
  - **Não** ativar `emergencia_ativa`

**Etapa D — Lógica de movimentação**
- Máquina de estados conforme seção 7 do design_spec
- Anti-colisão: dead-time de 100 ms ao inverter sentido
- Botão local do Painel tem prioridade sobre Remote se ambos forem ativos simultaneamente

**Etapa E — Controle de velocidade**
- Ao receber VEL1/2/3 (Remote ou local): desacionar relé atual → acionar novo → atualizar `velocidade_atual` → incluir no próximo `PacoteStatus`
- Remote sincroniza seus LEDs ao receber o campo `velocidade` no status

**Máquina de estados — implementação sugerida:**
```c
void atualizar_maquina_estados() {
  // Prioridade 1: emergência
  if (digitalRead(PIN_EMERGENCIA_PAINEL) || emergencia_ativa) {
    emergencia_ativa = true;
    acionar_freio();
    desligar_motor();
    estado = ESTADO_EMERGENCIA;
    return;
  }

  // Prioridade 2: watchdog
  if (millis() - ultimo_pacote_remote > WATCHDOG_TIMEOUT_MS) {
    acionar_freio();
    desligar_motor();
    estado = ESTADO_FALHA_COMUNICACAO;
    return;
  }

  // Prioridade 3: fim de curso
  if (fim_de_curso_acionado()) {
    desligar_motor();
    acionar_freio();
    estado = ESTADO_PARADO;
    return;
  }

  // Prioridade 4: microchave do freio
  if (!freio_liberado()) {
    desligar_motor();
    estado = ESTADO_PARADO;
    return;
  }

  // Prioridade 5: movimentação
  bool hold = botao_hold_local || pacote_remote.botao_hold;
  Direcao dir = obter_direcao_ativa();  // local tem prioridade

  if (hold && dir != NENHUMA) {
    liberar_freio();
    acionar_motor(dir);
    estado = (dir == SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
  } else {
    desligar_motor();
    acionar_freio();
    estado = ESTADO_PARADO;
  }
}
```

---

## Fase 3 — Integração e Montagem

**3.1 — Testes de bancada integrada (sem motor)**
- Conectar relés do Principal a cargas de teste (lâmpadas)
- Executar todos os fluxos de emergência e rearme com os dois módulos ativos
- Testar fluxo de LED ALARME: rearme pelo Painel com botão Remote travado
- Verificar que `emergencia_ativa` nunca é limpa automaticamente
- Simular fim de curso: verificar que motor para, freio aciona e sistema permanece operacional sem rearme
- Verificar sincronização dos LEDs de velocidade nos dois módulos
- Verificar que todos os LEDs piscam nas frequências corretas sem uso de `delay()`

**3.2 — Integração com o motor real (ambiente controlado)**
- Medir corrente de partida do motor; selecionar relés com fator de segurança 2x
- Substituir cargas de teste pelos cabos do motor
- Testar partida, parada, inversão, emergência e fim de curso com carrinho em posição segura

**3.3 — Fabricação dos enclosures**
- Remote: enclosure IP54; LEDs visíveis pelo painel frontal; botão de emergência (trava) em posição de destaque; bateria acessível
- Principal: caixa elétrica com isolação da rede; terminais rotulados; botão de emergência (trava) em posição de destaque; botão REARME identificado

**3.4 — Instalação no local**
- Fixar Principal próximo ao quadro elétrico do guincho
- Conectar microchave do freio ao Principal
- Instalar e conectar sensor de fim de curso na posição final de subida do estacionamento
- Conectar saída dos relés ao circuito do motor

---

## Fase 4 — Testes e Validação

### Plano de Testes

| ID | Caso de Teste | Procedimento | Critério de Aceite |
|---|---|---|---|
| T01 | Latência de resposta | Medir tempo entre botão hold e acionamento do relé | < 100 ms |
| T02 | Regra Homem-Morto | Pressionar e soltar SUBIR | Motor corta e freio aciona imediatamente ao soltar |
| T03 | Bloqueio por microchave | Tentar acionar motor com freio engatado | Motor não aciona |
| T04 | Watchdog — perda de sinal | Desligar Remote com motor ativo | Freio aciona em < 600 ms; estado FALHA_COMUNICACAO |
| T05 | Emergência pelo Remote (trava) | Travar botão EMERGÊNCIA no Remote | Freio aciona imediato; EMERGENCIA_ATIVA |
| T06 | Emergência pelo Painel (trava) | Travar botão EMERGÊNCIA no Painel | Freio aciona imediato; Remote ignorado |
| T07 | Sem rearme automático | Após emergência, reconectar Remote | Sistema permanece EMERGENCIA_ATIVA |
| T08 | Rearme normal (botão Remote solto) | Soltar botão Remote + REARME no Painel | Sistema retorna a PARADO; LED ALARME apagado |
| T09 | Rearme com botão Remote ainda travado | REARME no Painel sem soltar botão Remote | Sistema rearma; LED ALARME pisca no Remote |
| T10 | Apagamento do LED ALARME | Após T09, soltar botão emergência no Remote | LED ALARME apaga |
| T11 | Anti-colisão de direção | Inverter SUBIR → DESCER rapidamente | Sem acionamento simultâneo dos dois relés |
| T12 | Seleção de velocidade — sincronização | VEL1 no Remote → VEL2 no Painel → VEL3 no Remote | LED correto aceso em ambos os módulos a cada mudança |
| T13 | Fim de curso | Acionar sensor com motor ativo (SUBINDO) | Motor para, freio aciona, estado PARADO sem rearme |
| T14 | Operação após fim de curso | Após T13, pressionar DESCER hold | Motor aciona normalmente |
| T15 | LEDs não-bloqueantes | Observar LEDs piscando durante operação simultânea | Nenhum LED congela; operação não é afetada |
| T16 | Alcance de comunicação | Operar em linha de visada a 50 m | Sem timeout de watchdog |
| T17 | Teste de campo completo | Subir até fim de curso + descer + emergência na margem + rearme | Todos os comportamentos conforme especificação |

### Critérios de Aprovação para Deploy

- T01–T16 aprovados em bancada.
- T17 aprovado em pelo menos **3 ciclos consecutivos** no local real.
- Nenhuma falha de segurança: motor com freio engatado; rearme automático; dois relés de direção simultâneos; fim de curso exigindo rearme.

---

## Fase 5 — Deploy e Documentação Final

**5.1 — Deploy final**
- Gravar firmwares com versão tagueada (`v1.0.0`) em ambos os ESP32
- Registrar endereços MAC dos dois módulos

**5.2 — Documentação operacional (1 página)**
- Procedimento de ligar, operar e desligar
- Procedimento de acionamento e rearme de emergência
- Explicação do fim de curso e do LED ALARME
- Troubleshooting: LED LINK piscando = sem conexão; LED EMERGÊNCIA fixo = falha de comunicação; LED ALARME piscando = soltar botão de emergência no carrinho

**5.3 — Documentação técnica**
- Atualizar design_spec.md com desvios ocorridos
- Registrar pinout final, versão de firmware, MACs, posição do fim de curso e esquema elétrico simplificado

---

## Dependências e Riscos

| Risco | Probabilidade | Mitigação |
|---|---|---|
| Relés subdimensionados para corrente de partida | Média | Medir corrente antes da compra; fator 2x |
| Rearme acidental por bug de software | Baixa | Flag `emergencia_ativa` limpa apenas por REARME explícito; teste T07 obrigatório |
| Falso acionamento do fim de curso por vibração | Média | Debounce 20 ms; verificar fixação mecânica do sensor |
| Posicionamento incorreto do fim de curso | Média | Calibrar com carrinho real antes de fixar |
| Falso acionamento da microchave por vibração | Baixa | Debounce 20 ms |
| LEDs bloqueando o loop por uso de `delay()` | Média | Toda lógica de piscar via `millis()`; teste T15 valida isso |
| Interferência de RF (estruturas metálicas, rio) | Baixa | Testar alcance no local; antena externa se necessário |
| Umidade danificando o Remote | Média | Enclosure IP54; borracha de vedação nos botões |
| Bateria do Remote descarregando durante operação | Média | Watchdog garante freio se Remote desligar; carga antes de cada uso |

---

## Ferramentas e Materiais

**Hardware necessário:**
- 2x ESP32 DevKit (verificar disponibilidade de GPIOs: mínimo 20 no Principal, 13 no Remote)
- 1x Módulo relé de 6 canais para o Principal (ou 2x de 3 canais)
- 1x Bateria Li-Ion 18650 + módulo TP4056 + regulador 3.3V para o Remote
- 2x Botões com trava (emergência — Painel e Remote)
- 6x Botões tácteis para o Painel Central (SUBIR, DESCER, VEL1, VEL2, VEL3, REARME)
- 5x Botões tácteis com capas de borracha para o Remote (SUBIR, DESCER, VEL1, VEL2, VEL3)
- 1x Sensor fim de curso (microswitch ou chave de limite) para o estacionamento
- 12x LEDs 3V (padrão Arduino) — cor a definir no momento da montagem (7 Remote + 5 Principal)
- 12x Resistores 220Ω (um por LED)
- Cabos, conectores, terminais, enclosures

**Software:**
- PlatformIO (VS Code) — recomendado para múltiplos projetos
- ESP32 Arduino Core (Espressif)
- Monitor serial para debug em tempo real