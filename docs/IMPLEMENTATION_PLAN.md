# Plano de Implementação — Controle Remoto para Carrinho de Jet Ski

**Versão:** 3.2  
**Data:** 2026-03-16  
**Referência:** design_spec.md v3.2

---

## Visão Geral das Fases

```
Fase 1 → Fase 2 → Fase 3 → Fase 4 → Fase 5
Ambiente  Firmware  Integração  Testes    Deploy
  (3h)     (12h)     (4h)        (6h)      (2h)
```

Estimativa total: **~27 horas** de trabalho técnico.

---

## Fase 1 — Preparação do Ambiente de Desenvolvimento

**Objetivo:** Ambiente configurado e comunicação ESP-NOW validada antes de qualquer lógica de negócio.

**1.1 — Setup da IDE e SDK**
- Instalar PlatformIO (VS Code) ou Arduino IDE
- Adicionar suporte ao ESP32: Espressif ESP32 Arduino Core
- Bibliotecas: `esp_now.h` e `WiFi.h` (inclusas no core)

**1.2 — Teste de comunicação ESP-NOW básica**
- Gravar sketch mínimo de transmissão no ESP32-A (futuro Remote)
- Gravar sketch mínimo de recepção no ESP32-B (futuro Principal)
- Validar bidirecionalidade e medir latência com `millis()`
- **Critério de aceite:** latência < 20 ms em 100 envios consecutivos

**1.3 — Definição e documentação do pinout**

Módulo Principal — **15 GPIOs** (8 entradas + 7 saídas):

| Função | Tipo |
|---|---|
| Botão SUBIR | Entrada |
| Botão DESCER | Entrada |
| Botão VEL1 | Entrada |
| Botão VEL2 | Entrada |
| Botão VEL3 | Entrada |
| Botão EMERGÊNCIA (trava) | Entrada |
| Botão REARME | Entrada |
| Fim de curso | Entrada |
| Relé + LED DIREÇÃO A | Saída compartilhada |
| Relé + LED DIREÇÃO B | Saída compartilhada |
| Relé + LED VEL1 | Saída compartilhada |
| Relé + LED VEL2 | Saída compartilhada |
| Relé + LED VEL3 | Saída compartilhada |
| Relé + LED FREIO | Saída compartilhada |
| LED LINK REMOTE | Saída exclusiva |

Módulo Remote — **13 GPIOs** (6 entradas + 7 saídas):

| Função | Tipo |
|---|---|
| Botão SUBIR | Entrada |
| Botão DESCER | Entrada |
| Botão VEL1 | Entrada |
| Botão VEL2 | Entrada |
| Botão VEL3 | Entrada |
| Botão EMERGÊNCIA (trava) | Entrada |
| LED LINK | Saída |
| LED MOTOR | Saída |
| LED VEL1 | Saída |
| LED VEL2 | Saída |
| LED VEL3 | Saída |
| LED EMERGÊNCIA | Saída |
| LED ALARME | Saída |

> Ao definir os GPIOs, evitar os pinos 0, 2, 12 e 15 para entradas críticas (restrições de boot do ESP32). Registrar o mapa final no arquivo `pinout.h` de cada módulo.

**1.4 — Montagem dos protótipos em protoboard**

Principal:
- 7 botões tácteis com resistores pull-up
- 1 chave táctil para simular fim de curso
- 6 saídas: cada uma ligada a um LED 3V (220Ω) **em paralelo** com o módulo relé de teste
- 1 LED para LINK REMOTE

Remote:
- 6 botões tácteis com resistores pull-up (usar chave com trava para simular botão de emergência)
- 7 LEDs 3V com resistores 220Ω

> **Validação de hardware na protoboard:** medir a corrente total do GPIO ao acionar LED + driver de relé simultaneamente. Garantir que não ultrapasse 12 mA por GPIO. Se necessário, usar transistor NPN (ex: BC547) para isolar a carga do relé do GPIO.

---

## Fase 2 — Desenvolvimento do Firmware

**Objetivo:** Implementar toda a lógica conforme design_spec.md v3.2. Segurança implementada antes de movimentação.

### 2.1 — Estrutura de Arquivos dos Projetos

