#include "wifi_setup.h"
#include "app.h"
#include "config.h"
#include "store.h"
#include "lang.h"
#include "ota.h"
#include "link.h"
#include "pin.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_random.h>

static WebServer* web = nullptr;
static DNSServer* dns = nullptr;
static bool inSetup = false, setupAuto = false;
static unsigned long setupSince = 0, lastStaTry = 0;
static bool needPin() { return setupAuto && pinRequired(); }   // red abierta sola: la pagina pide el PIN de Ajustes
static unsigned long noWifiSince = 0, lastTry = 0;
static int tries = 0;

static String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

// La busqueda de redes se hace al entrar y con el enlace "buscar otra vez": buscar en cada visita
// saca al movil de la red de la bascula mientras se rellena el formulario.
static void pageForm() {
  int n = WiFi.scanComplete();
  String opts;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length() || opts.indexOf("value=\"" + htmlEscape(s) + "\"") >= 0) continue;
    opts += "<option value=\"" + htmlEscape(s) + "\">" + htmlEscape(s) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  const char* scanNote = n == WIFI_SCAN_RUNNING ? "Searching... reload in a few seconds" : (n < 0 ? "Search failed" : "");
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
    "<p><small>" + scanNote + " <a href=/scan style='color:#9fd8b8'>Search again</a></small></p>"
    "<input name=ssid maxlength=32 placeholder='Network name' value=\"" + htmlEscape(cfg.wifiSsid) + "\">"
    "<label>Password</label><input name=pass type=password maxlength=64 placeholder='(unchanged if empty)'>"
    "<label>Language / Idioma / Langue / Idioma</label><select name=lang>" + lsel + "</select>" +
    String(needPin() ? "<label>Settings PIN / PIN de ajustes</label><input name=pin type=password inputmode=numeric pattern='[0-9]{4}' maxlength=4 required>" : "") +
    "<details><summary><small>Advanced</small></summary><label>Server (wss://...)</label><input name=url maxlength=127 value=\"" +
    htmlEscape(cfg.serverUrl) + "\"></details>"
    "<button>Save and restart</button></form><p><small>MAC " + String(macStr) + " · " FW_VERSION "</small></p></body></html>";
  web->send(200, "text/html; charset=utf-8", h);
}

static void pageScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  web->sendHeader("Location", "/");
  web->send(303, "text/plain", "");
}

static void pageSave() {
  String ssid = web->arg("ssid"), pass = web->arg("pass"), url = web->arg("url"), lang = web->arg("lang");
  ssid.trim(); url.trim();
  if (needPin()) {
    PinResult pr = pinCheck(web->arg("pin").c_str());
    if (pr != PIN_OK) {
      web->send(403, "text/plain", pr == PIN_LOCKED ? "Locked for 1 minute / Bloqueado 1 minuto" : "Wrong PIN / PIN incorrecto");
      return;
    }
  }
  if (!ssid.length() || ssid.length() > 32) { web->send(400, "text/plain", "Wi-Fi name required"); return; }
  if (url.length() && (url.length() >= sizeof(cfg.serverUrl) || !linkUrlValid(url.c_str()))) {
    web->send(400, "text/plain", "Server must be wss://host[:port]/path");
    return;
  }
  bool sameNet = ssid == cfg.wifiSsid;
  strlcpy(cfg.wifiSsid, ssid.c_str(), sizeof(cfg.wifiSsid));
  if (pass.length() || !sameNet) strlcpy(cfg.wifiPass, pass.c_str(), sizeof(cfg.wifiPass));
  if (url.length() && url != cfg.serverUrl) {
    // otro servidor: la llave de alta era del anterior y no se le manda; hay que dar de alta otra vez
    strlcpy(cfg.serverUrl, url.c_str(), sizeof(cfg.serverUrl));
    cfg.token[0] = 0;
    cfg.number = 0;
    cfg.name[0] = 0;
    cfg.enabled = true;
  }
  if (lang.length()) strlcpy(cfg.lang, lang.c_str(), sizeof(cfg.lang));
  storeSave();
  web->send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><body style='font-family:sans-serif'><h2>Saved. The scale restarts now.</h2></body>");
  delay(1200);
  otaMarkValid();   // reinicio pedido por el usuario: la version funciona
  ESP.restart();
}

