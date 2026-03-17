/**
 * leds.h — Classe Led para controle de LEDs com piscar não-bloqueante
 *
 * Utiliza millis() para temporização — delay() é proibido.
 * No Módulo Principal, apenas o LED LINK REMOTE usa esta abstração.
 * LEDs compartilhados com relés são controlados diretamente pelo GPIO do relé.
 *
 * Ref: leds/SPEC.md §4.2, §5
 */

#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>

class Led {
public:
    Led(uint8_t gpio);

    void ligar();                        // GPIO HIGH, piscando = false
    void desligar();                     // GPIO LOW, piscando = false
    void piscar(uint16_t intervalo_ms);  // Inicia piscar não-bloqueante
    void atualizar();                    // Chamar no loop principal

private:
    uint8_t  _gpio;
    bool     _piscando;
    uint16_t _intervaloMs;
    uint32_t _ultimoToggle;
    bool     _estadoAtual;
};

#endif // LEDS_H
