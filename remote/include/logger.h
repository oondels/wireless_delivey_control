/**
 * logger.h — Sistema de logging via Serial para debug e testes
 *
 * Formato: [timestamp_ms] [NIVEL] [MODULO] mensagem
 *
 * Para desabilitar logging em producao, adicionar no platformio.ini:
 *   build_flags = -DLOG_DISABLED
 *
 * Arquivo compartilhado entre Principal e Remote.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Habilitado por padrao; desabilitar com -DLOG_DISABLED
#ifndef LOG_DISABLED
  #define LOG_ENABLED
#endif

#ifdef LOG_ENABLED

  // Log simples (sem valor)
  #define LOG_INFO(modulo, msg) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [INFO] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.println(msg); \
  } while(0)

  #define LOG_WARN(modulo, msg) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [WARN] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.println(msg); \
  } while(0)

  #define LOG_ERROR(modulo, msg) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [ERRO] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.println(msg); \
  } while(0)

  // Log com valor numerico ou string anexado
  #define LOG_INFO_VAL(modulo, msg, val) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [INFO] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.print(msg); Serial.println(val); \
  } while(0)

  #define LOG_WARN_VAL(modulo, msg, val) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [WARN] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.print(msg); Serial.println(val); \
  } while(0)

  #define LOG_ERROR_VAL(modulo, msg, val) do { \
      Serial.print("["); Serial.print(millis()); \
      Serial.print("] [ERRO] ["); Serial.print(modulo); \
      Serial.print("] "); Serial.print(msg); Serial.println(val); \
  } while(0)

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

#else

  #define LOG_INFO(modulo, msg)
  #define LOG_WARN(modulo, msg)
  #define LOG_ERROR(modulo, msg)
  #define LOG_INFO_VAL(modulo, msg, val)
  #define LOG_WARN_VAL(modulo, msg, val)
  #define LOG_ERROR_VAL(modulo, msg, val)
  inline const char* comandoParaString(uint8_t) { return ""; }

#endif // LOG_ENABLED

#endif // LOGGER_H
