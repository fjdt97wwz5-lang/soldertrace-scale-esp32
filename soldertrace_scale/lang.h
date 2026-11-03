// Textos de la pantalla en los 4 idiomas de SolderTrace (en, es, fr, pt-BR).
// El idioma lo elige el admin del cliente para cada bascula en el portal.
#pragma once
#include <Arduino.h>

enum Txt {
  T_STARTING, T_WIFI_SETUP, T_WIFI_SETUP_HINT1, T_WIFI_SETUP_HINT2, T_WIFI_PASSWORD,
  T_WAIT_TIME, T_WAIT_TIME_HINT, T_CONNECTING, T_OFFLINE, T_PAIR_TITLE, T_PAIR_HINT1, T_PAIR_HINT2,
  T_DISABLED, T_DISABLED_HINT, T_REQUEST_ACT, T_REQUEST_SENT, T_REQUEST_REJECTED,
  T_READY, T_READY_HINT, T_NO_SCALE, T_NO_SCALE_HINT,
  T_EMPTY_SCALE, T_PLACE_JAR, T_HOLD_STILL, T_SENDING, T_SAVED_OFFLINE,
  T_RESULT_OK, T_RESULT_OUT, T_RESULT_HOLD, T_REMOVE_JAR, T_CANCEL, T_CANCELLED, T_TIMEOUT,
  T_EXPECTED, T_LAST, T_MODE_REMOVE, T_MODE_RETURN, T_MODE_RECEIVE, T_MODE_CHECK,
  T_UPDATING, T_UPDATE_FAIL, T_MENU, T_MENU_WIFI, T_MENU_INFO, T_MENU_RESTART, T_BACK,
  T_SERVER, T_CONNECTED, T_NOT_CONNECTED, T_PENDING, T_LIVE, T_ERROR,
  T_EXIT_SETUP, T_WIFI_FAIL, T_WIFI_FAIL_HINT,
  T_SETTLING, T_TARE_HINT, T_PIN_TITLE, T_PIN_WRONG, T_PIN_LOCKED, T_DEL, T_WIFI_QR, T_AP_CLOSES, T_WIFI_NOT_FOUND,
  T_DEMO, T_DEMO_HINT, T_DEMO_PUT, T_DEMO_OUT, T_DEMO_REMOVE,
  T_WAIT_SERVER, T_ST_SEARCHING, T_ST_TIME, T_ST_TRY, T_ST_CONNECTING, T_ST_NO_WELCOME,
  T_R_WAIT_REPLY, T_R_NO_REPLY, T_R_HTTP, T_R_DNS, T_R_TCP, T_R_TLS, T_R_CONN_FAIL, T_R_LOST,
  T_COUNT
};

