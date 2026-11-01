#include "ota.h"
#include "app.h"
#include "config.h"
#include "link.h"
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

static String otaUrl, otaVersion, otaMd5;
static uint32_t otaSize = 0;
static bool pending = false;

void otaRequest(const char* url, const char* version, const char* md5, uint32_t size) {
  if (!url || strncmp(url, "https://", 8)) { linkSendOtaStatus("error", 0, "url"); return; }
  otaUrl = url; otaVersion = version; otaMd5 = md5; otaSize = size;
  pending = true;
  otaFailed = false;
  linkSendOtaStatus("waiting", 0, nullptr);
}

bool otaPending() { return pending; }

void otaMarkValid() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("OTA: version nueva marcada como buena");
  }
}

static void fail(const char* why) {
  Serial.print("OTA fallo: "); Serial.println(why);
  Update.abort();
  otaFailed = true;
  otaProgress = -1;
  pending = false;
  linkSendOtaStatus("error", 0, why);
}

void otaLoop(bool idle) {
  if (!pending || !idle || WiFi.status() != WL_CONNECTED) return;
  pending = false;
  otaProgress = 0;
  linkSendOtaStatus("downloading", 0, nullptr);

  NetworkClientSecure client;
  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, otaUrl)) { fail("begin"); return; }
  int code = http.GET();
  if (code != 200) { http.end(); fail("http"); return; }
  int len = http.getSize();
  if (len <= 0 && otaSize) len = otaSize;
  if (len <= 0 || (otaSize && (uint32_t)len != otaSize)) { http.end(); fail("size"); return; }
  if (!Update.begin(len)) { http.end(); fail("space"); return; }
  if (otaMd5.length() == 32) Update.setMD5(otaMd5.c_str());

  // se busca la marca de este firmware mientras se graba
  const char* mark = FW_MARK_PREFIX;
  size_t markLen = strlen(mark), matched = 0;
  bool markFound = false;
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[2048];
  int done = 0, lastPct = -1;
  unsigned long lastData = millis();
  while (done < len) {
    size_t avail = stream->available();
    if (!avail) {
      if (!http.connected() || millis() - lastData > 20000) { http.end(); fail("timeout"); return; }
      delay(2);
      continue;
    }
    int n = stream->readBytes(buf, min(avail, sizeof(buf)));
    if (n <= 0) continue;
    lastData = millis();
    for (int i = 0; i < n && !markFound; i++) {
      if (buf[i] == (uint8_t)mark[matched]) { if (++matched == markLen) markFound = true; }
      else matched = (buf[i] == (uint8_t)mark[0]) ? 1 : 0;
    }
    if (Update.write(buf, n) != (size_t)n) { http.end(); fail("write"); return; }
    done += n;
    int pct = (int)((int64_t)done * 100 / len);
    if (pct != lastPct) {
      lastPct = pct;
      otaProgress = pct;
      extern void uiLoop();
      uiLoop();
      if (pct % 10 == 0) linkSendOtaStatus("downloading", pct, nullptr);
    }
  }
  http.end();
  if (!markFound) { fail("not_soldertrace_firmware"); return; }
  if (!Update.end(true)) { fail(Update.getError() == UPDATE_ERROR_MD5 ? "md5" : "verify"); return; }
  linkSendOtaStatus("rebooting", 100, nullptr);
  Serial.println("OTA correcta: reiniciando con la version nueva");
  delay(800);
  ESP.restart();
}
