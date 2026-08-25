#include "packet-injection.h"


extern "C" size_t xPortGetFreeHeapSize(void);

static const size_t MAX_RAW_MGMT_FRAME_BYTES = 0x68;

#define PROBE_LOG_CAPACITY 128
static TxProbeEntry g_probe_log[PROBE_LOG_CAPACITY];
static volatile uint32_t g_probe_head = 0;
static volatile TxProbeSummary g_probe_summary = {0, 0, 0, 0, TX_STAGE_IDLE, 0};

static inline void probeRecord(uint32_t seq, uint8_t stage, void* alloc_ptr, int dump_ret) {
  uint32_t idx = g_probe_head % PROBE_LOG_CAPACITY;
  g_probe_log[idx].frame_seq = seq;
  g_probe_log[idx].timestamp_ms = millis();
  g_probe_log[idx].alloc_ptr = alloc_ptr;
  g_probe_log[idx].dump_ret = dump_ret;
  g_probe_log[idx].free_heap = xPortGetFreeHeapSize();
  g_probe_log[idx].stage = stage;
  g_probe_head++;

  g_probe_summary.current_stage = stage;
  g_probe_summary.last_activity_ms = millis();
}

void txProbeReset() {
  memset((void*)g_probe_log, 0, sizeof(g_probe_log));
  g_probe_head = 0;
  g_probe_summary.total_entered = 0;
  g_probe_summary.alloc_success = 0;
  g_probe_summary.alloc_null = 0;
  g_probe_summary.dump_success = 0;
  g_probe_summary.current_stage = TX_STAGE_IDLE;
  g_probe_summary.last_activity_ms = millis();
}

TxProbeSummary txProbeGetSummary() {
  TxProbeSummary s;
  s.total_entered = g_probe_summary.total_entered;
  s.alloc_success = g_probe_summary.alloc_success;
  s.alloc_null = g_probe_summary.alloc_null;
  s.dump_success = g_probe_summary.dump_success;
  s.current_stage = g_probe_summary.current_stage;
  s.last_activity_ms = g_probe_summary.last_activity_ms;
  return s;
}

void txProbePrintReport() {
  Serial.println(F("\n================= TX PROBE TELEMETRY REPORT ================="));
  Serial.print(F("Total Frame Submissions Entered : ")); Serial.println(g_probe_summary.total_entered);
  Serial.print(F("Alloc Success (Non-NULL) Count  : ")); Serial.println(g_probe_summary.alloc_success);
  Serial.print(F("Alloc NULL Count                : ")); Serial.println(g_probe_summary.alloc_null);
  Serial.print(F("Dump Success Count              : ")); Serial.println(g_probe_summary.dump_success);
  Serial.print(F("Last Active Stage               : ")); 
  switch (g_probe_summary.current_stage) {
    case TX_STAGE_IDLE:         Serial.println(F("IDLE (0)")); break;
    case TX_STAGE_ALLOC_ENTER:  Serial.println(F("STUCK IN alloc_mgtxmitframe() [STAGE 1]")); break;
    case TX_STAGE_ALLOC_RETURN: Serial.println(F("ALLOC_RETURN [STAGE 2]")); break;
    case TX_STAGE_ATTRIB_ENTER: Serial.println(F("STUCK IN update_mgntframe_attrib() [STAGE 3]")); break;
    case TX_STAGE_ATTRIB_DONE:  Serial.println(F("ATTRIB_DONE [STAGE 4]")); break;
    case TX_STAGE_DUMP_ENTER:   Serial.println(F("STUCK IN dump_mgntframe() [STAGE 5]")); break;
    case TX_STAGE_DUMP_RETURN:  Serial.println(F("DUMP_RETURN [STAGE 6]")); break;
    default:                    Serial.println(g_probe_summary.current_stage); break;
  }
  Serial.print(F("Current Free Heap (bytes)       : ")); Serial.println((uint32_t)xPortGetFreeHeapSize());

  uint32_t total = g_probe_head;
  uint32_t countToPrint = total < 32 ? total : 32;
  uint32_t startIdx = total > countToPrint ? total - countToPrint : 0;

  Serial.println(F("-------------------------------------------------------------"));
  Serial.println(F("Seq# | Time(ms) | Stage | Alloc Ptr   | Dump Ret | Free Heap"));
  Serial.println(F("-------------------------------------------------------------"));

  for (uint32_t i = startIdx; i < total; i++) {
    uint32_t idx = i % PROBE_LOG_CAPACITY;
    Serial.print(g_probe_log[idx].frame_seq); Serial.print(F("\t| "));
    Serial.print(g_probe_log[idx].timestamp_ms); Serial.print(F("\t| "));
    Serial.print(g_probe_log[idx].stage); Serial.print(F("\t| 0x"));
    Serial.print((uint32_t)g_probe_log[idx].alloc_ptr, HEX); Serial.print(F("\t| "));
    Serial.print(g_probe_log[idx].dump_ret); Serial.print(F("\t | "));
    Serial.println(g_probe_log[idx].free_heap);
  }
  Serial.println(F("=============================================================\n"));
}

