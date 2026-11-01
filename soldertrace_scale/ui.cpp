#include "ui.h"
#include "app.h"
#include "config.h"
#include "lang.h"
#include "store.h"
#include "scale.h"
#include "clock.h"
#include "link.h"
#include "wifi_setup.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>

static Arduino_DataBus* bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
static Arduino_GFX* gfx = new Arduino_ST7789(bus, GFX_NOT_DEFINED, 1 /*horizontal*/, true /*IPS*/, 240, 320, 0, 0, 0, 0);

constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static const uint16_t BG = C565(12, 18, 16), PANEL = C565(30, 40, 36), FG = C565(236, 242, 238), DIM = C565(130, 146, 140);
static const uint16_t GREEN = C565(34, 170, 90), AMBER = C565(230, 150, 20), RED = C565(215, 50, 45);
static const uint16_t BLUE = C565(40, 120, 220), GRAY = C565(80, 92, 88), TEAL = C565(20, 130, 130);

static const int W = 320, H = 240, HEAD = 34;
struct Btn { int x, y, w, h; };
static const Btn BTN_FULL = {8, 200, 304, 34};
static const Btn BTN_M1 = {8, 50, 148, 60}, BTN_M2 = {164, 50, 148, 60}, BTN_M3 = {8, 120, 148, 60}, BTN_M4 = {164, 120, 148, 60};

static int drawnScreen = -1;
static uint32_t drawnHash = 0;
static uint16_t headColor = BLUE;
static bool forceAll = true;

// ---------- texto ----------
static void txt(const char* utf8, int x, int y, uint8_t size, uint16_t fg, uint16_t bg) {
  char b[96];
  toCp437(utf8, b, sizeof(b));
  gfx->setTextSize(size);
  gfx->setTextColor(fg, bg);
  gfx->setCursor(x, y);
  gfx->print(b);
}
static void txtC(const char* utf8, int y, uint8_t size, uint16_t fg, uint16_t bg) {
  int w = textWidth(utf8, size);
  if (w > W && size > 1) { size--; w = textWidth(utf8, size); }
  txt(utf8, max(0, (W - w) / 2), y, size, fg, bg);
}
// texto que cabe en un ancho: si es largo se corta con "..."
static void txtFit(const char* utf8, int x, int y, uint8_t size, int maxW, uint16_t fg, uint16_t bg) {
  char b[64];
  strlcpy(b, utf8, sizeof(b));
  int maxChars = maxW / (6 * size);
  if (textWidth(b, size) > maxW && maxChars > 3) {
    // corta por caracteres (UTF-8)
    int n = 0; char* p = b;
    while (*p && n < maxChars - 3) { if ((*p & 0xC0) != 0x80) n++; p++; }
    while ((*p & 0xC0) == 0x80) p++;
    strcpy(p, "...");
  }
  txt(b, x, y, size, fg, bg);
}

static void wifiIcon(int cx, int cy, uint16_t c) {
  const float bands[4][2] = {{0, 2.3}, {4.6, 6.6}, {8.6, 10.6}, {12.6, 14.6}};
  for (int dy = -15; dy <= 0; dy++)
    for (int dx = -15; dx <= 15; dx++) {
      if (abs(dx) > -dy) continue;
      float d = sqrtf((float)(dx * dx + dy * dy));
      for (int b = 0; b < 4; b++) if (d >= bands[b][0] && d <= bands[b][1]) { gfx->drawPixel(cx + dx, cy + dy, c); break; }
    }
  gfx->fillCircle(cx, cy - 1, 2, c);
}

// ---------- encabezado: SolderTrace, B1, servidor, WiFi, hora ----------
static void headerDynamic(bool force) {
  static int lastMin = -2, lastNet = -1;
  char t[8];
  clockHHMM(t, sizeof(t));
  int minute = (t[0] - '0') * 600 + (t[1] - '0') * 60 + (t[3] - '0') * 10 + (t[4] - '0');
  int net = (wifiConnected() ? 1 : 0) | (linkReady() ? 2 : (linkConnected() ? 4 : 0));
  if (!force && minute == lastMin && net == lastNet) return;
  lastMin = minute; lastNet = net;
  txt(t, 258, 9, 2, FG, headColor);
  gfx->fillRect(212, 4, 42, 26, headColor);
  if (net & 1) wifiIcon(240, 25, FG);
  // punto de servidor: verde conectado, ambar sin alta, rojo sin conexion
  uint16_t dot = (net & 2) ? GREEN : ((net & 4) ? AMBER : RED);
  gfx->fillCircle(218, 17, 5, dot);
  gfx->drawCircle(218, 17, 5, FG);
}

