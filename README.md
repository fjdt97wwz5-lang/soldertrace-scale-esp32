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

Ajuste de compilación: `WEBSOCKETS_TCP_TIMEOUT=10000` (la báscula espera hasta 10 s a que el servidor acepte la conexión; con 2 s, en st-2026.11.01a y 11.03a, se cortaba antes de que contestara y se quedaba en "Conectando"). `compilar.sh` ya lo pone. En Arduino IDE no se puede poner por proyecto: crea el archivo `platform.local.txt` junto al `platform.txt` del core (`Arduino15/packages/esp32/hardware/esp32/3.3.2/`) con la línea
```
compiler.cpp.extra_flags=-DWEBSOCKETS_TCP_TIMEOUT=10000
```
y reinicia el IDE. Sin él también funciona, pero con 5 s por intento. Al arrancar, el monitor serie muestra el valor que lleva (`espera TCP … ms`).

## Puesta en marcha en el cliente

1. **WiFi.** Sin WiFi guardada, la báscula crea su propia red **SolderTrace-XXXX** y muestra en pantalla la contraseña (10 letras y números, nueva cada vez, sin 0/O/1/I para no confundirlos). Desde el móvil te conectas a esa red, se abre la página de configuración (o entra a http://192.168.4.1), eliges la WiFi del cliente, su contraseña y el idioma (*Search again* vuelve a buscar redes). La báscula se reinicia y se conecta. La pantalla muestra también un **QR**: con la cámara del móvil te une a esa red sin escribir la clave. Para cambiar la WiFi más tarde: mantén pulsada la cabecera de la pantalla 3 segundos → PIN (si el portal tiene uno puesto) → menú → *WI-FI*. Si ya hay una WiFi guardada, la pantalla de configuración tiene el botón **EXIT SETUP** para salir sin cambiar nada, y se cierra sola a los 10 minutos volviendo a la WiFi guardada (sin reiniciar).
   **Si la WiFi guardada no aparece en 1:30** (router nuevo, clave cambiada), la báscula abre sola esa red de configuración con el QR, y la cierra a los 10 minutos para volver a buscar la WiFi guardada. Nunca lo hace a mitad de un pesaje, y los pesajes hechos sin conexión siguen guardados. Mientras ningún móvil está conectado a esa red, sigue buscando la WiFi guardada y, si vuelve, la cierra sola. Si el portal tiene PIN puesto, la página de configuración de esa red lo pide. Los dos tiempos se cambian en el portal (*Settings > Weighing*).
   **PIN de Ajustes.** El administrador pone un PIN de 4 cifras en el portal (*Settings > Weighing*). La báscula lo pide para abrir Ajustes; 3 fallos la bloquean 1 minuto. Sin PIN puesto, Ajustes se abre directamente. La placa solo guarda una huella del PIN, nunca el PIN.
   En *Advanced* se puede cambiar el servidor: solo se acepta `wss://` (cifrado). Al cambiarlo, la báscula borra su alta y hay que darla de alta otra vez en el portal nuevo.
2. **Hora.** Al arrancar espera hasta 1:30 a tener la hora (por internet o del DS3231 si lo lleva). Si pasado ese tiempo sigue sin hora, pesa igual: el portal pone la hora de llegada a esos pesos y los marca como hora aproximada.
3. **Alta en el portal.** La pantalla muestra un **código de 6 dígitos**. El administrador del cliente entra en el portal → *Scales* → *Add scale*, escribe el código, elige la planta y el nombre. El portal le da su número (**B1**, **B2**…), que queda siempre visible en la pantalla.
4. **Activar / desactivar.** Desde el portal. Desactivada, la pantalla lo dice y tiene el botón para **pedir activación**, que le llega al administrador para aprobar o rechazar.

## Cómo pesa

1. El operario escanea el QR del tarro en el portal y elige la báscula.
2. La pantalla muestra el producto, el lote y el tarro. Primero pide la balanza vacía: el peso tiene que quedar entre -0,5 y +0,5 g **seguido 1 segundo** (pantalla *Estabilizando* con barra), igual que la dosificadora. Si no está en cero sale *Vacía la balanza* en ámbar: la tara se hace con el botón de la balanza, el firmware nunca hace tara. Luego **Place jar**.
3. Cuando el peso está estable 2 segundos, lo captura y lo manda. Hay botón **Cancel**.
4. El resultado sale en pantalla: **verde** dentro de tolerancia; **ámbar** fuera de tolerancia (el tarro queda retenido hasta que un supervisor lo apruebe o lo rechace a cuarentena en el portal); **rojo** si el portal lo rechaza; **azul** si se guardó sin conexión y se mandará solo.

Sin WiFi o sin servidor, los pesos se guardan en la placa (hasta 400) y se mandan solos cuando vuelve la conexión; el portal confirma cada uno y no los duplica.

La tolerancia (±1 g por defecto), el tiempo de estabilidad, la banda de cero (±0,5 g) y su tiempo (1 s) y los demás ajustes se cambian en el portal, por cliente y por producto; la báscula los recibe sola.

## Modo demo

Para enseñar SolderTrace sin balanza. Solo 4allfix lo enciende, en el portal, para un **cliente demo** (*Settings > Weighing*, apagado por defecto): todas las básculas de ese cliente pasan a balanza emulada, así los pesajes de demo nunca se mezclan con envases reales. La cabecera muestra **DEMO** y los pesajes van marcados como demo. Al pesar, la pantalla tiene botones: **Poner envase** (un peso cerca del esperado), **Fuera de rango** (para probar el ámbar) y **Quitar envase**. El resto es igual que con la balanza real: el portal pide el pesaje, la báscula captura el peso estable y lo manda.

## Actualizar por WiFi (OTA)

En el portal → *Scales* → *Update firmware*. La báscula descarga la versión por https, comprueba que es un firmware de SolderTrace y su MD5, y solo se reinicia si todo está bien. La versión nueva se da por buena cuando conecta con el portal (o tras 10 minutos funcionando con WiFi, o si el operario la reinicia desde el menú); si antes de eso se cuelga o se reinicia, la placa vuelve sola a la versión anterior. Nunca se actualiza a mitad de un pesaje.

## Compilar

```
./compilar.sh
```
Deja los archivos en `binarios/<versión>/` con su `md5.txt`.

## Seguridad

No hay ninguna contraseña ni WiFi escrita en el código. La WiFi del cliente y la llave de la báscula se guardan solo en la memoria de la placa. La conexión con el portal va siempre cifrada (solo `wss://`; las actualizaciones solo por `https://`) con los certificados raíz que trae la placa, y el portal guarda solo una huella de la llave. Si se cambia el servidor, la llave se borra y no se le manda al servidor nuevo.
