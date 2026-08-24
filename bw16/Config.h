#pragma once

#include <Arduino.h>

#define FIRMWARE_NAME    "RAONE"
#define FIRMWARE_VERSION "V5.0 ULTIMATE"
#define FIRMWARE_BOARD   "BW16 RTL8720DN"

// ─────────────────────────────────────────────────────────────
//  BW16 Clean Safe Pin Map — V2.7
// ─────────────────────────────────────────────────────────────

// ─── OLED I²C (SSD1306 / SH1106 128×64) ──────────────────────
#define OLED_ADDR   0x3C

// ─── Buttons ─────────────────────────────────────────────────
#define BTN_NAV   PB3   // D6 tactile push-button, active LOW (NAV)
// BTN_OK is PB20 TTP223 touch sensor, active HIGH — handled via mbed GPIO in HardwareManager
#define BTN_LONG_PRESS_MS  800

// ─── Status LEDs (Standard Arduino Pins) ──────────────────────
#define LED_RED    PA15  // D9  — Standard output pin
#define LED_GREEN  PA14  // D10 — Standard output pin
#define LED_YELLOW PA13  // D11 — Standard output pin

// If LEDs are wired Cathode to GPIO (Active LOW), set to 1. Default 0 (Active HIGH).
#define LED_ACTIVE_LOW 0

#if LED_ACTIVE_LOW
  #define LED_ON_STATE  LOW
  #define LED_OFF_STATE HIGH
#else
  #define LED_ON_STATE  HIGH
  #define LED_OFF_STATE LOW
#endif

// ─── Passive buzzer ──────────────────────────────────────────
#define BUZZER_PIN PB1   // D4  — Software PWM

// ─── IR blaster ──────────────────────────────────────────────
#define IR_TX_PIN  PB2   // D5  — Direct GPIO drive
