# Plano de Implementação — Controle Remoto para Carrinho de Jet Ski

**Versão:** 2.0  
**Data:** 2026-03-16  
**Referência:** design_spec.md v2.0

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
- Bibliotecas necessárias: `esp_now.h` e `WiFi.h` (inclusas no core)

**1.2 — Teste de comunicação ESP-NOW básica**
- Gravar sketch mínimo de transmissão no ESP32-A (futuro Remote)
- Gravar sketch mínimo de recepção no ESP32-B (futuro Principal)
- Validar recebimento bidirecional e medir latência com `millis()`
- **Critério de aceite:** latência < 20 ms em 100 envios consecutivos

**1.3 — Definição e documentação do pinout**
- Módulo Principal: pinos para 6 botões do painel, 5 LEDs, 6 relés (2 direção + 3 velocidade + 1 freio), 1 entrada microchave
- Módulo Remote: pinos para 6 botões, 3 LEDs
- Criar arquivo `pinout.h` para cada módulo

**1.4 — Montagem dos protótipos em protoboard**
- Montar Remote em protoboard (botões + LEDs + bateria)
- Montar Principal em protoboard (botões + LEDs + relés de teste + microchave simulada com táctil)

---

## Fase 2 — Desenvolvimento do Firmware

**Objetivo:** Implementar toda a lógica de negócio conforme design_spec.md v2.0, priorizando as camadas de segurança antes das funcionalidades de operação.

### 2.1 — Estrutura de Arquivos dos Projetos

```
principal/
├── principal.ino       (ou main.cpp)
├── pinout.h
├── protocolo.h         (structs compartilhadas)
├── maquina_estados.h   (enum EstadoSistema + transições)
├── motor.h             (abstração dos relés de direção + velocidade)
├── freio.h             (abstração do relé de freio + leitura microchave)
└── leds.h

remote/
├── remote.ino          (ou main.cpp)
├── pinout.h
├── protocolo.h         (cópia idêntica)
└── leds.h
```

### 2.2 — Módulo de Protocolo Compartilhado (`protocolo.h`)

Implementar as structs exatas definidas no design_spec seções 7.2 e 7.3:

```c
// Enum de comandos
typedef enum {
    CMD_HEARTBEAT = 0,
    CMD_SUBIR     = 1,
    CMD_DESCER    = 2,
    CMD_VEL1      = 3,
    CMD_VEL2      = 4,
    CMD_VEL3      = 5
} Comando;

// Enum de estados do sistema
typedef enum {
    ESTADO_PARADO           = 0,
    ESTADO_SUBINDO          = 1,
    ESTADO_DESCENDO         = 2,
    ESTADO_EMERGENCIA       = 3,
    ESTADO_FALHA_COMUNICACAO = 4
} EstadoSistema;

typedef struct {
    uint8_t  comando;
    uint8_t  botao_hold;
    uint8_t  emergencia;
    uint32_t timestamp;
    uint8_t  checksum;
} PacoteRemote;

typedef struct {
    uint8_t  estado_sistema;
    uint8_t  estado_freio;
    uint8_t  velocidade;
    uint8_t  trava_logica;
    uint8_t  checksum;
} PacoteStatus;

// Cálculo de checksum: XOR de todos os bytes exceto o próprio checksum
uint8_t calcular_checksum(uint8_t* data, size_t len);
```

### 2.3 — Firmware do Módulo Remote

**Ordem de implementação (segurança primeiro):**

**Etapa A — Comunicação base**
- Inicialização do ESP-NOW com MAC do Principal fixado
- Callback `OnDataSent`: registrar confirmação de entrega
- Callback `OnDataRecv`: receber e processar `PacoteStatus`; atualizar estado local

**Etapa B — Lógica dos botões**
- Leitura dos 6 botões com debounce por software (mínimo 50 ms)
- Botões SUBIR/DESCER: detectar hold (pressionado contínuo)
- Botões VEL1/VEL2/VEL3: detectar pulso (borda de descida)
- Botão EMERGÊNCIA: detectar pulso, sem debounce longo (resposta imediata)

**Etapa C — Transmissão**
- Heartbeat a cada 200 ms quando nenhum botão ativo
- Ao pressionar/soltar botão: enviar pacote imediatamente + continuar enviando a cada 200 ms enquanto estado ativo
- Campo `botao_hold`: 1 se SUBIR ou DESCER pressionado, 0 caso contrário
- Campo `emergencia`: 1 imediatamente ao pressionar EMERGÊNCIA

