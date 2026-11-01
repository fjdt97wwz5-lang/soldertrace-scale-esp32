// Pantalla 320x240 (ST7789) y tactil CST816D. Dibuja la pantalla que diga app.h.
#pragma once
#include <Arduino.h>

void uiBegin();            // pantalla + tactil (inicia el I2C)
void uiLoop();             // repinta lo que cambie y atiende los toques
void uiForceRedraw();
void uiBootStep(const char* text);
