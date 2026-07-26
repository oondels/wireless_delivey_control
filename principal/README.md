# Módulo Principal

Firmware do ESP32 instalado no painel fixo. Este módulo faz a ponte entre o `Remote` e o CLP:

- recebe `PacoteRemote` por ESP-NOW
- valida MAC físico, origem lógica, rota, checksum, autenticação e anti-replay
- replica sinais para o CLP em GPIO ativo em LOW
- lê feedbacks do CLP e da micro do freio
- devolve `PacoteStatus` ao `Remote` direto e, quando habilitado, via `Repeater`

## Responsabilidades

- manter `SUBIR` e `DESCER` em nível estável enquanto o hold remoto continuar válido
- gerar pulsos de 50 ms para `VEL1`, `VEL2` e `RESET`
- acionar `PIN_CLP_EMERGENCIA` em LOW se a emergência remota estiver ativa ou a perda de sinal exceder o timeout configurável
- bloquear movimento remoto quando houver:
  - watchdog expirado para movimento; perda prolongada configurável para emergência
  - emergência remota
  - `EMERGENCIA_ATIVA` do CLP
  - `micro_freio_ativa == 1`
- tratar `MOTOR_ATIVO` apenas como telemetria; esse feedback não bloqueia `SUBIR`/`DESCER`

Com `ONLY_EMERGENCY_MODE=true` no `.env`, este módulo:

- lê o botão local de emergência NC em `GPIO 33`
- ignora movimento, velocidade, reset e fim de curso
- mantém todas as saídas de controle em HIGH, exceto `PIN_CLP_EMERGENCIA`
- não lê feedbacks do CLP nem micro do freio para montar status
- reporta apenas `link_ok` real no `PacoteStatus`
- mantém emergência ativa se o link cair depois de receber `emergencia = 1`
- usa `GPIO 26` para LED LINK e `GPIO 27` para LED EMERGÊNCIA

## GPIOs

### Saídas

| Sinal | GPIO | Comportamento |
|---|---|---|
| `PIN_CLP_SUBIR` | 4 | LOW estável durante hold remoto |
| `PIN_CLP_DESCER` | 16 | LOW estável durante hold remoto |
| `PIN_CLP_VEL1` | 17 | Pulso LOW de 50 ms |
| `PIN_CLP_VEL2` | 5 | Pulso LOW de 50 ms |
| `PIN_CLP_EMERGENCIA` | 18 | LOW em emergência remota ou perda de sinal prolongada |
| `PIN_CLP_RESET` | 19 | Pulso LOW de 50 ms |
| `PIN_CLP_FIM_CURSO` | 22 | Temporariamente desabilitado; mantido HIGH |
| `PIN_LED_LINK` | 21 | Fixo com link válido; pisca 2 Hz sem link |

No modo `ONLY_EMERGENCY_MODE=true`, `PIN_LED_LINK` usa `GPIO 26` e `PIN_LED_EMERGENCIA` usa `GPIO 27`.

### Entradas

| Sinal | GPIO | Leitura |
|---|---|---|
| `PIN_FB_MOTOR_ATIVO` | 23 | LOW = ativo |
| `PIN_FB_EMERGENCIA_ATIVA` | 33 | LOW = ativo |
| `PIN_FB_VEL1_ATIVA` | 26 | LOW = ativo |
| `PIN_FB_VEL2_ATIVA` | 27 | LOW = ativo |
| `PIN_MICRO_FREIO` | 14 | HIGH = freio ativo |

No modo `ONLY_EMERGENCY_MODE=true`, estes feedbacks não são lidos. `GPIO 33` passa a ser `PIN_BTN_EMERGENCIA_LOCAL`, com contato NC: LOW = repouso e HIGH = emergência ativa.

## Comunicação

- peer fixo via ESP-NOW criptografado
- aceita rota direta do Remote e rota via Repeater quando `ENABLE_REPEATER_ROUTE=true`
- `PKT_LINK_PROBE` responde com ACK, mas não reseta watchdog
- depende de `.env` na raiz do repositório
- chaves e MACs são carregados no build por `../tools/load_security_env.py`

Campos do `.env` usados aqui:

- `REMOTE_MAC`
- `REPEATER_MAC` (quando `ENABLE_REPEATER_ROUTE=true`)
- `ENABLE_REPEATER_ROUTE`
- `ONLY_EMERGENCY_MODE`
- `ESPNOW_PMK`
- `ESPNOW_LMK`

## Estrutura do código

- [src/principal.cpp](/home/oendel/code/hendrius/automacao_rio/principal/src/principal.cpp:1): loop principal, bloqueios e saídas para o CLP
- [src/comunicacao.cpp](/home/oendel/code/hendrius/automacao_rio/principal/src/comunicacao.cpp:1): ESP-NOW, autenticação e anti-replay
- [src/watchdog_comm.cpp](/home/oendel/code/hendrius/automacao_rio/principal/src/watchdog_comm.cpp:1): watchdog de comunicação
- [src/leds.cpp](/home/oendel/code/hendrius/automacao_rio/principal/src/leds.cpp:1): abstração do LED LINK

## Build

```bash
cd principal
pio run
```

Modo desenvolvimento:

```bash
cd principal
PLATFORMIO_BUILD_FLAGS='-DAPP_ENV_DEV' pio run
```

## Logs de boot

No boot, este módulo sempre registra:

- banner de inicialização
- MAC local do ESP
- MAC do peer remoto configurado

Os logs detalhados de transição (`INFO`) só aparecem no modo `dev`.
