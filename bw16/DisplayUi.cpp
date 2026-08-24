#include "DisplayUi.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Theme.h"
#include "HardwareManager.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#include "Config.h"
#include "SplashScreen.h"
#include "Theme.h"
#include "IrBlaster.h"

// ─────────────────────────────────────────────────────────────
//  OLED instance  (128×64, reset pin -1 = share Arduino reset)
// ─────────────────────────────────────────────────────────────
static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);

// ─────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────

static void oledClear() {
  oled.clearDisplay();
}

void oledFlush() {
  uint8_t *buffer = oled.getBuffer();
  for (uint8_t page = 0; page < 8; page++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(0xB0 + page); // Set page
    Wire.write(0x02);        // Lower col addr (offset 2 for SH1106 compatibility)
    Wire.write(0x10);        // Higher col addr
    Wire.endTransmission();
    
    for (uint8_t c = 0; c < 128; c += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40); // data
      for (uint8_t i = 0; i < 16; i++) {
        Wire.write(buffer[page * 128 + c + i]);
      }
      Wire.endTransmission();
    }
  }
}

// Draw text at pixel (x, y) – y is top of character
static void txt(int16_t x, int16_t y, const char *s, bool inv = false) {
  oled.setTextColor(inv ? SSD1306_BLACK : SSD1306_WHITE);
  oled.setCursor(x, y);
  oled.print(s);
}

// Draw text formatted with snprintf
static void txtf(int16_t x, int16_t y, const char *fmt, ...) {
  char buf[40];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  txt(x, y, buf);
}

// Horizontal separator line
static void hline(int16_t y) {
  oled.drawFastHLine(0, y, OLED_W, SSD1306_WHITE);
}

// Draw the standard inverted status bar with title and optional right text
static void drawStatusBar(const char *title, const char *right = nullptr) {
  oled.fillRect(0, UI_STATUSBAR_Y, OLED_W, UI_STATUSBAR_H, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(UI_PAD, UI_STATUSBAR_Y + 2);
  oled.print(title);
  if (right) {
    int16_t rw = strlen(right) * 6;
    oled.setCursor(OLED_W - rw - UI_PAD, UI_STATUSBAR_Y + 2);
    oled.print(right);
  }
  oled.setTextColor(SSD1306_WHITE);
}

// Draw footer hint (bottom of screen, dimmed by being smaller area)
static void drawFooter(const char *hint) {
  oled.fillRect(0, UI_FOOTER_Y, OLED_W, UI_FOOTER_H, SSD1306_BLACK);
  oled.drawFastHLine(0, UI_FOOTER_Y, OLED_W, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_FOOTER_Y + 1);
  oled.print(hint);
}

// Print a text string truncated to maxChars, appending '.' if truncated
static void printTruncated(const char *s, uint8_t maxChars) {
  if (!s || s[0] == '\0') {
    oled.print("<hidden>");
    return;
  }
  char buf[32];
  uint8_t len = strlen(s);
  if (len <= maxChars) {
    oled.print(s);
  } else {
    strncpy(buf, s, maxChars - 1);
    buf[maxChars - 1] = '.';
    buf[maxChars]     = '\0';
    oled.print(buf);
  }
}

// RSSI as a 4-char bar string e.g. "[###.]"
static void drawRssiBar(int16_t x, int16_t y, int32_t rssi) {
  int level = constrain(map(rssi, -90, -40, 0, 4), 0, 4);
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (8 - bh);
    if (i < level) oled.fillRect(bx, by, 3, bh, SSD1306_WHITE);
    else           oled.drawRect(bx, by, 3, bh, SSD1306_WHITE);
  }
}

// Security abbreviation
static const char *secShort(uint32_t sec) {
  switch (sec) {
    case RTW_SECURITY_OPEN:           return "OPEN";
    case RTW_SECURITY_WEP_PSK:        return "WEP";
    case RTW_SECURITY_WPA3_AES_PSK:
    case RTW_SECURITY_WPA2_WPA3_MIXED:
    case RTW_SECURITY_WPA2_AES_CMAC:  return "WPA3";
    case RTW_SECURITY_WPA_TKIP_PSK:
    case RTW_SECURITY_WPA_AES_PSK:    return "WPA";
    default:                          return "WPA2";
  }
}

// ─────────────────────────────────────────────────────────────
//  Boot & init
// ─────────────────────────────────────────────────────────────

void uiBegin() {
  // Wire.begin(); // Removed to avoid AmebaD core bug (calling it twice crashes)
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // Flash RED LED if OLED fails
    while (true) {
      ledRedOn();
      delay(100);
      ledRedOff();
      delay(900);
    }
  }
  oled.setTextSize(1);
  oled.setTextWrap(false);
  oled.clearDisplay();
  oledFlush();
}

