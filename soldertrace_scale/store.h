// Memoria permanente de la bascula:
//  - NVS ("st"): WiFi, servidor, token de alta, numero, nombre, idioma, zona horaria, activada
//  - LittleFS /q.bin: pesajes hechos sin conexion (cola). Se reenvian al volver la conexion.
//    Un pesaje solo sale de la cola cuando el servidor confirma que lo grabo ("confirmado hasta").
//    El "confirmado hasta" se escribe rotando entre 8 posiciones para no gastar siempre la misma celda.
#pragma once
#include <Arduino.h>

struct Settings {
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char serverUrl[128] = "";
  char token[72] = "";       // lo da el portal al dar de alta la bascula
  int  number = 0;           // B1, B2... (0 = sin numero)
  char name[33] = "";
  char lang[8] = "en";
  char tz[64] = "";
  bool enabled = true;
  float stableBand, zeroTol, minJar;
  uint32_t stableMs;
};

struct QueuedReading {        // 64 bytes
  uint32_t seq;
  uint32_t ts;                // hora unix (0 = sin hora)
  float    grams;
  uint8_t  stable;
  uint8_t  approx;            // hora aproximada
  uint8_t  pad[2];
  char     requestId[48];
};

extern Settings cfg;
extern uint32_t nextSeq;     // siguiente numero de secuencia
extern uint32_t ackedUpTo;   // ultimo confirmado por el servidor

void storeBegin();
void storeSave();            // guarda cfg
void storeClearPairing();
uint32_t queueAdd(const char* requestId, float grams, bool stable);
int  queueCount();
bool queueGet(int i, QueuedReading& r);   // i-esimo pendiente (orden de seq)
void queueAck(uint32_t upTo);
