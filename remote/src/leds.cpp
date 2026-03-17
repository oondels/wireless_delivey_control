/**
 * leds.cpp — Implementação da classe Led
 *
 * Piscar não-bloqueante via millis().
 * Ref: leds/SPEC.md §5
 */

#include "leds.h"

Led::Led(uint8_t gpio)
    : _gpio(gpio)
    , _piscando(false)
    , _intervaloMs(0)
    , _ultimoToggle(0)
    , _estadoAtual(false)
{
    pinMode(_gpio, OUTPUT);
    digitalWrite(_gpio, LOW);
}

void Led::ligar() {
    _piscando = false;
    _estadoAtual = true;
    digitalWrite(_gpio, HIGH);
}

void Led::desligar() {
    _piscando = false;
    _estadoAtual = false;
    digitalWrite(_gpio, LOW);
}

void Led::piscar(uint16_t intervalo_ms) {
    if (!_piscando || _intervaloMs != intervalo_ms) {
        _piscando = true;
        _intervaloMs = intervalo_ms;
        _ultimoToggle = millis();
        _estadoAtual = true;
        digitalWrite(_gpio, HIGH);
    }
}

void Led::atualizar() {
    if (!_piscando) return;

    uint32_t agora = millis();
    if (agora - _ultimoToggle >= _intervaloMs) {
        _ultimoToggle = agora;
        _estadoAtual = !_estadoAtual;
        digitalWrite(_gpio, _estadoAtual ? HIGH : LOW);
    }
}
