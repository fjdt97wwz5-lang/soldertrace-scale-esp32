#include "pin.h"
#include "config.h"
#include "store.h"
#include <mbedtls/sha256.h>

static int fails = 0;
static unsigned long lockedAt = 0;
static bool locked = false;

bool pinRequired() { return strchr(cfg.pinHash, '$') != nullptr; }

unsigned long pinLockedFor() {
  if (!locked) return 0;
  unsigned long e = millis() - lockedAt;
  if (e >= PIN_LOCK_MS) { locked = false; fails = 0; return 0; }
  return PIN_LOCK_MS - e;
}

int pinTriesLeft() { return PIN_MAX_TRIES - fails; }

static bool matches(const char* pin) {
  const char* dollar = strchr(cfg.pinHash, '$');
  if (!dollar) return true;
  size_t saltLen = dollar - cfg.pinHash;
  char buf[112];
  if (saltLen + strlen(pin) >= sizeof(buf)) return false;
  memcpy(buf, cfg.pinHash, saltLen);
  strcpy(buf + saltLen, pin);
  uint8_t out[32];
  if (mbedtls_sha256((const unsigned char*)buf, strlen(buf), out, 0) != 0) return false;
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", out[i]);
  const char* want = dollar + 1;
  if (strlen(want) != 64) return false;
  uint8_t diff = 0;                     // comparacion sin atajos
  for (int i = 0; i < 64; i++) diff |= (uint8_t)(tolower((unsigned char)want[i]) ^ hex[i]);
  return diff == 0;
}

PinResult pinCheck(const char* pin) {
  if (pinLockedFor()) return PIN_LOCKED;
  if (strlen(pin) == 4 && matches(pin)) { fails = 0; return PIN_OK; }
  if (++fails >= PIN_MAX_TRIES) { locked = true; lockedAt = millis(); return PIN_LOCKED; }
  return PIN_WRONG;
}