static void header(const char* title, uint16_t color) {
  headColor = color;
  gfx->fillRect(0, 0, W, HEAD, color);
  txtFit(title, 8, 9, 2, 130, FG, color);
  if (cfg.number > 0) {
    char id[6]; snprintf(id, sizeof(id), "B%d", cfg.number);
    int w = strlen(id) * 12 + 6, x = 206 - w;
    gfx->fillRoundRect(x, 7, w, 20, 4, FG);
    txt(id, x + 3, 10, 2, color, FG);
  }
  headerDynamic(true);
}

static void button(const Btn& b, const char* label, uint16_t color, bool active) {
  uint16_t fill = active ? color : PANEL, ink = active ? FG : GRAY;
  gfx->fillRoundRect(b.x, b.y, b.w, b.h, 10, fill);
  uint8_t s = textWidth(label, 3) <= b.w - 10 ? 3 : 2;
  int tw = textWidth(label, s);
  txt(label, b.x + (b.w - tw) / 2, b.y + (b.h - 8 * s) / 2 + 1, s, ink, fill);
}

static bool inBtn(const Btn& b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

static void weightText(float g, char* out, size_t n) {
  if (isnan(g)) { strlcpy(out, "  --.-- g", n); return; }
  g = constrain(g, -999.99f, 9999.99f);
  snprintf(out, n, "%7.2f g", g);
}

static const char* modeText() {
  if (!strcmp(job.mode, "remove")) return tr(T_MODE_REMOVE);
  if (!strcmp(job.mode, "return")) return tr(T_MODE_RETURN);
  if (!strcmp(job.mode, "receive")) return tr(T_MODE_RECEIVE);
  return tr(T_MODE_CHECK);
}

// ---------- pantallas ----------
static void drawJobInfo() {
  gfx->fillRect(0, HEAD, W, 56, PANEL);
  txtFit(modeText(), 8, HEAD + 6, 1, 304, DIM, PANEL);
  txtFit(job.product[0] ? job.product : job.qr, 8, HEAD + 18, 2, 304, FG, PANEL);
  char l[72];
  if (!isnan(job.expected)) {
    char e[16]; snprintf(e, sizeof(e), "%.2f", job.expected);
    if (!isnan(job.tol)) snprintf(l, sizeof(l), "%s  %s: %s g +/-%.2f", job.lot, tr(T_EXPECTED), e, job.tol);
    else snprintf(l, sizeof(l), "%s  %s: %s g", job.lot, tr(T_EXPECTED), e);
  } else strlcpy(l, job.lot, sizeof(l));
  txtFit(l, 8, HEAD + 40, 1, 304, DIM, PANEL);
}

static void drawStatic(int s) {
  gfx->fillScreen(BG);
  char line[96];
  switch (s) {
    case S_BOOT:
      txtC("SolderTrace", 70, 4, FG, BG);
      txtC("Scale", 110, 2, TEAL, BG);
      txtC(tr(T_STARTING), 160, 2, DIM, BG);
      txtC(FW_VERSION, 220, 1, DIM, BG);
      break;
    case S_SETUP_AP:
      header(tr(T_WIFI_SETUP), BLUE);
      txtC(tr(T_WIFI_SETUP_HINT1), 50, 1, DIM, BG);
      txtC(apSsid, 68, 2, FG, BG);
      snprintf(line, sizeof(line), "%s %s", tr(T_WIFI_PASSWORD), apPassword);
      txtC(line, 104, 2, AMBER, BG);
      txtC(tr(T_WIFI_SETUP_HINT2), 140, 1, DIM, BG);
      snprintf(line, sizeof(line), "MAC %s  %s", macStr, FW_VERSION);
      txtC(line, 220, 1, DIM, BG);
      break;
    case S_WAIT_TIME:
      header("SolderTrace", AMBER);
      txtC(tr(T_WAIT_TIME), 70, 2, FG, BG);
      txtC(tr(T_WAIT_TIME_HINT), 100, 1, DIM, BG);
      break;
    case S_CONNECTING:
      header("SolderTrace", GRAY);
      txtC(tr(T_CONNECTING), 80, 2, FG, BG);
      snprintf(line, sizeof(line), "Wi-Fi: %s", cfg.wifiSsid);
      txtC(line, 120, 1, DIM, BG);
      break;
    case S_PAIR:
      header("SolderTrace", BLUE);
      txtC(tr(T_PAIR_TITLE), 48, 2, DIM, BG);
      if (pairCode[0]) {
        char c[10]; snprintf(c, sizeof(c), "%.3s %.3s", pairCode, pairCode + 3);
        txtC(c, 78, 5, FG, BG);
      } else txtC("...", 78, 5, DIM, BG);
      txtC(tr(T_PAIR_HINT2), 136, 1, DIM, BG);
      txtC(tr(T_PAIR_HINT1), 152, 2, TEAL, BG);
      snprintf(line, sizeof(line), "MAC %s", macStr);
      txtC(line, 220, 1, DIM, BG);
      break;
    case S_DISABLED:
      header("SolderTrace", RED);
      txtC(tr(T_DISABLED), 70, 2, RED, BG);
      txtC(tr(T_DISABLED_HINT), 104, 1, FG, BG);
      if (activationState == 1) button(BTN_FULL, tr(T_REQUEST_SENT), AMBER, false);
      else {
        if (activationState == 2) txtC(tr(T_REQUEST_REJECTED), 140, 2, AMBER, BG);
        button(BTN_FULL, tr(T_REQUEST_ACT), BLUE, linkReady());
      }
      break;
    case S_NO_SCALE:
      header("SolderTrace", AMBER);
      txtC(tr(T_NO_SCALE), 80, 2, AMBER, BG);
      txtC(tr(T_NO_SCALE_HINT), 112, 1, FG, BG);
      break;
    case S_READY:
      header(cfg.name[0] ? cfg.name : "SolderTrace", GREEN);
      txtC(tr(T_READY), 56, 4, GREEN, BG);
      txtC(tr(T_READY_HINT), 100, 1, FG, BG);
      if (cfg.number > 0) { snprintf(line, sizeof(line), "> B%d", cfg.number); txtC(line, 116, 2, TEAL, BG); }
      break;
    case S_JOB_ZERO:
    case S_JOB_PLACE:
    case S_JOB_HOLD:
      header(cfg.name[0] ? cfg.name : "SolderTrace", TEAL);
      drawJobInfo();
      txtC(s == S_JOB_ZERO ? tr(T_EMPTY_SCALE) : (s == S_JOB_PLACE ? tr(T_PLACE_JAR) : tr(T_HOLD_STILL)), 100, 3,
           s == S_JOB_ZERO ? AMBER : FG, BG);
      button(BTN_FULL, tr(T_CANCEL), RED, true);
      break;
    case S_RESULT: {
      uint16_t c = result.kind == RK_OK ? GREEN : result.kind == RK_OUT ? RED : result.kind == RK_HOLD ? AMBER
                 : result.kind == RK_SENDING ? TEAL : result.kind == RK_OFFLINE ? BLUE : GRAY;
      header(cfg.name[0] ? cfg.name : "SolderTrace", c);
      gfx->fillRect(0, HEAD, W, 120, c);
      const char* big = result.kind == RK_OK ? tr(T_RESULT_OK) : result.kind == RK_OUT ? tr(T_RESULT_OUT)
                      : result.kind == RK_HOLD ? tr(T_RESULT_OUT) : result.kind == RK_CANCELLED ? tr(T_CANCELLED)
                      : result.kind == RK_TIMEOUT ? tr(T_TIMEOUT) : result.kind == RK_SENDING ? tr(T_SENDING)
                      : result.kind == RK_OFFLINE ? tr(T_SAVED_OFFLINE) : tr(T_ERROR);
      txtC(big, HEAD + 14, result.kind == RK_OFFLINE ? 1 : 3, FG, c);
      char wt[20]; weightText(result.grams, wt, sizeof(wt));
      if (!isnan(result.grams)) txtC(wt, HEAD + 50, 4, FG, c);
      if (result.kind == RK_HOLD) txtC(tr(T_RESULT_HOLD), HEAD + 98, 1, FG, c);
      else if (result.msg[0]) txtC(result.msg, HEAD + 98, 1, FG, c);
      txtC(tr(T_REMOVE_JAR), 186, 2, DIM, BG);
      break;
    }
    case S_OTA:
      header(tr(T_UPDATING), BLUE);
      gfx->drawRoundRect(20, 110, 280, 30, 6, FG);
      if (otaFailed) txtC(tr(T_UPDATE_FAIL), 160, 1, AMBER, BG);
      break;
    case S_MENU:
      header(tr(T_MENU), GRAY);
      button(BTN_M1, tr(T_MENU_WIFI), BLUE, true);
      button(BTN_M2, tr(T_MENU_INFO), TEAL, true);
      button(BTN_M3, tr(T_MENU_RESTART), AMBER, true);
      button(BTN_M4, tr(T_BACK), GRAY, true);
      break;
    case S_INFO: {
      header(tr(T_MENU_INFO), GRAY);
      int y = 42;
      auto row = [&](const char* k, const char* v) { txt(k, 8, y, 1, DIM, BG); txtFit(v, 90, y, 1, 222, FG, BG); y += 13; };
      char b[64];
      snprintf(b, sizeof(b), "B%d %s", cfg.number, cfg.name); row("Scale", b);
      row("Firmware", FW_VERSION);
      row("MAC", macStr);
      row("Chip", chipId);
      row("IP", WiFi.localIP().toString().c_str());
      row("Wi-Fi", cfg.wifiSsid);
      row(tr(T_SERVER), cfg.serverUrl);
      row("", linkReady() ? tr(T_CONNECTED) : tr(T_NOT_CONNECTED));
      snprintf(b, sizeof(b), "%s%s", clockSource(), clockHasDs3231() ? " + DS3231" : ""); row("Clock", b);
      snprintf(b, sizeof(b), "%d", queueCount()); row(tr(T_PENDING), b);
      row("Language", cfg.lang);
      button(BTN_FULL, tr(T_BACK), GRAY, true);
      break;
    }
  }
}

static uint32_t staticHash(int s) {
  uint32_t h = (uint32_t)s * 2654435761u ^ (uint32_t)langIdx * 97u ^ (uint32_t)cfg.number * 7919u ^ (uint32_t)activationState * 31u;
  for (const char* p = pairCode; *p; p++) h = h * 31 + *p;
  for (const char* p = job.requestId; *p; p++) h = h * 31 + *p;
  for (const char* p = cfg.name; *p; p++) h = h * 31 + *p;
  h ^= (uint32_t)result.kind * 131u ^ (result.show ? 0x55u : 0) ^ (otaFailed ? 0x99u : 0) ^ (linkReady() ? 0x1000u : 0);
  if (s == S_RESULT) h ^= (uint32_t)(result.grams * 100);
  return h;
}

static void drawDynamic(int s) {
  char wt[20];
  if (s == S_WAIT_TIME) {
    unsigned long left = millis() < WAIT_TIME_MS ? (WAIT_TIME_MS - millis()) / 1000 : 0;
    char b[12]; snprintf(b, sizeof(b), " %lu:%02lu ", left / 60, left % 60);
    txtC(b, 130, 3, AMBER, BG);
  }
  if (s == S_READY || s == S_NO_SCALE) {
    weightText(scaleOnline() ? scale.grams : NAN, wt, sizeof(wt));
    txtC(wt, 150, 3, scaleOnline() ? DIM : GRAY, BG);
    int q = queueCount();
    char b[40];
    if (q > 0) snprintf(b, sizeof(b), "  %d %s  ", q, tr(T_PENDING)); else strlcpy(b, "                    ", sizeof(b));
    txtC(b, 186, 1, AMBER, BG);
  }
  if (s == S_JOB_ZERO || s == S_JOB_PLACE || s == S_JOB_HOLD) {
    weightText(scaleOnline() ? scale.grams : NAN, wt, sizeof(wt));
    txtC(wt, 136, 5, s == S_JOB_HOLD ? TEAL : FG, BG);
  }
  if (s == S_OTA && otaProgress >= 0) {
    gfx->fillRoundRect(23, 113, max(4, 274 * otaProgress / 100), 24, 4, GREEN);
    char b[8]; snprintf(b, sizeof(b), " %d%% ", otaProgress);
    txtC(b, 80, 2, FG, BG);
  }
  headerDynamic(false);
}

// ---------- tactil ----------
static bool readTouch(int& x, int& y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TP_ADDR, 5) != 5) return false;
  uint8_t d[5];
  for (int i = 0; i < 5; i++) d[i] = Wire.read();
  if ((d[0] & 0x0F) == 0) return false;
  int rx = ((d[1] & 0x0F) << 8) | d[2], ry = ((d[3] & 0x0F) << 8) | d[4];
  int tx = TOUCH_SWAP_XY ? ry : rx, ty = TOUCH_SWAP_XY ? rx : ry;
  if (TOUCH_INVERT_X) tx = W - 1 - tx;
  if (TOUCH_INVERT_Y) ty = H - 1 - ty;
  x = tx; y = ty;
  return true;
}

