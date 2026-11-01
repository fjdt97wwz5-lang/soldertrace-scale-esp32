#include "scale.h"

ScaleState scale;
static HardwareSerial ScaleSerial(1);
static char lineBuf[64];
static size_t lineLen = 0;
static bool lineOverflow = false;

// Ventana de estabilidad: lecturas de los ultimos cfgStableMs
static const int WIN = 64;
static float winG[WIN];
static unsigned long winT[WIN];
static int winHead = 0, winCount = 0;

static bool parseWeight(const char* s, float& out) {
  const char* p = s;
  while (*p) {
    if (isdigit((unsigned char)*p)) break;
    if (*p == '-' || *p == '+') {
      const char* q = p + 1;
      while (*q == ' ') q++;
      if (isdigit((unsigned char)*q)) break;
    }
    p++;
  }
  if (!*p) return false;
  float sign = 1.0f;
  if (*p == '-' || *p == '+') {
    if (*p == '-') sign = -1.0f;
    p++;
    while (*p == ' ') p++;
  }
  char* end;
  float v = strtof(p, &end);
  if (end == p) return false;
  if (strstr(end, "kg") || strstr(end, "KG")) v *= 1000.0f;
  out = sign * v;
  return true;
}

void scaleBegin() {
  ScaleSerial.setRxBufferSize(1024);
  ScaleSerial.begin(SCALE_BAUD, SERIAL_8N1, PIN_SCALE_RX, PIN_SCALE_TX);
}

static void pushReading(float g) {
  winG[winHead] = g;
  winT[winHead] = millis();
  winHead = (winHead + 1) % WIN;
  if (winCount < WIN) winCount++;
}

void scaleLoop() {
  while (ScaleSerial.available()) {
    char c = ScaleSerial.read();
    if (c == '\n' || c == '\r') {
      lineBuf[lineLen] = 0;
      if (lineLen > 0 && !lineOverflow) {
        float g;
        if (parseWeight(lineBuf, g)) {
          scale.grams = g;
          scale.unstableFlag = strstr(lineBuf, "US") != nullptr;
          scale.lastMs = millis();
          scale.has = true;
          pushReading(scale.unstableFlag ? NAN : g);
        }
      }
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }
}

bool scaleOnline() { return scale.has && millis() - scale.lastMs < SCALE_TIMEOUT_MS; }

bool scaleAtZero() { return scaleOnline() && fabsf(scale.grams) <= cfgZeroTol; }

void scaleResetStable() { winCount = 0; winHead = 0; }

bool scaleStable(float& g) {
  if (!scaleOnline() || winCount < 3) return false;
  unsigned long now = millis();
  float mn = 1e9f, mx = -1e9f, sum = 0;
  int n = 0;
  unsigned long oldest = now;
  for (int i = 0; i < winCount; i++) {
    int k = (winHead - 1 - i + WIN) % WIN;
    if (now - winT[k] > cfgStableMs) break;
    if (isnan(winG[k])) return false;      // la balanza dijo inestable dentro de la ventana
    mn = min(mn, winG[k]); mx = max(mx, winG[k]); sum += winG[k]; n++;
    oldest = winT[k];
  }
  // la ventana tiene que cubrir casi todo el tiempo pedido (lecturas continuas)
  if (n < 3 || now - oldest < cfgStableMs * 8 / 10) return false;
  if (mx - mn > cfgStableBand) return false;
  g = roundf(sum / n * 100.0f) / 100.0f;
  return true;
}
