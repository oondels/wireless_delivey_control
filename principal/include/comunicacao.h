/**
 * comunicacao.h — Comunicação ESP-NOW do Módulo Principal
 *
 * Inicialização WiFi + ESP-NOW, registro de peer (MAC do Remote),
 * callback OnDataRecv, envio de PacoteStatus.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "protocolo.h"
#include "watchdog_comm.h"
#include "emergencia.h"

class Comunicacao {
public:
    // init() recebe referências das dependências usadas no callback
    void init(WatchdogComm& watchdog, Emergencia& emergencia);

    // Envia PacoteStatus para o Remote
    void enviarStatus(const PacoteStatus& status);

    // Último pacote recebido do Remote (volatile — atualizado no callback)
    volatile PacoteRemote& ultimoPacote() { return _ultimoPacote; }
    bool novoPacoteRecebido() const { return _novoPacote; }
    void limparNovoPacote() { _novoPacote = false; }

private:
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);

    // Ponteiros estáticos para acesso no callback C
    static WatchdogComm* _pWatchdog;
    static Emergencia*   _pEmergencia;
    static volatile PacoteRemote _ultimoPacote;
    static volatile bool         _novoPacote;

    uint8_t _macRemote[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
};

#endif // COMUNICACAO_H
