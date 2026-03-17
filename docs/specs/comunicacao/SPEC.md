# Especificação de Comunicação (ESP-NOW)

**Versão:** 1.0
**Data:** 2026-03-16
**Referência:** DESIGN_SPEC.md v3.1

---

## 1. Visão Geral

Os dois módulos ESP32 comunicam-se via **ESP-NOW**, protocolo peer-to-peer da Espressif que opera sem roteador Wi-Fi. A comunicação é **bidirecional**: o Remote envia comandos e heartbeat; o Principal responde com status do sistema.

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

- O endereço MAC do Módulo Principal é **fixado em firmware** no código do Remote.
- Não há descoberta automática de peers.
- Ambos os módulos registram o peer na inicialização via `esp_now_add_peer()`.

---

## 4. Estrutura dos Pacotes

### 4.1 Pacote Remote → Principal (`PacoteRemote`)

```c
typedef struct {
    uint8_t  comando;       // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                            // 3=VEL1, 4=VEL2, 5=VEL3
    uint8_t  botao_hold;    // 1=SUBIR ou DESCER pressionado (Homem-Morto)
    uint8_t  emergencia;    // 1=botão de emergência com trava ativo no Remote
    uint32_t timestamp;     // millis() do Remote
    uint8_t  checksum;      // XOR de todos os bytes anteriores
} PacoteRemote;
```

**Tamanho:** 8 bytes

| Campo | Tipo | Valores | Descrição |
|---|---|---|---|
| `comando` | `uint8_t` | 0–5 | Comando ativo no momento do envio |
| `botao_hold` | `uint8_t` | 0 ou 1 | 1 = botão SUBIR ou DESCER fisicamente pressionado |
| `emergencia` | `uint8_t` | 0 ou 1 | 1 = botão de emergência com trava ativo no Remote |
| `timestamp` | `uint32_t` | millis() | Timestamp do Remote para diagnóstico |
| `checksum` | `uint8_t` | calculado | XOR de todos os bytes anteriores do pacote |

### 4.2 Pacote Principal → Remote (`PacoteStatus`)

```c
typedef struct {
    uint8_t  estado_sistema; // 0=PARADO, 1=SUBINDO, 2=DESCENDO,
                             // 3=EMERGENCIA_ATIVA, 4=FALHA_COMUNICACAO
    uint8_t  velocidade;     // 1, 2 ou 3
    uint8_t  trava_logica;   // 1=trava ativa (motor bloqueado)
    uint8_t  rearme_ativo;   // 1=Painel fez rearme com botão Remote ainda travado
    uint8_t  checksum;
} PacoteStatus;
```

**Tamanho:** 5 bytes

| Campo | Tipo | Valores | Descrição |
|---|---|---|---|
| `estado_sistema` | `uint8_t` | 0–4 | Estado atual da máquina de estados |
| `velocidade` | `uint8_t` | 1, 2 ou 3 | Nível de velocidade ativo |
| `trava_logica` | `uint8_t` | 0 ou 1 | 1 = movimentação bloqueada por software |
| `rearme_ativo` | `uint8_t` | 0 ou 1 | 1 = rearme feito com emergência Remote ainda travada |
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
    CMD_VEL3      = 5
} Comando;
```

### 5.2 Estados do Sistema (`EstadoSistema`)

```c
typedef enum {
    ESTADO_PARADO            = 0,
    ESTADO_SUBINDO           = 1,
    ESTADO_DESCENDO          = 2,
    ESTADO_EMERGENCIA        = 3,
    ESTADO_FALHA_COMUNICACAO = 4
} EstadoSistema;
```

---

## 6. Checksum

### 6.1 Algoritmo

XOR simples de todos os bytes do pacote, excluindo o próprio campo checksum.

```c
uint8_t calcular_checksum(uint8_t* data, size_t len) {
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}
```

### 6.2 Validação no Receptor

- O Principal **deve** validar o checksum de todo `PacoteRemote` recebido.
- Pacotes com checksum inválido são **descartados silenciosamente**.
- Pacotes descartados **não** resetam o timer do watchdog.

---

## 7. Frequência e Timing de Envio

### 7.1 Remote → Principal

| Condição | Frequência |
|---|---|
| Nenhum botão pressionado (heartbeat) | A cada **200 ms** |
| Mudança de estado de botão | **Imediato** + repetir a cada 200 ms enquanto ativo |

### 7.2 Principal → Remote

| Condição | Frequência |
|---|---|
| Status periódico | A cada **200 ms** |
| Mudança de estado do sistema | **Imediato** (além do periódico) |

---

## 8. Watchdog de Comunicação

| Parâmetro | Valor |
|---|---|
| Timeout | **500 ms** (`WATCHDOG_TIMEOUT_MS`) |
| Heartbeat do Remote | **200 ms** |
| Margem de segurança | 2.5x o intervalo de heartbeat |

- Verificado a cada ciclo do loop principal.
- Ao expirar: motor OFF → freio ON → `FALHA_COMUNICACAO`.
- Recuperação exige **rearme manual** no Painel Central.

Ver detalhes completos em `docs/specs/seguranca/SPEC.md`, seção 4.

---

## 9. Callbacks ESP-NOW

### 9.1 Remote

| Callback | Função |
|---|---|
| `OnDataSent` | Registrar confirmação de entrega (debug) |
| `OnDataRecv` | Atualizar variáveis locais: `estado_sistema`, `velocidade`, `rearme_ativo`; resetar timer de link |

### 9.2 Principal

| Callback | Função |
|---|---|
| `OnDataRecv` | Validar checksum; resetar watchdog (`_pWatchdog->resetar()`); processar emergência imediatamente se `emergencia == 1` (`_pEmergencia->ativa() = true`) |

---

## 10. Hierarquia de Comando

O Painel Central tem **autoridade máxima** sobre o sistema:

- Comandos de direção e velocidade do Painel Central têm **prioridade** sobre os do Remote.
- O Painel Central pode ativar **e** desativar emergências, incluindo emergências originadas no Remote.
- O Remote **nunca** pode desativar uma emergência por conta própria — apenas solicitar seu acionamento.

---

## 11. Tolerância a Falhas

| Cenário | Comportamento |
|---|---|
| Pacote corrompido (checksum inválido) | Descartado; watchdog não resetado |
| Pacote duplicado | Processado normalmente (idempotente) |
| Pacote fora de ordem | Processado normalmente (sem controle de sequência) |
| Perda total de comunicação | Watchdog aciona em 500 ms |
| Remote fora de alcance | Watchdog aciona em 500 ms |
