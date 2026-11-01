#include "store.h"
#include "config.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>

Settings cfg;
uint32_t nextSeq = 1;
uint32_t ackedUpTo = 0;

static Preferences prefs;
static uint8_t ackSlot = 0;
static const char* QFILE = "/q.bin";
static bool fsOk = false;

static void loadStr(const char* key, char* dst, size_t n, const char* def) {
  String v = prefs.getString(key, def);
  strlcpy(dst, v.c_str(), n);
}

void storeBegin() {
  prefs.begin("st", false);
  loadStr("ssid", cfg.wifiSsid, sizeof(cfg.wifiSsid), "");
  loadStr("pass", cfg.wifiPass, sizeof(cfg.wifiPass), "");
  loadStr("url", cfg.serverUrl, sizeof(cfg.serverUrl), DEFAULT_SERVER_URL);
  loadStr("tok", cfg.token, sizeof(cfg.token), "");
  cfg.number = prefs.getInt("num", 0);
  loadStr("name", cfg.name, sizeof(cfg.name), "");
  loadStr("lang", cfg.lang, sizeof(cfg.lang), DEFAULT_LANG);
  loadStr("tz", cfg.tz, sizeof(cfg.tz), DEFAULT_TZ);
  cfg.enabled = prefs.getBool("en", true);
  cfg.stableBand = prefs.getFloat("sband", DEFAULT_STABLE_BAND);
  cfg.zeroTol = prefs.getFloat("zero", DEFAULT_ZERO_TOL);
  cfg.minJar = prefs.getFloat("minj", DEFAULT_MIN_JAR_G);
  cfg.stableMs = prefs.getUInt("sms", DEFAULT_STABLE_MS);
  nextSeq = prefs.getUInt("seq", 1);
  // "confirmado hasta": el mayor de las 8 posiciones
  for (int i = 0; i < 8; i++) {
    char k[6]; snprintf(k, sizeof(k), "ack%d", i);
    uint32_t v = prefs.getUInt(k, 0);
    if (v > ackedUpTo) { ackedUpTo = v; ackSlot = (i + 1) % 8; }
  }
  fsOk = LittleFS.begin(true);
  // la cola manda: si quedo un seq mayor en el archivo, se sigue desde ahi
  if (fsOk) {
    File f = LittleFS.open(QFILE, "r");
    if (f) {
      QueuedReading r;
      while (f.read((uint8_t*)&r, sizeof(r)) == sizeof(r)) if (r.seq >= nextSeq) nextSeq = r.seq + 1;
      f.close();
    }
  }
}

void storeSave() {
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
  prefs.putString("url", cfg.serverUrl);
  prefs.putString("tok", cfg.token);
  prefs.putInt("num", cfg.number);
  prefs.putString("name", cfg.name);
  prefs.putString("lang", cfg.lang);
  prefs.putString("tz", cfg.tz);
  prefs.putBool("en", cfg.enabled);
  prefs.putFloat("sband", cfg.stableBand);
  prefs.putFloat("zero", cfg.zeroTol);
  prefs.putFloat("minj", cfg.minJar);
  prefs.putUInt("sms", cfg.stableMs);
}

void storeClearPairing() {
  cfg.token[0] = 0;
  cfg.number = 0;
  cfg.name[0] = 0;
  cfg.enabled = true;
  storeSave();
}

uint32_t queueAdd(const char* requestId, float grams, bool stable) {
  QueuedReading r = {};
  r.seq = nextSeq++;
  prefs.putUInt("seq", nextSeq);   // un pesaje es poco frecuente: no gasta la memoria
  time_t now = time(nullptr);
  r.ts = now > 1700000000 ? (uint32_t)now : 0;
  r.grams = grams;
  r.stable = stable;
  strlcpy(r.requestId, requestId ? requestId : "", sizeof(r.requestId));
  if (fsOk) {
    if (queueCount() >= QUEUE_MAX) {
      Serial.println("Cola llena: se descarta el pesaje mas viejo");
      queueAck(ackedUpTo + 1);
    }
    File f = LittleFS.open(QFILE, "a");
    if (f) { f.write((const uint8_t*)&r, sizeof(r)); f.close(); }
  }
  return r.seq;
}

int queueCount() {
  if (!fsOk) return 0;
  File f = LittleFS.open(QFILE, "r");
  if (!f) return 0;
  int n = 0;
  QueuedReading r;
  while (f.read((uint8_t*)&r, sizeof(r)) == sizeof(r)) if (r.seq > ackedUpTo) n++;
  f.close();
  return n;
}

bool queueGet(int i, QueuedReading& out) {
  if (!fsOk) return false;
  File f = LittleFS.open(QFILE, "r");
  if (!f) return false;
  int n = 0;
  bool found = false;
  while (f.read((uint8_t*)&out, sizeof(out)) == sizeof(out)) {
    if (out.seq <= ackedUpTo) continue;
    if (n++ == i) { found = true; break; }
  }
  f.close();
  return found;
}

void queueAck(uint32_t upTo) {
  if (upTo <= ackedUpTo) return;
  ackedUpTo = upTo;
  char k[6]; snprintf(k, sizeof(k), "ack%d", ackSlot);
  prefs.putUInt(k, ackedUpTo);
  ackSlot = (ackSlot + 1) % 8;
  // si ya no queda nada pendiente, se vacia el archivo
  if (fsOk && queueCount() == 0) LittleFS.remove(QFILE);
}
