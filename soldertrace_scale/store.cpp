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
static const char* QTMP = "/q.tmp";
static bool fsOk = false;

// Numeros pendientes en RAM (en orden de seq): contar y confirmar sin abrir el archivo
static uint32_t pend[QUEUE_MAX];
static int pendHead = 0, pendN = 0;
static int fileRecs = 0;             // registros en el archivo, incluidos los ya confirmados
static const int COMPACT_AT = 64;    // confirmados acumulados antes de reescribir el archivo

static uint32_t pendAt(int i) { return pend[(pendHead + i) % QUEUE_MAX]; }
static void pendPop() { pendHead = (pendHead + 1) % QUEUE_MAX; pendN--; }
static void pendPush(uint32_t s) {
  if (pendN >= QUEUE_MAX) pendPop();
  pend[(pendHead + pendN) % QUEUE_MAX] = s;
  pendN++;
}

static void loadStr(const char* key, char* dst, size_t n, const char* def) {
  String v = prefs.getString(key, def);
  strlcpy(dst, v.c_str(), n);
}

static void saveAck() {
  char k[8]; snprintf(k, sizeof(k), "ack%d", ackSlot);
  prefs.putUInt(k, ackedUpTo);
  ackSlot = (ackSlot + 1) % 8;
}

// Lee el archivo una vez: numeros pendientes, total de registros y siguiente seq
static void loadQueue() {
  pendHead = pendN = 0;
  fileRecs = 0;
  if (!fsOk) return;
  // un corte de luz a mitad de reescribir: si el archivo bueno no esta, vale el temporal
  if (LittleFS.exists(QTMP)) {
    if (LittleFS.exists(QFILE)) LittleFS.remove(QTMP);
    else LittleFS.rename(QTMP, QFILE);
  }
  if (!LittleFS.exists(QFILE)) return;
  File f = LittleFS.open(QFILE, "r");
  if (!f) return;
  QueuedReading r;
  uint32_t dropped = 0;
  while (f.read((uint8_t*)&r, sizeof(r)) == sizeof(r)) {
    fileRecs++;
    if (r.seq >= nextSeq) nextSeq = r.seq + 1;
    if (r.seq > ackedUpTo) {
      if (pendN >= QUEUE_MAX) dropped = pendAt(0);   // mas de los que caben: se pierde el mas viejo
      pendPush(r.seq);
    }
  }
  f.close();
  if (dropped > ackedUpTo) { ackedUpTo = dropped; saveAck(); }
}

// Reescribe la cola sin los registros <= keepAbove. Con newBase > 0 renumera los que quedan
// desde newBase + 1 (el servidor ya tiene numeros mayores que los nuestros).
static bool rewriteQueue(uint32_t keepAbove, uint32_t newBase) {
  if (!fsOk || !LittleFS.exists(QFILE)) return true;
  File in = LittleFS.open(QFILE, "r");
  if (!in) return false;
  File out = LittleFS.open(QTMP, "w");
  if (!out) { in.close(); return false; }
  QueuedReading r;
  int n = 0;
  bool ok = true;
  uint32_t s = newBase;
  while (in.read((uint8_t*)&r, sizeof(r)) == sizeof(r)) {
    if (r.seq <= keepAbove) continue;
    if (newBase) r.seq = ++s;
    if (out.write((const uint8_t*)&r, sizeof(r)) != sizeof(r)) { ok = false; break; }
    n++;
  }
  in.close();
  out.close();
  if (!ok) { LittleFS.remove(QTMP); return false; }
  if (n == 0) { LittleFS.remove(QTMP); LittleFS.remove(QFILE); return true; }
  if (!LittleFS.rename(QTMP, QFILE)) {          // por si rename no reemplaza el destino
    LittleFS.remove(QFILE);
    if (!LittleFS.rename(QTMP, QFILE)) return false;
  }
  return true;
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
  cfg.stableBand = constrain(prefs.getFloat("sband", DEFAULT_STABLE_BAND), 0.01f, 5.0f);
  cfg.zeroTol = constrain(prefs.getFloat("zero", DEFAULT_ZERO_TOL), 0.01f, 5.0f);
  cfg.minJar = constrain(prefs.getFloat("minj", DEFAULT_MIN_JAR_G), 0.5f, 1000.0f);
  cfg.stableMs = constrain(prefs.getUInt("sms", DEFAULT_STABLE_MS), STABLE_MS_MIN, STABLE_MS_MAX);
  cfg.zeroHoldMs = constrain(prefs.getUInt("zhold", DEFAULT_ZERO_HOLD_MS), (uint32_t)ZERO_HOLD_MS_MIN, (uint32_t)ZERO_HOLD_MS_MAX);
  cfg.wifiWaitS = constrain(prefs.getUInt("wwait", DEFAULT_WIFI_WAIT_S), (uint32_t)WIFI_WAIT_S_MIN, (uint32_t)WIFI_WAIT_S_MAX);
  cfg.apTimeoutMin = constrain(prefs.getUInt("apto", DEFAULT_AP_TIMEOUT_MIN), (uint32_t)AP_TIMEOUT_MIN_MIN, (uint32_t)AP_TIMEOUT_MIN_MAX);
  loadStr("pinh", cfg.pinHash, sizeof(cfg.pinHash), "");
  cfg.demo = prefs.getBool("demo", false);
  nextSeq = prefs.getUInt("seq", 1);
  // "confirmado hasta": el mayor de las 8 posiciones
  for (int i = 0; i < 8; i++) {
    char k[8]; snprintf(k, sizeof(k), "ack%d", i);
    uint32_t v = prefs.getUInt(k, 0);
    if (v > ackedUpTo) { ackedUpTo = v; ackSlot = (i + 1) % 8; }
  }
  fsOk = LittleFS.begin(true);
  loadQueue();   // la cola manda: si quedo un seq mayor en el archivo, se sigue desde ahi
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
  prefs.putUInt("zhold", cfg.zeroHoldMs);
  prefs.putUInt("wwait", cfg.wifiWaitS);
  prefs.putUInt("apto", cfg.apTimeoutMin);
  prefs.putString("pinh", cfg.pinHash);
  prefs.putBool("demo", cfg.demo);
}

