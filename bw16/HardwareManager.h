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

