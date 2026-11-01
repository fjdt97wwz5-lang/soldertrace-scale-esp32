#!/bin/bash
# Compila el firmware de la bascula SolderTrace y deja los .bin en binarios/<version>/
# Necesita arduino-cli con el core esp32 3.3.2 y las librerias: GFX Library for Arduino 1.6.4,
# WebSockets (Markus Sattler) 2.7.3 y ArduinoJson 7.4.2.
set -e
cd "$(dirname "$0")"
export ARDUINO_DIRECTORIES_DATA=${ARDUINO_DIRECTORIES_DATA:-/tmp/acli/data} ARDUINO_DIRECTORIES_USER=${ARDUINO_DIRECTORIES_USER:-/tmp/acli/user}
CLI=${ARDUINO_CLI:-arduino-cli}
VER=$(grep -oP 'FW_VERSION\s+"\K[^"]+' soldertrace_scale/config.h)
# WEBSOCKETS_TCP_TIMEOUT=2000: cada intento de conexion con el portal bloquea como mucho ~2 s (la libreria trae 5 s)
"$CLI" compile -b "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom" \
  --build-property "compiler.cpp.extra_flags=-DWEBSOCKETS_TCP_TIMEOUT=2000" \
  ${CTAGS:+--build-property "runtime.tools.ctags.path=$CTAGS"} --output-dir build soldertrace_scale
OUT=binarios/$VER
mkdir -p "$OUT"
cp build/soldertrace_scale.ino.bin "$OUT/soldertrace_scale_$VER.bin"
cp build/soldertrace_scale.ino.bootloader.bin build/soldertrace_scale.ino.partitions.bin "$OUT/"
BOOT=$(ls "$ARDUINO_DIRECTORIES_DATA"/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin | head -1)
cp "$BOOT" "$OUT/boot_app0.bin"
# Un solo archivo para grabar en 0x0 (sin rellenar los 16 MB)
ESPTOOL=$(ls "$ARDUINO_DIRECTORIES_DATA"/packages/esp32/tools/esptool_py/*/esptool | head -1)
"$ESPTOOL" --chip esp32s3 merge-bin -o "$OUT/soldertrace_scale_${VER}_completo.bin" --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 "$OUT/soldertrace_scale.ino.bootloader.bin" 0x8000 "$OUT/soldertrace_scale.ino.partitions.bin" 0xe000 "$OUT/boot_app0.bin" 0x10000 "$OUT/soldertrace_scale_$VER.bin"
( cd "$OUT" && md5sum *.bin > md5.txt )
echo "Listo: $OUT"
