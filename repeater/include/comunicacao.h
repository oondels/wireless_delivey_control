/**
 * comunicacao.h — Comunicação ESP-NOW do Módulo Repeater
 *
 * O Repeater valida origem/destino/rota e encaminha somente pacotes no sentido
 * permitido. Ele não decide movimento nem mantém último comando válido.
 */

#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include "protocolo.h"

class Comunicacao {
public:
    void init();

private:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
#endif
    static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);
};

#endif // COMUNICACAO_H
