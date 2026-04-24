/**
 * logger.h — Sistema de logging via Serial para debug e testes
 *
 * Formato: [timestamp_ms] [NIVEL] [MODULO] mensagem
 *
 * Modos suportados:
 *   - producao (padrao): INFO de debug oculto; WARN/ERRO ativos
 *   - desenvolvimento: adicionar -DAPP_ENV_DEV no build
 *   - desligado: adicionar -DLOG_DISABLED
 *
 * Arquivo compartilhado entre Principal e Remote.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

#define LOG_NOOP() do { } while (0)

#define LOG_IMPL(nivel, modulo, msg) do { \
    Serial.print("["); Serial.print(millis()); \
    Serial.print("] ["); Serial.print(nivel); \
    Serial.print("] ["); Serial.print(modulo); \
    Serial.print("] "); Serial.println(msg); \
} while(0)

#define LOG_IMPL_VAL(nivel, modulo, msg, val) do { \
    Serial.print("["); Serial.print(millis()); \
    Serial.print("] ["); Serial.print(nivel); \
    Serial.print("] ["); Serial.print(modulo); \
    Serial.print("] "); Serial.print(msg); Serial.println(val); \
} while(0)

#ifdef LOG_DISABLED

  #define LOG_INFO(modulo, msg)       LOG_NOOP()
  #define LOG_WARN(modulo, msg)       LOG_NOOP()
  #define LOG_ERROR(modulo, msg)      LOG_NOOP()
  #define LOG_ALWAYS(modulo, msg)     LOG_IMPL("INFO", modulo, msg)
  #define LOG_INFO_VAL(modulo, msg, val)   LOG_NOOP()
  #define LOG_WARN_VAL(modulo, msg, val)   LOG_NOOP()
  #define LOG_ERROR_VAL(modulo, msg, val)  LOG_NOOP()
  #define LOG_ALWAYS_VAL(modulo, msg, val) LOG_IMPL_VAL("INFO", modulo, msg, val)

#elif defined(APP_ENV_DEV)

  #define LOG_INFO(modulo, msg)       LOG_IMPL("INFO", modulo, msg)
  #define LOG_WARN(modulo, msg)       LOG_IMPL("WARN", modulo, msg)
  #define LOG_ERROR(modulo, msg)      LOG_IMPL("ERRO", modulo, msg)
  #define LOG_ALWAYS(modulo, msg)     LOG_IMPL("INFO", modulo, msg)
  #define LOG_INFO_VAL(modulo, msg, val)   LOG_IMPL_VAL("INFO", modulo, msg, val)
  #define LOG_WARN_VAL(modulo, msg, val)   LOG_IMPL_VAL("WARN", modulo, msg, val)
  #define LOG_ERROR_VAL(modulo, msg, val)  LOG_IMPL_VAL("ERRO", modulo, msg, val)
  #define LOG_ALWAYS_VAL(modulo, msg, val) LOG_IMPL_VAL("INFO", modulo, msg, val)

#else

  #define LOG_INFO(modulo, msg)       LOG_NOOP()
  #define LOG_WARN(modulo, msg)       LOG_IMPL("WARN", modulo, msg)
  #define LOG_ERROR(modulo, msg)      LOG_IMPL("ERRO", modulo, msg)
  #define LOG_ALWAYS(modulo, msg)     LOG_IMPL("INFO", modulo, msg)
  #define LOG_INFO_VAL(modulo, msg, val)   LOG_NOOP()
  #define LOG_WARN_VAL(modulo, msg, val)   LOG_IMPL_VAL("WARN", modulo, msg, val)
  #define LOG_ERROR_VAL(modulo, msg, val)  LOG_IMPL_VAL("ERRO", modulo, msg, val)
  #define LOG_ALWAYS_VAL(modulo, msg, val) LOG_IMPL_VAL("INFO", modulo, msg, val)

#endif

// Helper: converte Comando para string legivel
inline const char* comandoParaString(uint8_t cmd) {
    switch (cmd) {
        case 0: return "HEARTBEAT";
        case 1: return "SUBIR";
        case 2: return "DESCER";
        case 3: return "VEL1";
        case 4: return "VEL2";
        case 5: return "RESET";
        default: return "DESCONHECIDO";
    }
}

#endif // LOGGER_H
