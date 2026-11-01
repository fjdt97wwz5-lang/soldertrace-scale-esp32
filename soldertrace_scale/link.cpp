#include "link.h"
#include "app.h"
#include "config.h"
#include "store.h"
#include "scale.h"
#include "clock.h"
#include "ota.h"
#include "lang.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Certificados raiz que ya trae el core del ESP32 (los mismos que un navegador)
extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

static WebSocketsClient ws;
static bool started = false, open_ = false, authed = false;
static unsigned long lastStatus = 0, lastFlush = 0;
static char host[96], path[96];
static uint16_t port = 443;
static bool secure = true;

// wss://host[:puerto]/ruta  o  ws://...
static bool parseUrl(const char* url) {
  const char* p = url;
  if (!strncmp(p, "wss://", 6)) { secure = true; port = 443; p += 6; }
  else if (!strncmp(p, "ws://", 5)) { secure = false; port = 80; p += 5; }
  else return false;
  const char* slash = strchr(p, '/');
  size_t hl = slash ? (size_t)(slash - p) : strlen(p);
  if (hl == 0 || hl >= sizeof(host)) return false;
  memcpy(host, p, hl); host[hl] = 0;
  char* colon = strchr(host, ':');
  if (colon) { port = (uint16_t)atoi(colon + 1); *colon = 0; }
  strlcpy(path, slash ? slash : "/", sizeof(path));
  return true;
}

static void sendJson(JsonDocument& d) {
  if (!open_) return;
  String s;
  serializeJson(d, s);
  ws.sendTXT(s);
}

static void sendHello() {
  JsonDocument d;
  d["type"] = "hello";
  d["mac"] = macStr;
  d["chip"] = chipId;
  d["fw"] = FW_VERSION;
  if (cfg.token[0]) d["token"] = cfg.token;
  d["acked_upto"] = ackedUpTo;
  d["next_seq"] = nextSeq;
  d["queue"] = queueCount();
  d["lang"] = cfg.lang;
  sendJson(d);
}

static void sendStatus() {
  JsonDocument d;
  d["type"] = "status";
  d["rssi"] = WiFi.RSSI();
  d["uptime_s"] = millis() / 1000;
  d["scale_ok"] = scaleOnline();
  if (scaleOnline()) d["grams"] = scale.grams;
  d["queue"] = queueCount();
  d["heap"] = ESP.getFreeHeap();
  d["ip"] = WiFi.localIP().toString();
  d["clock"] = clockSource();
  d["ds3231"] = clockHasDs3231();
  sendJson(d);
}

void linkFlushQueue() {
  if (!authed) return;
  int n = queueCount();
  for (int i = 0; i < n && i < 20; i++) {   // de 20 en 20 para no llenar el socket
    QueuedReading r;
    if (!queueGet(i, r)) break;
    JsonDocument d;
    d["type"] = "reading";
    d["seq"] = r.seq;
    d["request_id"] = r.requestId;
    d["grams"] = serialized(String(r.grams, 2));
    d["stable"] = (bool)r.stable;
    d["ts"] = r.ts;
    sendJson(d);
  }
  lastFlush = millis();
}

static void applySettings(JsonObjectConst m) {
  bool changed = false;
  if (m["number"].is<int>()) { cfg.number = m["number"]; changed = true; }
  if (m["name"].is<const char*>()) { strlcpy(cfg.name, m["name"] | "", sizeof(cfg.name)); changed = true; }
  if (m["enabled"].is<bool>()) {
    bool en = m["enabled"];
    if (en) activationState = 0;
    cfg.enabled = en; changed = true;
  }
  if (m["lang"].is<const char*>()) { strlcpy(cfg.lang, m["lang"] | "en", sizeof(cfg.lang)); langIdx = langIndexOf(cfg.lang); changed = true; }
  if (m["tz"].is<const char*>()) {
    const char* tz = m["tz"];
    if (tz && *tz && strcmp(tz, cfg.tz)) { strlcpy(cfg.tz, tz, sizeof(cfg.tz)); clockSetTz(cfg.tz); }
    changed = true;
  }
  JsonObjectConst s = m["settings"];
  if (!s.isNull()) {
    if (s["stable_ms"].is<uint32_t>()) cfg.stableMs = constrain((uint32_t)s["stable_ms"], 500u, 10000u);
    if (s["stable_band_g"].is<float>()) cfg.stableBand = constrain((float)s["stable_band_g"], 0.01f, 5.0f);
    if (s["zero_tol_g"].is<float>()) cfg.zeroTol = constrain((float)s["zero_tol_g"], 0.01f, 5.0f);
    if (s["min_jar_g"].is<float>()) cfg.minJar = constrain((float)s["min_jar_g"], 0.5f, 1000.0f);
    cfgStableMs = cfg.stableMs; cfgStableBand = cfg.stableBand; cfgZeroTol = cfg.zeroTol;
    changed = true;
  }
  if (m["server_time"].is<uint32_t>()) clockFromServer(m["server_time"]);
  if (m["acked_upto"].is<uint32_t>()) queueAck(m["acked_upto"]);
  if (changed) { storeSave(); appOnConfigApplied(); }
}

