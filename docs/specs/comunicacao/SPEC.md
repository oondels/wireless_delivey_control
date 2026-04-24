# Especificação de Comunicação (ESP-NOW)

**Versão:** 1.2
**Data:** 2026-04-24
**Referência:** README.md v4.0

---

## 1. Visão Geral

Os dois módulos ESP32 comunicam-se via **ESP-NOW**, protocolo peer-to-peer da Espressif que opera sem roteador Wi-Fi.

- O **Remote** envia comandos, heartbeat, estado do botão de emergência e fim de curso de descida.
- O **Principal** responde com um `PacoteStatus` contendo validade do link, feedbacks digitais do CLP e o estado da micro do freio.

A lógica de controle permanece no **CLP**. Os ESP32 funcionam como ponte de comunicação sem fio.

---

## 2. Características do ESP-NOW

| Propriedade | Valor |
|---|---|
| Protocolo | ESP-NOW (Espressif) |
| Topologia | Peer-to-peer direto |
| Dependência de roteador | Nenhuma |
| Banda | 2.4 GHz |
| Tamanho máximo do pacote | 250 bytes |
| Alcance mínimo exigido | 50 metros (linha de visada) |
| Latência esperada | < 20 ms por pacote |

---

## 3. Emparelhamento

- Ambos os módulos iniciam em modo de descoberta usando **broadcast** (`FF:FF:FF:FF:FF:FF`) como peer inicial.
- O MAC real do peer é detectado dinamicamente a partir do primeiro pacote válido recebido.
- Ao detectar o MAC real, o módulo registra o peer via `esp_now_add_peer()` e passa a enviar diretamente para aquele endereço.
- Não é necessário hardcodar MACs em firmware.

---

## 4. Estrutura dos Pacotes

### 4.1 Pacote Remote → Principal (`PacoteRemote`)

```c
typedef struct {
    uint8_t  comando;            // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                                 // 3=VEL1, 4=VEL2, 5=RESET
    uint8_t  botao_hold;         // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;         // 1=botão de emergência com trava ativo
    uint8_t  fim_curso_descida;  // 1=carrinho na posição final de descida
    uint32_t timestamp;          // millis() do Remote
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} PacoteRemote;
```

**Tamanho:** 9 bytes

| Campo | Tipo | Valores | Descrição |
|---|---|---|---|
| `comando` | `uint8_t` | 0–5 | Comando ativo no momento do envio |
| `botao_hold` | `uint8_t` | 0 ou 1 | 1 = botão SUBIR ou DESCER fisicamente pressionado |
| `emergencia` | `uint8_t` | 0 ou 1 | 1 = botão de emergência com trava ativo no Remote |
| `fim_curso_descida` | `uint8_t` | 0 ou 1 | 1 = carrinho na posição final de descida |
| `timestamp` | `uint32_t` | millis() | Timestamp do Remote para diagnóstico |
| `checksum` | `uint8_t` | calculado | XOR de todos os bytes anteriores do pacote |

### 4.2 Pacote Principal → Remote (`PacoteStatus`)

```c
typedef struct {
    uint8_t  link_ok;             // 1=Principal recebendo pacotes válidos do Remote
    uint8_t  motor_ativo;         // 1=CLP reporta motor ativo
    uint8_t  emergencia_ativa;    // 1=CLP reporta emergência ativa
    uint8_t  vel1_ativa;          // 1=CLP reporta velocidade 1 ativa
    uint8_t  vel2_ativa;          // 1=CLP reporta velocidade 2 ativa
    uint8_t  micro_freio_ativa;   // 1=freio ativo; 0=freio liberado
    uint8_t  checksum;            // XOR de todos os bytes anteriores
} PacoteStatus;
```

**Tamanho:** 7 bytes

| Campo | Tipo | Valores | Descrição |
|---|---|---|---|
| `link_ok` | `uint8_t` | 0 ou 1 | 1 = Principal recebendo pacotes válidos do Remote |
| `motor_ativo` | `uint8_t` | 0 ou 1 | 1 = feedback do CLP em LOW no `GPIO 23` |
| `emergencia_ativa` | `uint8_t` | 0 ou 1 | 1 = feedback do CLP em LOW no `GPIO 25` |
| `vel1_ativa` | `uint8_t` | 0 ou 1 | 1 = feedback do CLP em LOW no `GPIO 26` |
| `vel2_ativa` | `uint8_t` | 0 ou 1 | 1 = feedback do CLP em LOW no `GPIO 27` |
| `micro_freio_ativa` | `uint8_t` | 0 ou 1 | 1 = freio ativo reportado pela micro no `GPIO 14`; 0 = freio liberado |
| `checksum` | `uint8_t` | calculado | XOR de todos os bytes anteriores do pacote |

