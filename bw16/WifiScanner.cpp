#include "HardwareManager.h"
#include "WifiScanner.h"
#include <WiFi.h>

static uint8_t networkCount = 0;
static NetworkInfo networks[MAX_NETWORKS];
static volatile bool scanComplete = false;
static bool scanInProgress = false;
static uint32_t scanStartedAt = 0;

static rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scanResult) {
  // 0 means valid AP details record. Non-zero means event completion header.
  if (scanResult->scan_complete != 0) {
    return RTW_SUCCESS;
  }

  rtw_scan_result_t *record = &scanResult->ap_details;

  // Format BSSID string
  char bssidBuf[18];
  snprintf(bssidBuf, sizeof(bssidBuf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           record->BSSID.octet[0], record->BSSID.octet[1], record->BSSID.octet[2],
           record->BSSID.octet[3], record->BSSID.octet[4], record->BSSID.octet[5]);

  // Check for existing BSSID in our table
  for (uint8_t i = 0; i < networkCount; i++) {
    if (strcmp(networks[i].bssid, bssidBuf) == 0) {
      // If we previously had an empty/hidden SSID and now we received a named SSID, UNCLOAK IT!
      if ((networks[i].ssid[0] == '\0' || strcmp(networks[i].ssid, "<hidden>") == 0) && record->SSID.len > 0) {
        uint8_t ssidLen = record->SSID.len > WL_SSID_MAX_LENGTH ? WL_SSID_MAX_LENGTH : record->SSID.len;
        memcpy(networks[i].ssid, record->SSID.val, ssidLen);
        networks[i].ssid[ssidLen] = '\0';
      }
      // CRITICAL FIX: If current record has empty SSID but we ALREADY know the name from a previous scan,
      // PRESERVE the known SSID name so it never turns into [Hidden]!
      networks[i].rssi = record->signal_strength;
      networks[i].channel = record->channel;
      networks[i].security = record->security;
      return RTW_SUCCESS;
    }
  }

  if (networkCount >= MAX_NETWORKS) {
    return RTW_SUCCESS;
  }

  NetworkInfo &network = networks[networkCount];
  uint8_t ssidLen = record->SSID.len;
  if (ssidLen > WL_SSID_MAX_LENGTH) {
    ssidLen = WL_SSID_MAX_LENGTH;
  }
  memcpy(network.ssid, record->SSID.val, ssidLen);
  network.ssid[ssidLen] = '\0';
  network.rssi = record->signal_strength;
  network.security = record->security;
  network.channel = record->channel;
  strcpy(network.bssid, bssidBuf);
  networkCount++;

  return RTW_SUCCESS;
}

#include <wifi_drv.h>
extern "C" {
  #include <wifi_conf.h>
  #include <wifi_constants.h>
}

void wifiScannerBegin() {
  Serial.println("[WIFI] wifiScannerBegin: initializing STA mode...");

  wifi_on(RTW_MODE_STA);
  wifi_change_channel_plan(0x7F); // Dual-band 2.4GHz + 5GHz full channel plan

  // Verify channel plan
  uint8_t readPlan = 0xFF;
  wifi_get_channel_plan(&readPlan);
  Serial.print("[WIFI] Channel plan initialized to: 0x");
  Serial.println(readPlan, HEX);

  delay(50);
  Serial.println("[WIFI] wifiScannerBegin: READY.");
}

bool wifiScannerScan() {
  bool succeeded = false;
  if (!wifiScannerStartScan()) {
    return false;
  }

  while (!wifiScannerPollScan(&succeeded)) {
    delay(20);
    ledTaskUpdate();
  }

  return succeeded;
}

static void _bgScanTask(const void *arg) {
  (void)arg;
  Serial.println("[SCAN] Background scan thread started...");
  wifi_scan_networks_mcc(scanResultHandler, NULL);
  scanComplete = true;
  Serial.println("[SCAN] Background scan thread finished.");
  vTaskDelete(NULL);
}

bool wifiScannerStartScan() {
  if (scanInProgress) {
    Serial.println("[SCAN] Already in progress, skipping.");
    return false;
  }

  scanComplete = false;
  scanStartedAt = millis();
  scanInProgress = true;
  networkCount = 0;

  Serial.println("[SCAN] Launching async scan thread...");
  os_thread_create_arduino(_bgScanTask, NULL, OS_PRIORITY_NORMAL, 4096);
  Serial.println("[SCAN] Dual-band scan started instantly (0ms latency).");

  return true;
}

bool wifiScannerPollScan(bool *succeeded) {
  if (!scanInProgress) {
    return false;
  }

  // Full 5.0 second dual-band scan duration
  if (millis() - scanStartedAt < 5000) {
    return false;
  }

  scanInProgress = false;
  Serial.print("[SCAN] Scan complete. Networks found: ");
  Serial.println(networkCount);

  // Print band breakdown
  uint8_t cnt24 = 0, cnt5 = 0;
  for (uint8_t i = 0; i < networkCount; i++) {
    if (networks[i].channel >= 36) cnt5++;
    else cnt24++;
  }
  Serial.print("[SCAN] 2.4GHz: ");
  Serial.print(cnt24);
  Serial.print("  5GHz: ");
  Serial.println(cnt5);

  if (succeeded != NULL) {
    *succeeded = true;
  }
  return true;
}

bool wifiScannerIsScanning() {
  return scanInProgress;
}

uint8_t wifiScannerCount() {
  return networkCount;
}

const NetworkInfo &wifiScannerNetwork(uint8_t index) {
  return networks[index];
}

bool wifiScannerIs5GHz(uint8_t channel) {
  return channel >= 36;
}

bool wifiScannerNetworkInBand(uint8_t index, uint8_t band) {
  return band == 5 ? wifiScannerIs5GHz(networks[index].channel) : !wifiScannerIs5GHz(networks[index].channel);
}

uint8_t wifiScannerCountBand(uint8_t band) {
  uint8_t count = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (wifiScannerNetworkInBand(i, band)) {
      count++;
    }
  }

  return count;
}