bool wifi_tx_raw_frame(const void* frame, size_t length) {
  if (rltk_wlan_info == NULL || frame == NULL || length == 0 || length > MAX_RAW_MGMT_FRAME_BYTES) {
    return false;
  }

  uint32_t **wlan_info_ptr = (uint32_t **)(rltk_wlan_info + 0x10);
  if (wlan_info_ptr == NULL || *wlan_info_ptr == NULL || **wlan_info_ptr == 0) {
    return false;
  }

  uint32_t seq = ++g_probe_summary.total_entered;
  uint8_t *ptr = (uint8_t *)**wlan_info_ptr;

  // STAGE 1: Enter Alloc
  probeRecord(seq, TX_STAGE_ALLOC_ENTER, NULL, 0);

  uint8_t *frame_control = (uint8_t *)alloc_mgtxmitframe(ptr + 0xae0);

  // STAGE 2: Return from Alloc
  probeRecord(seq, TX_STAGE_ALLOC_RETURN, frame_control, 0);

  if (frame_control == NULL) {
    g_probe_summary.alloc_null++;
    return false;
  }
  g_probe_summary.alloc_success++;

  // STAGE 3: Enter Attrib
  probeRecord(seq, TX_STAGE_ATTRIB_ENTER, frame_control, 0);
  update_mgntframe_attrib(ptr, frame_control + 8);
  probeRecord(seq, TX_STAGE_ATTRIB_DONE, frame_control, 0);

  uint32_t frame_buffer = *(uint32_t *)(frame_control + 0x80);
  if (frame_buffer == 0) {
    return false;
  }

  memset((void *)frame_buffer, 0, 0x68);
  uint8_t *frame_data = (uint8_t *)frame_buffer + 0x28;
  memcpy(frame_data, frame, length);
  *(uint32_t *)(frame_control + 0x14) = length;
  *(uint32_t *)(frame_control + 0x18) = length;

  // STAGE 5: Enter Dump
  probeRecord(seq, TX_STAGE_DUMP_ENTER, frame_control, 0);
  int dump_res = dump_mgntframe(ptr, frame_control);
  // STAGE 6: Return from Dump
  probeRecord(seq, TX_STAGE_DUMP_RETURN, frame_control, dump_res);

  g_probe_summary.dump_success++;
  return true;
}

/*

*/
bool wifi_tx_deauth_frame(void* src_mac, void* dst_mac, void* bssid, uint16_t reason) {
  DeauthFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, bssid, 6);
  memcpy(&frame.destination, dst_mac, 6);
  frame.reason = reason;
  return wifi_tx_raw_frame(&frame, sizeof(DeauthFrame));
}

/*

*/
bool wifi_tx_beacon_frame(void* src_mac, void* dst_mac, const char *ssid) {
  BeaconFrame frame;
  memcpy(&frame.source, src_mac, 6);
  memcpy(&frame.access_point, src_mac, 6);
  memcpy(&frame.destination, dst_mac, 6);
  for (int i = 0; ssid[i] != '\0'; i++) {
    frame.ssid[i] = ssid[i];
    frame.ssid_length++;
  }
  return wifi_tx_raw_frame(&frame, 38 + frame.ssid_length);
}