void uiDrawSplashProgress(uint8_t percent) {
  splashDrawProgress(oled, percent);
}

// ─────────────────────────────────────────────────────────────
//  Generic screens
// ─────────────────────────────────────────────────────────────

void uiDrawStatus(const char *message) {
  // Clear content area and print centered status message
  oled.fillRect(0, UI_CONTENT_Y, OLED_W, UI_CONTENT_H + UI_FOOTER_H, SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  int16_t sw = strlen(message) * 6;
  int16_t sx = max((int16_t)UI_PAD, (int16_t)((OLED_W - sw) / 2));
  oled.setCursor(sx, UI_CONTENT_Y + 10);
  oled.print(message);
  oledFlush();
}

void uiDrawTxCounter(uint32_t packetCount) {
  // Refresh TX counter area only (row 3 of content area)
  oled.fillRect(0, UI_CONTENT_Y + 20, OLED_W, 12, SSD1306_BLACK);
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)packetCount);
  int16_t sw = strlen(buf) * 12;
  oled.setCursor((OLED_W - sw) / 2, UI_CONTENT_Y + 20);
  oled.print(buf);
  oled.setTextSize(1);
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Menu – generic scrollable list (4 visible items)
// ─────────────────────────────────────────────────────────────

void uiDrawMenu(const char *title, const char *const items[],
                uint8_t itemCount, uint8_t selected, const char *footer) {
  oledClear();
  drawStatusBar(title);

  oled.setTextSize(1);

  // Calculate scroll window so selected item is always visible
  int8_t scrollTop = selected - (UI_MENU_VISIBLE - 1);
  if (scrollTop < 0) scrollTop = 0;
  if ((uint8_t)scrollTop + UI_MENU_VISIBLE > itemCount)
    scrollTop = (int8_t)itemCount - UI_MENU_VISIBLE;
  if (scrollTop < 0) scrollTop = 0;

  for (uint8_t v = 0; v < UI_MENU_VISIBLE && (scrollTop + v) < itemCount; v++) {
    uint8_t idx = scrollTop + v;
    int16_t ry  = UI_CONTENT_Y + v * UI_MENU_ROW_H;
    bool isSel  = (idx == selected);

    if (isSel) {
      oled.fillRect(0, ry, OLED_W, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setCursor(UI_PAD, ry + 1);
      oled.print("> ");
    } else {
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(UI_PAD + 12, ry + 1);
    }
    printTruncated(items[idx], 19);
  }

  // Scroll indicator dots on right edge
  if (itemCount > UI_MENU_VISIBLE) {
    for (uint8_t i = 0; i < itemCount; i++) {
      int16_t dy = UI_CONTENT_Y + i * (UI_CONTENT_H / itemCount);
      if (i == selected) oled.fillCircle(OLED_W - 2, dy + 3, 1, SSD1306_WHITE);
      else               oled.drawPixel(OLED_W - 2,  dy + 3, SSD1306_WHITE);
    }
  }

  if (footer) drawFooter(footer);
  oledFlush();
}

void uiTickMenuAnimation() {
  // OLED is static – no animation needed; kept for API compat
}

// ─────────────────────────────────────────────────────────────
//  Band selector
// ─────────────────────────────────────────────────────────────

void uiDrawBandMenu(uint8_t selectedBand) {
  oledClear();
  drawStatusBar("SELECT BAND");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  const char *opts[2] = { "2.4 GHz", "5 GHz" };
  uint8_t sel = (selectedBand == 5) ? 1 : 0;

  for (uint8_t i = 0; i < 2; i++) {
    int16_t ry = UI_CONTENT_Y + i * 14;
    if (i == sel) {
      oled.fillRect(0, ry, OLED_W, 12, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD + 6, ry + 2);
    oled.print(opts[i]);
  }

  drawFooter("NAV=switch  OK=select");
  oledFlush();
}

void uiDrawRadarBandMenu(uint8_t selectedBand) {
  oledClear();
  drawStatusBar("RADAR BAND");
  oled.setTextSize(1);

  const char *opts[2] = { "2.4 GHz", "5 GHz" };
  uint8_t sel = (selectedBand == 5) ? 1 : 0;

  for (uint8_t i = 0; i < 2; i++) {
    int16_t ry = UI_CONTENT_Y + i * 14;
    if (i == sel) {
      oled.fillRect(0, ry, OLED_W, 12, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD + 6, ry + 2);
    oled.print(opts[i]);
  }

  drawFooter("NAV=switch  OK=select");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Network list  (scrollable, 4 rows)
// ─────────────────────────────────────────────────────────────

void uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop) {
  oledClear();
  char header[20];
  snprintf(header, sizeof(header), "NETWORKS %uGHz",
           selectedBand == 5 ? 5 : 2);
  drawStatusBar(header);
  oled.setTextSize(1);

  // Count networks in band
  uint8_t total = wifiScannerCountBand(selectedBand);
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

  // List networks visible from listTop, 4 rows
  uint8_t row = 0;
  for (uint8_t i = listTop; i < wifiScannerCount() && row < UI_MENU_VISIBLE; i++) {
    if (!wifiScannerNetworkInBand(i, selectedBand)) continue;
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
    // SSID truncated to 13 chars
    char ssidBuf[14];
    strncpy(ssidBuf, (n.ssid[0] ? n.ssid : "<hidden>"), 13);
    ssidBuf[13] = '\0';
    oled.print(ssidBuf);

    // RSSI bars on the right
    oled.setTextColor(SSD1306_WHITE);
    drawRssiBar(OLED_W - 20, ry + 1, n.rssi);

    row++;
  }

  // "Back" as last item
  if (row < UI_MENU_VISIBLE) {
    int16_t ry  = UI_CONTENT_Y + row * UI_MENU_ROW_H;
    bool    sel = (selectedNetwork == -1);
    if (sel) {
      oled.fillRect(0, ry, OLED_W, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD, ry + 1);
    oled.print("[ Back ]");
  }

  drawFooter("NAV=next  OK=select");
  oledFlush();
}

void uiDrawNetworkListAll(int selectedNetwork, int listTop) {
  oledClear();
  char header[20];
  uint8_t total = wifiScannerCount();
  snprintf(header, sizeof(header), "ALL APs (%u)", total);
  drawStatusBar(header);
  oled.setTextSize(1);

  if (total == 0) {
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 8);
    oled.print("No networks found");
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 18);
    oled.print("OK to re-scan");
    drawFooter("OK=scan  Hold=back");
    oledFlush();
    return;
  }

  uint8_t itemCount = total + 1; // +1 for [ Back ]

  for (uint8_t i = 0; i < UI_MENU_VISIBLE && (listTop + i) < itemCount; i++) {
    uint8_t idx = listTop + i;
    int16_t ry  = UI_CONTENT_Y + i * UI_MENU_ROW_H;
    bool    sel = (idx == selectedNetwork);

    if (sel) {
      oled.fillRect(0, ry, OLED_W - 18, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }

    oled.setCursor(UI_PAD, ry + 1);

    if (idx < total) {
      const NetworkInfo &n = wifiScannerNetwork(idx);
      bool is5G = wifiScannerIs5GHz(n.channel);
      oled.print(is5G ? "5G " : "2G ");

      char ssidBuf[13];
      strncpy(ssidBuf, (n.ssid[0] ? n.ssid : "<hidden>"), 12);
      ssidBuf[12] = '\0';
      oled.print(ssidBuf);

      oled.setTextColor(SSD1306_WHITE);
      drawRssiBar(OLED_W - 18, ry + 1, n.rssi);
    } else {
      oled.print("[ Back ]");
    }
  }

  drawFooter("NAV=next  OK=select");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Network details
// ─────────────────────────────────────────────────────────────

void uiDrawNetworkDetails(const NetworkInfo &network, bool saved) {
  oledClear();
  drawStatusBar("NETWORK DETAILS", saved ? "[SET]" : "");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  char buf[32];

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  printTruncated(network.ssid[0] ? network.ssid : "<hidden>", 20);

  snprintf(buf, sizeof(buf), "BSSID: %s", network.bssid);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  printTruncated(buf, 21);

  snprintf(buf, sizeof(buf), "CH %u  RSSI %d dBm", network.channel, (int)network.rssi);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  oled.print(buf);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print("Sec: ");
  oled.print(secShort(network.security));

  if (saved) {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
    oled.print("Target SET - OK=deauth");
  } else {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
    oled.print("OK=set target");
  }

  drawFooter("Long OK=back");
  oledFlush();
}

void uiDrawTargetDetails(const NetworkInfo &network) {
  uiDrawNetworkDetails(network, true);
}

// ─────────────────────────────────────────────────────────────
//  Deauth TX screen
// ─────────────────────────────────────────────────────────────
// uiDrawTxCounter is already defined above

// ─────────────────────────────────────────────────────────────
//  Lab precheck
// ─────────────────────────────────────────────────────────────

void uiDrawLabPrecheck(bool hasTarget, const NetworkInfo *network) {
  oledClear();
  drawStatusBar("PRECHECK");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  if (!hasTarget || !network) {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 4);
    oled.print("No target set.");
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 14);
    oled.print("Scan & set target");
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 24);
    oled.print("in WiFi > Scan.");
  } else {
    char buf[24];
    oled.setCursor(UI_PAD, UI_CONTENT_Y);
    printTruncated(network->ssid[0] ? network->ssid : "<hidden>", 20);

    snprintf(buf, sizeof(buf), "CH %u  %s", network->channel, secShort(network->security));
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
    oled.print(buf);

    bool blocked = (network->security == RTW_SECURITY_WPA3_AES_PSK ||
                    network->security == RTW_SECURITY_WPA2_WPA3_MIXED ||
                    network->security == RTW_SECURITY_WPA2_AES_CMAC);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 22);
    if (blocked) {
      oled.print("! WPA3 - may block TX");
    } else {
      oled.print("Target OK - ready");
    }

    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)network->rssi);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 32);
    oled.print(buf);
  }

  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Target monitor (RSSI tracking)
