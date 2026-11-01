// WiFi de la bascula.
//  - Normal: se conecta a la WiFi del cliente guardada en la placa (nada quemado en el codigo),
//    con el mismo vigilante que las dosificadoras (reintenta a los 20 s y reinicia el WiFi cada minuto).
//  - Configuracion: la placa crea su propia red "SolderTrace-XXXX" con una contrasena de 10 caracteres
//    que sale en la pantalla. Desde el movil se abre http://192.168.4.1 y se elige la WiFi, el idioma y el servidor.
#pragma once
#include <Arduino.h>

void wifiBegin();          // modo normal (o configuracion si no hay WiFi guardada)
void wifiLoop();
bool wifiConnected();
void wifiStartSetup();     // entra en modo configuracion
bool wifiInSetup();
unsigned long wifiDownFor();  // ms sin WiFi teniendo una guardada (0 = conectada o en configuracion)
