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
static bool started = false, open_ = false, authed = false, wifiWasUp = false, firstTry = true;
static unsigned long lastStatus = 0, lastFlush = 0, lastAckMs = 0, downSince = 0, retryMs = 0;
static char host[96], path[96];
static uint16_t port = 443;
static uint32_t sentUpTo = 0;          // ultimo pesaje mandado (en vuelo hasta que llegue su ack)
static const int FLUSH_WINDOW = 20;    // pesajes en vuelo sin confirmar, como maximo
static const unsigned long RESEND_MS = 10000;

// Solo wss://host[:puerto]/ruta (la conexion siempre va cifrada)
static bool parseUrlTo(const char* url, char* h, size_t hn, char* pa, size_t pn, uint16_t& po) {
  const char* p = url;
  if (!p || strncmp(p, "wss://", 6)) return false;
  po = 443;
  p += 6;
  const char* slash = strchr(p, '/');
  size_t hl = slash ? (size_t)(slash - p) : strlen(p);
  if (hl == 0 || hl >= hn) return false;
  if (slash && strlen(slash) >= pn) return false;
  memcpy(h, p, hl); h[hl] = 0;
  char* colon = strchr(h, ':');
  if (colon) {
    long v = atol(colon + 1);
    if (v <= 0 || v > 65535) return false;
    po = (uint16_t)v;
    *colon = 0;
    if (!h[0]) return false;
  }
  strlcpy(pa, slash ? slash : "/", pn);
  return true;
}
static bool parseUrl(const char* url) { return parseUrlTo(url, host, sizeof(host), path, sizeof(path), port); }

bool linkUrlValid(const char* url) {
  char h[sizeof(host)], pa[sizeof(path)];
  uint16_t po;
  return parseUrlTo(url, h, sizeof(h), pa, sizeof(pa), po);
}

// Numero entero sin signo aunque el servidor lo mande como 2000.0
static bool asU32(JsonVariantConst v, uint32_t& out) {
  if (v.is<uint32_t>()) { out = v.as<uint32_t>(); return true; }
  if (v.is<double>()) {
    double d = v.as<double>();
    if (d >= 0 && d < 4294967295.0) { out = (uint32_t)d; return true; }
  }
  return false;
}