// ─────────────────────────────────────────────────────────────

void uiDrawTargetMonitor(const NetworkInfo &network, bool found) {
  oledClear();
  drawStatusBar("TARGET MONITOR", found ? "VISIBLE" : "HIDDEN");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  char buf[24];

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  printTruncated(network.ssid[0] ? network.ssid : "<hidden>", 20);

  snprintf(buf, sizeof(buf), "CH %u  %s", network.channel, secShort(network.security));
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  oled.print(buf);

  if (found) {
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)network.rssi);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 22);
    oled.print(buf);
    drawRssiBar(UI_PAD, UI_CONTENT_Y + 32, network.rssi);
  } else {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 22);
    oled.print("Not visible (hidden)");
  }

  drawFooter("OK=refresh  Long OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Lab stats
// ─────────────────────────────────────────────────────────────

void uiDrawLabStats(const LabStats &stats) {
  oledClear();
  drawStatusBar("LAB STATS");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  char buf[28];

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  printTruncated(stats.bssid, 17);

  snprintf(buf, sizeof(buf), "Samples: %u", stats.samples);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "Found: %u  Missed: %u", stats.found, stats.missed);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  oled.print(buf);

  if (stats.samples > 0) {
    int32_t avg = labStatsAverageRssi();
    snprintf(buf, sizeof(buf), "RSSI avg: %d dBm", (int)avg);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
    oled.print(buf);
    snprintf(buf, sizeof(buf), "min:%d max:%d", (int)stats.minRssi, (int)stats.maxRssi);
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
    oled.print(buf);
  }

  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Principal test result
// ─────────────────────────────────────────────────────────────

void uiDrawPrincipalTest(const LabTestReport &report) {
  oledClear();
  drawStatusBar("TEST RESULT");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  printTruncated(report.title, 20);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  printTruncated(report.line1, 21);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  printTruncated(report.line2, 21);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  printTruncated(report.line3, 21);

  char buf[16];
  snprintf(buf, sizeof(buf), "Attempts: %u", report.attempts);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
  oled.print(buf);

  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  WiFi analyzer (simple bar chart – sniffer-based)
// ─────────────────────────────────────────────────────────────

void uiDrawAnalyzer(uint8_t band) {
  oledClear();
  char header[16];
  snprintf(header, sizeof(header), "ANALYZER %uGHz", band == 5 ? 5 : 2);
  drawStatusBar(header);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 4);
  oled.print("Scanning channels...");
  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  WiFi Radar
// ─────────────────────────────────────────────────────────────

static uint8_t _radarAngle = 0;

void uiDrawWifiRadar(const NetworkInfo *network, bool found) {
  oledClear();
  drawStatusBar("WIFI RADAR", found ? "FOUND" : "...");

  // Draw a simple radar circle + sweep line
  int8_t cx = 32, cy = 38, r = 20;
  oled.drawCircle(cx, cy, r,     SSD1306_WHITE);
  oled.drawCircle(cx, cy, r / 2, SSD1306_WHITE);
  // Sweep line using stored angle
  float rad = _radarAngle * 0.01745f;
  oled.drawLine(cx, cy,
                cx + (int8_t)(cos(rad) * r),
                cy + (int8_t)(sin(rad) * r),
                SSD1306_WHITE);
  if (found) oled.fillCircle(cx, cy, 3, SSD1306_WHITE);

  // Network info on the right
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  if (network && network->ssid[0]) {
    oled.setCursor(68, 14);
    printTruncated(network->ssid, 9);
    char buf[12];
    snprintf(buf, sizeof(buf), "CH%u", network->channel);
    oled.setCursor(68, 24);
    oled.print(buf);
    if (found) {
      snprintf(buf, sizeof(buf), "%ddBm", (int)network->rssi);
      oled.setCursor(68, 34);
      oled.print(buf);
    }
  } else {
    oled.setCursor(68, 24);
    oled.print("No target");
  }

  drawFooter("OK=list  Long OK=back");
  oledFlush();
}

void uiTickWifiRadar() {
  static uint32_t last = 0;
  if (millis() - last > 80) {
    last = millis();
    _radarAngle = (_radarAngle + 10) % 360;
  }
}

// ─────────────────────────────────────────────────────────────
//  System info
// ─────────────────────────────────────────────────────────────

void uiDrawSystemInfo(bool hasTarget, uint8_t scanCount,
                      uint8_t count24, uint8_t count5) {
  oledClear();
  drawStatusBar("SYSTEM");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  char buf[28];

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  oled.print("FW: ");
  oled.print(FIRMWARE_VERSION);

  snprintf(buf, sizeof(buf), "Heap: %lu B", (unsigned long)xPortGetFreeHeapSize());
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "Nets: %u (2G:%u 5G:%u)", scanCount, count24, count5);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  oled.print(buf);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print(hasTarget ? "Target: SET" : "Target: none");

  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  BLE list
// ─────────────────────────────────────────────────────────────

void uiDrawBleList(int selected, int listTop) {
  oledClear();
  char header[16];
  snprintf(header, sizeof(header), "BLE DEVICES [%u]", bleCount());
  drawStatusBar(header);
  uiRefreshBleList(selected, listTop);
}

void uiRefreshBleList(int selected, int listTop) {
  oled.fillRect(0, UI_CONTENT_Y, OLED_W, UI_CONTENT_H, SSD1306_BLACK);
  oled.setTextSize(1);

  uint8_t total = bleCount();
  uint8_t row = 0;

  for (uint8_t i = listTop; i < total && row < UI_MENU_VISIBLE; i++) {
    BleDeviceInfo dev;
    if (!bleCopyDevice(i, dev)) continue;
    int16_t ry  = UI_CONTENT_Y + row * UI_MENU_ROW_H;
    bool    sel = ((int)i == selected);

    if (sel) {
      oled.fillRect(0, ry, OLED_W, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }

    oled.setCursor(UI_PAD, ry + 1);
    // Show name or address
    if (dev.name[0]) {
      printTruncated(dev.name, 14);
    } else {
      printTruncated(dev.addr, 14);
    }

    // RSSI mini on right
    oled.setTextColor(SSD1306_WHITE);
    char rssi[6];
    snprintf(rssi, sizeof(rssi), "%d", (int)dev.rssi);
    oled.setCursor(OLED_W - strlen(rssi) * 6 - UI_PAD, ry + 1);
    oled.print(rssi);

    row++;
  }

  // Back entry
  if (row < UI_MENU_VISIBLE) {
    int16_t ry  = UI_CONTENT_Y + row * UI_MENU_ROW_H;
    bool    sel = (selected == -1);
    if (sel) {
      oled.fillRect(0, ry, OLED_W, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD, ry + 1);
    oled.print("[ Back ]");
  }

  drawFooter("NAV=next  OK=detail");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  BLE device details
// ─────────────────────────────────────────────────────────────

void uiDrawBleDetails(const BleDeviceInfo &dev) {
  oledClear();
  drawStatusBar("BLE DETAIL");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  char buf[24];

  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  printTruncated(dev.name[0] ? dev.name : "<no name>", 20);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  printTruncated(dev.addr, 17);

  snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)dev.rssi);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  oled.print(buf);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print(bleKindLabel(dev.kind));

  snprintf(buf, sizeof(buf), "Seen: %u", dev.seenCount);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
  oled.print(buf);

  drawFooter("OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  BLE analyzer  (oscilloscope-style packet rate)
// ─────────────────────────────────────────────────────────────

void uiDrawBleAnalyzer() {
  oledClear();
  drawStatusBar("BLE ANALYZER");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 4);
  oled.print("Listening...");
  uiRefreshBleAnalyzer();
}

void uiRefreshBleAnalyzer() {
  // Draw packet-rate history as a small waveform
  const uint8_t *hist = blePpsHistory();
  uint8_t head  = blePpsHistoryHead();
  uint16_t pps  = blePps();
  uint16_t base = bleBaseline();

  // Clear scope area
  oled.fillRect(0, UI_CONTENT_Y + 14, OLED_W, 30, SSD1306_BLACK);

  // Draw waveform (60 bins, each 2px wide → 120px)
  uint8_t maxV = 1;
  for (uint8_t i = 0; i < BLE_HISTORY_SIZE; i++) {
    if (hist[i] > maxV) maxV = hist[i];
  }

  for (uint8_t i = 0; i < BLE_HISTORY_SIZE; i++) {
    uint8_t idx = (head + i) % BLE_HISTORY_SIZE;
    uint8_t v   = hist[idx];
    int16_t bh  = v ? map(v, 0, maxV, 1, 28) : 0;
    int16_t bx  = i * 2;
    int16_t by  = UI_CONTENT_Y + 14 + 28 - bh;
    if (bh > 0) oled.drawFastVLine(bx, by, bh, SSD1306_WHITE);
  }

  // Stats line
  oled.fillRect(0, UI_CONTENT_Y + 44, OLED_W, 9, SSD1306_BLACK);
  char buf[28];
  snprintf(buf, sizeof(buf), "pps:%u base:%u avg:%d", pps, base, (int)bleAvgRssi());
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 44);
  oled.print(buf);

  drawFooter("OK=stop  Long OK=reset");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  WiFi Sniffer
// ─────────────────────────────────────────────────────────────

void uiDrawSniffer(const SnifferStats &stats) {
  oledClear();
  char header[16];
  snprintf(header, sizeof(header), "SNIFFER %uGHz",
           stats.band == 5 ? 5 : 2);
  drawStatusBar(header);
  uiRefreshSniffer(stats);
}

void uiRefreshSniffer(const SnifferStats &stats) {
  oled.fillRect(0, UI_CONTENT_Y, OLED_W, UI_CONTENT_H, SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  char buf[28];

  snprintf(buf, sizeof(buf), "Frames: %lu", (unsigned long)stats.totalFrames);
  oled.setCursor(UI_PAD, UI_CONTENT_Y);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "Mgmt:%lu Ctrl:%lu",
           (unsigned long)stats.mgmtFrames, (unsigned long)stats.ctrlFrames);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "Data: %lu", (unsigned long)stats.dataFrames);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 20);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "Bcn:%lu Deauth:%lu",
           (unsigned long)stats.beacons, (unsigned long)stats.deauths);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print(buf);

  snprintf(buf, sizeof(buf), "CH: %u", stats.currentChannel);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
  oled.print(buf);

  drawFooter("OK=stop  Long OK=back");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  WiFi traffic analyzer (scope + channel bars)
// ─────────────────────────────────────────────────────────────

void uiDrawWifiAnalyzer() {
  oledClear();
  uint8_t band = wifiAnalyzerBand();
  char header[16];
  snprintf(header, sizeof(header), "TRAFFIC %uGHz", band == 5 ? 5 : 2);
  drawStatusBar(header);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 4);
  oled.print("Listening...");
  uiRefreshWifiAnalyzer();
}

void uiRefreshWifiAnalyzer() {
  // Scope area: rows 12-40 (28px tall)
  oled.fillRect(0, UI_CONTENT_Y, OLED_W, 28, SSD1306_BLACK);
  oled.setTextColor(SSD1306_WHITE);

  const uint8_t *hist = wifiAnalyzerHistory();
  uint8_t head  = wifiAnalyzerHistoryHead();
  uint16_t pps  = wifiAnalyzerPps();
  uint16_t base = wifiAnalyzerBaseline();

  uint8_t maxV = 1;
  for (uint8_t i = 0; i < WIFI_ANAL_HIST; i++) {
    if (hist[i] > maxV) maxV = hist[i];
  }

  for (uint8_t i = 0; i < WIFI_ANAL_HIST; i++) {
    uint8_t idx = (head + i) % WIFI_ANAL_HIST;
    uint8_t v   = hist[idx];
    int16_t bh  = v ? map(v, 0, maxV, 1, 26) : 0;
    int16_t bx  = i * 2;
    int16_t by  = UI_CONTENT_Y + 26 - bh;
    if (bh > 0) oled.drawFastVLine(bx, by, bh, SSD1306_WHITE);
  }

  // Channel bar row: 8px tall below scope
  oled.fillRect(0, UI_CONTENT_Y + 28, OLED_W, 10, SSD1306_BLACK);
  uint8_t chCount = wifiBandChannelCount();
  if (chCount > 0) {
    int busiest = wifiBandBusiestChannelIdx();
    uint8_t curCh = sniffGetStats().currentChannel;
    uint8_t barW = OLED_W / chCount;
    uint32_t maxF = 1;
    for (uint8_t i = 0; i < chCount; i++) {
      if (wifiBandChannelFrames(i) > maxF) maxF = wifiBandChannelFrames(i);
    }
    for (uint8_t i = 0; i < chCount; i++) {
      uint32_t v  = wifiBandChannelFrames(i);
      int bh = v ? map(v, 0, maxF, 1, 9) : 0;
      int bx = i * barW;
      int by = UI_CONTENT_Y + 28 + 9 - bh;
      if ((int)i == busiest && v > 0) {
        oled.fillRect(bx, by, barW - 1, bh, SSD1306_WHITE);
      } else if (bh > 0) {
        oled.drawRect(bx, by, barW - 1, bh, SSD1306_WHITE);
      }
      if (wifiBandChannelNumber(i) == curCh) {
        oled.drawPixel(bx + barW / 2, UI_CONTENT_Y + 28, SSD1306_WHITE);
      }
    }
  }

  // Stats text
  oled.fillRect(0, UI_CONTENT_Y + 38, OLED_W, 9, SSD1306_BLACK);
  char buf[28];
  int topIdx = wifiBandBusiestChannelIdx();
  if (topIdx >= 0) {
    snprintf(buf, sizeof(buf), "pps:%u top:CH%u", pps, wifiBandChannelNumber(topIdx));
  } else {
    snprintf(buf, sizeof(buf), "pps:%u base:%u", pps, base);
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 38);
  oled.print(buf);

  drawFooter("OK=stop  Long OK=reset");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Beacon Spam
// ─────────────────────────────────────────────────────────────

void uiDrawBeaconSpam() {
  oledClear();
  const BeaconSpamStats &s = beaconSpamGetStats();
  char header[16];
  snprintf(header, sizeof(header), "BEACON %uGHz", s.band == 5 ? 5 : 2);
  drawStatusBar(header, s.active ? "TX" : "OFF");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 2);
  oled.print("PACKETS SENT:");
  uiRefreshBeaconSpam();
}

void uiRefreshBeaconSpam() {
  const BeaconSpamStats &s = beaconSpamGetStats();
  char buf[24];

  // Counter (big text)
  oled.fillRect(0, UI_CONTENT_Y + 12, OLED_W, 16, SSD1306_BLACK);
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.totalTx);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 12);
  oled.print(buf);
  oled.setTextSize(1);

  // Pulse dot
  if (s.active) {
    uint8_t pulse = (millis() / 300) & 1;
    if (pulse) oled.fillCircle(OLED_W - 8, UI_CONTENT_Y + 18, 4, SSD1306_WHITE);
    else       oled.drawCircle(OLED_W - 8, UI_CONTENT_Y + 18, 4, SSD1306_WHITE);
  }

  // Channel and SSID
  oled.fillRect(0, UI_CONTENT_Y + 30, OLED_W, 18, SSD1306_BLACK);
  oled.setTextColor(SSD1306_WHITE);
  snprintf(buf, sizeof(buf), "CH:%u  %u/%u SSIDs",
           s.currentChannel, s.currentSsidIdx + 1, beaconSpamSsidCount());
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print(buf);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
  printTruncated(beaconSpamCurrentSsid(), 20);

  drawFooter("OK=stop  Lab only!");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  BLE Spam
