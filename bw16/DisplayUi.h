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
void uiRunGlitchBootLock();
void uiDrawSplashProgress(uint8_t percent, const char *msg = nullptr);
void uiDrawInitialLogo();

// Generic building blocks
void uiDrawStatus(const char *message);
void uiDrawTxCounter(uint32_t packetCount);
void uiDrawMenu(const char *title, const char *const items[],
                uint8_t itemCount, uint8_t selected, const char *footer);
void uiTickMenuAnimation();   // no-op on OLED (kept for API compat)

// WiFi screens
void uiDrawHome();
void uiDrawScanCountdown(uint8_t remainingSeconds, uint32_t elapsedMs, uint8_t foundCount, uint8_t animFrame);
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
void uiDrawDeauthScreen(const char *ssid, uint8_t channel, bool is5g, uint32_t packetCount);
void uiRefreshDeauthCounter(uint32_t packetCount);
void uiRefreshDeauthLive(uint8_t currentChannel, uint32_t totalSent, uint32_t totalFail, uint16_t pps, bool isHopping, const char *ssid = nullptr);
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

// Slave Mode screen
void uiDrawSlaveLinked(const char *masterInfo = "TetraX ESP32");
void uiRefreshSlaveStatus(const char *cmd, uint32_t count);

// System Settings screen
void uiDrawSystemSettings(uint8_t selected, bool buzzerOn, bool ledOn);


