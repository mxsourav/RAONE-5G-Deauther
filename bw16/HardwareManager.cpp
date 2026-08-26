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

  // Set onboard RGB LED permanently to Solid PURPLE (Red PA12 HIGH + Blue PA13 HIGH, Green PA14 LOW)
  ledSetOnboardPurple();

  // OK touch sensor on PB_20 — mbed GPIO (TTP223, active HIGH)
  gpio_init(&_btnOk, PB_20);
  gpio_dir(&_btnOk, PIN_INPUT);
  gpio_mode(&_btnOk, PullDown);

  // NAV push button on PB3 — standard Arduino (active LOW with pullup)
  pinMode(BTN_NAV, INPUT_PULLUP);
}

void ledSetOnboardPurple() {
  pinMode(PA14, OUTPUT);
  digitalWrite(PA14, LOW);   // Green: 0 (from #7b00ff)
  analogWrite(PA12, 123);    // Red: 123 (from #7b00ff)
  analogWrite(PA13, 255);    // Blue: 255 (from #7b00ff)
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

void ledRedOn()     { pinMode(PA15, OUTPUT); digitalWrite(PA15, HIGH); }
void ledRedOff()    { pinMode(PA15, OUTPUT); digitalWrite(PA15, LOW); }

void ledGreenOn()   { pinMode(PA14, OUTPUT); digitalWrite(PA14, HIGH); }
void ledGreenOff()  { pinMode(PA14, OUTPUT); digitalWrite(PA14, LOW); }

void ledYellowOn()  { pinMode(PA13, OUTPUT); digitalWrite(PA13, HIGH); }
void ledYellowOff() { pinMode(PA13, OUTPUT); digitalWrite(PA13, LOW); }

void ledAllOff() {
  pinMode(PA15, OUTPUT);
  digitalWrite(PA15, LOW); // External Red OFF
  ledSetOnboardPurple();   // Restores Solid Purple (#7b00ff) on onboard RGB
}

// ── LED flash patterns ────────────────────────────────────────

void ledFlashRed(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    pinMode(PA15, OUTPUT);
    digitalWrite(PA15, HIGH); delay(ms);
    digitalWrite(PA15, LOW);  delay(ms);
  }
  ledSetOnboardPurple();
}

void ledFlashGreen(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    pinMode(PA14, OUTPUT);
    digitalWrite(PA14, HIGH); delay(ms);
    digitalWrite(PA14, LOW);  delay(ms);
  }
  ledSetOnboardPurple();
}

void ledFlashYellow(uint8_t times, uint16_t ms) {
  for (uint8_t i = 0; i < times; i++) {
    pinMode(PA13, OUTPUT);
    digitalWrite(PA13, HIGH); delay(ms);
    digitalWrite(PA13, LOW);  delay(ms);
  }
  ledSetOnboardPurple();
}

void ledSetSole(uint8_t led) {
  switch (led) {
    case 1: ledRedOn();  ledYellowOff(); ledGreenOff(); break;
    case 2: ledRedOff(); ledYellowOn();  ledGreenOff(); break;
    case 3: ledRedOff(); ledYellowOff(); ledGreenOn();  break;
    default: ledAllOff(); break;
  }
}

// Discrete Melody LED controller with zero bleed during note rests
void ledMelodySet(uint8_t led) {
  if (!g_ledEnabled) { // System setting: LEDs disabled
    digitalWrite(PA15, LOW);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, LOW);
    return;
  }
  pinMode(PA15, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  if (led == 1) {        // RED ONLY (PA15)
    digitalWrite(PA15, HIGH);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, LOW);
  } else if (led == 2) { // GREEN ONLY (PA14)
    digitalWrite(PA15, LOW);
    digitalWrite(PA14, HIGH);
    digitalWrite(PA13, LOW);
  } else if (led == 3) { // YELLOW ONLY (PA13)
    digitalWrite(PA15, LOW);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, HIGH);
  } else if (led == 4) { // ALL THREE TOGETHER (PA15 + PA14 + PA13)
    digitalWrite(PA15, HIGH);
    digitalWrite(PA14, HIGH);
    digitalWrite(PA13, HIGH);
  } else {               // ZERO / REST (Strictly OFF, zero color bleed!)
    digitalWrite(PA15, LOW);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, LOW);
  }
}

// Strict cycle: RED(1) -> GREEN(2) -> YELLOW(3) -> RED(1) -> GREEN(2) -> YELLOW(3) ...
void ledStepRGY(uint8_t index) {
  static const uint8_t RGY[] = { 1, 2, 3 }; // 1=Red, 2=Green, 3=Yellow
  ledMelodySet(RGY[index % 3]);
}

void ledChaseStep(uint8_t step) {
  ledStepRGY(step);
}

void ledCelebrateSync() {
  for (uint8_t i = 0; i < 3; i++) {
    ledMelodySet(4); // All three LEDs ON
    playTone(2800, 60);
    ledMelodySet(0); // All three LEDs OFF
    delay(60);
  }
  ledSetOnboardPurple();
}

void ledBootSequence() {
  for (uint8_t i = 0; i < 6; i++) {
    ledStepRGY(i);
    delay(60);
  }
  ledCelebrateSync();
}

// ── LED State Machine ─────────────────────────────────────────
static LedMode _currentLedMode = LED_MODE_OFF;

void setLedMode(LedMode mode) {
  _currentLedMode = mode;
  if (mode == LED_MODE_IDLE || mode == LED_MODE_OFF) {
    ledSetOnboardPurple();
  }
}

void ledTaskUpdate() {
  // Idle state preserves solid purple with zero flickering
  if (_currentLedMode == LED_MODE_IDLE || _currentLedMode == LED_MODE_OFF) {
    ledSetOnboardPurple();
  }
}
// ── System Settings Globals ──────────────────────────────────
bool g_buzzerEnabled = true;
bool g_ledEnabled    = true;

// ── Buzzer ────────────────────────────────────────────────────
void playTone(uint16_t freq, uint16_t durationMs) {
  if (!g_buzzerEnabled) { delay(durationMs); return; }
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
  if (!g_buzzerEnabled) return;
  playTone(3000, 40);
}

void buzzerBeep(uint16_t freq, uint16_t ms) {
  if (!g_buzzerEnabled) return;
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
