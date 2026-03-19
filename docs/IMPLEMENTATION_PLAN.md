# Plano de Implementação — Monitoramento de Energia da Rede (GPIO 13)

**Referência:** README.md v3.4 / IMPLEMENTATION_PLAN.md v3.2
**Data:** 2026-03-19
**Status:** Aplicado — documentos atualizados; implementação pendente em firmware

---

## Contexto e Decisão de Design

O ESP32 Principal passa a ser alimentado por **bateria de backup**, mantendo o firmware vivo mesmo com a queda da rede elétrica. O GPIO 13 monitora a presença de tensão da rede. Quando a rede cai, o sistema detecta via software e aciona o freio imediatamente, bloqueando operação remota — comportamento análogo ao `FALHA_COMUNICACAO`.

**Por que GPIO 13?** É um GPIO bidirecional sem restrição de boot (diferente dos pinos 0, 2, 12, 15), compatível com entrada digital com pull-up/pull-down externo. É a única adição de GPIO ao módulo Principal, elevando o total de **15 para 16 GPIOs**.

---

## 1. Hardware

### 1.1 Lógica da Entrada

| Estado da rede | Nível no GPIO 13 |
|---|---|
| Rede presente | HIGH |
| Rede ausente | LOW |

O sinal de "rede presente" deve vir de um ponto **após a fonte chaveada 5V**, nunca diretamente da rede 110/220V.

### 1.2 Circuito Recomendado

Divisor resistivo entre a saída 5V da fonte e o GPIO 13:

```
Saída 5V da fonte chaveada
        │
       [R1 — 10 kΩ]
        │
        ├──► GPIO 13 (ESP32)
        │
       [R2 — 10 kΩ]   ← garante ~2,5 V no GPIO (nível HIGH seguro)
        │
       GND
```

> **Alternativa mais robusta:** optoacoplador (ex: PC817) entre o 5V da fonte e o GPIO 13, com pull-down externo de 10 kΩ no lado do ESP32. Garante isolação galvânica total entre o circuito de detecção e o ESP32.

### 1.3 Atualização do `pinout.h` do Principal

```c
#define PIN_MONITOR_REDE  13   // HIGH = rede presente; LOW = rede ausente
```

**Debounce:** 50 ms — transitórios de rede (brownout, micro-quedas) não devem causar falso acionamento.

**Total de GPIOs no Principal:** 16 (antes: 15)

---

## 2. Novo Estado do Sistema: `FALHA_ENERGIA`

Adicionar ao enum `EstadoSistema` em `protocolo.h`:

```c
typedef enum {
    ESTADO_PARADO            = 0,
    ESTADO_SUBINDO           = 1,
    ESTADO_DESCENDO          = 2,
    ESTADO_EMERGENCIA        = 3,
    ESTADO_FALHA_COMUNICACAO = 4,
    ESTADO_FALHA_ENERGIA     = 5   // ← novo
} EstadoSistema;
```

**Comportamento:** idêntico ao `FALHA_COMUNICACAO` — motor OFF, freio ON, Remote ignorado, **requer rearme manual**.

**Por que um estado separado** (em vez de reusar `FALHA_COMUNICACAO`): permite diagnóstico preciso. O operador e o LED do Remote conseguem distinguir "perdi o link" de "faltou energia no painel".

---

## 3. Firmware — Módulo Principal

### 3.1 Nova Classe `MonitorRede` (`monitor_rede.h`)

```cpp
class MonitorRede {
public:
    void init();            // pinMode(PIN_MONITOR_REDE, INPUT)
    bool redePresente();    // retorna true se rede OK (com debounce)
    void atualizar();       // chamar no loop — processa debounce via millis()
private:
    bool     _ultimoEstavel;
    bool     _leituraAnterior;
    uint32_t _tempoMudanca;
    static const uint16_t DEBOUNCE_MS = 50;
};
```

### 3.2 Integração na Máquina de Estados

Nova prioridade entre emergência (P1) e watchdog (P3):

```cpp
void atualizar_maquina_estados() {
    // Prioridade 1: emergência
    if (emergencia.verificar(pacoteRemote.emergencia)) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_EMERGENCIA;
        return;
    }

    // Prioridade 2: falha de energia  ← NOVO
    if (!monitorRede.redePresente()) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_FALHA_ENERGIA;
        return;
    }

    // Prioridade 3: watchdog
    if (watchdog.expirado()) {
        freio.acionar();
        motor.desligar();
        estado = ESTADO_FALHA_COMUNICACAO;
        return;
    }

    // Prioridade 4: fim de curso
    if (sensores.fimDeCursoAcionado()) {
        motor.desligar();
        freio.acionar();
        estado = ESTADO_PARADO;
        return;
    }

    // Prioridade 5: movimentação (inalterada)
    bool hold = botao_hold_local || pacoteRemote.botao_hold;
    Direcao dir = obter_direcao_ativa();

    if (hold && dir != DIR_NENHUMA) {
        freio.liberar();
        motor.acionar(dir);
        estado = (dir == DIR_SUBIR) ? ESTADO_SUBINDO : ESTADO_DESCENDO;
    } else {
        motor.desligar();
        freio.acionar();
        estado = ESTADO_PARADO;
    }
}
```