**Etapa D — LEDs**
- LINK: piscar 1 Hz se sem status recebido nos últimos 1000 ms; fixo se link ativo
- MOTOR: aceso se `estado_sistema == SUBINDO || DESCENDO`
- EMERGÊNCIA: piscar 4 Hz se `estado_sistema == EMERGENCIA`; fixo se `FALHA_COMUNICACAO`

**Pseudo-código do loop principal do Remote:**
```
loop():
  ler_botoes_com_debounce()
  
  if emergencia_pressionada:
    pacote.emergencia = 1
    enviar_pacote_imediato()
  
  if botao_direcao_mudou OR botao_velocidade_pulsado:
    enviar_pacote_imediato()
  
  if millis() - ultimo_envio > 200:
    enviar_pacote_heartbeat_ou_estado_atual()
  
  atualizar_leds(status_recebido_do_principal)
```

### 2.4 — Firmware do Módulo Principal

**Ordem de implementação (segurança primeiro):**

**Etapa A — Camada de segurança (implementar e testar ANTES de qualquer movimentação)**
- Leitura da microchave com debounce de 20 ms
- Relé do freio: função `acionar_freio()` e `liberar_freio()` com proteção contra duplo acionamento
- Watchdog: timer baseado em `millis()`, timeout 500 ms
- Flag `emergencia_ativa`: quando `true`, bloqueia toda movimentação e ignora Remote
- Botão REARME local: único mecanismo de limpeza de `emergencia_ativa` e `falha_comunicacao`

**Etapa B — Comunicação**
- Inicialização do ESP-NOW
- Callback `OnDataRecv`: validar checksum; se válido, atualizar `ultimo_pacote` e resetar watchdog; se campo `emergencia == 1`, ativar emergência imediatamente
- Envio de `PacoteStatus` a cada 200 ms ou imediato em mudança de estado

**Etapa C — Lógica de movimentação**
- Implementar máquina de estados conforme seção 6 do design_spec
- Aplicar tabela de condições (seção 6, Tabela Completa) antes de qualquer acionamento de relé
- Anti-colisão: nunca acionar dois relés de direção simultaneamente; dead-time de 100 ms ao inverter

**Etapa D — Controle de velocidade**
- Ao receber CMD_VEL1/2/3 (Remote ou botão local): desacionar relé de velocidade atual → acionar novo relé → atualizar LED correspondente
- Armazenar `velocidade_atual` em variável de estado
- Velocidade não é alterada durante emergência ou falha de comunicação

**Máquina de estados — implementação sugerida:**
```c
void atualizar_maquina_estados() {
  // Prioridade máxima: emergência
  if (emergencia_ativa) {
    acionar_freio();
    desligar_motor();
    estado = ESTADO_EMERGENCIA;
    return;
  }
  
  // Segunda prioridade: watchdog
  if (millis() - ultimo_pacote_remote > WATCHDOG_TIMEOUT_MS) {
    acionar_freio();
    desligar_motor();
    estado = ESTADO_FALHA_COMUNICACAO;
    return;
  }
  
  // Condições físicas
  if (!freio_liberado()) {
    desligar_motor();
    estado = ESTADO_PARADO;
    return;
  }
  
  // Lógica de movimentação
  if (botao_hold_ativo && comando_direcao_valido) {
    liberar_freio();
    acionar_motor(direcao);
    estado = (direcao == SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
  } else {
    desligar_motor();
    acionar_freio();
    estado = ESTADO_PARADO;
  }
}
```

---

## Fase 3 — Integração e Montagem

**Objetivo:** Integrar os dois módulos e validar o sistema completo em bancada antes do contato com o motor real.

**3.1 — Testes de bancada integrada (sem motor)**
- Conectar relés do Principal a cargas de teste (lâmpadas)
- Executar todos os fluxos de emergência com os dois módulos ativos
- Verificar dead-time com medição via serial (`Serial.println(millis())` antes e depois)
- Verificar que `emergencia_ativa` nunca é limpa automaticamente

**3.2 — Integração com o motor real (ambiente controlado)**
- Medir corrente de partida do motor antes de selecionar relés definitivos
- Substituir cargas de teste pelos cabos do motor
- Testar partida, parada, inversão e emergência com o carrinho em posição segura

**3.3 — Fabricação dos enclosures**
- Remote: enclosure IP54 com botões com capas de borracha; bateria acessível para recarga
- Principal: caixa elétrica fixada no painel, com isolação da rede elétrica e terminais rotulados

**3.4 — Instalação no local**
- Fixar Principal próximo ao quadro elétrico do guincho
- Conectar microchave do freio ao Principal
- Conectar saída dos relés ao circuito do motor (em paralelo ou substituindo o painel fixo existente)

---

## Fase 4 — Testes e Validação

**Objetivo:** Validar o sistema completo contra todos os requisitos do design_spec v2.0 antes da operação regular.

