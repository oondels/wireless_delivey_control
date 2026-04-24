# Índice de Especificações Técnicas

> **Para agentes de código:** use este índice para localizar a spec correta antes de agir.
> Leia **apenas** os arquivos relevantes à sua tarefa — não carregue tudo.

---

## Roteamento por Palavra-Chave

| Palavra-chave / tema | Arquivo | Seção |
|---|---|---|
| emergência, rearme, fail-safe, EMERGENCIA_ATIVA | `seguranca/SPEC.md` | §3 (emergência), §3.4–3.5 (rearme) |
| watchdog, timeout, heartbeat, FALHA_COMUNICACAO | `seguranca/SPEC.md` | §4 |
| homem-morto, dead-man, botão hold | `seguranca/SPEC.md` | §5 |
| invariantes de segurança | `seguranca/SPEC.md` | §9 |
| ESP-NOW, pacote, PacoteRemote, PacoteStatus, checksum | `comunicacao/SPEC.md` | §4 (structs), §6 (checksum) |
| comando, enum, CMD_SUBIR, CMD_DESCER, CMD_RESET | `comunicacao/SPEC.md` | §5 |
| callback, OnDataRecv, OnDataSent | `comunicacao/SPEC.md` | §9 |
| frequência de envio, timing, 200 ms | `comunicacao/SPEC.md` | §7 |
| hierarquia de comando, prioridade painel vs remote | `comunicacao/SPEC.md` | §10 |
| motor, relé direção, SUBIR, DESCER, dead-time | `motor/SPEC.md` | §2 |
| velocidade, VEL1, VEL2, potenciômetro, relé velocidade | `motor/SPEC.md` | §3 |
| freio, relé freio, acionar_freio, liberar_freio | `motor/SPEC.md` | §4 |
| micro do freio, micro_freio_ativa | `hardware_io/SPEC.md` | §4.3, §13 |
| fim de curso, estacionamento, sensor posição | `motor/SPEC.md` | §5 |
| sequência de partida, parada, inversão | `motor/SPEC.md` | §6 |
| condições de acionamento do motor (tabela) | `motor/SPEC.md` | §7 |
| máquina de estados, transição, estado, PARADO, SUBINDO, DESCENDO | `maquina_estados/SPEC.md` | §2–§3 |
| prioridade de avaliação (loop principal) | `maquina_estados/SPEC.md` | §4 |
| pseudocódigo atualizar_maquina_estados | `maquina_estados/SPEC.md` | §6 |
| invariantes da máquina | `maquina_estados/SPEC.md` | §8 |
| LED, piscar, led_ligar, led_desligar, led_piscar, leds.h | `leds/SPEC.md` | §5 (API), §3 (Remote), §4 (Principal) |
| LED LINK, LED MOTOR, LED EMERGÊNCIA | `leds/SPEC.md` | §3.1 (tabela completa) |
| frequência LED, 1 Hz, 2 Hz, 4 Hz | `leds/SPEC.md` | §6 |
| LED compartilhado com relé | `leds/SPEC.md` | §2.1 |
| troubleshooting visual, diagnóstico LED | `leds/SPEC.md` | §8 |
| GPIO, pinout, pino, strapping pin, pull-up | `hardware_io/SPEC.md` | §8 (restrições), §9 (pull-up) |
| botão, entrada digital, debounce | `hardware_io/SPEC.md` | §4 (Principal), §6 (Remote) |
| relé, módulo relé, canais, ativo HIGH | `hardware_io/SPEC.md` | §5 (saídas Principal), §11 |
| alimentação, bateria, 18650, LM2596, TP4056 | `hardware_io/SPEC.md` | §3 |
| lista de materiais, BOM | `hardware_io/SPEC.md` | §14 |
| enclosure, IP54 | `hardware_io/SPEC.md` | §3.2 |
| logging, debug, Serial, logger.h, LOG_INFO, LOG_WARN | `README.md` | §11 (detalhes completos), `DESIGN_SPEC.md` §11 (resumo) |
| visão geral, arquitetura, mestre-escravo, glossário | `DESIGN_SPEC.md` | §1–§2 (arquitetura), §13 (glossário) |
| requisitos não-funcionais, latência, alcance | `DESIGN_SPEC.md` | §10 |
| fora de escopo v1.0 | `DESIGN_SPEC.md` | §12 |