uint8_t wifiScannerCountChannel(uint8_t channel) {
  uint8_t count = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (networks[i].channel == channel) {
      count++;
    }
  }

  return count;
}

int wifiScannerStrongestIndex(uint8_t band) {
  int strongest = -1;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (!wifiScannerNetworkInBand(i, band)) {
      continue;
    }

    if (strongest < 0 || networks[i].rssi > networks[strongest].rssi) {
      strongest = i;
    }
  }

  return strongest;
}

int wifiScannerFindBssid(const char *bssid) {
  for (uint8_t i = 0; i < networkCount; i++) {
    if (strcmp(networks[i].bssid, bssid) == 0) {
      return i;
    }
  }

  return -1;
}

uint8_t wifiScannerBusiestChannel(uint8_t band) {
  uint8_t bestChannel = 0;
  uint8_t bestCount = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (!wifiScannerNetworkInBand(i, band)) {
      continue;
    }

    uint8_t channel = networks[i].channel;
    uint8_t count = wifiScannerCountChannel(channel);
    if (count > bestCount) {
      bestCount = count;
      bestChannel = channel;
    }
  }

  return bestChannel;
}

const char *wifiScannerSecurityName(uint32_t security) {
  switch (security) {
    case RTW_SECURITY_OPEN:
      return "OPEN";
    case RTW_SECURITY_WEP_PSK:
      return "WEP";
    case RTW_SECURITY_WPA_TKIP_PSK:
      return "WPA TKIP";
    case RTW_SECURITY_WPA_AES_PSK:
      return "WPA AES";
    case RTW_SECURITY_WPA2_AES_PSK:
      return "WPA2 AES";
    case RTW_SECURITY_WPA2_TKIP_PSK:
      return "WPA2 TKIP";
    case RTW_SECURITY_WPA2_MIXED_PSK:
      return "WPA2 MIX";
    case RTW_SECURITY_WPA_WPA2_MIXED_PSK:
      return "WPA/WPA2";
    case RTW_SECURITY_WPA3_AES_PSK:
      return "WPA3 AES";
    case RTW_SECURITY_WPA2_WPA3_MIXED:
      return "WPA2/WPA3";
    default:
      return "SEC?";
  }
}

void wifiScannerPrintToSerial() {
  Serial.print("Networks found: ");
  Serial.println(networkCount);

  for (uint8_t i = 0; i < networkCount; i++) {
    const NetworkInfo &network = networks[i];

    Serial.print(i);
    Serial.print(" SSID=");
    Serial.print(network.ssid[0] ? network.ssid : "<hidden>");
    Serial.print(" CH=");
    Serial.print(network.channel);
    Serial.print(" BAND=");
    Serial.print(wifiScannerIs5GHz(network.channel) ? "5G" : "2.4G");
    Serial.print(" RSSI=");
    Serial.print(network.rssi);
    Serial.print(" BSSID=");
    Serial.print(network.bssid);
    Serial.print(" SEC=");
    Serial.println(wifiScannerSecurityName(network.security));
  }
}
