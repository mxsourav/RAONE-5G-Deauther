#ifndef PACKET_INJECTION_H
#define PACKET_INJECTION_H

#include <Arduino.h>

typedef struct {
  uint16_t frame_control = 0xC0;
  uint16_t duration = 0xFFFF;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  uint16_t reason = 0x06;
} DeauthFrame;

typedef struct {
  uint16_t frame_control = 0x80;
  uint16_t duration = 0;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t access_point[6];
  const uint16_t sequence_number = 0;
  const uint64_t timestamp = 0;
  uint16_t beacon_interval = 0x64;
  uint16_t ap_capabilities = 0x21;
  const uint8_t ssid_tag = 0;
  uint8_t ssid_length = 0;
  uint8_t ssid[255];
} BeaconFrame;

/*

*/
extern uint8_t* rltk_wlan_info;
extern "C" void* alloc_mgtxmitframe(void* ptr);
extern "C" void update_mgntframe_attrib(void* ptr, void* frame_control);
extern "C" int dump_mgntframe(void* ptr, void* frame_control);

enum TxProbeStage : uint8_t {
  TX_STAGE_IDLE = 0,
  TX_STAGE_ALLOC_ENTER = 1,
  TX_STAGE_ALLOC_RETURN = 2,
  TX_STAGE_ATTRIB_ENTER = 3,
  TX_STAGE_ATTRIB_DONE = 4,
  TX_STAGE_DUMP_ENTER = 5,
  TX_STAGE_DUMP_RETURN = 6
};

struct TxProbeEntry {
  uint32_t frame_seq;
  uint32_t timestamp_ms;
  void*    alloc_ptr;
  int      dump_ret;
  uint32_t free_heap;
  uint8_t  stage;
};

struct TxProbeSummary {
  uint32_t total_entered;
  uint32_t alloc_success;
  uint32_t alloc_null;
  uint32_t dump_success;
  uint8_t  current_stage;
  uint32_t last_activity_ms;
};

bool wifi_tx_raw_frame(const void* frame, size_t length);
bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, void* bssid, uint16_t reason = 0x06);
bool wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid);

void txProbeReset();
void txProbePrintReport();
TxProbeSummary txProbeGetSummary();

#endif