```
principal/
├── principal.ino
├── pinout.h            (todos os GPIOs nomeados)
├── protocolo.h         (structs e enums compartilhados)
├── maquina_estados.h   (enum EstadoSistema + função de transição)
├── motor.h             (relés DIREÇÃO A/B + velocidade; sem leitura de microchave)
├── freio.h             (apenas relé de freio — microchave é hardware externo)
├── sensores.h          (fim de curso com debounce)
└── leds.h              (piscar não-bloqueante via millis())

remote/
├── remote.ino
├── pinout.h
├── protocolo.h         (cópia idêntica)
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
    uint8_t  comando;
    uint8_t  botao_hold;    // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;    // 1=botão com trava ativo no Remote
    uint32_t timestamp;
    uint8_t  checksum;
} PacoteRemote;

typedef struct {
    uint8_t  estado_sistema;
    uint8_t  velocidade;     // 1, 2 ou 3 — sincroniza LEDs no Remote
    uint8_t  trava_logica;
    uint8_t  rearme_ativo;   // 1=Painel fez rearme com botão Remote ainda travado
    uint8_t  checksum;
} PacoteStatus;

uint8_t calcular_checksum(uint8_t* data, size_t len);
```

> Comparando com a versão anterior: `estado_freio` foi **removido** do `PacoteStatus` pois o Principal não lê mais a microchave.

### 2.3 — Módulo `freio.h` (Principal)

O módulo de freio **não possui leitura de sensor**. Ele apenas controla o GPIO do relé de freio:

```c
void acionar_freio();   // GPIO HIGH → relé energizado → freio aplicado + LED aceso
void liberar_freio();   // GPIO LOW  → relé desenergizado → freio liberado + LED apagado
```

A microchave do freio é hardware externo, atuando diretamente no circuito. O firmware não tem visibilidade sobre seu estado.

### 2.4 — Módulo `leds.h`

Abstração de LED não-bloqueante baseada em `millis()`:

```c
typedef struct {
    uint8_t  gpio;
    bool     piscando;
    uint16_t intervalo_ms;
    uint32_t ultimo_toggle;
    bool     estado_atual;
} Led;

void led_ligar(Led* led);
void led_desligar(Led* led);
void led_piscar(Led* led, uint16_t intervalo_ms);
void led_atualizar(Led* led);  // chamar no loop principal — nunca usar delay()
```

Frequências do projeto:
- **1 Hz** → 500 ms (LINK sem conexão)
- **2 Hz** → 250 ms (ALARME)
- **4 Hz** → 125 ms (EMERGÊNCIA ativo)

> No Módulo Principal, os LEDs compartilhados com relés **não usam** esta abstração — são controlados diretamente pelas funções de motor/freio (o GPIO já é acionado junto com o relé). Esta abstração é usada apenas para: LED LINK REMOTE (Principal) e todos os LEDs do Remote.

### 2.5 — Firmware do Módulo Remote

**Etapa A — Comunicação**
- ESP-NOW com MAC do Principal fixado
- `OnDataSent`: registrar confirmação
- `OnDataRecv`: atualizar `estado_sistema`, `velocidade`, `rearme_ativo`

**Etapa B — Botões**
- Debounce 50 ms em todos os botões
- SUBIR/DESCER: ler nível (hold)
- VEL1/VEL2/VEL3: detectar borda de descida (pulso)
- EMERGÊNCIA: `digitalRead()` direto — trava mecânica mantém o nível

**Etapa C — Transmissão**
- Heartbeat a cada 200 ms
- Envio imediato + repetir a cada 200 ms em mudança de estado

**Etapa D — LEDs**

| LED | Lógica |
|---|---|
| LINK | Piscar 1 Hz se sem status há > 1000 ms; fixo caso contrário |
| MOTOR | Ligado se `SUBINDO` ou `DESCENDO` |
| VEL1/2/3 | Ligado conforme campo `velocidade` do status |
| EMERGÊNCIA | Piscar 4 Hz se `EMERGENCIA`; fixo se `FALHA_COMUNICACAO` |
| ALARME | Piscar 2 Hz se `rearme_ativo == 1` **e** botão emergência local ainda HIGH |

**Loop principal do Remote:**
```
loop():
  ler_botoes()
  emergencia_local = digitalRead(PIN_EMERGENCIA)

  if mudou_algum_estado:
    enviar_pacote_imediato()

  if millis() - ultimo_envio > 200:
    enviar_pacote()

  atualizar_todos_leds(status_recebido)
```

### 2.6 — Firmware do Módulo Principal