static bool sendJson(JsonDocument& d) {
  if (!open_) return false;
  String s;
  serializeJson(d, s);
  return ws.sendTXT(s);
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

// Manda los pendientes que aun no estan en vuelo (una sola lectura del archivo). Lo que no se
// confirma en RESEND_MS se vuelve a mandar desde el ultimo confirmado.
void linkFlushQueue() {
  lastFlush = millis();
  if (!authed || !open_ || queueCount() == 0) return;
  if (sentUpTo < ackedUpTo) sentUpTo = ackedUpTo;
  int inflight = queueCountUpTo(sentUpTo);
  int room = FLUSH_WINDOW - inflight;
  if (room <= 0 || queueCount() - inflight <= 0) return;
  static QueuedReading batch[FLUSH_WINDOW];
  int n = queueRead(sentUpTo, batch, room);
  if (n > 0 && inflight == 0) lastAckMs = millis();   // el plazo de reenvio cuenta desde ahora
  for (int i = 0; i < n; i++) {
    const QueuedReading& r = batch[i];
    JsonDocument d;
    d["type"] = "reading";
    d["seq"] = r.seq;
    d["request_id"] = r.requestId;
    if (isfinite(r.grams)) d["grams"] = serialized(String(r.grams, 2));
    d["stable"] = (bool)r.stable;
    d["ts"] = r.ts;
    if (!sendJson(d)) break;
    sentUpTo = r.seq;
  }
}

static void ackTo(uint32_t upTo) {
  uint32_t before = ackedUpTo;
  queueAck(upTo);
  if (ackedUpTo != before) lastAckMs = millis();
}

static void applySettings(JsonObjectConst m) {
  bool changed = false;
  uint32_t v;
  if (asU32(m["number"], v)) { cfg.number = (int)min(v, (uint32_t)9999); changed = true; }
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
    if (asU32(s["stable_ms"], v)) cfg.stableMs = constrain(v, STABLE_MS_MIN, STABLE_MS_MAX);
    if (s["stable_band_g"].is<float>()) cfg.stableBand = constrain((float)s["stable_band_g"], 0.01f, 5.0f);
    if (s["zero_tol_g"].is<float>()) cfg.zeroTol = constrain((float)s["zero_tol_g"], 0.01f, 5.0f);
    if (s["min_jar_g"].is<float>()) cfg.minJar = constrain((float)s["min_jar_g"], 0.5f, 1000.0f);
    cfgStableMs = cfg.stableMs; cfgStableBand = cfg.stableBand; cfgZeroTol = cfg.zeroTol;
    changed = true;
  }
  if (asU32(m["server_time"], v)) clockFromServer(v);
  if (asU32(m["acked_upto"], v)) ackTo(v);
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
    uint32_t st;
    if (asU32(m["server_time"], st)) clockFromServer(st);
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
    if (!strcmp(type, "welcome")) {
      otaMarkValid();              // version nueva: conecta con el portal, se da por buena
      sentUpTo = ackedUpTo;        // lo que estaba en vuelo en la conexion anterior se vuelve a mandar
      sendStatus();
      linkFlushQueue();
    }
  } else if (!strcmp(type, "weigh")) {
    uint32_t ex = 180;
    asU32(m["expires_s"], ex);
    appOnWeigh(d["request_id"] | "", d["qr"] | "", d["product"] | "", d["lot"] | "", d["mode"] | "check",
               m["expected_g"].is<float>() ? (float)d["expected_g"] : NAN,
               m["tol_g"].is<float>() ? (float)d["tol_g"] : NAN, ex);
  } else if (!strcmp(type, "cancel")) {
    appOnCancel(d["request_id"] | "");
  } else if (!strcmp(type, "result")) {
    appOnResult(d["request_id"] | "", d["status"] | "error", d["message"] | "");   // sin estado = error, nunca OK
  } else if (!strcmp(type, "ack")) {
    uint32_t up;
    if (asU32(m["upto"], up)) ackTo(up);
    linkFlushQueue();
  } else if (!strcmp(type, "activation")) {
    const char* st = d["status"] | "";
    activationState = !strcmp(st, "rejected") ? 2 : 0;
  } else if (!strcmp(type, "ota")) {
    uint32_t size = 0;
    asU32(m["size"], size);
    otaRequest(d["url"] | "", d["version"] | "", d["md5"] | "", size);
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
      sentUpTo = 0;
      Serial.println("SolderTrace: conectado");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      if (open_) { Serial.println("SolderTrace: desconectado (reintentando)"); downSince = millis(); }
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
    // solo se acepta wss://: con otra direccion se vuelve al portal de SolderTrace y se pide alta nueva
    Serial.print("URL del servidor no valida (tiene que ser wss://): "); Serial.println(cfg.serverUrl);
    strlcpy(cfg.serverUrl, DEFAULT_SERVER_URL, sizeof(cfg.serverUrl));
    storeClearPairing();
    if (!parseUrl(cfg.serverUrl)) return;
  }
  ws.beginSslWithBundle(host, port, path, caBundleStart, caBundleEnd - caBundleStart, "");
  ws.onEvent(onEvent);
  retryMs = WS_RETRY_MIN_MS;
  ws.setReconnectInterval(retryMs);
  ws.enableHeartbeat(15000, 5000, 2);   // ping cada 15 s; 2 sin respuesta = reconecta
  started = true;
  // WEBSOCKETS_TCP_TIMEOUT se pone al compilar (compilar.sh / README): lo que tarda como maximo cada intento
  Serial.printf("SolderTrace: %s:%u%s  (espera TCP %d ms)\n", host, port, path, (int)WEBSOCKETS_TCP_TIMEOUT);
}

void linkRestart() {
  if (started) ws.disconnect();
  started = false; open_ = false; authed = false;
  linkBegin();
}

// Cada intento de conexion bloquea el loop (DNS + TCP + TLS): sin servidor se espera 15 s entre
// intentos el primer minuto, 30 s hasta los 3 min y luego 60 s.
static void setRetry() {
  unsigned long iv;
  if (firstTry) iv = 1000;                       // el primer intento, en cuanto hay WiFi
  else {
    unsigned long d = millis() - downSince;
    iv = d < 60000UL ? WS_RETRY_MIN_MS : d < 180000UL ? 2 * WS_RETRY_MIN_MS : WS_RETRY_MAX_MS;
    if (iv > WS_RETRY_MAX_MS) iv = WS_RETRY_MAX_MS;
  }
  if (iv != retryMs) { retryMs = iv; ws.setReconnectInterval(iv); }
}

void linkLoop() {
  if (!started) return;
  if (WiFi.status() != WL_CONNECTED) {
    // se cierra el socket viejo: si no, al volver la WiFi podria seguir vivo sin que nos enteremos
    if (wifiWasUp) { wifiWasUp = false; ws.disconnect(); }
    open_ = false; authed = false;
    return;
  }
  if (!wifiWasUp) { wifiWasUp = true; downSince = millis(); }
  if (!open_) setRetry();
  ws.loop();
  if (millis() > 1000) firstTry = false;
  if (!open_) return;
  if (millis() - lastStatus > STATUS_EVERY_MS) { lastStatus = millis(); sendStatus(); }
  if (authed && millis() - lastFlush > 1000 && queueCount() > 0) {
    if (sentUpTo > ackedUpTo && millis() - lastAckMs > RESEND_MS) {
      Serial.println("Cola: sin confirmacion, se reenvia");
      sentUpTo = ackedUpTo;
    }
    linkFlushQueue();
  }
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
