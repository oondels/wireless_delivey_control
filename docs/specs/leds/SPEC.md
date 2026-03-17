# Especificação de Indicadores Visuais (LEDs)

**Versão:** 1.0
**Data:** 2026-03-16
**Referência:** DESIGN_SPEC.md v3.1

---

## 1. Visão Geral

Todos os LEDs são componentes discretos de **3V (padrão Arduino)** com cor física definida externamente no momento da montagem. O firmware controla apenas o **estado lógico** de cada GPIO: ligado (`HIGH`), desligado (`LOW`) ou piscando em uma frequência específica.

**Não há controle de cor por software.** Cada LED corresponde a **exatamente 1 GPIO de saída** no ESP32.

---

## 2. Categorias de LEDs

### 2.1 LEDs Compartilhados com Relés (Módulo Principal)

Estes LEDs estão conectados em paralelo com o módulo relé no mesmo GPIO. Acendem/apagam automaticamente quando o relé é acionado/desacionado. **Não** utilizam a abstração `Led` do firmware.

| LED | Função associada |
|---|---|
| LED DIREÇÃO A | Relé SUBIR |
| LED DIREÇÃO B | Relé DESCER |
| LED VEL1 | Relé velocidade 1 |
| LED VEL2 | Relé velocidade 2 |
| LED VEL3 | Relé velocidade 3 |
| LED FREIO | Relé freio |

**Total: 6 LEDs compartilhados**

### 2.2 LEDs Exclusivos com Abstração de Software

Estes LEDs são controlados pelo módulo `leds.h` e suportam piscar não-bloqueante.

| Módulo | LED | Total |
|---|---|---|
| Principal | LINK REMOTE | 1 |
| Remote | LINK, MOTOR, VEL1, VEL2, VEL3, EMERGÊNCIA, ALARME | 7 |

**Total: 8 LEDs com abstração**

---

## 3. LEDs do Módulo Remote

### 3.1 Tabela Completa

| LED | Comportamento | Condição | Frequência |
|---|---|---|---|
| LINK | Piscando | Sem comunicação com o Principal (> 1000 ms sem status) | 1 Hz (500 ms) |
| LINK | Ligado fixo | Comunicação ativa | — |
| MOTOR | Ligado fixo | `estado_sistema == SUBINDO` ou `DESCENDO` | — |
| MOTOR | Desligado | Qualquer outro estado | — |
| VEL1 | Ligado fixo | `velocidade == 1` (recebido no PacoteStatus) | — |
| VEL2 | Ligado fixo | `velocidade == 2` (recebido no PacoteStatus) | — |
| VEL3 | Ligado fixo | `velocidade == 3` (recebido no PacoteStatus) | — |
| EMERGÊNCIA | Piscando | `estado_sistema == EMERGENCIA_ATIVA` | 4 Hz (125 ms) |
| EMERGÊNCIA | Ligado fixo | `estado_sistema == FALHA_COMUNICACAO` | — |
| EMERGÊNCIA | Desligado | Estado normal (`PARADO`, `SUBINDO`, `DESCENDO`) | — |
| ALARME | Piscando | `rearme_ativo == 1` **E** botão emergência local ainda travado (HIGH) | 2 Hz (250 ms) |
| ALARME | Desligado | Qualquer outra condição | — |

**Total: 7 GPIOs de saída no Remote**

### 3.2 Lógica de Atualização

Todos os LEDs do Remote são atualizados com base no `PacoteStatus` recebido do Principal:

```
atualizar_leds(status):
    // LINK
    if (millis() - ultimo_status_recebido > 1000):
        led_piscar(LED_LINK, 500)    // 1 Hz
    else:
        led_ligar(LED_LINK)

    // MOTOR
    if (status.estado_sistema == SUBINDO || status.estado_sistema == DESCENDO):
        led_ligar(LED_MOTOR)
    else:
        led_desligar(LED_MOTOR)

    // VELOCIDADE
    led_desligar(LED_VEL1, LED_VEL2, LED_VEL3)
    if (status.velocidade == 1): led_ligar(LED_VEL1)
    if (status.velocidade == 2): led_ligar(LED_VEL2)
    if (status.velocidade == 3): led_ligar(LED_VEL3)

    // EMERGÊNCIA
    if (status.estado_sistema == EMERGENCIA):
        led_piscar(LED_EMERGENCIA, 125)   // 4 Hz
    elif (status.estado_sistema == FALHA_COMUNICACAO):
        led_ligar(LED_EMERGENCIA)
    else:
        led_desligar(LED_EMERGENCIA)

    // ALARME
    if (status.rearme_ativo == 1 && digitalRead(PIN_EMERGENCIA) == HIGH):
        led_piscar(LED_ALARME, 250)   // 2 Hz
    else:
        led_desligar(LED_ALARME)
```

