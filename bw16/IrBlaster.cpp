#include "IrBlaster.h"

// ─────────────────────────────────────────────────────────────
//  IrBlaster.cpp – NEC-protocol IR transmission for RAONE
//
//  NEC frame format:
//    9ms  burst  →  4.5ms space  →  32 bits (LSB first)
//    Logical '1': 562µs burst + 1687µs space
//    Logical '0': 562µs burst +  562µs space
//    End:         562µs burst
//
//  Carrier: 38 kHz  (period ≈ 26.3 µs: 13 µs HIGH + 13 µs LOW)
// ─────────────────────────────────────────────────────────────

// ─── NEC 32-bit codes: [address_byte, ~address, command, ~command]
// Stored as uint32_t in PROGMEM
struct IrCode {
  uint32_t    raw;           // full 32-bit NEC frame
  const char *name;          // human-readable label
};

// NEC raw code packing helper: addr, ~addr, cmd, ~cmd
#define NEC(a, c) ( ((uint32_t)(a)) | (((uint32_t)(~(a)&0xFF))<<8) | \
                    (((uint32_t)(c))<<16) | (((uint32_t)(~(c)&0xFF))<<24) )

// ─── Code table ────────────────────────────────────────────────
// Sources: public IR database / LIRC / OpenHAB community
static const IrCode IR_CODES[IR_CODE_COUNT] PROGMEM = {
  // Name                     Addr  Cmd
  { NEC(0x07, 0x02), "Samsung TV Power"   },  // Samsung BN59 remote
  { NEC(0x07, 0x07), "Samsung TV Vol+"    },
  { NEC(0x07, 0x0B), "Samsung TV Vol-"    },
  { NEC(0x04, 0x08), "LG TV Power"        },  // LG AKB remotes
  { NEC(0x04, 0x02), "LG TV Vol+"         },
  { NEC(0x04, 0x03), "LG TV Vol-"         },
  { NEC(0x10, 0x01), "Daikin AC Power"    },  // Generic Daikin
  { NEC(0x10, 0x18), "AC Cool 24C"        },  // Daikin cool 24°C
  { NEC(0x28, 0x3A), "Fan On/Off"         },  // Generic ceiling fan
  { NEC(0x83, 0x02), "Projector Power"    },  // BenQ projector
};

// ─── Low-level carrier + bit helpers ───────────────────────────

static const uint8_t _pin = IR_TX_PIN;

// Emit 38 kHz carrier for given microseconds
static void irMark(uint16_t us) {
  uint32_t end = micros() + us;
  while ((int32_t)(end - micros()) > 0) {
    digitalWrite(_pin, HIGH);
    delayMicroseconds(13);
    digitalWrite(_pin, LOW);
    delayMicroseconds(13);
  }
}

// Silence for given microseconds
static void irSpace(uint16_t us) {
  digitalWrite(_pin, LOW);
  delayMicroseconds(us);
}

// Send one NEC 32-bit frame (LSB first)
static void irSendNec(uint32_t code) {
  // AGC leader
  irMark(9000);
  irSpace(4500);

  // 32 data bits, LSB first
  for (uint8_t i = 0; i < 32; i++) {
    irMark(562);
    if (code & 1UL) irSpace(1687);
    else            irSpace(562);
    code >>= 1;
  }

  // Stop bit
  irMark(562);
  irSpace(562);
  digitalWrite(_pin, LOW);
}

// ─── Public API ─────────────────────────────────────────────────

void irBegin() {
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);
}

void irTransmit(uint8_t index) {
  if (index >= IR_CODE_COUNT) return;
  uint32_t raw = pgm_read_dword(&IR_CODES[index].raw);
  irSendNec(raw);
}

const char* irCodeName(uint8_t index) {
  if (index >= IR_CODE_COUNT) return "---";
  // Name pointer is itself stored in PROGMEM (points to flash string)
  return IR_CODES[index].name;
}

uint8_t irCodeCount() {
  return IR_CODE_COUNT;
}
