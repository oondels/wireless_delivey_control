# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Wireless delivery control system — automating a cable-driven cart (carrinho) that currently can only be operated from a central control station. The project adds an **onboard panel with wireless communication** (ESP32-based) so the cart can be controlled directly from the cart itself, without depending on the central station.

Key capabilities: wireless remote control, adjustable speed (3 levels), forward/reverse movement, and dual emergency stop buttons (one on cart, one at central station).

## Hardware Architecture

Two ESP32 WROOM-32U boards communicate wirelessly:
- **Central panel** — stationary control station with buttons for speed, direction, and emergency stop
- **Onboard panel** — mounted on the cart with identical controls, plus limit switches for obstacle detection and parking position

Other components: 8-channel relay module (motor/system actuation), LM2596 step-down converters (power regulation), micro limit switches (end-of-travel and obstacle detection).

## Language

Project documentation is in Brazilian Portuguese (pt-BR). Maintain this convention for comments, commit messages, and documentation.

## GOLDEN RULES
1. **Security** - You just have acces to view or edit files in this project/repository. Do not attempt to access any other files or repositories. Just if it was explicitly shared with you by user.