---

## 5. Enumerações do Protocolo

### 5.1 Comandos (`Comando`)

```c
typedef enum {
    CMD_HEARTBEAT = 0,
    CMD_SUBIR     = 1,
    CMD_DESCER    = 2,
    CMD_VEL1      = 3,
    CMD_VEL2      = 4,
    CMD_RESET     = 5
} Comando;
```

> `CMD_RESET` substituiu o antigo `CMD_VEL3`.

---

## 6. Checksum

### 6.1 Algoritmo

XOR simples de todos os bytes do pacote, excluindo o próprio campo checksum.

```c
uint8_t calcular_checksum(const uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}
```

### 6.2 Validação no Receptor

- O **Principal** valida o checksum de todo `PacoteRemote` recebido.
- O **Remote** valida o checksum de todo `PacoteStatus` recebido.
- Pacotes com checksum inválido são descartados silenciosamente.
- Pacotes descartados não resetam watchdog nem timer de link.

---

## 7. Frequência e Timing de Envio

### 7.1 Remote → Principal

| Condição | Frequência |
|---|---|
| Nenhum botão pressionado (heartbeat) | A cada **100 ms** |
| Mudança de estado de botão/sensor | **Imediato** + repetir a cada 100 ms enquanto ativo |

### 7.2 Principal → Remote

| Condição | Frequência |
|---|---|
| Status periódico | A cada **200 ms** |
| Mudança em qualquer campo do status | **Imediato** (além do periódico) |

---

## 8. Watchdog e Validade do Link

| Parâmetro | Valor |
|---|---|
| Timeout do Principal | **500 ms** (`WATCHDOG_TIMEOUT_MS`) |
| Heartbeat do Remote | **100 ms** (`HEARTBEAT_INTERVALO_MS`) |
| Timeout de validade do status no Remote | **500 ms** |

- O **Principal** considera a comunicação perdida quando não recebe pacote válido do Remote por mais de 500 ms.
- Ao expirar o watchdog, o Principal:
  - força `PIN_CLP_EMERGENCIA` para LOW
  - para sinais de movimento
  - passa a enviar `link_ok = 0` no `PacoteStatus`
- O **Remote** considera o status inválido quando:
  - `link_ok == 0`, ou
  - o último `PacoteStatus` recebido tem mais de 500 ms

---

## 9. Callbacks ESP-NOW

### 9.1 Remote

| Callback | Função |
|---|---|
| `OnDataSent` | Registrar confirmação de entrega para debug |
| `OnDataRecv` | Validar checksum, atualizar `PacoteStatus`, atualizar timestamp do último status |

### 9.2 Principal

| Callback | Função |
|---|---|
| `OnDataRecv` | Validar checksum, atualizar MAC do peer, resetar watchdog e armazenar o último `PacoteRemote` |

---

## 10. Hierarquia de Comando

- O Principal continua sendo o ponto de intermediação com o CLP.
- O Remote pode sempre enviar heartbeat, emergência, fim de curso e comandos de pulso.
- O Remote **bloqueia** `SUBIR` e `DESCER` localmente quando:
  - o status do Principal expira, ou
  - o botão de emergência local está ativo, ou
  - `emergencia_ativa == 1` no `PacoteStatus`

---

## 11. Tolerância a Falhas

| Cenário | Comportamento |
|---|---|
| Pacote corrompido (checksum inválido) | Descartado; watchdog/timer não resetado |
| Pacote duplicado | Processado normalmente |
| Pacote fora de ordem | Processado normalmente |
| Perda total de comunicação | Watchdog do Principal aciona em 500 ms |
| Remote fora de alcance | Watchdog do Principal aciona em 500 ms |
| Freio ativo ou circuito NC aberto | `micro_freio_ativa = 1` no status enviado ao Remote |
