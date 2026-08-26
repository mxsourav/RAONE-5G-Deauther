#include "SplashScreen.h"
#include "Theme.h"
#include <Arduino.h>
#include "DisplayUi.h"

// ─────────────────────────────────────────────────────────────
//  RAONE boot splash  –  128 × 64 OLED
//
//  Layout:
//    Row 0-10   ─ inverted banner  "R A O N E"
//    Row 12-20  ─ separator line
//    Row 22-30  ─ subtitle "BW16  2.4G / 5GHz"
//    Row 33-40  ─ chip "RTL8720DN"
//    Row 46-54  ─ progress bar animation (driven by caller)
//    Row 56-63  ─ version "v1.0"
// ─────────────────────────────────────────────────────────────

void splashDrawProgress(Adafruit_SSD1306 &oled, uint8_t percent, const char *msg) {
  oled.clearDisplay();

  // ── Top Title ────────────────────────────────────────────────
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  const char *title = "RAONE  2.4G / 5GHz";
  int16_t tx = (OLED_W - (int16_t)(strlen(title) * 6)) / 2;
  oled.setCursor(tx, 3);
  oled.print(title);

  // ── Boxed "dev/mx_sourav" ────────────────────────────────────
  const char *dev = "dev/mx_sourav";
  int16_t devW = strlen(dev) * 6;
  int16_t devX = (OLED_W - devW) / 2;
  int16_t devY = 17;
  oled.drawRect(devX - 4, devY - 3, devW + 8, 14, SSD1306_WHITE);
  oled.setCursor(devX, devY);
  oled.print(dev);

  // ── Animated Boot Status Message ─────────────────────────────
  if (msg && msg[0]) {
    int16_t msgW = strlen(msg) * 6;
    int16_t msgX = max((int16_t)2, (int16_t)((OLED_W - msgW) / 2));
    oled.setCursor(msgX, 35);
    oled.print(msg);
  }

  // ── Progress bar outline & fill ──────────────────────────────
  int16_t barY = 49;
  oled.drawRect(UI_PAD, barY, OLED_W - UI_PAD * 2, 8, SSD1306_WHITE);

  if (percent > 100) percent = 100;
  int16_t fillW = ((OLED_W - UI_PAD * 2 - 2) * percent) / 100;
  if (fillW > 0) {
    oled.fillRect(UI_PAD + 1, barY + 1, fillW, 6, SSD1306_WHITE);
  }

  oledFlush();
}

void splashDrawInitialScreen(Adafruit_SSD1306 &oled) {
  // Completely removed pre-boot screen
}