// ─────────────────────────────────────────────────────────────

void uiDrawBleSpam() {
  oledClear();
  const BleSpamStats &s = bleSpamGetStats();
  drawStatusBar("BLE SPAM", s.active ? "ADV" : "OFF");
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 2);
  oled.print("ID CHANGES:");
  uiRefreshBleSpam();
}

void uiRefreshBleSpam() {
  const BleSpamStats &s = bleSpamGetStats();
  char buf[24];

  oled.fillRect(0, UI_CONTENT_Y + 12, OLED_W, 16, SSD1306_BLACK);
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.totalTx);
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 12);
  oled.print(buf);
  oled.setTextSize(1);

  if (s.active) {
    uint8_t pulse = (millis() / 300) & 1;
    if (pulse) oled.fillCircle(OLED_W - 8, UI_CONTENT_Y + 18, 4, SSD1306_WHITE);
    else       oled.drawCircle(OLED_W - 8, UI_CONTENT_Y + 18, 4, SSD1306_WHITE);
  }

  oled.fillRect(0, UI_CONTENT_Y + 30, OLED_W, 18, SSD1306_BLACK);
  oled.setTextColor(SSD1306_WHITE);
  snprintf(buf, sizeof(buf), "%u / %u devices", s.currentIdx + 1, bleSpamCount());
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 30);
  oled.print(buf);

  oled.setCursor(UI_PAD, UI_CONTENT_Y + 40);
  printTruncated(bleSpamCurrent(), 20);

  drawFooter("OK=stop  Lab only!");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  IR Remote menu