**Etapa A — Segurança (implementar e testar primeiro)**
- Leitura do fim de curso com debounce 20 ms
- `acionar_freio()` e `liberar_freio()` sem leitura de sensor
- Watchdog: timeout `WATCHDOG_TIMEOUT_MS = 500`
- Flag `emergencia_ativa`: limpa apenas por REARME explícito
- Botão EMERGÊNCIA local (trava): `digitalRead()` — nível HIGH ativa imediatamente
- Botão REARME: limpa `emergencia_ativa` e `falha_comunicacao`; seta `rearme_ativo` se `pacote_remote.emergencia == 1`

**Etapa B — Comunicação**
- `OnDataRecv`: validar checksum; resetar watchdog; se `emergencia == 1`, ativar imediatamente
- Enviar `PacoteStatus` a cada 200 ms ou imediato em mudança

**Etapa C — Fim de curso**
- Ao detectar: `desligar_motor()` → `acionar_freio()` → `estado = PARADO`
- **Não** ativar `emergencia_ativa`

**Etapa D — Movimentação**
- Dead-time 100 ms ao inverter direção
- Botão local tem prioridade sobre Remote

**Etapa E — Velocidade**
- VEL1/2/3: desacionar relé atual → acionar novo → `velocidade_atual` → incluir no próximo status
- Os relés de velocidade têm LED em paralelo — o LED acende/apaga automaticamente com o relé

**Máquina de estados:**
```c
void atualizar_maquina_estados() {
  // Prioridade 1: emergência
  if (digitalRead(PIN_EMERGENCIA_PAINEL) || emergencia_ativa) {
    emergencia_ativa = true;
    acionar_freio();     // GPIO HIGH → relé freio ON + LED freio ON
    desligar_motor();    // GPIOs direção LOW → relés OFF + LEDs direção OFF
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

  // Prioridade 4: movimentação
  bool hold = botao_hold_local || pacote_remote.botao_hold;
  Direcao dir = obter_direcao_ativa();

  if (hold && dir != NENHUMA) {
    liberar_freio();     // GPIO LOW → relé freio OFF + LED freio OFF
    acionar_motor(dir);  // GPIO HIGH → relé direção ON + LED direção ON
    estado = (dir == SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
  } else {
    desligar_motor();
    acionar_freio();
    estado = ESTADO_PARADO;
  }
}
```

> Observe que nesta versão não há verificação de `freio_liberado()` no firmware — essa camada é responsabilidade exclusiva da microchave de hardware.

---

## Fase 3 — Integração e Montagem

**3.1 — Testes de bancada (sem motor)**
- Validar que ao acionar cada GPIO de saída o relé **e** o LED disparam simultaneamente
- Verificar corrente do GPIO com LED + driver em paralelo (não ultrapassar 12 mA)
- Executar fluxos de emergência/rearme; verificar LED ALARME
- Simular fim de curso e confirmar ausência de rearme obrigatório
- Confirmar que `emergencia_ativa` nunca limpa automaticamente
- Verificar sincronização de LEDs de velocidade entre Principal e Remote

**3.2 — Integração com motor real**
- Medir corrente de partida; selecionar relés com fator 2x
- Testar partida, parada, inversão, emergência e fim de curso com carrinho seguro
- Validar atuação da microchave do freio diretamente no circuito (sem ESP32)

**3.3 — Fabricação dos enclosures**
- Principal: caixa elétrica; isolação da rede; botão EMERGÊNCIA em destaque; terminais rotulados; LEDs visíveis no painel
- Remote: IP54; botão EMERGÊNCIA em destaque; 7 LEDs visíveis; bateria acessível

**3.4 — Instalação no local**
- Conectar saída dos relés ao circuito do motor
- Instalar fim de curso na posição final de subida
- Conectar microchave do freio diretamente ao circuito do freio (sem passar pelo ESP32)

---

## Fase 4 — Testes e Validação

