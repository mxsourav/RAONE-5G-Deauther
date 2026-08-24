#pragma once

#include <Adafruit_SSD1306.h>

// Draw the RAONE boot splash screen.
// Must be called after oled.begin() and before oled.display().
void splashDrawProgress(Adafruit_SSD1306 &oled, uint8_t percent, const char *msg = nullptr);

