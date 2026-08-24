import sys
import re

content = open("bw16/bw16.ino", encoding="utf-8").read()

old_menus = r"static const char \*const MAIN_MENU_ITEMS\[\] = \{[\s\S]*?static const uint8_t BLE_MENU_COUNT  = sizeof\(BLE_MENU_ITEMS\)  / sizeof\(BLE_MENU_ITEMS\[0\]\);"

new_menus = """static const char *const MAIN_MENU_ITEMS[] = {
  "Networks",
  "Rescan",
  "Beacon Spam",
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
static const uint8_t BLE_MENU_COUNT  = sizeof(BLE_MENU_ITEMS)  / sizeof(BLE_MENU_ITEMS[0]);"""

content = re.sub(old_menus, new_menus, content)

# 2. Setup splash screen and scan
old_setup = r"// ─── Fast 2-Second Boot Animation ─────────────────────────────[\s\S]*?drawMainMenu\(\);\s*\}"

new_setup = """// ─── Fast 6-Second Boot Animation ─────────────────────────────
  struct Tone { uint16_t freq; uint16_t dur; };
  static const Tone splashMelody[] = {
    { 493, 200 }, { 659, 250 }, { 783, 100 }, { 739, 200 },
    { 659, 400 }, { 987, 200 }, { 880, 400 }, { 739, 400 },
    { 659, 250 }, { 783, 100 }, { 739, 200 }, { 622, 400 }
  };
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t progress = (i * 100) / 12;
    uiDrawSplashProgress(progress);
    if (i % 3 == 0) ledRedOn(); else if (i % 3 == 1) { ledRedOff(); ledGreenOn(); } else { ledGreenOff(); ledYellowOn(); }
    playTone(splashMelody[i].freq, splashMelody[i].dur);
    delay(splashMelody[i].dur);
    ledAllOff();
  }
  uiDrawSplashProgress(100);
  delay(100);

  ledAllOff();
  setLedMode(LED_MODE_IDLE); // Red solid = idle

  wifiScannerBegin();
  sniffBegin();
  bleBegin();
  beaconSpamBegin();
  bleSpamBegin();

  uiDrawStatus("Scanning...");
  wifiScannerScan(); // Auto-scan on boot
  drawMainMenu();
}"""
content = re.sub(old_setup, new_setup, content)

# 3. openMainMenuItem changes
old_open_main = r"void openMainMenuItem\(\) \{[\s\S]*?\}\s*\}"
new_open_main = """void openMainMenuItem() {
  switch (mainMenuIndex) {
    case 0: 
      // Networks -> combined list
      selectedNetwork = 0;
      listTop = 0;
      uiState = UI_NETWORK_LIST;
      uiDrawNetworkListAll(selectedNetwork, listTop);
      break;
    case 1:
      // Rescan
      runScan(); 
      break;
    case 2:
      // Beacon Spam
      selectedBand = 2; // Default to 2.4G
      uiState = UI_BAND_MENU; // Use the band menu to pick 2.4G/5G for beacon spam
      uiDrawBandMenu(selectedBand);
      break;
    case 3: 
      drawBleMenu();   
      break;
    case 4:
      uiState = UI_IR_MENU;
      irMenuIndex = 0;
      uiDrawIrMenu(irMenuIndex);
      break;
    case 5: {
      uiState = UI_SYSTEM_INFO;
      uint8_t c24 = wifiScannerCountBand(2);
      uint8_t c5  = wifiScannerCountBand(5);
      uint8_t tot = wifiScannerCount();
      uiDrawSystemInfo(targetHasSelection(), tot, c24, c5);
      break;
    }
  }
}"""
content = re.sub(old_open_main, new_open_main, content)

# 4. Remove openWifiMenuItem, openLabMenuItem, drawWifiMenu, drawLabMenu, etc
content = re.sub(r"void drawWifiMenu\(\) \{[\s\S]*?\}\s*void drawLabMenu\(\) \{[\s\S]*?\}\s*void drawBleMenu", "void drawBleMenu", content)
content = re.sub(r"void openWifiMenuItem\(\) \{[\s\S]*?\}\s*void openLabMenuItem\(\) \{[\s\S]*?\}\s*void openBleMenuItem", "void openBleMenuItem", content)


open("bw16/fix1.ino", "w", encoding="utf-8").write(content)
