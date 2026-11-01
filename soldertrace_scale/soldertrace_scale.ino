// =====================================================
// SolderTrace Scale - estacion de pesaje de envases para el portal SolderTrace
// Waveshare ESP32-S3-Touch-LCD-2 + balanza Rhuisan 5000 g x 0.01 g (RS-232)
//
// Flujo: en el portal el operario escanea el QR del envase y elige esta bascula (B1, B2...).
// El portal le manda el envase; la pantalla pide vaciar la balanza, poner el envase y esperar a
// que el peso quede estable. El peso vuelve solo al portal, que lo compara con lo esperado
// (verde = OK; ambar = retenido hasta que un supervisor lo apruebe). Todo queda en el log.
//
// Arduino IDE (core esp32 3.3.x):  placa "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM",
// USB CDC On Boot "Enabled", Partition Scheme "Custom" (usa el partitions.csv de esta carpeta).
// Librerias: "GFX Library for Arduino" 1.6.4, "WebSockets" (Markus Sattler) 2.7.x, "ArduinoJson" 7.x
// =====================================================
// El programa esta repartido en modulos (.h/.cpp de esta carpeta); setup() y loop() estan en app.cpp.
