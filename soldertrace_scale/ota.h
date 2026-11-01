// Actualizacion por WiFi desde el portal: el servidor manda la direccion del .bin.
// Nunca corta un pesaje: espera a que la bascula este libre. Se comprueba la marca
// SOLDERTRACE-SCALE-FW: y el MD5; si algo falla, la placa sigue con la version que tenia.
#pragma once
#include <Arduino.h>

void otaRequest(const char* url, const char* version, const char* md5, uint32_t size);
bool otaPending();
void otaLoop(bool idle);   // idle = no hay pesaje en curso
void otaMarkValid();       // en el arranque: la version nueva funciona