static void handleMessage(const char* txt, size_t len) {
  JsonDocument d;
  if (deserializeJson(d, txt, len)) return;
  const char* type = d["type"] | "";
  JsonObjectConst m = d.as<JsonObjectConst>();
  if (!strcmp(type, "pair_code")) {
    authed = false;
    strlcpy(pairCode, d["code"] | "", sizeof(pairCode));
    if (m["server_time"].is<uint32_t>()) clockFromServer(m["server_time"]);
  } else if (!strcmp(type, "paired")) {
    strlcpy(cfg.token, d["token"] | "", sizeof(cfg.token));
    pairCode[0] = 0;
    applySettings(m);
    storeSave();
    Serial.println("Bascula dada de alta en SolderTrace");
    sendHello();   // se autentica ya con el token nuevo
  } else if (!strcmp(type, "welcome") || !strcmp(type, "config")) {
    authed = true;
    pairCode[0] = 0;
    applySettings(m);
    if (!strcmp(type, "welcome")) { sendStatus(); linkFlushQueue(); }
  } else if (!strcmp(type, "weigh")) {
    appOnWeigh(d["request_id"] | "", d["qr"] | "", d["product"] | "", d["lot"] | "", d["mode"] | "check",
               m["expected_g"].is<float>() ? (float)d["expected_g"] : NAN,
               m["tol_g"].is<float>() ? (float)d["tol_g"] : NAN, d["expires_s"] | 180);
  } else if (!strcmp(type, "cancel")) {
    appOnCancel(d["request_id"] | "");
  } else if (!strcmp(type, "result")) {
    appOnResult(d["request_id"] | "", d["status"] | "ok", d["message"] | "");
  } else if (!strcmp(type, "ack")) {
    queueAck(d["upto"] | 0);
    if (queueCount() > 0) linkFlushQueue();
  } else if (!strcmp(type, "activation")) {
    const char* st = d["status"] | "";
    activationState = !strcmp(st, "rejected") ? 2 : 0;
  } else if (!strcmp(type, "ota")) {
    otaRequest(d["url"] | "", d["version"] | "", d["md5"] | "", d["size"] | 0);
  } else if (!strcmp(type, "unpair") || (!strcmp(type, "error") && !strcmp(d["code"] | "", "bad_token"))) {
    Serial.println("El servidor no reconoce esta bascula: vuelve a pedir el alta");
    authed = false;
    storeClearPairing();
    sendHello();
  }
}

static void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      open_ = true; authed = false;
      Serial.println("SolderTrace: conectado");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      if (open_) Serial.println("SolderTrace: desconectado (reintentando)");
      open_ = false; authed = false;
      break;
    case WStype_TEXT:
      handleMessage((const char*)payload, length);
      break;
    default: break;
  }
}

void linkBegin() {
  if (!parseUrl(cfg.serverUrl)) {
    Serial.print("URL del servidor no valida: "); Serial.println(cfg.serverUrl);
    return;
  }
  if (secure) ws.beginSslWithBundle(host, port, path, caBundleStart, caBundleEnd - caBundleStart, "");
  else ws.begin(host, port, path, "");
  ws.onEvent(onEvent);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(15000, 5000, 2);   // ping cada 15 s; 2 sin respuesta = reconecta
  started = true;
}

void linkRestart() {
  if (started) ws.disconnect();
  started = false; open_ = false; authed = false;
  linkBegin();
}

void linkLoop() {
  if (!started) return;
  if (WiFi.status() != WL_CONNECTED) { open_ = false; authed = false; return; }
  ws.loop();
  if (!open_) return;
  if (millis() - lastStatus > STATUS_EVERY_MS) { lastStatus = millis(); sendStatus(); }
  if (authed && queueCount() > 0 && millis() - lastFlush > 10000) linkFlushQueue();
}

bool linkConnected() { return open_; }
bool linkReady() { return open_ && authed; }

void linkSendLive(const char* requestId, float grams, bool stable, bool zero) {
  JsonDocument d;
  d["type"] = "live";
  d["request_id"] = requestId;
  if (!isnan(grams)) d["grams"] = serialized(String(grams, 2));
  d["stable"] = stable;
  d["zero"] = zero;
  sendJson(d);
}

void linkSendCancelled(const char* requestId, const char* reason) {
  JsonDocument d;
  d["type"] = "cancelled";
  d["request_id"] = requestId;
  d["reason"] = reason;
  sendJson(d);
}

void linkSendActivationRequest() {
  JsonDocument d;
  d["type"] = "activation_request";
  sendJson(d);
}

void linkSendOtaStatus(const char* phase, int progress, const char* err) {
  JsonDocument d;
  d["type"] = "ota_status";
  d["phase"] = phase;
  d["progress"] = progress;
  if (err) d["error"] = err;
  sendJson(d);
  ws.loop();
}
