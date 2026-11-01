// =====================================================
// SolderTrace Scale - configuracion fija de la placa
// Waveshare ESP32-S3-Touch-LCD-2 (2", 240x320, tactil CST816D)
// + balanza Rhuisan 5000 g x 0.01 g por RS-232 (MAX3232), 9600 8N1, modo continuo
//
// Todo lo que el cliente puede cambiar (WiFi, servidor, idioma, zona horaria, tolerancias de
// estabilidad, numero de bascula...) NO esta aqui: llega del portal o de la pagina de
// configuracion de la propia bascula y se guarda en la memoria de la placa.
// =====================================================
#pragma once
#include <Arduino.h>

#define FW_VERSION "st-2026.11.01a"
// Marca que lleva todo firmware de esta bascula: la actualizacion OTA rechaza archivos sin ella.
#define FW_MARK_PREFIX "SOLDERTRACE-SCALE-FW:"
#define FW_MARK FW_MARK_PREFIX FW_VERSION

// ---------- Pines (numero impreso en la placa) ----------
#define PIN_SCALE_RX 16   // <- TXD del modulo RS-232/TTL
#define PIN_SCALE_TX 17   // -> RXD del modulo RS-232/TTL
#define SCALE_BAUD   9600

// Pantalla ST7789 240x320 (horizontal 320x240) y tactil CST816D
#define LCD_MOSI 38
#define LCD_SCK  39
#define LCD_CS   45
#define LCD_DC   42
#define LCD_BL   1
#define SD_CS    41
#define TP_SDA   48
#define TP_SCL   47
#define TP_INT   46
#define TP_ADDR  0x15
#define DS3231_ADDR 0x68   // reloj opcional en el mismo I2C que el tactil

// Ajuste del tactil (igual que en las basculas dosificadoras)
#define TOUCH_SWAP_XY  true
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y true

// ---------- Valores por defecto (el portal los cambia) ----------
#define DEFAULT_SERVER_URL   "wss://app.soldertrace.com/api/scales/ws"
#define DEFAULT_TZ           "EST5EDT,M3.2.0,M11.1.0"
#define DEFAULT_LANG         "en"
#define DEFAULT_STABLE_MS    2000      // peso quieto este tiempo = estable
#define DEFAULT_STABLE_BAND  0.05f     // variacion maxima (g) dentro de la ventana
#define DEFAULT_ZERO_TOL     0.20f     // |peso| <= esto = balanza en cero
#define DEFAULT_MIN_JAR_G    5.0f      // por debajo no hay envase encima

// ---------- Tiempos ----------
#define WAIT_TIME_MS        90000UL    // al encender espera hasta 1:30 por WiFi y hora
#define SCALE_TIMEOUT_MS    1500UL     // sin lecturas = balanza desconectada
#define STATUS_EVERY_MS     30000UL    // estado al servidor
#define LIVE_EVERY_MS       300UL      // peso en vivo al portal durante un pesaje
#define RESULT_HOLD_MS      30000UL    // el resultado se ve hasta quitar el envase (max 30 s)
#define QUEUE_MAX           400        // pesajes guardados sin conexion

// Limites del tiempo de estabilidad (la ventana de scale.cpp cubre hasta 256 x 50 ms = 12,8 s)
#define STABLE_MS_MIN       500UL
#define STABLE_MS_MAX       10000UL

// Conexion con el portal: reintento cada 15 s, que sube a 30 s y 60 s si sigue sin servidor
#define WS_RETRY_MIN_MS     15000UL
#define WS_RETRY_MAX_MS     60000UL
#define MARK_VALID_AFTER_MS 600000UL   // version nueva: se da por buena al conectar o tras 10 min con WiFi

// Particiones: mismo partitions.csv que las dosificadoras (2 x 3 MB de programa para OTA)
