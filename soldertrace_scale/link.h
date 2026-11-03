// Enlace con el portal SolderTrace por WebSocket seguro (wss).
// La bascula abre la conexion hacia el servidor: no hace falta IP fija ni abrir puertos en la red del cliente.
//
// Mensajes (JSON, un objeto por mensaje, campo "type"):
//  bascula -> servidor
//    hello      {mac, chip, fw, token?, acked_upto, next_seq, queue, lang}
//    reading    {seq, request_id, grams, stable, ts}     (se reenvia hasta recibir ack)
//    live       {request_id, grams, stable, zero}        (peso en vivo durante un pesaje)
//    cancelled  {request_id, reason}                     (operario o tiempo agotado)
//    status     {rssi, uptime_s, scale_ok, queue, heap, ip, clock, ds3231}
//    activation_request {}
//    ota_status {phase, progress, error?}
//  servidor -> bascula
//    pair_code  {code, expires_s}                        placa sin dar de alta
//    paired     {token, number, name}                    el admin la dio de alta
//    welcome    {number, name, enabled, lang, tz, server_time, settings{stable_ms, stable_band_g, zero_tol_g, min_jar_g}, acked_upto}
//    config     {igual que welcome}                      cambios desde el portal
//    weigh      {request_id, qr, product, lot, mode, expected_g, tol_g, expires_s}
//    cancel     {request_id}
//    result     {request_id, status: ok|out_of_range|on_hold|rejected|error, message}
//    ack        {upto}
//    activation {status: rejected}
//    ota        {url, version, md5, size}
//    unpair     {}
//    error      {code: bad_token|...}
#pragma once
#include <Arduino.h>

void linkBegin();
void linkLoop();
bool linkConnected();     // socket abierto
bool linkReady();         // socket abierto y bascula autenticada (dada de alta)

// Estado de la conexion con el servidor, para la pantalla "Esperando al servidor"
enum LinkDiag {
  LD_NONE,        // aun no hubo intento
  LD_WAIT_REPLY,  // conexion segura abierta, esperando que el servidor acepte el WebSocket
  LD_NO_REPLY,    // el servidor no contesto a tiempo (code = segundos de espera)
  LD_HTTP,        // el servidor contesto con un error HTTP (code)
  LD_DNS,         // no se encuentra el nombre del servidor (text = nombre)
  LD_TCP,         // no se llega al servidor (text = IP)
  LD_TLS,         // falla la conexion segura (code = error mbedtls, text = descripcion)
  LD_CONN_FAIL,   // no se pudo abrir la conexion (sin saber aun por que)
  LD_OTHER,       // otro motivo (text)
  LD_OPEN,        // socket abierto
  LD_LOST         // estaba conectada y se corto
};
int linkAttempts();
LinkDiag linkDiag();
int linkDiagCode();
const char* linkDiagText();
const char* linkHost();
void linkSendLive(const char* requestId, float grams, bool stable, bool zero);
void linkSendCancelled(const char* requestId, const char* reason);
void linkSendActivationRequest();
void linkSendDemoWeigh();              // v0.8.0: boton PESANDO DEMO
void linkSendOtaStatus(const char* phase, int progress, const char* err);
void linkFlushQueue();    // manda los pesajes pendientes
void linkRestart();       // tras cambiar el servidor
bool linkUrlValid(const char* url);   // wss://host[:puerto]/ruta que la bascula acepta
