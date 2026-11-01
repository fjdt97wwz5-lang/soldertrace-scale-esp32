#include "wifi_setup.h"
#include "app.h"
#include "config.h"
#include "store.h"
#include "lang.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>

static WebServer* web = nullptr;
static DNSServer* dns = nullptr;
static bool inSetup = false;
static unsigned long setupSince = 0;

static String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

static void pageForm() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); n = 0; }
  String opts;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || opts.indexOf("value=\"" + htmlEscape(s) + "\"") >= 0) continue;
    opts += "<option value=\"" + htmlEscape(s) + "\">" + htmlEscape(s) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  if (n >= 0) { WiFi.scanDelete(); WiFi.scanNetworks(true); }
  const char* langs[4][2] = {{"en", "English"}, {"es", "Español"}, {"fr", "Français"}, {"pt-BR", "Português (Brasil)"}};
  String lsel;
  for (auto& l : langs) lsel += String("<option value=\"") + l[0] + "\"" + (langIndexOf(cfg.lang) == langIndexOf(l[0]) ? " selected" : "") + ">" + l[1] + "</option>";
  String h =
    "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>SolderTrace Scale</title><style>body{font-family:system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;"
    "background:#0c1210;color:#ecf2ee}h1{font-size:20px}label{display:block;margin:14px 0 4px;font-size:14px;color:#9fb0a9}"
    "input,select{width:100%;padding:10px;font-size:16px;border-radius:8px;border:1px solid #3a4a44;background:#1e2824;color:#ecf2ee;box-sizing:border-box}"
    "button{margin-top:20px;width:100%;padding:12px;font-size:16px;border:0;border-radius:8px;background:#22aa5a;color:#fff}"
    "small{color:#82928c}</style></head><body><h1>SolderTrace Scale</h1>"
    "<form method=post action=/save>"
    "<label>Wi-Fi (2.4 GHz)</label><select name=ssid_l onchange=\"document.getElementsByName('ssid')[0].value=this.value\">"
    "<option value=''>--</option>" + opts + "</select>"
    "<input name=ssid maxlength=32 placeholder='Network name' value=\"" + htmlEscape(cfg.wifiSsid) + "\">"
    "<label>Password</label><input name=pass type=password maxlength=64 placeholder='(unchanged if empty)'>"
    "<label>Language / Idioma / Langue / Idioma</label><select name=lang>" + lsel + "</select>"
    "<details><summary><small>Advanced</small></summary><label>Server</label><input name=url maxlength=127 value=\"" +
    htmlEscape(cfg.serverUrl) + "\"></details>"
    "<button>Save and restart</button></form><p><small>MAC " + String(macStr) + " · " FW_VERSION "</small></p></body></html>";
  web->send(200, "text/html; charset=utf-8", h);
}

static void pageSave() {
  String ssid = web->arg("ssid"), pass = web->arg("pass"), url = web->arg("url"), lang = web->arg("lang");
  ssid.trim(); url.trim();
  if (!ssid.length() || ssid.length() > 32) { web->send(400, "text/plain", "Wi-Fi name required"); return; }
  if (url.length() && !(url.startsWith("wss://") || url.startsWith("ws://"))) { web->send(400, "text/plain", "Server must start with wss://"); return; }
  bool sameNet = ssid == cfg.wifiSsid;
  strlcpy(cfg.wifiSsid, ssid.c_str(), sizeof(cfg.wifiSsid));
  if (pass.length() || !sameNet) strlcpy(cfg.wifiPass, pass.c_str(), sizeof(cfg.wifiPass));
  if (url.length()) strlcpy(cfg.serverUrl, url.c_str(), sizeof(cfg.serverUrl));
  if (lang.length()) strlcpy(cfg.lang, lang.c_str(), sizeof(cfg.lang));
  storeSave();
  web->send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><body style='font-family:sans-serif'><h2>Saved. The scale restarts now.</h2></body>");
  delay(1200);
  ESP.restart();
}

void wifiStartSetup() {
  inSetup = true;
  setupMode = true;
  setupSince = millis();
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  // contrasena de 8 cifras nueva cada vez, se ve en la pantalla
  snprintf(apPassword, sizeof(apPassword), "%08lu", (unsigned long)(esp_random() % 100000000UL));
  WiFi.softAP(apSsid, apPassword);
  WiFi.scanNetworks(true);
  dns = new DNSServer();
  dns->start(53, "*", WiFi.softAPIP());
  web = new WebServer(80);
  web->on("/", HTTP_GET, pageForm);
  web->on("/save", HTTP_POST, pageSave);
  web->onNotFound(pageForm);   // portal cautivo: cualquier direccion abre el formulario
  web->begin();
  Serial.print("Modo configuracion WiFi: red "); Serial.print(apSsid); Serial.print("  clave "); Serial.println(apPassword);
}

bool wifiInSetup() { return inSetup; }

void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(apSsid[0] ? apSsid : "soldertrace-scale");
  if (!cfg.wifiSsid[0]) { wifiStartSetup(); return; }
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
}

bool wifiConnected() { return !inSetup && WiFi.status() == WL_CONNECTED; }

void wifiLoop() {
  if (inSetup) {
    dns->processNextRequest();
    web->handleClient();
    // 15 min sin guardar y con WiFi guardada: vuelve solo a trabajar
    if (cfg.wifiSsid[0] && millis() - setupSince > 15UL * 60UL * 1000UL) ESP.restart();
    return;
  }
  static unsigned long noWifiSince = 0, lastTry = 0;
  static int tries = 0;
  if (WiFi.status() == WL_CONNECTED) { noWifiSince = 0; tries = 0; return; }
  unsigned long now = millis();
  if (noWifiSince == 0) { noWifiSince = now; lastTry = now; return; }
  if (now - noWifiSince < 20000UL || now - lastTry < 20000UL) return;
  lastTry = now;
  if (++tries % 3 == 0) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
  } else {
    WiFi.reconnect();
  }
}
