#pragma once

#include <Arduino.h>
#include "Config.h"

enum LedMode {
  LED_MODE_OFF,
  LED_MODE_IDLE,
  LED_MODE_SCANNING,
  LED_MODE_TASK_RUNNING,
  LED_MODE_TASK_SUCCESS,
  LED_MODE_TASK_FAIL
};

void hwBegin();
bool okPressed();
bool navPressed();
void ledAllOff();
void ledRedOn();
void ledRedOff();
void ledGreenOn();
void ledGreenOff();
void ledYellowOn();
void ledYellowOff();
void ledFlashRed(uint8_t times, uint16_t ms);
void ledFlashGreen(uint8_t times, uint16_t ms);
void ledFlashYellow(uint8_t times, uint16_t ms);
void ledSetSole(uint8_t led); // 0=All Off, 1=Red, 2=Yellow, 3=Green
void ledStepRGY(uint8_t index); // Exact Red -> Green -> Yellow cycle
void ledMelodySet(uint8_t led);  // 0=All Off, 1=Red, 2=Green, 3=Yellow, 4=All Three (Zero Bleed)
void ledSetOnboardPurple();     // Set onboard RGB to Solid Purple
void ledChaseStep(uint8_t step);
void ledCelebrateSync();
void ledBootSequence();
void setLedMode(LedMode mode);
void ledTaskUpdate();

void playTone(uint16_t freq, uint16_t durationMs);
void buzzerClick();
void buzzerBeep(uint16_t freq, uint16_t ms);
void buzzerError();
void buzzerSuccess();
void buzzerBootMelody();
void buzzerScanDone();

