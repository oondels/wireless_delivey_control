# Módulo Principal

Firmware do ESP32 instalado no painel fixo. Este módulo faz a ponte entre o `Remote` e o CLP:

- recebe `PacoteRemote` por ESP-NOW
- valida MAC, checksum, autenticação e anti-replay
- replica sinais para o CLP em GPIO ativo em LOW
- lê feedbacks do CLP e da micro do freio
- devolve `PacoteStatus` ao `Remote`

## Responsabilidades

- manter `SUBIR` e `DESCER` em nível estável enquanto o hold remoto continuar válido
- gerar pulsos de 50 ms para `VEL1`, `VEL2` e `RESET`
- acionar `PIN_CLP_EMERGENCIA` em LOW se a emergência remota estiver ativa ou o watchdog expirar
- bloquear movimento remoto quando houver:
  - watchdog expirado
  - emergência remota
  - `EMERGENCIA_ATIVA` do CLP
  - `micro_freio_ativa == 1`
  - ausência de `MOTOR_ATIVO`
- oferecer botões de teste local em `GPIO 32` e `GPIO 33`

## GPIOs

### Saídas

| Sinal | GPIO | Comportamento |
|---|---|---|
| `PIN_CLP_SUBIR` | 4 | LOW estável durante hold remoto/local |
| `PIN_CLP_DESCER` | 16 | LOW estável durante hold remoto/local |
| `PIN_CLP_VEL1` | 17 | Pulso LOW de 50 ms |
| `PIN_CLP_VEL2` | 5 | Pulso LOW de 50 ms |
| `PIN_CLP_EMERGENCIA` | 18 | LOW em emergência remota ou watchdog expirado |
| `PIN_CLP_RESET` | 19 | Pulso LOW de 50 ms |
| `PIN_CLP_FIM_CURSO` | 22 | LOW quando o Remote reporta fim de curso descida |
| `PIN_LED_LINK` | 21 | Fixo com link válido; pisca 2 Hz sem link |

### Entradas

| Sinal | GPIO | Leitura |
|---|---|---|
| `PIN_BTN_TESTE_SUBIR` | 32 | `INPUT_PULLUP`, LOW = pressionado |
| `PIN_BTN_TESTE_DESCER` | 33 | `INPUT_PULLUP`, LOW = pressionado |
| `PIN_FB_MOTOR_ATIVO` | 23 | LOW = ativo |
| `PIN_FB_EMERGENCIA_ATIVA` | 25 | LOW = ativo |
| `PIN_FB_VEL1_ATIVA` | 26 | LOW = ativo |
| `PIN_FB_VEL2_ATIVA` | 27 | LOW = ativo |
| `PIN_MICRO_FREIO` | 14 | HIGH = freio ativo |

## Comunicação

- peer fixo via ESP-NOW criptografado
- depende de `.env` na raiz do repositório
- chaves e MACs são carregados no build por `../tools/load_security_env.py`

Campos do `.env` usados aqui:

- `REMOTE_MAC`
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
