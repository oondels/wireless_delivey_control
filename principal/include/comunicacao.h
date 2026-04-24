/**
 * comunicacao.h — Comunicação ESP-NOW do Módulo Principal
 *
 * Inicialização WiFi + ESP-NOW, registro fixo do peer remoto com criptografia,
 * callback OnDataRecv, envio de PacoteStatus.
 *
 * Ref: comunicacao/SPEC.md §7-9
 */

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include "protocolo.h"
#include "watchdog_comm.h"

class Comunicacao {
public:
    // init() recebe referências das dependências usadas no callback
    void init(WatchdogComm& watchdog);

    // Envia PacoteStatus para o Remote
    void enviarStatus(const PacoteStatus& status);

    // Último pacote recebido do Remote (volatile — atualizado no callback)
    volatile PacoteRemote& ultimoPacote() { return _ultimoPacote; }
    bool novoPacoteRecebido() const { return _novoPacote; }
    void limparNovoPacote() { _novoPacote = false; }

private:
    // Assinatura compatível com ESP-IDF 5.x (Arduino ESP32 >= 3.x) e versões anteriores
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len);
#endif

    // Ponteiros estáticos para acesso no callback C
    static WatchdogComm* _pWatchdog;
    static volatile PacoteRemote _ultimoPacote;
    static volatile bool         _novoPacote;
};

#endif // COMUNICACAO_H
