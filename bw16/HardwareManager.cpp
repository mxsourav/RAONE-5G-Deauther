#include "HardwareManager.h"
#include "gpio_api.h"

// ─────────────────────────────────────────────────────────────
//  HardwareManager.cpp – LED + Buzzer for RAONE V2.5
//  All 3 LEDs (RED, GREEN, YELLOW) on standard safe Arduino pins
//  RED    = PA15 (D9)
//  GREEN  = PA14 (D10)
//  YELLOW = PA13 (D11)
// ─────────────────────────────────────────────────────────────

static gpio_t _btnNav;

void hwBegin() {
  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // All 3 Status LEDs on standard pins
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  // Pulse all 3 LEDs HIGH for 500ms on startup for visual confirmation
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, HIGH);
  delay(500);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);

  // NAV touch sensor on PB20 — mbed GPIO (TTP223, active HIGH)
  gpio_init(&_btnNav, PB_20);
  gpio_dir(&_btnNav, PIN_INPUT);
  gpio_mode(&_btnNav, PullDown);

  ledAllOff();
}

// ── Button primitives ────────────────────────────────────────

bool navPressed() {
  return gpio_read(&_btnNav) == 1;
}

// ── LED primitives ───────────────────────────────────────────

void ledRedOn()     { digitalWrite(LED_RED, LED_ON_STATE); }
void ledRedOff()    { digitalWrite(LED_RED, LED_OFF_STATE);  }

void ledGreenOn()   { digitalWrite(LED_GREEN, LED_ON_STATE); }
void ledGreenOff()  { digitalWrite(LED_GREEN, LED_OFF_STATE);  }

void ledYellowOn()  { digitalWrite(LED_YELLOW, LED_ON_STATE); }
void ledYellowOff() { digitalWrite(LED_YELLOW, LED_OFF_STATE);  }

void ledAllOff() {
  ledRedOff();
  ledGreenOff();
  ledYellowOff();
}

// ── LED flash patterns ────────────────────────────────────────

void ledFlashRed(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    ledRedOn();   delay(ms);
    ledRedOff();  delay(ms);
  }
}

void ledFlashGreen(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    ledGreenOn();   delay(ms);
    ledGreenOff();  delay(ms);
  }
}

void ledFlashYellow(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    ledYellowOn();   delay(ms);
    ledYellowOff();  delay(ms);
  }
}

void ledBootSequence() {
  for (uint8_t pass = 0; pass < 2; pass++) {
    ledRedOn();   delay(150); ledRedOff();   delay(80);
    ledGreenOn(); delay(150); ledGreenOff(); delay(80);
    ledYellowOn();delay(150); ledYellowOff();delay(80);
  }
}

// ── LED State Machine ─────────────────────────────────────────
static LedMode _currentLedMode = LED_MODE_OFF;
static uint32_t _ledLastAt = 0;
static bool     _ledState  = false;

void setLedMode(LedMode mode) {
  if (_currentLedMode == mode) return;
  _currentLedMode = mode;
  _ledState = false;
  _ledLastAt = millis();
  ledAllOff();

  if (mode == LED_MODE_IDLE) {
    ledRedOn();
  } else if (mode == LED_MODE_TASK_SUCCESS) {
    ledGreenOn();
  }
}

void ledTaskUpdate() {
  if (_currentLedMode == LED_MODE_IDLE ||
      _currentLedMode == LED_MODE_TASK_SUCCESS ||
      _currentLedMode == LED_MODE_OFF) return;

  uint32_t now = millis();
  uint32_t interval = 250;

  if (_currentLedMode == LED_MODE_TASK_RUNNING) {
    interval = 100;
  }

  if (now - _ledLastAt >= interval) {
    _ledLastAt = now;
    _ledState = !_ledState;

    if (_currentLedMode == LED_MODE_SCANNING) {
      if (_ledState) ledYellowOn(); else ledYellowOff();
    } else if (_currentLedMode == LED_MODE_TASK_RUNNING) {
      if (_ledState) ledGreenOn(); else ledGreenOff();
    } else if (_currentLedMode == LED_MODE_TASK_FAIL) {
      if (_ledState) ledRedOn(); else ledRedOff();
    }
  }
}

// ── Buzzer ────────────────────────────────────────────────────
void playTone(uint16_t freq, uint16_t durationMs) {
  if (freq == 0) { delay(durationMs); return; }
  uint32_t periodUs = 1000000UL / freq;
  uint32_t halfPeriodUs = periodUs / 2;
  uint32_t cycles = (durationMs * 1000UL) / periodUs;

  for (uint32_t i = 0; i < cycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(halfPeriodUs);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(halfPeriodUs);
  }
}

void buzzerClick() {
  playTone(3000, 40);
}

void buzzerBeep(uint16_t freq, uint16_t ms) {
  playTone(freq, ms);
  delay(10);
}

void buzzerError() {
  playTone(800, 80);
  delay(20);
  playTone(500, 150);
}

void buzzerSuccess() {
  playTone(1800, 60);
  delay(20);
  playTone(2400, 80);
}

// ── Hedwig's Theme (Harry Potter) ─────────────────────────────
void buzzerBootMelody() {
  // B4 - E5 - G5 - F#5 - E5 - B5 - A5 - (rest) -
  // F#5 - E5 - G5 - F#5 - D#5 - F5 - B4
  playTone(494, 200); delay(30);   // B4
  playTone(659, 300); delay(30);   // E5
  playTone(784, 150); delay(30);   // G5
  playTone(740, 300); delay(30);   // F#5
  playTone(659, 500); delay(30);   // E5
  playTone(988, 300); delay(30);   // B5
  playTone(880, 600); delay(60);   // A5
  playTone(740, 600); delay(60);   // F#5 (hold)

  playTone(659, 300); delay(30);   // E5
  playTone(784, 150); delay(30);   // G5
  playTone(740, 300); delay(30);   // F#5
  playTone(622, 500); delay(30);   // D#5
  playTone(698, 300); delay(30);   // F5
  playTone(494, 800);              // B4 (final hold)
}

void buzzerScanDone() {
  playTone(1400, 60);
  delay(20);
  playTone(1400, 60);
}
