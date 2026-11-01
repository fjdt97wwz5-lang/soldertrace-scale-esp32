#include "scale.h"

ScaleState scale;
static HardwareSerial ScaleSerial(1);
static char lineBuf[64];
static size_t lineLen = 0;
static bool lineOverflow = false;

// Ventana de estabilidad: lecturas agrupadas en tramos de 50 ms (cada tramo guarda minimo, maximo,
// suma y si la balanza dijo "US"), asi la ventana cubre el tiempo pedido sea cual sea la velocidad
// de la balanza: 256 tramos x 50 ms = 12,8 s >= STABLE_MS_MAX.
static const int WIN = 256;
static const unsigned long BUCKET_MS = 50;
static_assert(WIN * BUCKET_MS >= STABLE_MS_MAX, "la ventana de estabilidad no cubre STABLE_MS_MAX");
struct Bucket { float mn, mx, sum; uint16_t n; bool unstable; unsigned long t; };
static Bucket win[WIN];
static int winHead = 0, winCount = 0;
static Bucket cur;
static bool curOpen = false;

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
  ScaleSerial.setRxBufferSize(8192);   // ~8 s a 9600 baudios: aguanta un bloqueo de la conexion
  ScaleSerial.begin(SCALE_BAUD, SERIAL_8N1, PIN_SCALE_RX, PIN_SCALE_TX);
}

static void closeBucket() {
  if (!curOpen) return;
  win[winHead] = cur;
  winHead = (winHead + 1) % WIN;
  if (winCount < WIN) winCount++;
  curOpen = false;
}

static void pushReading(float g, bool unstable) {
  unsigned long now = millis();
  if (curOpen && now - cur.t >= BUCKET_MS) closeBucket();
  if (!curOpen) { cur.mn = cur.mx = g; cur.sum = 0; cur.n = 0; cur.unstable = false; cur.t = now; curOpen = true; }
  cur.mn = min(cur.mn, g); cur.mx = max(cur.mx, g);
  cur.sum += g; cur.n++;
  cur.unstable |= unstable;
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
          pushReading(g, scale.unstableFlag);
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

void scaleResetStable() { winCount = 0; winHead = 0; curOpen = false; }

bool scaleStable(float& g) {
  if (!scaleOnline()) return false;
  unsigned long now = millis();
  float mn = 1e9f, mx = -1e9f, sum = 0;
  int n = 0;
  unsigned long oldest = now;
  // el tramo abierto (lo mas reciente) y luego los cerrados, del mas nuevo al mas viejo
  for (int i = -1; i < winCount; i++) {
    const Bucket* b;
    if (i < 0) { if (!curOpen) continue; b = &cur; }
    else b = &win[(winHead - 1 - i + WIN) % WIN];
    if (now - b->t > cfgStableMs) break;
    if (b->unstable) return false;         // la balanza dijo inestable dentro de la ventana
    mn = min(mn, b->mn); mx = max(mx, b->mx); sum += b->sum; n += b->n;
    oldest = b->t;
  }
  // la ventana tiene que cubrir casi todo el tiempo pedido (lecturas continuas)
  if (n < 3 || now - oldest < cfgStableMs * 8 / 10) return false;
  if (mx - mn > cfgStableBand) return false;
  g = roundf(sum / n * 100.0f) / 100.0f;
  return true;
}