| ID | Caso de Teste | Procedimento | Critério de Aceite |
|---|---|---|---|
| T01 | Latência | Medir botão → relé | < 100 ms |
| T02 | GPIO compartilhado | Acionar cada saída | Relé e LED acionam simultaneamente |
| T03 | Regra Homem-Morto | Pressionar e soltar SUBIR | Motor corta e freio aciona ao soltar |
| T04 | Microchave hardware | Acionar microchave com motor ativo | Freio aplicado independente do firmware |
| T05 | Watchdog | Desligar Remote com motor ativo | Freio em < 600 ms; FALHA_COMUNICACAO |
| T06 | Emergência Remote (trava) | Travar botão no Remote | Freio imediato; EMERGENCIA_ATIVA |
| T07 | Emergência Painel (trava) | Travar botão no Painel | Freio imediato; Remote ignorado |
| T08 | Sem rearme automático | Reconectar Remote após emergência | Sistema permanece EMERGENCIA_ATIVA |
| T09 | Rearme normal | Soltar botão + REARME no Painel | PARADO; sem LED ALARME |
| T10 | Rearme com botão Remote travado | REARME sem soltar botão Remote | Rearma; LED ALARME pisca no Remote |
| T11 | Apagamento LED ALARME | Após T10, soltar botão Remote | LED ALARME apaga |
| T12 | Anti-colisão | Inverter SUBIR → DESCER rapidamente | Sem dois relés de direção simultâneos |
| T13 | Sincronização velocidade | VEL1 Remote → VEL2 Painel → VEL3 Remote | LEDs corretos e iguais nos dois módulos |
| T14 | Fim de curso | Acionar sensor com motor SUBINDO | Para + freio; PARADO sem rearme |
| T15 | Operação após fim de curso | Após T14, DESCER hold | Motor aciona normalmente |
| T16 | LEDs não-bloqueantes | Observar LEDs piscando durante operação | Nenhum LED trava; operação normal |
| T17 | Alcance | Linha de visada 50 m | Sem watchdog timeout |
| T18 | Teste de campo completo | Subir até fim de curso + descer + emergência + rearme | Todos os comportamentos conforme spec |

### Critérios de Aprovação para Deploy

- T01–T17 aprovados em bancada.
- T18 aprovado em pelo menos **3 ciclos consecutivos** no local real.
- Nenhuma falha: motor com freio engatado via microchave; rearme automático; dois relés de direção simultâneos; fim de curso exigindo rearme.

---

## Fase 5 — Deploy e Documentação Final

**5.1 — Deploy**
- Gravar firmware `v1.0.0` em ambos os ESP32
- Registrar MACs dos dois módulos

**5.2 — Documentação operacional**
- Ligar / operar / desligar
- Acionamento e rearme de emergência (incluindo LED ALARME)
- Fim de curso: parada automática ao chegar ao depósito, sem necessidade de rearme
- Troubleshooting: LINK piscando = sem conexão; EMERGÊNCIA fixo = falha comunicação; ALARME piscando = soltar botão no carrinho

**5.3 — Documentação técnica**
- Atualizar spec com desvios ocorridos
- Pinout final, MACs, posição do fim de curso, esquema elétrico simplificado (incluindo ligação microchave direta no circuito)

---

## Dependências e Riscos

| Risco | Probabilidade | Mitigação |
|---|---|---|
| GPIO sobrecarregado por LED + driver relé | Média | Medir corrente na protoboard; usar transistor se > 12 mA; teste T02 valida isso |
| Relés subdimensionados | Média | Medir corrente de partida; fator 2x |
| Rearme acidental por bug | Baixa | `emergencia_ativa` limpa só por REARME explícito; teste T08 obrigatório |
| Falso acionamento do fim de curso | Média | Debounce 20 ms; fixação mecânica do sensor |
| LEDs bloqueando loop por `delay()` | Média | Toda lógica de piscar via `millis()`; teste T16 valida |
| Interferência RF | Baixa | Testar alcance no local; antena externa se necessário |
| Umidade no Remote | Média | Enclosure IP54 |
| Bateria descarregando | Média | Watchdog garante freio; carga antes de cada uso |

---

## Ferramentas e Materiais

**Hardware necessário:**
- 2x ESP32 DevKit
- 1x Módulo relé 6 canais (ou 2x de 3 canais) para o Principal
- 1x Bateria Li-Ion 18650 + TP4056 + regulador 3.3V para o Remote
- 2x Botões com trava (emergência — Painel e Remote)
- 6x Botões tácteis para o Painel (SUBIR, DESCER, VEL1, VEL2, VEL3, REARME)
- 5x Botões tácteis com capas borracha para o Remote (SUBIR, DESCER, VEL1, VEL2, VEL3)
- 1x Sensor fim de curso (microswitch ou chave de limite)
- 13x LEDs 3V padrão Arduino — cor a definir na montagem (6 compartilhados c/ relés + 1 LINK no Principal; 7 no Remote)
- 13x Resistores 220Ω (um por LED)
- Transistores NPN (ex: BC547) ou ULN2003 se necessário para driver dos relés
- Cabos, conectores, terminais, enclosures

**Software:**
- PlatformIO (VS Code)
- ESP32 Arduino Core (Espressif)
- Monitor serial para debug