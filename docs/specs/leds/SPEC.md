# Especificação de Indicadores Visuais (LEDs)

**Versão:** 1.2
**Data:** 2026-04-24
**Referência:** README.md v4.0

---

## 1. Visão Geral

Todos os LEDs são componentes discretos de 3V. O firmware controla apenas o estado lógico do GPIO: ligado, desligado ou piscando.

Não há controle de cor por software.

---

## 2. LEDs com Abstração de Software

### 2.1 Principal

| LED | GPIO | Função |
|---|---|---|
| LINK REMOTE | 21 | Indica se o watchdog do Remote está válido |

> As saídas do Principal para o CLP podem acender LEDs físicos do módulo de relé ou da instalação elétrica, mas esses indicadores não têm controle lógico separado no firmware.

### 2.2 Remote

| LED | GPIO | Função |
|---|---|---|
| LINK | 4 | Indica validade do status recebido do Principal |
| MOTOR | 16 | Reflete `motor_ativo` |
| VEL1 | 17 | Reflete `vel1_ativa` |
| VEL2 | 5 | Reflete `vel2_ativa` |
| EMERGÊNCIA | 19 | Emergência local ou emergência do CLP |

**Total com abstração `Led`: 6 GPIOs**

---

## 3. LEDs do Módulo Remote

### 3.1 Tabela Completa

| LED | Comportamento | Condição | Frequência |
|---|---|---|---|
| LINK | Ligado fixo | `link_ok == 1` e último status há <= 500 ms | — |
| LINK | Piscando | Sem status válido do Principal | 1 Hz (500 ms) |
| MOTOR | Ligado fixo | `motor_ativo == 1` | — |
| MOTOR | Desligado | `motor_ativo == 0` | — |
| VEL1 | Ligado fixo | `vel1_ativa == 1` | — |
| VEL2 | Ligado fixo | `vel2_ativa == 1` | — |
| EMERGÊNCIA | Piscando | Botão de emergência local ativo | 4 Hz (125 ms) |
| EMERGÊNCIA | Piscando | `emergencia_ativa == 1` | 4 Hz (125 ms) |
| EMERGÊNCIA | Desligado | Estado normal e link válido | — |

### 3.2 Lógica de Atualização

Todos os LEDs do Remote são atualizados com base no `PacoteStatus` recebido do Principal.

```cpp
void atualizarLeds(const PacoteStatus& status) {
    bool linkOk = (status.link_ok == 1) &&
                  (millis() - ultimoStatusRecebido <= WATCHDOG_TIMEOUT_MS);

    if (linkOk) ledLink.ligar();
    else ledLink.piscar(500);

    if (status.motor_ativo == 1) ledMotor.ligar();
    else ledMotor.desligar();

    ledVel1.desligar();
    ledVel2.desligar();
    if (status.vel1_ativa == 1) ledVel1.ligar();
    if (status.vel2_ativa == 1) ledVel2.ligar();

    if (emergenciaLocal || status.emergencia_ativa == 1) ledEmergencia.piscar(125);
    else ledEmergencia.desligar();
}
```

---

## 4. LED do Módulo Principal

| LED | Comportamento | Condição |
|---|---|---|
| LINK REMOTE | Ligado fixo | Comunicação com Remote ativa (`watchdog` válido) |
| LINK REMOTE | Piscando 2 Hz | Watchdog expirado / Remote ausente |

---

## 5. Abstração de Software (`leds.h`)

```cpp
class Led {
public:
    Led(uint8_t gpio);
    void ligar();
    void desligar();
    void piscar(uint16_t intervalo_ms);
    void atualizar();
};
```

Regras:

- Nunca usar `delay()` para LEDs.
- `led.atualizar()` deve ser chamada a cada ciclo do loop.

---

## 6. Frequências Padronizadas

| Frequência | Intervalo | Uso |
|---|---|---|
| 1 Hz | 500 ms | LINK sem status válido |
| 2 Hz | 250 ms | LINK do Principal sem Remote |
| 4 Hz | 125 ms | Emergência local no Remote |

---

## 7. Especificações Elétricas

| Parâmetro | Valor |
|---|---|
| Tensão do LED | 3V |
| Resistor limitador | 220Ω |
| Corrente máxima por GPIO | 12 mA |

---

## 8. Troubleshooting Visual

| Sintoma | Significado |
|---|---|
| LINK do Remote piscando | Sem status válido do Principal |
| MOTOR aceso | CLP reporta motor ativo |
| MOTOR piscando 2 Hz | Solicitação de SUBIR/DESCER aguardando freio liberar e `motor_ativo` |
| VEL1 ou VEL2 aceso | CLP reporta velocidade ativa correspondente |
| EMERGÊNCIA piscando 4 Hz | Emergência local ou `emergencia_ativa` reportada pelo CLP |
