# Especificação de Comunicação (ESP-NOW)

**Versão:** 1.3
**Data:** 2026-05-24
**Referência:** README.md v4.0

---

## 1. Visão Geral

Os módulos ESP32 comunicam-se via **ESP-NOW** com peers fixos, criptografia habilitada e sem broadcast para comandos críticos.

- O **Remote** envia comandos, heartbeat, emergência local e probes de link.
- O **Principal** recebe comandos válidos, alimenta o watchdog e envia `PacoteStatus`.
- O **Repeater** opcional apenas valida e encaminha pacotes autenticados entre Remote e Principal.
- A lógica de motor, freio, estados e segurança permanece no **CLP**.

Rotas suportadas:

- Direta: `Remote -> Principal` e `Principal -> Remote`
- Via Repeater: `Remote -> Repeater -> Principal` e `Principal -> Repeater -> Remote`

---

## 2. Emparelhamento

- `PRINCIPAL_MAC`, `REMOTE_MAC`, `REPEATER_MAC`, `ESPNOW_PMK` e `ESPNOW_LMK` são carregados do `.env`.
- `REPEATER_MAC` é obrigatório quando `ENABLE_REPEATER_ROUTE=true` ou ao compilar `repeater/`.
- Cada peer é registrado com `encrypt = true` e LMK configurada.
- A PMK é configurada no boot via `esp_now_set_pmk()`.
- Pacotes de MAC físico desconhecido são rejeitados.

---

## 3. Estrutura dos Pacotes

Todos os pacotes carregam um cabeçalho autenticado:

```c
typedef struct {
    uint8_t tipo;       // PKT_REMOTE_CMD, PKT_STATUS, PKT_LINK_PROBE, PKT_LINK_ACK
    uint8_t origem;     // NODE_REMOTE, NODE_PRINCIPAL, NODE_REPEATER
    uint8_t destino;    // destino lógico final
    uint8_t rota;       // ROUTE_DIRECT, ROUTE_VIA_REPEATER
    uint8_t hop_count;  // 0 direto, 1 via Repeater
} CabecalhoPacote;
```

`PacoteRemote` adiciona esse cabeçalho antes dos campos já existentes (`comando`, `botao_hold`, `emergencia`, `fim_curso_descida`, `timestamp`, `seq`, `session_id`, `auth_tag`, `checksum`).

`PacoteStatus` adiciona esse cabeçalho antes dos feedbacks do CLP (`link_ok`, `motor_ativo`, `emergencia_ativa`, `vel1_ativa`, `vel2_ativa`, `micro_freio_ativa`, `seq`, `session_id`, `auth_tag`, `checksum`).

`PacoteLink` usa o mesmo cabeçalho para `PKT_LINK_PROBE` e `PKT_LINK_ACK`. Esses pacotes medem qualidade de link e **não** resetam watchdog nem alteram estado operacional.

---

## 4. Validação

Todo receptor valida, nesta ordem:

1. MAC físico esperado
2. tamanho/tipo do pacote
3. origem, destino, rota e `hop_count`
4. checksum XOR
5. `auth_tag`
6. anti-replay por `session_id/seq`

Regras específicas:

- Principal aceita comando direto apenas do MAC do Remote com `origem=NODE_REMOTE`, `destino=NODE_PRINCIPAL`, `rota=ROUTE_DIRECT`, `hop_count=0`.
- Principal aceita comando via Repeater apenas do MAC do Repeater com `origem=NODE_REMOTE`, `destino=NODE_PRINCIPAL`, `rota=ROUTE_VIA_REPEATER`, `hop_count=1`.
- Repeater aceita `PKT_REMOTE_CMD` somente do Remote para o Principal.
- Repeater aceita `PKT_STATUS` somente do Principal para o Remote.
- Remote aceita status direto do Principal e status via Repeater separadamente, para medir as duas rotas.
- Pacotes inválidos não resetam watchdog nem atualizam link.

---

## 5. Seleção Preventiva de Rota

O Remote mantém métricas separadas para rota direta e via Repeater:

- status autenticado recente
- `PKT_LINK_ACK` recente
- sucesso/falha de envio ESP-NOW
- RSSI suavizado quando disponível em `esp_now_recv_info_t.rx_ctrl`

Constantes iniciais:

- `LINK_QUALITY_TIMEOUT_MS = 500`
- `LINK_PROBE_INTERVALO_MS = 250`
- `ROUTE_SWITCH_SCORE_MARGIN = 10`
- `ROUTE_SWITCH_CONSECUTIVE_SAMPLES = 2`
- `LINK_SWITCH_MARGIN_DB = 8`
- `LINK_RSSI_MIN_DBM = -82`

O Remote troca a rota ativa no próximo envio quando a rota candidata está operacional e supera a rota atual por margem de score. Ele não espera a rota atual expirar.

---

## 6. Frequência e Timing

| Direção | Condição | Frequência |
|---|---|---|
| Remote -> Principal | Heartbeat/comando | A cada 100 ms ou mudança imediata |
| Principal -> Remote | Status | A cada 200 ms ou mudança imediata |
| Remote -> peers | Link probe | A cada 250 ms |

---

## 7. Tolerância a Falhas

| Cenário | Comportamento |
|---|---|
| Pacote corrompido | Descartado |
| MAC desconhecido | Descartado |
| Origem lógica inválida | Descartado |
| Replay/duplicata | Descartado por `seq/session_id` |
| Repeater cai | Remote tenta rota direta se operacional; senão bloqueia movimento |
| Perda total | Principal aciona watchdog; Remote bloqueia `SUBIR`/`DESCER` |
