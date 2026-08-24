#pragma once

#include <Arduino.h>
#include "LabTestEngine.h"
#include "LabStats.h"
#include "WifiScanner.h"
#include "PromiscuousSniffer.h"
#include "BluetoothScanner.h"
#include "BeaconSpam.h"
#include "BleSpam.h"

// ─────────────────────────────────────────────────────────────
//  DisplayUi – RAONE OLED 128×64 UI API
// ─────────────────────────────────────────────────────────────

void uiBegin();
void oledFlush();
void uiDrawSplashProgress(uint8_t percent);

// Generic building blocks
void uiDrawStatus(const char *message);
void uiDrawTxCounter(uint32_t packetCount);
void uiDrawMenu(const char *title, const char *const items[],
                uint8_t itemCount, uint8_t selected, const char *footer);
void uiTickMenuAnimation();   // no-op on OLED (kept for API compat)

// WiFi screens
void uiDrawHome();
void uiDrawBandMenu(uint8_t selectedBand);
void uiDrawRadarBandMenu(uint8_t selectedBand);
void uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop);
void uiDrawNetworkListAll(int selectedNetwork, int listTop);
void uiDrawNetworkDetails(const NetworkInfo &network, bool saved);
void uiDrawTargetDetails(const NetworkInfo &network);
void uiDrawAnalyzer(uint8_t band);
void uiDrawWifiRadar(const NetworkInfo *network, bool found);
void uiTickWifiRadar();       // animation tick
void uiDrawSystemInfo(bool hasTarget, uint8_t scanCount,
                      uint8_t count24, uint8_t count5);

// Lab / Deauth screens
void uiDrawLabPrecheck(bool hasTarget, const NetworkInfo *network);
void uiDrawTargetMonitor(const NetworkInfo &network, bool found);
void uiDrawLabStats(const LabStats &stats);
void uiDrawPrincipalTest(const LabTestReport &report);

// BLE screens
void uiDrawBleList(int selected, int listTop);
void uiDrawBleDetails(const BleDeviceInfo &dev);
void uiRefreshBleList(int selected, int listTop);
void uiDrawBleAnalyzer();
void uiRefreshBleAnalyzer();

// Sniffer / Analyzer screens
void uiDrawWifiAnalyzer();
void uiRefreshWifiAnalyzer();
void uiDrawSniffer(const SnifferStats &stats);
void uiRefreshSniffer(const SnifferStats &stats);

// Spam screens
void uiDrawBeaconSpam();
void uiRefreshBeaconSpam();
void uiDrawBleSpam();
void uiRefreshBleSpam();

// IR Remote screen
void uiDrawIrMenu(uint8_t selectedIndex);
void uiRefreshIrMenu(uint8_t selectedIndex);




// Action menu and Client list
void uiDrawClientList(const char *macs[], uint8_t selected, uint8_t listTop, uint8_t total);
void uiDrawActionMenu(const NetworkInfo &network, uint8_t selected);
void uiDrawGenericMessage(const char *title, const char *msg1, const char *msg2);


