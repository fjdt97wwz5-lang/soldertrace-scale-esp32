// Lectura de la balanza Rhuisan por RS-232 (modo continuo) y deteccion de peso estable.
// Mismo analizador que las basculas dosificadoras: toma el primer numero de cada linea,
// con signo, y pasa kg a g. Si la balanza manda "US" (unstable) en la linea, no cuenta como estable.
#pragma once
#include <Arduino.h>
#include "config.h"

struct ScaleState {
  float  grams = NAN;          // ultima lectura
  bool   unstableFlag = false; // la balanza dijo "US"
  unsigned long lastMs = 0;    // millis de la ultima lectura valida
  bool   has = false;
};

extern ScaleState scale;
extern float cfgStableBand, cfgZeroTol;
extern unsigned long cfgStableMs;

void scaleBegin();
void scaleLoop();
bool scaleOnline();
bool scaleAtZero();          // |peso| <= tolerancia de cero
bool scaleStable(float& g);  // peso quieto cfgStableMs dentro de cfgStableBand; devuelve la media
void scaleResetStable();
