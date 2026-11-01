# Versiones del firmware de la báscula SolderTrace

Cada versión queda guardada para siempre en su propia rama `version/…` y sus archivos en `binarios/<versión>/`. Nunca se sobrescribe una versión: cada cambio es una versión nueva. La versión de cada báscula se ve en su pantalla (menú → Info) y en el portal (Scales).

| Versión | Qué cambió |
|---|---|
| st-2026.11.01a | Primera versión. Balanza Rhuisan por RS-232, pantalla táctil en 4 idiomas (inglés, español, francés, portugués de Brasil), WiFi del cliente configurable desde el móvil (red SolderTrace-XXXX), espera de hora al arrancar (internet o DS3231), alta segura con código de 6 dígitos y número B1/B2 en pantalla, activar/desactivar y pedir activación, pesaje pedido desde el portal con captura automática al estar estable, resultado verde/rojo/ámbar, cola sin conexión con confirmación del servidor, actualización por WiFi con vuelta atrás automática. Requiere el portal con el módulo de pesaje. Se graba una vez por cable. |

## Volver a una versión anterior
En el portal → Scales → Update firmware, sube el `.bin` de la versión que quieras (están en `binarios/`). O por cable, como dice el README.