### Plano de Testes

| ID | Caso de Teste | Procedimento | Critério de Aceite |
|---|---|---|---|
| T01 | Latência de resposta | Medir tempo entre botão hold e acionamento do relé | < 100 ms |
| T02 | Regra Homem-Morto | Pressionar e soltar SUBIR rapidamente | Motor corta e freio aciona imediatamente ao soltar |
| T03 | Bloqueio por microchave | Tentar acionar motor com freio engatado (microchave LOW) | Motor não aciona |
| T04 | Watchdog — perda de sinal | Desligar Remote com motor ativo | Freio aciona em < 600 ms; estado FALHA_COMUNICACAO |
| T05 | Emergência pelo Remote | Pressionar EMERGÊNCIA no Remote | Freio aciona imediato; EMERGENCIA_ATIVA no Principal |
| T06 | Emergência pelo Painel | Pressionar EMERGÊNCIA no Painel Central | Freio aciona imediato; Remote ignorado |
| T07 | Sem rearme automático | Após emergência, ligar Remote novamente | Sistema permanece em EMERGENCIA_ATIVA sem rearmar |
| T08 | Rearme manual | Pressionar REARME no Painel após emergência resolvida | Sistema retorna a PARADO; operação liberada |
| T09 | Rearme de emergência do Remote pelo Painel | Remote aciona emergência; Painel ativa REARME | Estado de emergência desativado |
| T10 | Anti-colisão de direção | Inverter SUBIR → DESCER rapidamente | Sem acionamento simultâneo dos dois relés de direção |
| T11 | Seleção de velocidade | Pulsar VEL1, VEL2, VEL3 em sequência | Apenas um relé de velocidade ativo por vez; LEDs corretos |
| T12 | Alcance de comunicação | Operar em linha de visada a 50 m | Sem timeout de watchdog; operação normal |
| T13 | Teste de campo completo | Ciclo completo: subir + parar + descer + emergência na margem | Todos os comportamentos conforme especificação |

### Critérios de Aprovação para Deploy

- T01–T12 aprovados em bancada.
- T13 aprovado em pelo menos **3 ciclos consecutivos** no local real.
- Nenhuma falha de segurança do tipo: motor aciona com freio engatado, emergência rearma automaticamente, dois relés de direção ativos simultaneamente.

---

## Fase 5 — Deploy e Documentação Final

**Objetivo:** Colocar o sistema em operação e registrar toda a informação necessária para manutenção futura.

**5.1 — Deploy final**
- Gravar firmwares com versão tagueada (`v1.0.0`) em ambos os ESP32
- Registrar endereços MAC dos dois módulos no documento técnico

**5.2 — Documentação operacional (1 página)**
- Procedimento de ligar, operar e desligar
- Procedimento de acionamento e rearme de emergência
- Guia de troubleshooting: LED piscando = sem link; LED vermelho fixo = falha; como rearmar

**5.3 — Documentação técnica**
- Atualizar design_spec.md com desvios ocorridos durante a implementação
- Registrar pinout final, versão de firmware, endereços MAC e esquema elétrico simplificado

---

## Dependências e Riscos

| Risco | Probabilidade | Mitigação |
|---|---|---|
| Relés subdimensionados para corrente de partida do motor | Média | Medir corrente de partida antes da compra; fator de segurança 2x |
| Rearme acidental de emergência por bug de software | Baixa | Revisão de código focada na flag `emergencia_ativa`; teste T07 obrigatório |
| Falso acionamento da microchave por vibração | Baixa | Debounce por software mínimo 20 ms no Principal |
| Interferência de RF no ambiente (estruturas metálicas, rio) | Baixa | Testar alcance no local; antena externa se necessário |
| Umidade danificando o Remote | Média | Enclosure IP54; borracha de vedação nos botões |
| Bateria do Remote descarregando durante operação | Média | LED de bateria fraca (se possível); procedimento de carga antes de cada uso |

---

## Ferramentas e Materiais

**Hardware necessário:**
- 2x ESP32 (DevKit ou equivalente)
- 1x Módulo relé de 6 canais (ou 2x de 3 canais) para o Principal
- 1x Bateria Li-Ion 18650 + TP4056 + regulador 3.3V para o Remote
- 6x Botões para o Painel Central (com capas e fixação em painel)
- 6x Botões para o Remote (com capas de borracha IP54)
- LEDs e resistores (conforme seção 8 do design_spec)
- Cabos, conectores, terminais, enclosures

**Software:**
- PlatformIO (VS Code) — recomendado para gerenciamento de múltiplos projetos
- ESP32 Arduino Core (Espressif)
- Monitor serial para debug em tempo real