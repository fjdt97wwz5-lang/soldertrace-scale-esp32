#include "clock.h"
#include "config.h"
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

static volatile bool ntpArrived = false;
static const char* source = "none";
static bool hasDs = false;

static void onNtp(struct timeval*) { ntpArrived = true; }

static uint8_t fromBcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t toBcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static int32_t civilDays(int y, int m, int d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  int32_t yoe = y - era * 400;
  int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static bool dsRead(uint8_t reg, uint8_t* buf, int n) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(DS3231_ADDR, n) != n) return false;
  for (int i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool dsGet(uint32_t& epoch) {
  uint8_t r[7], st;
  if (!dsRead(0x00, r, 7) || !dsRead(0x0F, &st, 1)) return false;
  hasDs = true;
  if (st & 0x80) return false;   // sin pila o nunca ajustado
  int s = fromBcd(r[0] & 0x7F), mi = fromBcd(r[1] & 0x7F), hh;
  if (r[2] & 0x40) hh = fromBcd(r[2] & 0x1F) % 12 + ((r[2] & 0x20) ? 12 : 0);
  else hh = fromBcd(r[2] & 0x3F);
  int dd = fromBcd(r[4] & 0x3F), mm = fromBcd(r[5] & 0x1F), yy = 2000 + fromBcd(r[6]);
  if (yy < 2024 || mm < 1 || mm > 12 || dd < 1 || dd > 31 || hh > 23 || mi > 59 || s > 59) return false;
  epoch = (uint32_t)((int64_t)civilDays(yy, mm, dd) * 86400LL + hh * 3600L + mi * 60L + s);
  return true;
}

static bool dsSet(uint32_t epoch) {
  time_t t = epoch; struct tm u; gmtime_r(&t, &u);
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.write(toBcd(u.tm_sec)); Wire.write(toBcd(u.tm_min)); Wire.write(toBcd(u.tm_hour));
  Wire.write(toBcd(u.tm_wday + 1)); Wire.write(toBcd(u.tm_mday)); Wire.write(toBcd(u.tm_mon + 1));
  Wire.write(toBcd(u.tm_year - 100));
  if (Wire.endTransmission() != 0) return false;
  uint8_t st;
  if (!dsRead(0x0F, &st, 1)) return false;
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((uint8_t)0x0F); Wire.write((uint8_t)(st & ~0x80));
  return Wire.endTransmission() == 0;
}

void clockSetTz(const char* tz) {
  setenv("TZ", (tz && *tz) ? tz : DEFAULT_TZ, 1);
  tzset();
}

void clockBegin(const char* tz) {
  clockSetTz(tz);
  uint32_t e;
  if (dsGet(e)) {
    struct timeval tv = { (time_t)e, 0 };
    settimeofday(&tv, nullptr);
    source = "ds3231";
  }
  sntp_set_time_sync_notification_cb(onNtp);
  configTzTime((tz && *tz) ? tz : DEFAULT_TZ, "pool.ntp.org", "time.google.com");
}

bool clockValid() { return time(nullptr) > 1700000000; }

void clockLoop() {
  if (!ntpArrived) return;
  ntpArrived = false;
  source = "internet";
  static uint32_t lastDsWrite = 0;
  uint32_t now = (uint32_t)time(nullptr);
  if (hasDs && (lastDsWrite == 0 || now - lastDsWrite > 6UL * 3600UL)) {
    if (dsSet(now)) lastDsWrite = now;
  }
}

void clockFromServer(uint32_t epoch) {
  if (epoch < 1700000000) return;
  if (clockValid()) {
    // la hora de internet manda; la del DS3231 (o una anterior del servidor) se corrige si se fue > 2 min
    if (!strcmp(source, "internet")) return;
    int64_t diff = (int64_t)epoch - (int64_t)time(nullptr);
    if (diff >= -120 && diff <= 120) return;
    Serial.printf("Hora corregida con la del servidor (%+lld s)\n", (long long)diff);
  }
  struct timeval tv = { (time_t)epoch, 0 };
  settimeofday(&tv, nullptr);
  source = "server";
  if (hasDs) dsSet(epoch);
}

const char* clockSource() { return source; }
bool clockHasDs3231() { return hasDs; }

void clockHHMM(char* out, size_t n) {
  if (!clockValid()) { strlcpy(out, "--:--", n); return; }
  time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
  strftime(out, n, "%H:%M", &tm);
}
