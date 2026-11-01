// setup(), loop() y flujo del pesaje
#include <Arduino.h>
#include "config.h"
#include "lang.h"
#include "app.h"
#include "store.h"
#include "scale.h"
#include "clock.h"
#include "wifi_setup.h"
#include "link.h"
#include "ota.h"
#include "ui.h"
#include <WiFi.h>
#include <esp_mac.h>

// ---------- estado compartido (app.h) ----------
Job job;
Result result;
Screen screen = S_BOOT;
bool menuOpen = false, infoOpen = false, setupMode = false;
int activationState = 0;
char pairCode[8] = "";
char apPassword[12] = "";
char apSsid[32] = "";
int otaProgress = -1;
bool otaFailed = false;
char chipId[16] = "", macStr[18] = "";
int langIdx = 0;
float cfgStableBand = DEFAULT_STABLE_BAND, cfgZeroTol = DEFAULT_ZERO_TOL;
unsigned long cfgStableMs = DEFAULT_STABLE_MS;

volatile const char* fwMark = FW_MARK;   // la marca tiene que ir dentro del .bin (la OTA la busca)

// ---------- pesaje ----------
enum JobPhase { JP_ZERO, JP_PLACE, JP_HOLD };
static JobPhase phase = JP_ZERO;
static unsigned long lastLive = 0;

static void showResult(ResultKind k, float g, const char* reqId, const char* msg) {
  result.show = true;
  result.kind = k;
  result.grams = g;
  strlcpy(result.requestId, reqId ? reqId : "", sizeof(result.requestId));
  strlcpy(result.msg, msg ? msg : "", sizeof(result.msg));
  result.at = millis();
}

void appOnWeigh(const char* requestId, const char* qr, const char* product, const char* lot,
                const char* mode, float expected, float tol, uint32_t expiresS) {
  if (!cfg.enabled) { linkSendCancelled(requestId, "disabled"); return; }
  if (otaProgress >= 0) { linkSendCancelled(requestId, "updating"); return; }
  if (job.active && strcmp(job.requestId, requestId)) { linkSendCancelled(requestId, "busy"); return; }
  job = Job();
  job.active = true;
  strlcpy(job.requestId, requestId, sizeof(job.requestId));
  strlcpy(job.qr, qr, sizeof(job.qr));
  strlcpy(job.product, product, sizeof(job.product));
  strlcpy(job.lot, lot, sizeof(job.lot));
  strlcpy(job.mode, mode, sizeof(job.mode));
  job.expected = expected;
  job.tol = tol;
  job.deadline = millis() + constrain(expiresS, 20u, 900u) * 1000UL;
  result.show = false;
  menuOpen = infoOpen = false;
  phase = scaleAtZero() ? JP_PLACE : JP_ZERO;
  scaleResetStable();
  Serial.print("Pesaje pedido: "); Serial.print(qr); Serial.print(" "); Serial.println(mode);
}

void appOnCancel(const char* requestId) {
  if (job.active && !strcmp(job.requestId, requestId)) {
    job.active = false;
    showResult(RK_CANCELLED, NAN, requestId, "");
  }
}

void appOnResult(const char* requestId, const char* status, const char* message) {
  // el resultado puede llegar con el envase todavia encima (lo normal) o despues. Solo se muestra el
  // del ultimo pesaje de esta pantalla: nunca encima de un pesaje nuevo ni de envases ya retirados
  // (p. ej. los resultados de una cola sin conexion que se manda al volver la red).
  if (job.active || !result.show || strcmp(result.requestId, requestId) ||
      (result.kind != RK_SENDING && result.kind != RK_OFFLINE)) {
    Serial.print("Resultado ignorado (no es el pesaje en pantalla): "); Serial.println(requestId);
    return;
  }
  ResultKind k = RK_ERROR;
  if (!strcmp(status, "ok")) k = RK_OK;
  else if (!strcmp(status, "on_hold") || !strcmp(status, "out_of_range")) k = RK_HOLD;
  else if (!strcmp(status, "rejected")) k = RK_OUT;
  showResult(k, result.grams, requestId, message);
}

void appCancelJob(const char* reason) {
  if (!job.active) return;
  linkSendCancelled(job.requestId, reason);
  job.active = false;
  showResult(RK_CANCELLED, NAN, job.requestId, "");
}

