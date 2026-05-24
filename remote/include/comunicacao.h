/**
 * comunicacao.h — Comunicação ESP-NOW do Módulo Remote
 *
 * Inicialização WiFi + ESP-NOW, registro fixo do peer Principal com criptografia,
 * envio de PacoteRemote (heartbeat 100ms + imediato em mudança),
 * callback OnDataRecv para receber PacoteStatus.
 *
 * Ref: comunicacao/SPEC.md §7, §9.1
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

    // Envia PacoteRemote para o Principal (calcula checksum antes)
    void enviarPacote(const PacoteRemote& pacote);

    // Mede e escolhe preventivamente a melhor rota disponivel.
    void enviarProbes();
    void atualizarRota();
    uint8_t rotaAtual() const;
    bool rotaAtualOperacional() const;
    const char* rotaAtualNome() const;
    int scoreRota(uint8_t rota) const;

    // Último PacoteStatus recebido do Principal (volatile — atualizado no callback)
    volatile PacoteStatus& ultimoStatus() { return _ultimoStatus; }

    // Timestamp do último status recebido (para controle de timeout LINK)
    uint32_t ultimoStatusRecebidoMs() const { return _ultimoStatusMs; }

    // Membros estáticos atualizados pelo callback ESP-NOW.
    static volatile PacoteStatus _ultimoStatus;
    static volatile uint32_t     _ultimoStatusMs;
    static volatile uint8_t      _rotaAtual;

private:
    // Assinatura compatível com ESP-IDF 5.x (Arduino ESP32 >= 3.x) e versões anteriores
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onDataRecv(const uint8_t* mac, const uint8_t* data, int len);
#endif
    static void onDataSent(const uint8_t* mac, esp_now_send_status_t status);

};

#endif // COMUNICACAO_H
