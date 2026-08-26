#include "ClientScanner.h"
#include <wifi_conf.h>
#include <wifi_constants.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────
//  ClientScanner — discovers clients connected to a target AP
//  by sniffing Data frames in promiscuous mode and extracting
//  source MACs where the BSSID matches the target.
// ─────────────────────────────────────────────────────────────

static ClientInfo clients[MAX_CLIENTS];
static uint8_t clientCount = 0;
static uint8_t targetBssidBytes[6];
static bool scanning = false;
static uint32_t scanStartedAt = 0;
static const uint32_t CLIENT_SCAN_DURATION_MS = 6000; // 6 seconds

// Format MAC bytes into string
static void macToStr(const uint8_t *mac, char *out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Check if MAC is broadcast/multicast
static bool isBroadcastMac(const uint8_t *mac) {
  return (mac[0] & 0x01) != 0; // multicast bit
}

// Check if MAC matches the target BSSID (skip AP itself)
static bool isBssidMac(const uint8_t *mac) {
  return memcmp(mac, targetBssidBytes, 6) == 0;
}

// Find existing client by MAC, returns index or -1
static int findClient(const uint8_t *mac) {
  for (uint8_t i = 0; i < clientCount; i++) {
    if (memcmp(clients[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

// Promiscuous mode callback
static void clientSniffCallback(unsigned char *buf, unsigned int len, void *userdata) {
  (void)userdata;
  if (!scanning || len < 36) return; // Need at least 802.11 header

  // 802.11 frame: buf layout on AmebaD promisc
  // buf[0..1] = frame control
  uint8_t frameType = (buf[0] >> 2) & 0x03;    // Type field
  uint8_t frameSubtype = (buf[0] >> 4) & 0x0F; // Subtype field

  // We want Data frames (type 2) of any subtype
  if (frameType != 2) return;

  // In Data frames: addr1=DA(receiver), addr2=SA(transmitter), addr3=BSSID
  // For To-DS frames (from client to AP):
  //   addr1=BSSID, addr2=Source(client), addr3=Destination
  // For From-DS frames (from AP to client):
  //   addr1=Destination(client), addr2=BSSID, addr3=Source

  uint8_t *addr1 = buf + 4;   // bytes 4-9
  uint8_t *addr2 = buf + 10;  // bytes 10-15
  uint8_t *addr3 = buf + 16;  // bytes 16-21

  uint8_t toDS   = (buf[1] >> 0) & 0x01;
  uint8_t fromDS = (buf[1] >> 1) & 0x01;

  const uint8_t *clientMac = NULL;

  if (toDS && !fromDS) {
    // Client -> AP: addr1=BSSID, addr2=client
    if (memcmp(addr1, targetBssidBytes, 6) == 0) {
      clientMac = addr2;
    }
  } else if (!toDS && fromDS) {
    // AP -> Client: addr2=BSSID, addr1=client
    if (memcmp(addr2, targetBssidBytes, 6) == 0) {
      clientMac = addr1;
    }
  }

  if (clientMac == NULL) return;
  if (isBroadcastMac(clientMac)) return;
  if (isBssidMac(clientMac)) return;

  int idx = findClient(clientMac);
  if (idx >= 0) {
    clients[idx].packetCount++;
  } else if (clientCount < MAX_CLIENTS) {
    memcpy(clients[clientCount].mac, clientMac, 6);
    macToStr(clientMac, clients[clientCount].macStr);
    clients[clientCount].rssi = 0;
    clients[clientCount].packetCount = 1;
    clientCount++;
  }
}

// ── Public API ────────────────────────────────────────────────

void clientScanStart(uint8_t channel, const uint8_t *targetBssid) {
  clientCount = 0;
  memset(clients, 0, sizeof(clients));
  memcpy(targetBssidBytes, targetBssid, 6);
  scanning = true;
  scanStartedAt = millis();

  Serial.print("[CLIENT] Starting client scan on CH ");
  Serial.print(channel);
  Serial.print(" for BSSID ");
  char bssidStr[18];
  macToStr(targetBssid, bssidStr);
  Serial.println(bssidStr);

  wifi_off();
  delay(100);
  wifi_on(RTW_MODE_STA);
  wifi_change_channel_plan(0x7F);
  delay(100);
  wifi_set_channel(channel);
  delay(50);

  wifi_set_promisc(RTW_PROMISC_ENABLE_2, clientSniffCallback, 0);
  Serial.println("[CLIENT] Promiscuous mode enabled, sniffing...");
}

bool clientScanPoll() {
  if (!scanning) return true;
  if (millis() - scanStartedAt >= CLIENT_SCAN_DURATION_MS) {
    clientScanStop();
    return true;
  }
  return false;
}

void clientScanStop() {
  if (scanning) {
    wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
    scanning = false;
    Serial.print("[CLIENT] Scan complete. Found ");
    Serial.print(clientCount);
    Serial.println(" client(s).");
  }
}

uint8_t clientScanCount() {
  return clientCount;
}

const ClientInfo &clientScanGet(uint8_t index) {
  return clients[index];
}
