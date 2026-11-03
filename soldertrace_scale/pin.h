// PIN de 4 cifras para abrir Ajustes en la bascula.
// El PIN se pone en el portal; a la placa solo le llega su huella "<sal>$<sha256(sal + PIN) en hex>".
// Sin huella (PIN no puesto en el portal) Ajustes se abre sin pedirlo. 3 fallos = bloqueado 1 minuto.
#pragma once
#include <Arduino.h>

enum PinResult { PIN_OK, PIN_WRONG, PIN_LOCKED };

bool pinRequired();                  // hay PIN puesto desde el portal
PinResult pinCheck(const char* pin); // comprueba 4 cifras (cuenta fallos y bloquea)
unsigned long pinLockedFor();        // ms que faltan de bloqueo (0 = libre)
int pinTriesLeft();