// 0 en, 1 es, 2 fr, 3 pt-BR
static const char* const TXT[T_COUNT][4] = {
  {"Starting", "Iniciando", "Démarrage", "Iniciando"},
  {"Wi-Fi setup", "Configurar WiFi", "Config. Wi-Fi", "Configurar Wi-Fi"},
  {"On your phone, join:", "En el móvil, conéctate a:", "Sur le téléphone :", "No celular, conecte-se a:"},
  {"then open http://192.168.4.1", "y abre http://192.168.4.1", "puis ouvrez http://192.168.4.1", "e abra http://192.168.4.1"},
  {"Password:", "Contraseña:", "Mot de passe :", "Senha:"},
  {"Waiting for Wi-Fi and time", "Esperando WiFi y hora", "Attente Wi-Fi et heure", "Aguardando Wi-Fi e hora"},
  {"Weighing starts when ready", "Se podrá pesar al tenerlos", "Pesée possible ensuite", "Pesagem liberada em seguida"},
  {"Connecting to SolderTrace", "Conectando a SolderTrace", "Connexion à SolderTrace", "Conectando ao SolderTrace"},
  {"Offline: weighing needs SolderTrace", "Sin conexión: pesar necesita SolderTrace", "Hors ligne : pesée impossible", "Offline: pesagem precisa do SolderTrace"},
  {"Pairing code", "Código de alta", "Code d'appairage", "Código de pareamento"},
  {"SolderTrace > Scales > Add scale", "SolderTrace > Básculas > Añadir", "SolderTrace > Balances > Ajouter", "SolderTrace > Balanças > Adicionar"},
  {"Enter this code to register it", "Escribe este código para darla de alta", "Saisissez ce code pour l'enregistrer", "Digite este código para registrá-la"},
  {"SCALE DISABLED", "BÁSCULA DESACTIVADA", "BALANCE DÉSACTIVÉE", "BALANÇA DESATIVADA"},
  {"Please activate it in SolderTrace", "Actívala en SolderTrace, por favor", "Activez-la dans SolderTrace", "Ative-a no SolderTrace, por favor"},
  {"REQUEST ACTIVATION", "PEDIR ACTIVACIÓN", "DEMANDER ACTIVATION", "PEDIR ATIVAÇÃO"},
  {"Request sent", "Solicitud enviada", "Demande envoyée", "Solicitação enviada"},
  {"Request rejected", "Solicitud rechazada", "Demande refusée", "Solicitação recusada"},
  {"READY", "LISTA", "PRÊTE", "PRONTA"},
  {"Scan a jar in SolderTrace", "Escanea un envase en SolderTrace", "Scannez un pot dans SolderTrace", "Escaneie um pote no SolderTrace"},
  {"NO SCALE DATA", "SIN DATOS DE BALANZA", "PAS DE DONNÉES", "SEM DADOS DA BALANÇA"},
  {"Check RS-232 cable and power", "Revisa el cable RS-232 y la luz", "Vérifiez le câble RS-232", "Verifique o cabo RS-232"},
  {"Empty the scale", "Vacía la balanza", "Videz la balance", "Esvazie a balança"},
  {"Place jar", "Pon el envase", "Posez le pot", "Coloque o pote"},
  {"Hold still...", "Quieto...", "Ne bougez pas...", "Aguarde..."},
  {"Sending...", "Enviando...", "Envoi...", "Enviando..."},
  {"Saved - will send when online", "Guardado - se enviará al conectar", "Enregistré - envoi à la reconnexion", "Salvo - envia ao reconectar"},
  {"WEIGHT OK", "PESO OK", "POIDS OK", "PESO OK"},
  {"OUT OF RANGE", "FUERA DE RANGO", "HORS TOLÉRANCE", "FORA DA FAIXA"},
  {"Jar on hold: supervisor approval", "Envase retenido: aprueba supervisor", "Pot bloqué : accord superviseur", "Pote retido: aprovação supervisor"},
  {"Remove the jar", "Retira el envase", "Retirez le pot", "Retire o pote"},
  {"CANCEL", "CANCELAR", "ANNULER", "CANCELAR"},
  {"Cancelled", "Cancelado", "Annulé", "Cancelado"},
  {"Time out", "Tiempo agotado", "Délai dépassé", "Tempo esgotado"},
  {"Expected", "Esperado", "Attendu", "Esperado"},
  {"Last", "Último", "Dernier", "Último"},
  {"Fridge out", "Salida de nevera", "Sortie frigo", "Saída da geladeira"},
  {"Back to fridge", "Vuelta a nevera", "Retour frigo", "Volta à geladeira"},
  {"Receiving", "Recepción", "Réception", "Recebimento"},
  {"Check weight", "Control de peso", "Contrôle de poids", "Conferir peso"},
  {"UPDATING", "ACTUALIZANDO", "MISE À JOUR", "ATUALIZANDO"},
  {"Update failed: keeps current version", "Falló: sigue la versión actual", "Échec : version actuelle conservée", "Falhou: mantém a versão atual"},
  {"Settings", "Ajustes", "Réglages", "Ajustes"},
  {"WI-FI", "WIFI", "WI-FI", "WI-FI"},
  {"INFO", "INFO", "INFO", "INFO"},
  {"RESTART", "REINICIAR", "REDÉMARRER", "REINICIAR"},
  {"BACK", "VOLVER", "RETOUR", "VOLTAR"},
  {"Server", "Servidor", "Serveur", "Servidor"},
  {"connected", "conectado", "connecté", "conectado"},
  {"not connected", "sin conexión", "non connecté", "sem conexão"},
  {"pending", "pendientes", "en attente", "pendentes"},
  {"live", "en vivo", "en direct", "ao vivo"},
  {"Error", "Error", "Erreur", "Erro"},
  {"EXIT SETUP", "SALIR", "QUITTER", "SAIR"},
  {"Can't connect to Wi-Fi", "No conecta a la WiFi", "Wi-Fi injoignable", "Sem conexão Wi-Fi"},
  {"Hold the top bar 3 s, then WI-FI", "Mantén pulsada la barra de arriba 3 s y elige WIFI",
   "Appuyez 3 s sur la barre du haut, puis WI-FI", "Segure a barra superior 3 s e toque em WI-FI"},
  {"Stabilizing", "Estabilizando", "Stabilisation", "Estabilizando"},
  {"Zero it with the scale's TARE key", "Haz tara con el botón de la balanza", "Faites la tare sur la balance", "Faça a tara no botão da balança"},
  {"Settings PIN", "PIN de ajustes", "PIN des réglages", "PIN de ajustes"},
  {"Wrong PIN", "PIN incorrecto", "PIN incorrect", "PIN incorreto"},
  {"Locked, wait", "Bloqueado, espera", "Bloqué, patientez", "Bloqueado, aguarde"},
  {"DEL", "BORRAR", "EFF.", "APAGAR"},
  {"or scan the QR code", "o escanea el código QR", "ou scannez le QR code", "ou escaneie o QR code"},
  {"Closes in", "Se cierra en", "Fermeture dans", "Fecha em"},
  {"Saved Wi-Fi not found", "No encuentra la WiFi guardada", "Wi-Fi enregistré introuvable", "Wi-Fi salvo não encontrado"},
  {"DEMO", "DEMO", "DÉMO", "DEMO"},
  {"Demo mode: emulated scale", "Modo demo: báscula emulada", "Mode démo : balance simulée", "Modo demo: balança simulada"},
  {"PUT JAR", "PONER ENVASE", "POSER POT", "COLOCAR POTE"},
  {"OUT OF RANGE", "FUERA RANGO", "HORS PLAGE", "FORA DA FAIXA"},
  {"REMOVE JAR", "QUITAR ENVASE", "RETIRER POT", "RETIRAR POTE"},
  // pantalla "Esperando al servidor" (con %d: se rellena con un numero)
  {"Waiting for the server", "Esperando al servidor", "Attente du serveur", "Aguardando o servidor"},
  {"searching", "buscando", "recherche", "procurando"},
  {"Time", "Hora", "Heure", "Hora"},
  {"try", "intento", "essai", "tentativa"},
  {"connecting", "conectando", "connexion", "conectando"},
  {"connected, waiting reply", "conectado, esperando respuesta", "connecté, attente réponse", "conectado, aguardando resposta"},
  {"Secure link OK, waiting reply", "Conexión segura OK, esperando", "Lien sécurisé OK, attente", "Conexão segura OK, aguardando"},
  {"No reply in %d s", "Sin respuesta en %d s", "Pas de réponse en %d s", "Sem resposta em %d s"},
  {"Server refused (HTTP %d)", "El servidor rechazó (HTTP %d)", "Refusé par le serveur (HTTP %d)", "Servidor recusou (HTTP %d)"},
  {"Server name not found (DNS)", "No encuentra el servidor (DNS)", "Serveur introuvable (DNS)", "Servidor não encontrado (DNS)"},
  {"Cannot reach the server", "No llega al servidor", "Serveur injoignable", "Servidor inacessível"},
  {"Secure connection failed", "Falla la conexión segura", "Échec connexion sécurisée", "Falha na conexão segura"},
  {"Could not open the connection", "No se pudo abrir la conexión", "Connexion impossible", "Não foi possível conectar"},
  {"Connection lost, retrying", "Se cortó la conexión, reintentando", "Connexion perdue, nouvel essai", "Conexão perdida, tentando de novo"},
};

