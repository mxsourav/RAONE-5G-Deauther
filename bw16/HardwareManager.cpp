#include "HardwareManager.h"
#include "gpio_api.h"

// ─────────────────────────────────────────────────────────────
//  HardwareManager.cpp – LED + Buzzer for RAONE V7.0
//  All 3 LEDs (RED, GREEN, YELLOW) on standard safe Arduino pins
//  RED    = PA15 (D9)
//  GREEN  = PA14 (D10)
//  YELLOW = PA13 (D11)
// ─────────────────────────────────────────────────────────────

static gpio_t _btnOk;

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

  // OK touch sensor on PB_20 — mbed GPIO (TTP223, active HIGH)
  gpio_init(&_btnOk, PB_20);
  gpio_dir(&_btnOk, PIN_INPUT);
  gpio_mode(&_btnOk, PullDown);

  // NAV push button on PB3 — standard Arduino (active LOW with pullup)
  pinMode(BTN_NAV, INPUT_PULLUP);

  ledAllOff();
}

// ── Button primitives ────────────────────────────────────────

// OK button = TTP223 touch sensor on PB_20 (active HIGH)
bool okPressed() {
  return gpio_read(&_btnOk) == 1;
}

// NAV button = Tactile push button on PB3 (active LOW)
bool navPressed() {
  return digitalRead(BTN_NAV) == LOW;
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
// Exact note frequencies and timing from Harry_Potter.ino reference
void buzzerBootMelody() {
  struct MelodyNote {
    uint16_t freq;
    uint16_t dur;
    uint16_t pause;
  };

  static const MelodyNote NOTES[] = {
    { 494, 200, 120 }, // B4
    { 659, 250, 150 }, // E5
    { 784, 100,  80 }, // G5
    { 740, 200, 120 }, // F#5
    { 659, 400, 200 }, // E5
    { 988, 200, 120 }, // B5
    { 880, 400, 200 }, // A5
    { 740, 400, 200 }, // F#5
    { 659, 250, 150 }, // E5
    { 784, 100,  80 }, // G5
    { 740, 200, 120 }, // F#5
    { 622, 400, 200 }, // D#5
    { 698, 200, 120 }, // F5
    { 494, 400, 250 }, // B4
    { 440, 200, 120 }, // A4
    { 494, 250, 200 }, // B4
    // Second phrase
    { 494, 200, 120 }, // B4
    { 659, 250, 150 }, // E5
    { 784, 100,  80 }, // G5
    { 740, 200, 120 }, // F#5
    { 659, 400, 200 }, // E5
    { 988, 200, 120 }, // B5
    { 1175, 400, 200 }, // D6
    { 1109, 200, 120 }, // C#6
    { 1046, 400, 200 }, // C6
    { 880, 200, 120 }, // A5
    { 1046, 250, 150 }, // C6
    { 988, 100,  80 }, // B5
    { 932, 200, 120 }, // A#5
    { 880, 400, 200 }, // A5
    { 784, 200, 120 }, // G5
    { 659, 600, 100 }  // E5
  };

  for (size_t i = 0; i < sizeof(NOTES)/sizeof(NOTES[0]); i++) {
    playTone(NOTES[i].freq, NOTES[i].dur);
    if (NOTES[i].pause > 0) delay(NOTES[i].pause);
  }
}

void buzzerScanDone() {
  playTone(1400, 60);
  delay(20);
  playTone(1400, 60);
}
