// Hora: internet (NTP), reloj DS3231 opcional y, si no hay otra, la hora que manda el servidor.
// Las mismas reglas que las basculas dosificadoras: al encender espera hasta 1:30 a tener hora.
#pragma once
#include <Arduino.h>

void clockBegin(const char* tz);     // zona horaria POSIX
void clockSetTz(const char* tz);
void clockLoop();
bool clockValid();
void clockFromServer(uint32_t epoch); // solo si aun no hay hora
const char* clockSource();            // "internet", "ds3231", "server", "none"
bool clockHasDs3231();
void clockHHMM(char* out, size_t n);
