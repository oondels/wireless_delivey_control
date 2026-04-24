# Módulo Remote

Firmware do ESP32 embarcado no carrinho. Este módulo lê botões e sensor local, envia comandos ao `Principal` e traduz o `PacoteStatus` em bloqueios e LEDs para o operador.

## Responsabilidades

- ler botões `SUBIR`, `DESCER`, `VEL1`, `VEL2`, `EMERGÊNCIA`
- monitorar o fim de curso de descida em `GPIO 36`
- enviar `PacoteRemote` por ESP-NOW a cada 100 ms ou imediatamente em mudanças
- bloquear `SUBIR` e `DESCER` quando:
  - status do Principal expirou
  - `link_ok == 0`
  - emergência local está ativa
  - `emergencia_ativa == 1` recebido do Principal
- atualizar LEDs de `LINK`, `MOTOR`, `VEL1`, `VEL2` e `EMERGÊNCIA`

## GPIOs

### Entradas

| Sinal | GPIO | Leitura |
|---|---|---|
| `PIN_BTN_SUBIR` | 32 | `INPUT_PULLUP`, LOW = pressionado |
| `PIN_BTN_DESCER` | 33 | `INPUT_PULLUP`, LOW = pressionado |
| `PIN_BTN_VEL1` | 39 | pull-up externo, LOW = pressionado |
| `PIN_BTN_VEL2` | 34 | pull-up externo, LOW = pressionado |
| `PIN_BTN_RESET` | 255 | desabilitado nesta versão |
| `PIN_BTN_EMERGENCIA` | 13 | NC, HIGH = ativo |
| `PIN_FIM_CURSO_DESCIDA` | 36 | pull-up externo, LOW = acionado |

### Saídas

| LED | GPIO | Comportamento |
|---|---|---|
| `PIN_LED_LINK` | 4 | Fixo com status válido; pisca 1 Hz sem link |
| `PIN_LED_MOTOR` | 16 | Pisca 2 Hz aguardando partida; fixo com `motor_ativo == 1` |
| `PIN_LED_VEL1` | 17 | Reflete `vel1_ativa` |
| `PIN_LED_VEL2` | 5 | Reflete `vel2_ativa` |
| `PIN_LED_EMERGENCIA` | 19 | Pisca 4 Hz com emergência local ou `emergencia_ativa` |

## Regras operacionais

- `SUBIR` e `DESCER` são hold
- `VEL1` e `VEL2` são pulsos por borda
- o LED `MOTOR` pisca enquanto existe solicitação de movimento, mas o sistema ainda aguarda:
  - `micro_freio_ativa == 0`
  - `motor_ativo == 1`
- o sensor de fim de curso mantém bloqueio lógico por 10 s após a liberação física

## Comunicação

- peer fixo via ESP-NOW criptografado
- depende de `.env` na raiz do repositório
- chaves e MACs são carregados no build por `../tools/load_security_env.py`

Campos do `.env` usados aqui:

- `PRINCIPAL_MAC`
- `ESPNOW_PMK`
- `ESPNOW_LMK`

## Estrutura do código

- [src/remote.cpp](/home/oendel/code/hendrius/automacao_rio/remote/src/remote.cpp:1): loop principal, montagem de `PacoteRemote` e bloqueios locais
- [src/comunicacao.cpp](/home/oendel/code/hendrius/automacao_rio/remote/src/comunicacao.cpp:1): ESP-NOW, validação de `PacoteStatus`, autenticação e anti-replay
- [src/botoes.cpp](/home/oendel/code/hendrius/automacao_rio/remote/src/botoes.cpp:1): debounce e leitura dos botões
- [src/fim_curso.cpp](/home/oendel/code/hendrius/automacao_rio/remote/src/fim_curso.cpp:1): debounce e retenção pós-liberação do fim de curso
- [src/atualizar_leds.cpp](/home/oendel/code/hendrius/automacao_rio/remote/src/atualizar_leds.cpp:1): lógica dos LEDs

## Build

```bash
cd remote
pio run
```

Modo desenvolvimento:

```bash
cd remote
PLATFORMIO_BUILD_FLAGS='-DAPP_ENV_DEV' pio run
```

## Logs de boot

No boot, este módulo sempre registra:

- banner de inicialização
- MAC local do ESP
- MAC do peer principal configurado

Os logs detalhados de botões, bloqueios e transições só aparecem no modo `dev`.