void appOnConfigApplied() { uiForceRedraw(); }
void appEnterSetup() { wifiStartSetup(); }
void appRestart() {
  otaMarkValid();   // reinicio pedido por el operario: la version funciona (si no, volveria a la anterior)
  delay(200);
  ESP.restart();
}

static void jobLoop() {
  if (!job.active) return;
  if ((long)(millis() - job.deadline) > 0) {
    linkSendCancelled(job.requestId, "timeout");
    job.active = false;
    showResult(RK_TIMEOUT, NAN, job.requestId, "");
    return;
  }
  bool online = scaleOnline();
  float g = online ? scale.grams : NAN;
  switch (phase) {
    case JP_ZERO:
      if (scaleAtZero()) { phase = JP_PLACE; scaleResetStable(); }
      break;
    case JP_PLACE:
      if (online && g >= cfg.minJar) { phase = JP_HOLD; scaleResetStable(); }
      break;
    case JP_HOLD: {
      if (!online || g < cfg.minJar) { phase = JP_PLACE; break; }
      float stableG;
      if (scaleStable(stableG)) {
        uint32_t seq = queueAdd(job.requestId, stableG, true);
        Serial.print("Peso estable "); Serial.print(stableG, 2); Serial.print(" g  seq "); Serial.println(seq);
        bool ready = linkReady();
        showResult(ready ? RK_SENDING : RK_OFFLINE, stableG, job.requestId, "");
        job.active = false;
        if (ready) linkFlushQueue();
        return;
      }
      break;
    }
  }
  if (millis() - lastLive >= LIVE_EVERY_MS && linkReady()) {
    lastLive = millis();
    linkSendLive(job.requestId, g, phase == JP_HOLD, scaleAtZero());
  }
}

static void resultLoop() {
  if (!result.show) return;
  unsigned long age = millis() - result.at;
  bool jarOff = !scaleOnline() || scale.grams < cfg.minJar;
  // el resultado se queda hasta quitar el envase (minimo 2 s) o 30 s; "enviando" espera la respuesta
  if (result.kind == RK_SENDING) { if (age > 20000 && jarOff) result.show = false; return; }
  if ((age > 2000 && jarOff) || age > RESULT_HOLD_MS) result.show = false;
}

static Screen pickScreen() {
  if (otaProgress >= 0) return S_OTA;
  if (setupMode) return S_SETUP_AP;
  if (menuOpen) return S_MENU;
  if (infoOpen) return S_INFO;
  if (result.show) return S_RESULT;
  if (job.active) return phase == JP_ZERO ? S_JOB_ZERO : (phase == JP_PLACE ? S_JOB_PLACE : S_JOB_HOLD);
  if (otaFailed && millis() - otaFailedAt < 8000) return S_OTA;   // "fallo la actualizacion" unos segundos
  if (!clockValid() && millis() < WAIT_TIME_MS) return S_WAIT_TIME;
  if (!linkReady()) return (linkConnected() && pairCode[0]) ? S_PAIR : S_CONNECTING;
  if (!cfg.enabled) return S_DISABLED;
  if (!scaleOnline()) return S_NO_SCALE;
  return S_READY;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(chipId, sizeof(chipId), "%012llX", ESP.getEfuseMac());
  snprintf(apSsid, sizeof(apSsid), "SolderTrace-%02X%02X", mac[4], mac[5]);

  uiBegin();                 // pantalla y I2C (tactil y DS3231)
  uiBootStep("Memory");
  storeBegin();
  langIdx = langIndexOf(cfg.lang);
  cfgStableMs = cfg.stableMs; cfgStableBand = cfg.stableBand; cfgZeroTol = cfg.zeroTol;
  // la version se da por buena al conectar con el portal (link.cpp) o tras 10 min con WiFi (loop)
  uiBootStep("Scale RS-232");
  scaleBegin();
  uiBootStep("Clock");
  clockBegin(cfg.tz);
  uiBootStep("Wi-Fi");
  wifiBegin();
  linkBegin();
  Serial.printf("SolderTrace Scale %s  MAC %s  B%d  %s\n", FW_VERSION, macStr, cfg.number, (const char*)fwMark);
  delay(600);
}

void loop() {
  scaleLoop();
  wifiLoop();
  if (millis() > MARK_VALID_AFTER_MS && WiFi.status() == WL_CONNECTED) otaMarkValid();
  if (!setupMode) {
    linkLoop();
    clockLoop();
    jobLoop();
    resultLoop();
    otaLoop(!job.active && !result.show);
  }
  screen = pickScreen();
  uiLoop();
}
