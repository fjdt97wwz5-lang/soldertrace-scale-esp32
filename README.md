# SolderTrace · Báscula de pesaje (ESP32-S3)

Firmware de la estación de pesaje de SolderTrace: una **Waveshare ESP32-S3-Touch-LCD-2** conectada a la balanza **Rhuisan 5000 g × 0,01 g** (la misma de las dosificadoras, sin servo ni relé). La báscula se conecta a la WiFi del cliente y al portal `app.soldertrace.com`; el operario escanea el tarro en el portal, elige la báscula (B1, B2…) y la pantalla le pide poner el tarro. El peso estable se manda solo al portal.

Versión actual: ver `VERSIONES.md`. Cada versión queda en su rama `version/…` y nunca se sobrescribe.

## Conexiones

| Desde | Hasta |
|---|---|
| Balanza (salida RS-232, DB9) | Módulo RS-232 ↔ TTL (MAX3232) |
| TXD del módulo | **GPIO 16** de la placa (RX balanza) |
| RXD del módulo | **GPIO 17** de la placa (TX) |
| VCC del módulo | **3V3** de la placa |
| GND del módulo | **G** de la placa |
| Reloj DS3231 (opcional) | SDA 48, SCL 47 (el mismo bus del táctil), 3V3 y G |

La balanza va en **9600 8N1, modo continuo**. Alimentación de la placa por USB-C (5 V).

## Grabar la primera vez (por cable)

La primera vez se graba por USB. Después ya se actualiza sola desde el portal (OTA).

**Opción fácil, un solo archivo** (navegador Chrome o Edge, sin instalar nada):
1. Abre https://espressif.github.io/esptool-js/
2. Conecta la placa por USB-C. Si no aparece, mantén pulsado **BOOT**, pulsa y suelta **RESET**, y suelta BOOT.
3. Pulsa **Connect**, elige el puerto, baud 921600.
4. En *Flash Address* pon **0x0** y elige `binarios/<versión>/soldertrace_scale_<versión>_completo.bin`.
5. Pulsa **Program**. Al acabar, pulsa **RESET** en la placa.

**Con esptool en la computadora** (el mismo resultado, por partes):
```
esptool --chip esp32s3 --baud 921600 write_flash 0x0 soldertrace_scale.ino.bootloader.bin 0x8000 soldertrace_scale.ino.partitions.bin 0xe000 boot_app0.bin 0x10000 soldertrace_scale_<versión>.bin
```
(o `esptool --chip esp32s3 write_flash 0x0 soldertrace_scale_<versión>_completo.bin`)

**Con Arduino IDE** (compilando el código): placa *ESP32S3 Dev Module*, Flash Size **16MB**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**, Partition Scheme **Custom** (usa el `partitions.csv` de la carpeta). Librerías: *GFX Library for Arduino* 1.6.4, *WebSockets* (Markus Sattler) 2.7.3, *ArduinoJson* 7.4.2. Core esp32 3.3.2.

## Puesta en marcha en el cliente

1. **WiFi.** Sin WiFi guardada, la báscula crea su propia red **SolderTrace-XXXX** y muestra en pantalla la contraseña. Desde el móvil te conectas a esa red, se abre la página de configuración (o entra a http://192.168.4.1), eliges la WiFi del cliente, su contraseña y el idioma. La báscula se reinicia y se conecta. Para cambiar la WiFi más tarde: mantén pulsada la cabecera de la pantalla 3 segundos → menú → *WiFi setup*.
2. **Hora.** Al arrancar espera hasta 1:30 a tener la hora (por internet o del DS3231 si lo lleva). Sin hora no pesa, para que todo registro lleve fecha y hora correctas.
3. **Alta en el portal.** La pantalla muestra un **código de 6 dígitos**. El administrador del cliente entra en el portal → *Scales* → *Add scale*, escribe el código, elige la planta y el nombre. El portal le da su número (**B1**, **B2**…), que queda siempre visible en la pantalla.
4. **Activar / desactivar.** Desde el portal. Desactivada, la pantalla lo dice y tiene el botón para **pedir activación**, que le llega al administrador para aprobar o rechazar.

## Cómo pesa

1. El operario escanea el QR del tarro en el portal y elige la báscula.
2. La pantalla muestra el producto, el lote y el tarro: primero pide la balanza vacía (en cero) y luego **Place jar**.
3. Cuando el peso está estable 2 segundos, lo captura y lo manda. Hay botón **Cancel**.
4. El resultado sale en pantalla: **verde** dentro de tolerancia, **rojo** fuera (el tarro queda retenido hasta que un supervisor lo apruebe o lo rechace a cuarentena en el portal), **ámbar** si se guardó sin conexión.

Sin WiFi o sin servidor, los pesos se guardan en la placa (hasta 400) y se mandan solos cuando vuelve la conexión; el portal confirma cada uno y no los duplica.

La tolerancia (±1 g por defecto), el tiempo de estabilidad y los demás ajustes se cambian en el portal, por cliente y por producto; la báscula los recibe sola.

## Actualizar por WiFi (OTA)

En el portal → *Scales* → *Update firmware*. La báscula descarga la versión por https, comprueba que es un firmware de SolderTrace y su MD5, y solo se reinicia si todo está bien. Si la versión nueva no arranca, vuelve sola a la anterior. Nunca se actualiza a mitad de un pesaje.

## Compilar

```
./compilar.sh
```
Deja los archivos en `binarios/<versión>/` con su `md5.txt`.

## Seguridad

No hay ninguna contraseña ni WiFi escrita en el código. La WiFi del cliente y la llave de la báscula se guardan solo en la memoria de la placa. La conexión con el portal va cifrada (wss / https) con los certificados raíz que trae la placa, y el portal guarda solo una huella de la llave.
