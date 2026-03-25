/**
 * freio.h — Controle do freio solenoide de dupla bobina
 *
 * FREIO_ON  (GPIO 19): bobina de aplicação — cilindro avança, freio trava. LED aceso quando ativo.
 * FREIO_OFF (GPIO 22): bobina de liberação — cilindro recua, freio libera. Sem LED.
 *
 * Invariante: FREIO_ON e FREIO_OFF nunca ficam LOW (ativos) simultaneamente.
 * A troca segue: desativa lado ativo → dead-time ~10 ms → ativa lado oposto.
 *
 * Microchave (GPIO 27, NA + pull-up interno):
 *   HIGH = freio engatado (cilindro avançado, microchave aberta)
 *   LOW  = freio liberado (cilindro retraído, microchave pressionada)
 *
 * O cilindro leva ~10s para completar o curso. O cilindro possui fim de
 * curso mecânico próprio — os relés permanecem ativos continuamente:
 * FREIO_ON enquanto o freio está engatado; FREIO_OFF enquanto liberado.
 * Estado padrão e seguro: FREIO_ON ativo.
 *
 * Ref: motor/SPEC.md §4
 */

#ifndef FREIO_H
#define FREIO_H

#include <Arduino.h>
#include "pinout.h"

enum EstadoFreio {
    FREIO_ENGATADO,    // Freio aplicado, confirmado pela microchave (GPIO 27 = HIGH)
    FREIO_ENGATANDO,   // Relé FREIO_ON ativo, aguardando microchave confirmar (GPIO 27 → HIGH)
    FREIO_LIBERADO,    // Freio liberado, confirmado pela microchave (GPIO 27 = LOW)
    FREIO_LIBERANDO    // Relé FREIO_OFF ativo, aguardando microchave confirmar (GPIO 27 → LOW)
};

class Freio {
public:
    void init();
    void acionar();    // Inicia engate do freio (idempotente se já engatado/engatando)
    void liberar();    // Inicia liberação do freio (idempotente se já liberado/liberando)
    void atualizar();  // Monitora microchave e desativa relé após confirmação

    bool isLiberado()  const { return _estado == FREIO_LIBERADO; }
    bool isEngatado()  const { return _estado == FREIO_ENGATADO; }
    bool isTransicao() const { return _estado == FREIO_ENGATANDO || _estado == FREIO_LIBERANDO; }
    EstadoFreio estado() const { return _estado; }

    // Controle manual do cilindro (REARME + SUBIR/DESCER)
    // Controla relés diretamente, ignorando máquina de estados do freio
    void manualAcionar();   // Força relé FREIO_ON (cilindro avança)
    void manualLiberar();   // Força relé FREIO_OFF (cilindro retrai)
    void manualParar();     // Ativa FREIO_ON (estado seguro) e sincroniza estado pela microchave
    bool emModoManual() const { return _modoManual; }

private:
    EstadoFreio _estado = FREIO_ENGATADO;
    bool _modoManual = false;
};

#endif // FREIO_H
