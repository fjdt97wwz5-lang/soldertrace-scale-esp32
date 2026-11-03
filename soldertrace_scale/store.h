// Memoria permanente de la bascula:
//  - NVS ("st"): WiFi, servidor, token de alta, numero, nombre, idioma, zona horaria, activada
//  - LittleFS /q.bin: pesajes hechos sin conexion (cola). Se reenvian al volver la conexion.
//    Un pesaje solo sale de la cola cuando el servidor confirma que lo grabo ("confirmado hasta").
//    El "confirmado hasta" se escribe rotando entre 8 posiciones para no gastar siempre la misma celda.
//    Los numeros pendientes se llevan tambien en RAM (contar no lee el archivo) y el archivo se
//    reescribe sin los confirmados (/q.tmp + rename, seguro ante un corte de luz).
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
  uint32_t zeroHoldMs;       // tiempo seguido dentro de la banda de cero
  uint32_t wifiWaitS;        // sin la WiFi guardada este tiempo = abre su red de configuracion
  uint32_t apTimeoutMin;     // la red de configuracion se cierra sola
  char pinHash[136] = "";     // "<sal>$<sha256 hex>" del PIN de Ajustes ("" = sin PIN)
  bool demo = false;         // modo demo: balanza emulada, pesajes marcados como demo
};

struct QueuedReading {        // 64 bytes
  uint32_t seq;
  uint32_t ts;                // hora unix (0 = sin hora)
  float    grams;
  uint8_t  stable;
  uint8_t  approx;            // hora aproximada
  uint8_t  demo;              // pesaje hecho en modo demo
  uint8_t  pad[1];
  char     requestId[48];
};

extern Settings cfg;
extern uint32_t nextSeq;     // siguiente numero de secuencia
extern uint32_t ackedUpTo;   // ultimo confirmado por el servidor

void storeBegin();
void storeSave();            // guarda cfg
void storeClearPairing();
uint32_t queueAdd(const char* requestId, float grams, bool stable, bool demo);
int  queueCount();                        // pendientes (RAM, no lee el archivo)
int  queueCountUpTo(uint32_t seq);        // pendientes con numero <= seq (RAM)
int  queueRead(uint32_t afterSeq, QueuedReading* out, int max);  // pendientes con numero > afterSeq, en orden, una pasada
void queueAck(uint32_t upTo);
