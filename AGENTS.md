# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

Wireless delivery control system — automating a cable-driven cart (carrinho) that currently can only be operated from a central control station. The project adds an **onboard panel with wireless communication** (ESP32-based) so the cart can be controlled directly from the cart itself, without depending on the central station.

Key capabilities: wireless remote control, adjustable speed (3 levels), forward/reverse movement, dual emergency stop buttons (one on cart, one at central station), hardware brake safety via microswitch (independent of firmware), and parking end-of-travel sensor.

---

## Hardware Architecture

Two ESP32 WROOM-32U boards communicate via **ESP-NOW** (peer-to-peer, no router required):

- **Módulo Principal** (central panel) — stationary control station with buttons for speed, direction, emergency stop, and rearm. Controls 6 relays (2 direction + 3 speed + 1 brake). Each relay GPIO also drives an LED in parallel via physical wiring.
- **Módulo Remote** (onboard panel) — mounted on the cart with identical direction/speed controls, emergency stop button, and 7 dedicated LED GPIOs for status feedback.

Other components: relay module (motor/system actuation), LM2596 step-down converters (power regulation), microswitch for brake (wired directly into brake circuit — **not connected to ESP32**), end-of-travel sensor at parking position (connected to Principal ESP32).

---

## Language

Project documentation and code comments are in **Brazilian Portuguese (pt-BR)**. Maintain this convention for all comments, variable can be in english, commit messages, and documentation.

---

## Reading Order — Understand Before Acting

Before writing or modifying any code, read the following files **in this exact order**:

1. `README.md` — full system design specification (v3.2): architecture, business rules, state machine, protocol structs, GPIO layout, and safety logic
2. `docs/specs/README.md` — specs index; use this as a lookup to find which specific SPEC.md is relevant to the current task
3. `docs/specs/DESIGN_SPEC.md` — earlier version of the design spec with additional context (refer to README.md as the authoritative source when they diverge)

Then, depending on the task domain, just read the relevant spec before touching code just if the task requires it. For example, if the task is about motor control, read `docs/specs/motor/SPEC.md` to understand the required logic and constraints for that domain. Dont read unrelated specs for the current task, as that can lead to confusion and scope creep. Focus on the specific domain relevant to the task at hand.:

| Domain | Spec file |
|---|---|
| Communication protocol, ESP-NOW, packet structs | `docs/specs/comunicacao/SPEC.md` |
| GPIO mapping, pinout, relay/LED wiring | `docs/specs/hardware_io/SPEC.md` |
| LED behavior, blink patterns, logic | `docs/specs/leds/SPEC.md` |
| State machine, transitions, priority order | `docs/specs/maquina_estados/SPEC.md` |
| Motor control, direction, speed, dead-time | `docs/specs/motor/SPEC.md` |
| Safety rules, emergency, watchdog, rearm | `docs/specs/seguranca/SPEC.md` |

Do not skip or partially read the required files. The safety requirements in `docs/specs/seguranca/SPEC.md` and `docs/specs/DESIGN_SPEC.md` are non-negotiable constraints that govern every implementation decision.

## Project File Structure

```
.
├── CHANGELOG.md                        ← version history; update after each completed feature
├── CLAUDE.md                           ← this file
├── docs/
│   ├── IMPLEMENTATION_PLAN.md          ← phased implementation plan
│   ├── materiais.md                    ← hardware bill of materials
│   ├── Proposta-Automacao-de-Sistema-de-Controle.pdf  ← original project proposal
│   ├── RAW_PROJECT_LOGIC.md            ← raw logic notes from project owner
│   ├── TASKS.md                        ← Ralph task list (read every iteration)
│   └── specs/
│       ├── README.md                   ← spec lookup index (read first)
│       ├── DESIGN_SPEC.md              ← master design document (authoritative)
│       ├── comunicacao/
│       │   └── SPEC.md                 ← ESP-NOW protocol, packet structs, timing
│       ├── hardware_io/
│       │   └── SPEC.md                 ← GPIO mapping, pinout, relay/LED wiring
│       ├── leds/
│       │   └── SPEC.md                 ← LED behavior, blink patterns, millis() logic
│       ├── maquina_estados/
│       │   └── SPEC.md                 ← state machine, transitions, priority order
│       ├── motor/
│       │   └── SPEC.md                 ← motor control, direction, speed, dead-time
│       └── seguranca/
│           └── SPEC.md                 ← safety rules, emergency, watchdog, rearm
└── README.md
```

> Source code directories (`principal/`, `remote/`) contain the firmware implementation.
> Both modules share `logger.h` (header-only logging via Serial, desabilitável com `-DLOG_DISABLED`).

---

## Build and Verify

- Firmware is compiled with **PlatformIO** (preferred) or Arduino IDE with ESP32 Arduino Core.
- `principal/` and `remote/` are independent PlatformIO projects.
- A successful build with zero errors and zero warnings is the minimum verification bar.
- If tests exist under `tests/`, run them and ensure 100% pass rate before committing.
- If build tooling is not yet set up, verify by reviewing the code for correctness against the relevant SPEC.md and document the verification method in the commit message.

---

## Commit Convention

Format: `tipo(escopo): descrição em pt-BR`

| Tipo | Quando usar |
|---|---|
| `feat` | nova funcionalidade |
| `fix` | correção de bug |
| `refactor` | refatoração sem mudança de comportamento |
| `docs` | atualização de documentação |
| `test` | adição ou correção de testes |
| `chore` | configuração, build, pinout |

Examples:
- `feat(motor): implementa dead-time de 100ms na inversão de direção`
- `fix(watchdog): corrige reset do timer no callback OnDataRecv`
- `feat(leds): implementa piscar não-bloqueante com millis() no módulo remote`
- `chore(pinout): define mapeamento de GPIOs do módulo principal`

Each commit covers **exactly one task** from `docs/TASKS.md`.

All commits must include the trailer:
```
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

---

## GOLDEN RULES

1. **Segurança em primeiro lugar** — The Fail-Safe rules defined in `docs/specs/seguranca/SPEC.md` must never be weakened, bypassed, or simplified. If a task conflicts with safety requirements, halt that task, flag it in `docs/TASKS.md`, and do not proceed.

2. **Sem rearm automático** — The system must never automatically clear `emergencia_ativa`. This flag is only cleared by an explicit REARME button press on the Principal. Any code path that could auto-clear this flag is a critical bug.

3. **Microchave é hardware** — The brake microswitch operates directly in the physical brake circuit, not connected to the ESP32. The firmware does not read or model brake sensor state. Do not add any such logic.

4. **GPIO compartilhado** — On the Principal, each relay GPIO also drives its LED via parallel physical wiring. Motor and brake relay functions handle both simultaneously. Do not add separate LED control logic for relay channels.

5. **Sem delay() em LEDs** — All blinking must use non-blocking `millis()` patterns. `delay()` is forbidden for LED state management.

6. **Dead-time obrigatório** — Minimum 100 ms between deactivating one direction relay and activating the other. Never activate both direction relays simultaneously under any condition.

7. **Uma tarefa por iteração** — Implement exactly one task per Ralph loop iteration. No bundling, no opportunistic refactoring.

8. **Scope restrito** — Only access files within this repository. Do not attempt to read, write, or execute anything outside the project tree unless explicitly instructed by the user.
