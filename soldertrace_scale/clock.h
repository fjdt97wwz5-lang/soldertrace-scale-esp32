// Hora: internet (NTP), reloj DS3231 opcional y, si no hay otra, la hora que manda el servidor.
// Las mismas reglas que las basculas dosificadoras: al encender espera hasta 1:30 a tener hora.
#pragma once
#include <Arduino.h>

void clockBegin(const char* tz);     // zona horaria POSIX
void clockSetTz(const char* tz);
void clockLoop();
bool clockValid();
void clockFromServer(uint32_t epoch); // si no hay hora, o si la que hay no es de internet y difiere > 2 min
const char* clockSource();            // "internet", "ds3231", "server", "none"
bool clockHasDs3231();
void clockHHMM(char* out, size_t n);