### 3.3 Rearme

A classe `Rearme` já existente deve incluir `FALHA_ENERGIA` nas condições aceitas, sem mudança estrutural:

```cpp
// Em rearme.verificar():
if (estado == ESTADO_EMERGENCIA
    || estado == ESTADO_FALHA_COMUNICACAO
    || estado == ESTADO_FALHA_ENERGIA) {   // ← adicionar
    // aceitar rearme
}
```

---

## 4. Protocolo — `PacoteStatus`

O campo `estado_sistema` já comporta o valor `5` (`FALHA_ENERGIA`) **sem nenhuma mudança estrutural no pacote**. O Remote receberá o estado e atualizará seus LEDs automaticamente.

---

## 5. Indicadores Visuais — Remote

**Novo padrão de LED EMERGÊNCIA para `FALHA_ENERGIA`:**

| Estado | LED EMERGÊNCIA | LED LINK | Frequência |
|---|---|---|---|
| `EMERGENCIA_ATIVA` | Piscando | Fixo | 4 Hz (125 ms) |
| `FALHA_COMUNICACAO` | Fixo | Piscando 1 Hz | — |
| `FALHA_ENERGIA` | **Piscando** | **Fixo** | **2 Hz (250 ms)** |

A distinção é intencional: link OK (LINK fixo) mas energia ausente (EMERGÊNCIA piscando 2 Hz).

Atualizar `atualizarLeds()` no Remote:

```cpp
// EMERGÊNCIA — adicionar caso FALHA_ENERGIA
if (status.estado_sistema == ESTADO_EMERGENCIA)
    ledEmergencia.piscar(125);          // 4 Hz
else if (status.estado_sistema == ESTADO_FALHA_COMUNICACAO)
    ledEmergencia.ligar();              // fixo
else if (status.estado_sistema == ESTADO_FALHA_ENERGIA)
    ledEmergencia.piscar(250);          // 2 Hz ← novo
else
    ledEmergencia.desligar();
```

---

## 6. Tabela de Prioridades Atualizada

| Prioridade | Condição | Estado Resultante | Requer Rearme |
|---|---|---|---|
| 1 (máxima) | Emergência ativa (Painel ou Remote) | `EMERGENCIA_ATIVA` | Sim |
| 2 | Queda de energia da rede | `FALHA_ENERGIA` | Sim |
| 3 | Watchdog timeout (> 500 ms) | `FALHA_COMUNICACAO` | Sim |
| 4 | Fim de curso acionado | `PARADO` | Não |
| 5 | Botão de acionamento solto | `PARADO` | Não |

---

## 7. Documentos a Atualizar

| Arquivo | Seção | Alteração |
|---|---|---|
| `README.md` (pinout) | §5.1 | Adicionar GPIO 13: entrada `MONITOR_REDE`, pull-down externo, total 16 GPIOs |
| `hardware_io/SPEC.md` | §4.2 Sensores | Novo sensor `MONITOR_REDE`, GPIO 13, debounce 50 ms |
| `seguranca/SPEC.md` | §2 e §8 | Nova prioridade 2 (`FALHA_ENERGIA`); atualizar tabela de condições |
| `maquina_estados/SPEC.md` | §2, §4, §6 | Novo estado, nova prioridade, pseudocódigo atualizado |
| `comunicacao/SPEC.md` | §5.2 | Adicionar `ESTADO_FALHA_ENERGIA = 5` no enum |
| `leds/SPEC.md` | §3.1 | Nova linha: EMERGÊNCIA piscando 2 Hz para `FALHA_ENERGIA` |
| `IMPLEMENTATION_PLAN.md` | Fase 4 — testes | Adicionar caso T19 |

---

## 8. Novo Caso de Teste

**T19 — Falha de energia da rede:**

| Campo | Detalhe |
|---|---|
| **Procedimento** | Com motor ativo (estado `SUBINDO`), desligar a fonte chaveada do Principal simulando queda de rede |
| **Critério de aceite** | Freio acionado em < 150 ms; estado `FALHA_ENERGIA`; LED EMERGÊNCIA piscando 2 Hz no Remote; LED LINK permanece fixo; operação bloqueada até rearme manual no Painel Central |

---

## 9. Resumo das Mudanças

| Item | Mudança |
|---|---|
| GPIOs no Principal | 15 → **16** (GPIO 13, entrada, pull-down externo) |
| Enum `EstadoSistema` | Novo valor `ESTADO_FALHA_ENERGIA = 5` |
| Nova classe | `MonitorRede` com debounce 50 ms |
| Máquina de estados | Nova prioridade 2 entre emergência e watchdog |
| LED Remote | EMERGÊNCIA piscando 2 Hz para `FALHA_ENERGIA` |
| Rearme | Inclui `FALHA_ENERGIA` — sem mudança estrutural |
| `PacoteStatus` | **Inalterado** — campo `estado_sistema` já comporta valor 5 |