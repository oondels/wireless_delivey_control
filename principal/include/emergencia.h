/**
 * emergencia.h — Lógica de emergência do Módulo Principal
 *
 * Gerencia a flag _ativa e a leitura do botão EMERGÊNCIA
 * (trava mecânica, nível contínuo).
 *
 * Auto-liberação: quando botão local é solto E remote não sinaliza emergência,
 * _ativa é limpa automaticamente. REARME é necessário apenas quando o remote
 * mantém emergência ativa e não é possível acessá-lo.
 *
 * Ref: seguranca/SPEC.md §3
 */

#ifndef EMERGENCIA_H
#define EMERGENCIA_H

#include <Arduino.h>
#include "pinout.h"

class Emergencia {
public:
    void init();

    // Verifica botão local e emergência do Remote. Se qualquer um ativo,
    // seta _ativa = true. Retorna estado atual da flag.
    bool verificar(bool emergenciaRemote);

    // Retorna true se o botão EMERGÊNCIA local está fisicamente pressionado (LOW)
    bool botaoLocalAtivo() const;

    // Acesso à flag — volatile pois é alterada no callback ESP-NOW
    volatile bool& ativa() { return _ativa; }
    bool isAtiva() const { return _ativa; }

private:
    volatile bool _ativa              = false;
    bool          _botaoLocalAnterior = false;  // false = não estava ativo
};

#endif // EMERGENCIA_H
