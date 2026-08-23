#pragma once

#include <Arduino.h>
#include "Config.h"

// ─────────────────────────────────────────────────────────────
//  IrBlaster — NEC-protocol IR transmitter for RAONE
//  Bit-banged at 38 kHz carrier using IR_TX_PIN.
//  Codes are stored in PROGMEM to save RAM.
// ─────────────────────────────────────────────────────────────

// Number of built-in IR codes
#define IR_CODE_COUNT  10

// Initialise IR pin
void irBegin();

// Transmit code at given index (0-based)
void irTransmit(uint8_t index);

// Return human-readable name string (from PROGMEM)
const char* irCodeName(uint8_t index);

// Return total number of codes
uint8_t irCodeCount();
