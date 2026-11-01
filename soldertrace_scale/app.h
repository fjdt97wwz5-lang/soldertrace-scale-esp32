// Estado de la aplicacion compartido entre modulos (pantalla, conexion, pesaje).
#pragma once
#include <Arduino.h>

enum Screen {
  S_BOOT, S_SETUP_AP, S_WAIT_TIME, S_CONNECTING, S_PAIR, S_DISABLED, S_NO_SCALE, S_READY,
  S_JOB_ZERO, S_JOB_PLACE, S_JOB_HOLD, S_RESULT, S_OTA, S_MENU, S_INFO
};

// Pesaje pedido por el portal: "pesa este envase en esta bascula"
struct Job {
  bool active = false;
  char requestId[48] = "";
  char qr[48] = "";
  char product[48] = "";
  char lot[32] = "";
  char mode[12] = "";        // remove | return | receive | check
  float expected = NAN;      // peso esperado (g), NAN = sin dato
  float tol = NAN;           // tolerancia (g)
  unsigned long deadline = 0;
};

enum ResultKind { RK_OK, RK_OUT, RK_HOLD, RK_CANCELLED, RK_TIMEOUT, RK_OFFLINE, RK_SENDING, RK_ERROR };
struct Result {
  bool show = false;
  ResultKind kind = RK_OK;
  float grams = NAN;
  char requestId[48] = "";
  char msg[64] = "";
  unsigned long at = 0;
};

extern Job job;
extern Result result;
extern Screen screen;
extern bool menuOpen, infoOpen;
extern int activationState;     // 0 nada, 1 enviada, 2 rechazada
extern char pairCode[8];
extern char apPassword[12];
extern char apSsid[32];
extern bool setupMode;
extern int otaProgress;         // -1 = sin OTA, 0..100
extern bool otaFailed;
extern char chipId[16], macStr[18];

// Llamadas del enlace con el servidor
void appOnWeigh(const char* requestId, const char* qr, const char* product, const char* lot,
                const char* mode, float expected, float tol, uint32_t expiresS);
void appOnCancel(const char* requestId);
void appOnResult(const char* requestId, const char* status, const char* message);
void appOnConfigApplied();
void appCancelJob(const char* reason);   // desde el boton CANCELAR
void appEnterSetup();                    // red de configuracion WiFi
void appRestart();                       // marca la version como buena y reinicia