extern int langIdx;   // 0..3

inline int langIndexOf(const char* code) {
  if (!code) return 0;
  if (!strncasecmp(code, "es", 2)) return 1;
  if (!strncasecmp(code, "fr", 2)) return 2;
  if (!strncasecmp(code, "pt", 2)) return 3;
  return 0;
}

inline const char* tr(Txt t) { return TXT[t][langIdx]; }

// La fuente clasica de Arduino_GFX (glcdfont, 6x8) sigue la tabla CP437 completa (sin el desfase
// de 176 de Adafruit_GFX, no hace falta cp437(true)): se pasan los acentos de UTF-8 a CP437.
// Los que no existen en CP437 (ã, õ, Ã, Õ, À, Ê, Ô...) se cambian por la letra sin acento.
inline void toCp437(const char* in, char* out, size_t n) {
  size_t o = 0;
  const unsigned char* s = (const unsigned char*)in;
  while (*s && o + 1 < n) {
    unsigned char c = *s++;
    if (c < 0x80) { out[o++] = (char)c; continue; }
    unsigned int cp = 0;
    if ((c & 0xE0) == 0xC0 && *s) { cp = ((c & 0x1F) << 6) | (*s++ & 0x3F); }
    else { while ((*s & 0xC0) == 0x80) s++; out[o++] = '?'; continue; }
    char m;
    switch (cp) {
      case 0xE1: m = (char)0xA0; break;  case 0xE9: m = (char)0x82; break;  case 0xED: m = (char)0xA1; break;
      case 0xF3: m = (char)0xA2; break;  case 0xFA: m = (char)0xA3; break;  case 0xF1: m = (char)0xA4; break;
      case 0xD1: m = (char)0xA5; break;  case 0xE7: m = (char)0x87; break;  case 0xC7: m = (char)0x80; break;
      case 0xE2: m = (char)0x83; break;  case 0xE0: m = (char)0x85; break;  case 0xEA: m = (char)0x88; break;
      case 0xE8: m = (char)0x8A; break;  case 0xEB: m = (char)0x89; break;  case 0xEE: m = (char)0x8C; break;
      case 0xEF: m = (char)0x8B; break;  case 0xF4: m = (char)0x93; break;  case 0xFB: m = (char)0x96; break;
      case 0xF9: m = (char)0x97; break;  case 0xFC: m = (char)0x81; break;  case 0xC9: m = (char)0x90; break;
      case 0xE3: m = 'a'; break;  case 0xF5: m = 'o'; break;  case 0xC1: case 0xC0: case 0xC2: case 0xC3: m = 'A'; break;
      case 0xCA: case 0xC8: m = 'E'; break;  case 0xCD: m = 'I'; break;  case 0xD3: case 0xD4: case 0xD5: m = 'O'; break;
      case 0xDA: m = 'U'; break;  case 0xBA: m = (char)0xA7; break;  case 0xAA: m = (char)0xA6; break;
      case 0xB0: m = (char)0xF8; break;
      default: m = '?';
    }
    out[o++] = m;
  }
  out[o] = 0;
}

// Ancho en pixeles de un texto UTF-8 con la fuente 6x8 escalada
inline int textWidth(const char* utf8, uint8_t size) {
  int n = 0;
  for (const unsigned char* s = (const unsigned char*)utf8; *s; s++) if ((*s & 0xC0) != 0x80) n++;
  return n * 6 * size;
}