void storeClearPairing() {
  cfg.token[0] = 0;
  cfg.number = 0;
  cfg.name[0] = 0;
  cfg.enabled = true;
  cfg.pinHash[0] = 0;      // el PIN y la demo eran de ese cliente
  cfg.demo = false;
  storeSave();
}

uint32_t queueAdd(const char* requestId, float grams, bool stable, bool demo) {
  QueuedReading r = {};
  r.seq = nextSeq++;
  prefs.putUInt("seq", nextSeq);   // un pesaje es poco frecuente: no gasta la memoria
  time_t now = time(nullptr);
  r.ts = now > 1700000000 ? (uint32_t)now : 0;
  r.grams = grams;
  r.stable = stable;
  r.demo = demo;
  strlcpy(r.requestId, requestId ? requestId : "", sizeof(r.requestId));
  if (fsOk) {
    if (pendN >= QUEUE_MAX) {
      Serial.println("Cola llena: se descarta el pesaje mas viejo");
      queueAck(pendAt(0));
    }
    File f = LittleFS.open(QFILE, "a");
    if (f) {
      if (f.write((const uint8_t*)&r, sizeof(r)) == sizeof(r)) { pendPush(r.seq); fileRecs++; }
      f.close();
    }
  }
  return r.seq;
}

int queueCount() { return pendN; }

int queueCountUpTo(uint32_t seq) {
  int n = 0;
  while (n < pendN && pendAt(n) <= seq) n++;
  return n;
}

int queueRead(uint32_t afterSeq, QueuedReading* out, int max) {
  if (!fsOk || pendN == 0 || max <= 0) return 0;
  if (afterSeq < ackedUpTo) afterSeq = ackedUpTo;
  File f = LittleFS.open(QFILE, "r");
  if (!f) return 0;
  // los confirmados estan siempre al principio del archivo (se agrega en orden de seq)
  int skip = fileRecs - pendN;
  if (skip > 0 && !f.seek((uint32_t)skip * sizeof(QueuedReading))) f.seek(0);
  int n = 0;
  QueuedReading r;
  while (n < max && f.read((uint8_t*)&r, sizeof(r)) == sizeof(r)) if (r.seq > afterSeq) out[n++] = r;
  f.close();
  return n;
}

void queueAck(uint32_t upTo) {
  if (upTo <= ackedUpTo) return;
  if (upTo >= nextSeq) {
    // El servidor confirma numeros que esta placa no ha usado (memoria borrada al regrabar):
    // los pendientes se renumeran despues del suyo para que no se den por grabados sin estarlo.
    Serial.printf("Cola: el servidor va por %lu y la placa por %lu: se renumera\n", (unsigned long)upTo, (unsigned long)(nextSeq - 1));
    if (rewriteQueue(ackedUpTo, upTo)) {
      ackedUpTo = upTo;
      nextSeq = upTo + 1;
      loadQueue();                     // vuelve a leer pendientes y siguiente seq del archivo nuevo
      if (nextSeq <= upTo) nextSeq = upTo + 1;
      prefs.putUInt("seq", nextSeq);
      saveAck();
    }
    return;
  }
  ackedUpTo = upTo;
  saveAck();
  while (pendN > 0 && pendAt(0) <= upTo) pendPop();
  if (!fsOk) return;
  if (pendN == 0) {
    if (fileRecs > 0) LittleFS.remove(QFILE);   // ya no queda nada pendiente: se vacia el archivo
    fileRecs = 0;
  } else if (fileRecs - pendN >= COMPACT_AT) {
    if (rewriteQueue(ackedUpTo, 0)) fileRecs = pendN;
  }
}