---

## Resumo dos Arquivos

### `DESIGN_SPEC.md` — Especificação Geral (documento raiz)

Visão completa do sistema: arquitetura mestre-escravo, descrição dos módulos, regras de negócio, diagramas e glossário. **Leia primeiro** se precisar de contexto geral, ou se nenhuma spec específica cobre o que você procura.

### `seguranca/SPEC.md` — Segurança e Emergência

Hierarquia de prioridades de segurança, botões de emergência com trava, watchdog de comunicação, regra homem-morto, proteções de hardware e invariantes de segurança.

**Depende de:** `comunicacao/SPEC.md` (estrutura de pacotes para campo `emergencia`), `motor/SPEC.md` (relé de freio e motor).

### `comunicacao/SPEC.md` — Protocolo ESP-NOW

Structs `PacoteRemote` (9 bytes) e `PacoteStatus` (7 bytes), enum `Comando`, checksum XOR, frequências de envio, callbacks, watchdog e tolerância a falhas.

**Depende de:** `seguranca/SPEC.md` (watchdog, §4 para detalhes de timeout).

### `motor/SPEC.md` — Motor e Freio

Sinais de direção e velocidade entregues ao CLP, regra homem-morto, feedback da micro do freio, fim de curso e tabela de condições para envio de comandos ao CLP.

**Depende de:** `maquina_estados/SPEC.md` (estados resultantes), `comunicacao/SPEC.md` (sincronização de velocidade com Remote).

### `maquina_estados/SPEC.md` — Máquina de Estados

Documento histórico da máquina de estados central. Para a arquitetura atual, use `README.md` e `seguranca/SPEC.md` como fonte principal do comportamento do ESP32 bridge.

**Depende de:** `seguranca/SPEC.md` (condições de emergência e watchdog), `motor/SPEC.md` (ações de motor/freio em cada transição).

### `leds/SPEC.md` — Indicadores Visuais

5 LEDs no Remote (LINK, MOTOR, VEL1, VEL2, EMERGÊNCIA) + 1 LED no Principal (LINK REMOTE). Abstração `leds.h`, frequências padronizadas, pseudocódigo de atualização e troubleshooting visual.

**Depende de:** `comunicacao/SPEC.md` (campos do `PacoteStatus` para atualizar LEDs do Remote).

### `hardware_io/SPEC.md` — Hardware e I/O

2x ESP32 WROOM-32U, alimentação (rede e bateria), mapa de entradas/saídas (15 GPIOs Principal, 11 GPIOs Remote), restrições de pinout, pull-ups, sensor fim de curso, micro do freio e lista de materiais.

**Depende de:** nenhum (referência de base para todos os demais).

---

## Contagem de GPIOs (referência rápida)

| Módulo | Entradas | Saídas | Total |
|---|---|---|---|
| Principal | 7 | 8 | 15 |
| Remote | 6 ativas + 1 desabilitada | 5 | 11 |

---

## Grafo de Dependência entre Specs

```
hardware_io ─────────────────────────────────────────────────┐
  (base: pinout, elétrica, componentes)                      │
                                                             ▼
comunicacao ◄─────── seguranca ◄─────── maquina_estados ◄── motor
(pacotes,            (emergência,       (estados,            (relés,
 ESP-NOW,             watchdog,          transições,          freio,
 enums)               rearme)            pseudocódigo)        velocidade)
     │                                        │
     └──────────────── leds ◄─────────────────┘
                      (indicadores visuais, leds.h)
```

Leitura sugerida: `hardware_io` → `comunicacao` → `seguranca` → `motor` → `maquina_estados` → `leds`.