---

## 4. LEDs do Módulo Principal

### 4.1 LEDs Compartilhados com Relés

Controlados diretamente pelo acionamento dos relés — sem lógica de LED separada.

| LED | Comportamento | Condição |
|---|---|---|
| VEL1 | Ligado fixo | `velocidade_atual == 1` (relé VEL1 ativo) |
| VEL2 | Ligado fixo | `velocidade_atual == 2` (relé VEL2 ativo) |
| VEL3 | Ligado fixo | `velocidade_atual == 3` (relé VEL3 ativo) |
| DIREÇÃO A | Ligado fixo | Motor SUBINDO (relé DIREÇÃO A ativo) |
| DIREÇÃO B | Ligado fixo | Motor DESCENDO (relé DIREÇÃO B ativo) |
| FREIO | Ligado fixo | Freio acionado (relé FREIO ativo) |

### 4.2 LED Exclusivo

| LED | Comportamento | Condição |
|---|---|---|
| LINK REMOTE | Ligado fixo | Comunicação com Remote ativa (watchdog OK) |
| LINK REMOTE | Desligado | Watchdog expirado ou Remote ausente |

**Total no Painel Central: 6 compartilhados + 1 exclusivo = 7 GPIOs de saída**

---

## 5. Abstração de Software (`leds.h`)

### 5.1 Estrutura de Dados

```c
typedef struct {
    uint8_t  gpio;
    bool     piscando;
    uint16_t intervalo_ms;
    uint32_t ultimo_toggle;
    bool     estado_atual;
} Led;
```

### 5.2 API

```c
void led_ligar(Led* led);       // GPIO HIGH, piscando = false
void led_desligar(Led* led);    // GPIO LOW, piscando = false
void led_piscar(Led* led, uint16_t intervalo_ms);  // Inicia piscar
void led_atualizar(Led* led);   // Chamar no loop principal
```

### 5.3 Regras de Implementação

- **Nunca** usar `delay()` — toda lógica de temporização via `millis()`.
- `led_atualizar()` deve ser chamada a cada ciclo do loop principal para **todos** os LEDs.
- A função verifica se `millis() - ultimo_toggle >= intervalo_ms` e inverte o GPIO se necessário.
- LEDs compartilhados com relés no Principal **não** usam esta abstração.

---

## 6. Frequências Padronizadas

| Frequência | Intervalo | Uso |
|---|---|---|
| 1 Hz | 500 ms | LINK sem conexão |
| 2 Hz | 250 ms | ALARME (rearme com botão Remote travado) |
| 4 Hz | 125 ms | EMERGÊNCIA ativa |

---

## 7. Especificações Elétricas

| Parâmetro | Valor |
|---|---|
| Tensão do LED | 3V (padrão Arduino) |
| Resistor limitador | 220Ω (um por LED) |
| Corrente máxima por GPIO | 12 mA |
| Cor do LED | Definida pelo componente físico na montagem |

> **Se a corrente do GPIO ultrapassar 12 mA** (especialmente nos LEDs compartilhados com relés no Principal), utilizar transistor NPN (ex: BC547) para isolar a carga.

---

## 8. Troubleshooting Visual

Guia rápido de diagnóstico baseado nos LEDs do Remote:

| Sintoma | Significado | Ação |
|---|---|---|
| LINK piscando (1 Hz) | Sem comunicação com o Principal | Verificar alcance, ligação do Principal |
| EMERGÊNCIA piscando (4 Hz) | Emergência ativa | Rearmar no Painel Central |
| EMERGÊNCIA fixo | Falha de comunicação | Verificar link + rearmar no Painel |
| ALARME piscando (2 Hz) | Rearme feito com botão local travado | Soltar botão de emergência no carrinho |
| Nenhum LED aceso | Remote sem energia | Verificar bateria |
