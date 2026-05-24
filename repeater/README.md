# Módulo Repeater

Firmware do ESP32 usado como ponte ESP-NOW entre `Remote` e `Principal`.

## Responsabilidades

- registrar `REMOTE_MAC` e `PRINCIPAL_MAC` como peers criptografados
- receber `PKT_REMOTE_CMD` do Remote e encaminhar ao Principal
- receber `PKT_STATUS` do Principal e encaminhar ao Remote
- responder `PKT_LINK_PROBE` local do Remote com `PKT_LINK_ACK`
- rejeitar MAC físico desconhecido, direção inválida, checksum inválido, `auth_tag` inválido e replay

O Repeater não decide movimento, emergência, freio ou fim de curso. Ele não mantém último comando válido e não reenvia comandos antigos fora do recebimento atual.

## Comunicação

Campos do `.env` usados aqui:

- `REMOTE_MAC`
- `PRINCIPAL_MAC`
- `REPEATER_MAC`
- `ESPNOW_PMK`
- `ESPNOW_LMK`

## Build

```bash
cd repeater
pio run
```

Modo desenvolvimento:

```bash
cd repeater
PLATFORMIO_BUILD_FLAGS='-DAPP_ENV_DEV' pio run
```

## Logs

Em modo `dev`, o Repeater registra pacotes recebidos, encaminhamentos, rejeições por validação e falhas de envio. Em produção, ficam visíveis logs `WARN`, `ERRO` e logs essenciais de boot.