static void onTap(int x, int y) {
  int s = screen;
  if (s == S_MENU) {
    if (inBtn(BTN_M1, x, y)) { menuOpen = false; appEnterSetup(); }
    else if (inBtn(BTN_M2, x, y)) { menuOpen = false; infoOpen = true; }
    else if (inBtn(BTN_M3, x, y)) appRestart();
    else if (inBtn(BTN_M4, x, y)) menuOpen = false;
    return;
  }
  if (s == S_INFO) { if (inBtn(BTN_FULL, x, y)) infoOpen = false; return; }
  if ((s == S_JOB_ZERO || s == S_JOB_PLACE || s == S_JOB_HOLD) && inBtn(BTN_FULL, x, y)) { appCancelJob("operator"); return; }
  if (s == S_DISABLED && inBtn(BTN_FULL, x, y) && activationState != 1 && linkReady()) {
    linkSendActivationRequest();
    activationState = 1;
    return;
  }
  if (s == S_RESULT) { result.show = false; return; }   // tocar = cerrar el resultado
}

static void touchLoop() {
  static unsigned long last = 0, pressStart = 0;
  static bool down = false, longDone = false;
  if (millis() - last < 30) return;
  last = millis();
  int x, y;
  bool has = readTouch(x, y);
  if (has && !down) {
    pressStart = millis(); longDone = false;
    onTap(x, y);
  }
  // mantener 3 s el encabezado = menu de ajustes (fuera de un pesaje)
  if (has && down && !longDone && y < HEAD && millis() - pressStart > 3000 && !job.active && screen != S_OTA) {
    longDone = true;
    menuOpen = true; infoOpen = false;
  }
  down = has;
}

void uiBegin() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);   // la microSD comparte el SPI
  gfx->begin();
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  pinMode(TP_INT, INPUT_PULLUP);
  Wire.begin(TP_SDA, TP_SCL, 400000);
  screen = S_BOOT;
  drawStatic(S_BOOT);
  drawnScreen = S_BOOT;
}

void uiBootStep(const char* text) {
  gfx->fillRect(0, 180, W, 12, BG);
  txtC(text, 182, 1, DIM, BG);
}

void uiForceRedraw() { forceAll = true; }

void uiLoop() {
  touchLoop();
  static unsigned long lastDyn = 0;
  int s = screen;
  uint32_t h = staticHash(s);
  if (forceAll || s != drawnScreen || h != drawnHash) {
    forceAll = false;
    drawnScreen = s; drawnHash = h;
    drawStatic(s);
    drawDynamic(s);
    lastDyn = millis();
    return;
  }
  if (millis() - lastDyn >= 150) { lastDyn = millis(); drawDynamic(s); }
}