// ─────────────────────────────────────────────────────────────

void uiDrawIrMenu(uint8_t selectedIndex) {
  oledClear();
  drawStatusBar("IR REMOTE");
  uiRefreshIrMenu(selectedIndex);
}

void uiRefreshIrMenu(uint8_t selectedIndex) {
  oled.fillRect(0, UI_CONTENT_Y, OLED_W, UI_CONTENT_H, SSD1306_BLACK);
  oled.setTextSize(1);

  uint8_t total = irCodeCount();
  int8_t  top   = (int8_t)selectedIndex - (UI_MENU_VISIBLE - 1);
  if (top < 0) top = 0;
  if ((uint8_t)top + UI_MENU_VISIBLE > total)
    top = (int8_t)total - UI_MENU_VISIBLE;
  if (top < 0) top = 0;

  for (uint8_t v = 0; v < UI_MENU_VISIBLE && (top + v) < total; v++) {
    uint8_t idx = top + v;
    int16_t ry  = UI_CONTENT_Y + v * UI_MENU_ROW_H;
    bool    sel = (idx == selectedIndex);

    if (sel) {
      oled.fillRect(0, ry, OLED_W, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setCursor(UI_PAD, ry + 1);
      oled.print("> ");
    } else {
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(UI_PAD + 12, ry + 1);
    }
    printTruncated(irCodeName(idx), 19);
  }

  // Scroll dots
  if (total > UI_MENU_VISIBLE) {
    for (uint8_t i = 0; i < total; i++) {
      int16_t dy = UI_CONTENT_Y + i * (UI_CONTENT_H / total);
      if (i == selectedIndex) oled.fillCircle(OLED_W - 2, dy + 3, 1, SSD1306_WHITE);
      else                    oled.drawPixel(OLED_W - 2,  dy + 3, SSD1306_WHITE);
    }
  }

  drawFooter("NAV=next  OK=transmit");
  oledFlush();
}

// ─────────────────────────────────────────────────────────────
//  Home (legacy stub – goes straight to main menu redraw)
// ─────────────────────────────────────────────────────────────

void uiDrawHome() {
  // Not used directly; main menu is drawn via uiDrawMenu in bw16.ino
}



void uiDrawActionMenu(const NetworkInfo &network, uint8_t selected) {
  oled.clearDisplay();
  char titleBuf[22];
  bool is5G = wifiScannerIs5GHz(network.channel);
  snprintf(titleBuf, sizeof(titleBuf), "[%s] %s", is5G ? "5G" : "2G", network.ssid[0] ? network.ssid : "<hidden>");
  drawStatusBar(titleBuf, "");

  static const char *const ACTION_ITEMS[] = {
    "Deauth (All)",
    "Scan Clients",
    "Clone & Beacon",
    "Sniff Traffic",
    "Set as Target",
    "Back"
  };
  static const uint8_t ACTION_COUNT = 6;

  uint8_t listTop = 0;
  if (selected >= UI_MENU_VISIBLE) listTop = selected - UI_MENU_VISIBLE + 1;

  for (uint8_t i = 0; i < UI_MENU_VISIBLE && (listTop + i) < ACTION_COUNT; i++) {
    uint8_t idx = listTop + i;
    int16_t y = UI_CONTENT_Y + 1 + i * UI_MENU_ROW_H;
    if (idx == selected) {
      oled.fillRect(0, y, 120, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD, y + 1);
    oled.print(ACTION_ITEMS[idx]);
    oled.setTextColor(SSD1306_WHITE);
  }

  if (ACTION_COUNT > UI_MENU_VISIBLE) {
    uint8_t dotY = UI_CONTENT_Y + 2 + (selected * (40 / (ACTION_COUNT - 1)));
    oled.fillCircle(125, dotY, 2, SSD1306_WHITE);
  }

  drawFooter("OK=select");
  oledFlush();
}

void uiDrawClientList(const char *macs[], uint8_t selected, uint8_t listTop, uint8_t total) {
  oled.clearDisplay();
  char titleBuf[20];
  snprintf(titleBuf, sizeof(titleBuf), "Clients: %d", total);
  drawStatusBar(titleBuf, "");

  if (total == 0) {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
    oled.print("No clients found.");
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 22);
    oled.print("Try again later.");
    drawFooter("NAV=back");
    oledFlush();
    return;
  }

  uint8_t itemCount = total + 1;

  for (uint8_t i = 0; i < UI_MENU_VISIBLE && (listTop + i) < itemCount; i++) {
    uint8_t idx = listTop + i;
    int16_t y = UI_CONTENT_Y + 1 + i * UI_MENU_ROW_H;
    if (idx == selected) {
      oled.fillRect(0, y, 120, UI_MENU_ROW_H, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    } else {
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.setCursor(UI_PAD, y + 1);
    if (idx < total) {
      oled.print(macs[idx]);
    } else {
      oled.print("[ Back ]");
    }
    oled.setTextColor(SSD1306_WHITE);
  }

  drawFooter("OK=deauth");
  oledFlush();
}

void uiDrawGenericMessage(const char *title, const char *msg1, const char *msg2) {
  oled.clearDisplay();
  drawStatusBar(title, "");
  oled.setCursor(UI_PAD, UI_CONTENT_Y + 10);
  oled.print(msg1);
  if (msg2) {
    oled.setCursor(UI_PAD, UI_CONTENT_Y + 22);
    oled.print(msg2);
  }
  oledFlush();
}


