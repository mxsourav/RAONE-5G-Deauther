import sys
import re

content = open("bw16/DisplayUi.cpp", encoding="utf-8").read()

new_func = """void uiDrawNetworkListAll(int selectedNetwork, int listTop) {
  oledClear();
  char header[20];
  snprintf(header, sizeof(header), "ALL NETWORKS");
  drawStatusBar(header);
  oled.setTextSize(1);

  uint8_t total = wifiScannerCount();
  if (total == 0) {
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 8);
    oled.print("No networks found");
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 18);
    oled.print("OK to scan");
    drawFooter("OK=scan  Long OK=back");
    oledFlush();
    return;
  }

  uint8_t row = 0;
  for (uint8_t i = listTop; i < wifiScannerCount() && row < UI_MENU_VISIBLE; i++) {
    const NetworkInfo &n = wifiScannerNetwork(i);
    int16_t ry  = UI_CONTENT_Y + row * UI_MENU_ROW_H;
    bool    sel = ((int)i == selectedNetwork);

    if (sel) {
      oled.fillRect(0, ry, OLED_W - 18, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }

    oled.setCursor(UI_PAD, ry + 1);
    
    char prefix[6];
    if (wifiScannerIs5GHz(n.channel)) strcpy(prefix, "[5G] ");
    else                              strcpy(prefix, "[2G] ");
    
    char ssidBuf[14];
    strncpy(ssidBuf, (n.ssid[0] ? n.ssid : "<hidden>"), 8);
    ssidBuf[8] = '\\0';
    
    char rowBuf[24];
    snprintf(rowBuf, sizeof(rowBuf), "%s%s", prefix, ssidBuf);
    
    oled.print(rowBuf);

    oled.setTextColor(SSD1306_WHITE);
    drawRssiBar(OLED_W - 20, ry + 1, n.rssi);
    row++;
  }
  drawScrollbar(row, total, listTop);
  drawFooter("NAV=scroll  OK=actions");
  oledFlush();
}
"""

content = content.replace("void uiDrawNetworkList(", new_func + "\nvoid uiDrawNetworkList(")

# Fix target selection from targetHasSelection to the footer
# Wait, the prompt says "Update footer text references", we just did that (NAV=scroll OK=actions).
# Update action menu to show Deauth / Scan Clients / Sniffer / Clone+Beacon / Back
old_action_menu = r"static const char \*actionItems\[\] = \{\"Set Target\", \"Deauth Broad.\", \"Scan Clients\", \"Clone SSID\", \"Back\"\};"
new_action_menu = 'static const char *actionItems[] = {"Deauth", "Scan Clients", "Sniffer", "Clone+Beacon", "Back"};'
content = re.sub(old_action_menu, new_action_menu, content)

open("bw16/DisplayUi.cpp", "w", encoding="utf-8").write(content)

# Now DisplayUi.h
header_content = open("bw16/DisplayUi.h", encoding="utf-8").read()
header_content = header_content.replace("void uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop);", "void uiDrawNetworkListAll(int selectedNetwork, int listTop);\nvoid uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop);")
open("bw16/DisplayUi.h", "w", encoding="utf-8").write(header_content)
