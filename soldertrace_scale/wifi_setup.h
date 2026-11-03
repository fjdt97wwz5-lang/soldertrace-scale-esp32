// WiFi de la bascula.
//  - Normal: se conecta a la WiFi del cliente guardada en la placa (nada quemado en el codigo),
//    con el mismo vigilante que las dosificadoras (reintenta a los 20 s y reinicia el WiFi cada minuto).
//    Si la WiFi guardada no aparece en cfg.wifiWaitS (1:30 por defecto, el portal lo cambia), abre sola la red de
//    configuracion con QR en pantalla; se cierra sola a los cfg.apTimeoutMin minutos (10 por defecto) y vuelve a
//    buscar la WiFi guardada. Nunca se abre a mitad de un pesaje. Los pesajes sin conexion quedan en la cola.
//  - Configuracion: la placa crea su propia red "SolderTrace-XXXX" con una contrasena de 10 caracteres
//    que sale en la pantalla. Desde el movil se abre http://192.168.4.1 y se elige la WiFi, el idioma y el servidor.
#pragma once
#include <Arduino.h>

void wifiBegin();          // modo normal (o configuracion si no hay WiFi guardada)
void wifiLoop();
bool wifiConnected();
void wifiStartSetup(bool automatic = false);  // entra en modo configuracion (automatic: la WiFi guardada no aparece)
void wifiStopSetup();      // cierra la red de configuracion y vuelve a la WiFi guardada (sin reiniciar)
unsigned long wifiSetupLeft();  // ms hasta que la red de configuracion se cierre sola (0 = no se cierra sola)
bool wifiSetupAuto();
bool wifiInSetup();
unsigned long wifiDownFor();  // ms sin WiFi teniendo una guardada (0 = conectada o en configuracion)