void wifiStartSetup(bool automatic) {
  if (inSetup) return;
  inSetup = true;
  setupAuto = automatic;
  lastStaTry = millis();
  setupMode = true;
  setupSince = millis();
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  // contrasena de 10 caracteres nueva cada vez, se ve en la pantalla. Sin 0/O/1/I/l para leerla
  // sin dudas y en mayusculas para escribirla facil en el movil (32 simbolos: 50 bits).
  static const char ALPHA[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
  for (int i = 0; i < 10; i++) apPassword[i] = ALPHA[esp_random() % (sizeof(ALPHA) - 1)];
  apPassword[10] = 0;
  WiFi.softAP(apSsid, apPassword);
  WiFi.scanNetworks(true);
  dns = new DNSServer();
  dns->start(53, "*", WiFi.softAPIP());
  web = new WebServer(80);
  web->on("/", HTTP_GET, pageForm);
  web->on("/save", HTTP_POST, pageSave);
  web->on("/scan", HTTP_GET, pageScan);
  web->onNotFound(pageForm);   // portal cautivo: cualquier direccion abre el formulario
  web->begin();
  Serial.print("Modo configuracion WiFi: red "); Serial.print(apSsid); Serial.print("  clave "); Serial.println(apPassword);
}

bool wifiInSetup() { return inSetup; }
bool wifiSetupAuto() { return inSetup && setupAuto; }

static void startSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
}

void wifiStopSetup() {
  if (!inSetup) return;
  if (web) { web->stop(); delete web; web = nullptr; }
  if (dns) { dns->stop(); delete dns; dns = nullptr; }
  WiFi.scanDelete();
  bool up = WiFi.status() == WL_CONNECTED;   // ya volvio la WiFi guardada: se sigue con esa conexion
  WiFi.softAPdisconnect(true);
  inSetup = false;
  setupAuto = false;
  setupMode = false;
  apPassword[0] = 0;
  noWifiSince = 0;
  tries = 0;
  if (up) WiFi.mode(WIFI_STA);
  else if (cfg.wifiSsid[0]) startSta();
  Serial.println("Red de configuracion cerrada: vuelve a la WiFi guardada");
}

unsigned long wifiSetupLeft() {
  if (!inSetup || !cfg.wifiSsid[0]) return 0;   // sin WiFi guardada se queda abierta
  unsigned long lim = (unsigned long)cfg.apTimeoutMin * 60000UL, e = millis() - setupSince;
  return e >= lim ? 1 : lim - e;
}

void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(apSsid[0] ? apSsid : "soldertrace-scale");
  if (!cfg.wifiSsid[0]) { wifiStartSetup(); return; }
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
}

bool wifiConnected() { return !inSetup && WiFi.status() == WL_CONNECTED; }

unsigned long wifiDownFor() {
  if (inSetup || !cfg.wifiSsid[0] || WiFi.status() == WL_CONNECTED || noWifiSince == 0) return 0;
  return millis() - noWifiSince;
}

void wifiLoop() {
  if (inSetup) {
    dns->processNextRequest();
    web->handleClient();
    int phones = WiFi.softAPgetStationNum();
    if (setupAuto && phones == 0) {
      // abierta sola: mientras nadie esta conectado a ella sigue buscando la WiFi guardada cada 30 s,
      // y si vuelve, la cierra (con un movil conectado no se busca: cambiar de canal lo desconectaria)
      if (WiFi.status() == WL_CONNECTED) { wifiStopSetup(); return; }
      if (millis() - lastStaTry > 30000UL) { lastStaTry = millis(); WiFi.begin(cfg.wifiSsid, cfg.wifiPass); }
    }
    // con WiFi guardada se cierra sola pasados cfg.apTimeoutMin minutos (no con un movil en el formulario)
    if (cfg.wifiSsid[0] && phones == 0 && millis() - setupSince > (unsigned long)cfg.apTimeoutMin * 60000UL) wifiStopSetup();
    return;
  }
  // la WiFi guardada no aparece en cfg.wifiWaitS: abre la red de configuracion (nunca a mitad de un pesaje)
  if (cfg.wifiSsid[0] && WiFi.status() != WL_CONNECTED && noWifiSince &&
      millis() - noWifiSince >= (unsigned long)cfg.wifiWaitS * 1000UL &&
      !job.active && !result.show && otaProgress < 0 && !menuOpen && !infoOpen && !pinOpen) {
    Serial.println("La WiFi guardada no aparece: se abre la red de configuracion");
    wifiStartSetup(true);
    return;
  }
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
