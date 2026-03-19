# Plano de Implementação — Fim de Curso no Módulo Remote (GPIO 13)

**Referência:** README.md v3.2 / IMPLEMENTATION_PLAN.md v3.2  
**Data:** 2026-03-18  
**Status:** Aplicado — documentos e firmware atualizados

---

## Contexto e Motivação

O módulo Principal já possui um fim de curso que detecta a chegada do carrinho ao
estacionamento (posição final de subida). O Remote embarcado no carrinho precisa de
um fim de curso próprio para detectar a **posição final de descida** — quando o
carrinho chega à margem do rio.

Isso resolve um cenário hoje fora de escopo (v1.0 lista "fim de curso na posição
inferior" como item futuro) e simetriza a proteção: ambas as extremidades do trilho
têm parada automática por hardware.

**GPIO escolhido: 13**

- Bidirecional, sem restrição de boot (evita GPIOs 0, 2, 12, 15)
- Pull-up interno disponível (`INPUT_PULLUP`)
- Não conflita com nenhum pino já usado no Remote
- Mesma numeração do GPIO de monitoramento de energia no Principal —
  consistência intencional: GPIO 13 = sensor de condição externa em ambos os módulos

---

## 1. Hardware

### 1.1 Pinout atualizado do Módulo Remote

| Função | Tipo | GPIO | Observação |
|---|---|---|---|
| Botão SUBIR | Entrada | 36 | Input-only, pull-up externo obrigatório |
| Botão DESCER | Entrada | 39 | Input-only, pull-up externo obrigatório |
| Botão VEL1 | Entrada | 34 | Input-only, pull-up externo obrigatório |
| Botão VEL2 | Entrada | 35 | Input-only, pull-up externo obrigatório |
| Botão VEL3 | Entrada | 32 | Pull-up interno (INPUT_PULLUP) |
| Botão EMERGÊNCIA (trava) | Entrada | 33 | Pull-up interno (INPUT_PULLUP) |
| **Fim de curso (descida)** | **Entrada** | **13** | **Pull-up interno (INPUT_PULLUP) — novo** |
| LED LINK | Saída | 4 | Comunicação com Principal |
| LED MOTOR | Saída | 16 | Motor em operação |
| LED VEL1 | Saída | 17 | Velocidade 1 ativa |
| LED VEL2 | Saída | 5 | Velocidade 2 ativa |
| LED VEL3 | Saída | 18 | Velocidade 3 ativa |
| LED EMERGÊNCIA | Saída | 19 | Emergência ou falha de comunicação |
| LED ALARME | Saída | 21 | Rearme com botão local ainda travado |
| **Total** | | **14** | **7 entradas + 7 saídas** |

### 1.2 Sensor

Tipo microswitch (igual ao fim de curso do Principal), instalado na posição final
de descida do trilho. Lógica com pull-up interno:

| Estado | Nível GPIO 13 |
|---|---|
| Carrinho fora da posição | HIGH (pull-up) |
| Carrinho na posição final de descida | LOW (microswitch fecha para GND) |

Debounce: **20 ms** (mesmo padrão do fim de curso do Principal).

### 1.3 `pinout.h` do Remote

```c
#define PIN_FIM_CURSO_DESCIDA  13   // LOW = carrinho na posição final de descida
```

---

## 2. Comportamento ao Acionar

O fim de curso do Remote sinaliza uma **condição operacional esperada** (chegada ao
destino), não uma emergência. O comportamento é idêntico ao fim de curso do
Principal:

1. Remote detecta GPIO 13 = LOW (com debounce 20 ms).
2. Remote envia pacote imediato ao Principal com flag `fim_curso_descida = 1`.
3. Principal recebe o flag, corta o motor e aciona o freio.
4. Estado do sistema → `PARADO`.
5. **Não requer rearme.** O operador pode retomar a operação normalmente.

---

## 3. Protocolo — Atualização do `PacoteRemote`

Adicionar o campo `fim_curso_descida` ao pacote existente:

```c
typedef struct {
    uint8_t  comando;            // 0=HEARTBEAT, 1=SUBIR, 2=DESCER,
                                 // 3=VEL1, 4=VEL2, 5=VEL3
    uint8_t  botao_hold;         // 1=SUBIR ou DESCER pressionado
    uint8_t  emergencia;         // 1=botão com trava ativo no Remote
    uint8_t  fim_curso_descida;  // 1=carrinho na posição final de descida ← novo
    uint32_t timestamp;          // millis() do Remote
    uint8_t  checksum;           // XOR de todos os bytes anteriores
} PacoteRemote;
```

**Tamanho:** 9 bytes (era 8).

> O checksum continua sendo calculado sobre todos os bytes anteriores ao próprio
> campo `checksum` — nenhuma mudança no algoritmo, apenas um byte a mais no XOR.

---

## 4. Firmware — Módulo Remote

### 4.1 Leitura do sensor (loop principal)

```cpp
// Em remote.cpp — loop():
monitorRede.atualizar();   // já existente (debounce)
fimCursoDescida.atualizar(); // novo — debounce 20 ms no GPIO 13

bool fc_descida = fimCursoDescida.acionado();

if (fc_descida) {
    pacote.fim_curso_descida = 1;
    enviar_pacote_imediato();   // envia fora do ciclo de 200 ms
} else {
    pacote.fim_curso_descida = 0;
}
```

### 4.2 Classe `FimDeCurso` (reutilizável)

O Remote pode reutilizar a mesma abstração de debounce já prevista para o fim de
curso do Principal:

```cpp
class FimDeCurso {
public:
    FimDeCurso(uint8_t gpio, uint8_t debounce_ms = 20);
    void init();          // pinMode(gpio, INPUT_PULLUP)
    void atualizar();     // chamar no loop
    bool acionado();      // true se LOW estável após debounce
private:
    uint8_t  _gpio;
    uint8_t  _debounceMs;
    bool     _estadoEstavel;
    bool     _leituraAnterior;
    uint32_t _tempoMudanca;
};
```

### 4.3 Transmissão

| Condição | Comportamento |
|---|---|
| Fim de curso acionado (transição HIGH→LOW) | Envio imediato + repetir a cada 200 ms enquanto LOW |
| Fim de curso solto (transição LOW→HIGH) | Próximo pacote periódico já reflete `fim_curso_descida = 0` |

---

## 5. Firmware — Módulo Principal

### 5.1 Recepção e processamento

No callback `OnDataRecv`, após validar o checksum:

```cpp
if (pacoteRemote.fim_curso_descida == 1) {
    motor.desligar();
    freio.acionar();
    estado = ESTADO_PARADO;
    // Não ativa emergencia_ativa — sem rearme necessário
}
```

> Mesma prioridade do fim de curso local (Prioridade 3 na máquina de estados).
> Se `emergencia_ativa` ou `FALHA_ENERGIA` já estiverem ativos, eles prevalecem —
> o flag de fim de curso é processado apenas no estado operacional normal.

### 5.2 Integração na máquina de estados

```cpp
// Prioridade 3: fim de curso (local OU remoto)
if (sensores.fimDeCursoAcionado() || pacoteRemote.fim_curso_descida == 1) {
    motor.desligar();
    freio.acionar();
    estado = ESTADO_PARADO;
    return;
}
```

---

## 6. Indicadores Visuais

O fim de curso de descida **não adiciona novo LED**. O comportamento visual já
existente é suficiente:

| LED no Remote | Comportamento ao acionar fim de curso |
|---|---|
| LED MOTOR | Apaga (motor OFF) |
| LED LINK | Permanece fixo (comunicação ativa) |
| Demais LEDs | Inalterados |

O operador percebe a parada pelo corte do motor e pelo LED MOTOR apagando.

---

## 7. Diferença entre os dois fins de curso

| Aspecto | Fim de curso — Principal | Fim de curso — Remote |
|---|---|---|
| Localização física | Estacionamento / depósito | Margem do rio |
| GPIO | 26 (no Principal) | 13 (no Remote) |
| Quem lê o sensor | ESP32 Principal (direto) | ESP32 Remote → envia ao Principal |
| Estado resultante | `PARADO` | `PARADO` |
| Requer rearme | Não | Não |
| Debounce | 20 ms | 20 ms |

---

## 8. Caso de Teste

**T20 — Fim de curso de descida (Remote):**

| Campo | Detalhe |
|---|---|
| **Procedimento** | Com motor ativo (estado `DESCENDO`), acionar manualmente o microswitch no GPIO 13 do Remote |
| **Critério de aceite** | Motor cortado e freio acionado em < 150 ms; estado `PARADO`; LED MOTOR apaga; sistema retoma operação normalmente ao pressionar SUBIR — sem rearme necessário |

---

## 9. Documentos a Atualizar

| Arquivo | Seção | Alteração |
|---|---|---|
| `README.md` (pinout) | §5.2 | Adicionar GPIO 13: entrada `FIM_CURSO_DESCIDA`, total 14 GPIOs no Remote |
| `hardware_io/SPEC.md` | §6 Entradas Remote | Novo sensor fim de curso, GPIO 13, debounce 20 ms |
| `comunicacao/SPEC.md` | §4.1 `PacoteRemote` | Novo campo `fim_curso_descida`, tamanho 9 bytes |
| `motor/SPEC.md` | §5 Fim de Curso | Adicionar seção para fim de curso de descida |
| `maquina_estados/SPEC.md` | §6 pseudocódigo | Condição OR no fim de curso (local ou remoto) |
| `IMPLEMENTATION_PLAN.md` | Fase 4 — testes | Adicionar caso T20 |
| `DESIGN_SPEC.md` | §11 Fora de Escopo | Remover "Fim de curso na posição inferior" |

---

## 10. Resumo das Mudanças

| Item | Mudança |
|---|---|
| GPIOs no Remote | 13 → **14** (GPIO 13, entrada, pull-up interno) |
| `PacoteRemote` | Novo campo `fim_curso_descida` — tamanho 8 → **9 bytes** |
| Nova classe | `FimDeCurso` (reutilizável em ambos os módulos) |
| Máquina de estados | Condição OR no fim de curso (local OU remoto) |
| LEDs | **Inalterados** — sem novo LED necessário |
| Rearme | **Não requerido** — comportamento idêntico ao fim de curso do Principal |
| Item fora de escopo v1.0 | "Fim de curso na posição inferior" → **implementado** |