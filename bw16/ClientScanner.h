#pragma once

#include <Arduino.h>

#define MAX_CLIENTS 16

struct ClientInfo {
  uint8_t mac[6];
  char macStr[18];        // "AA:BB:CC:DD:EE:FF"
  int16_t rssi;
  uint16_t packetCount;
};

void clientScanStart(uint8_t channel, const uint8_t *targetBssid);
bool clientScanPoll();    // returns true when scan is complete
void clientScanStop();
uint8_t clientScanCount();
const ClientInfo &clientScanGet(uint8_t index);
