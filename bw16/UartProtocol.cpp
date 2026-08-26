#include "UartProtocol.h"
#include "Config.h"
#include <string.h>
#include <ctype.h>

static SystemMode _currentMode = SYS_MODE_STANDALONE;
static char _rxBuf[128];
static uint8_t _rxIdx = 0;
static unsigned long _lastBeaconMs = 0;

// Quick LED flash to visually confirm UART activity
static void uartFlashLed() {
  digitalWrite(LED_GREEN, LED_ON_STATE);
  delay(30);
  digitalWrite(LED_GREEN, LED_OFF_STATE);
}

void uartProtocolBegin(unsigned long baud) {
  Serial.begin(baud); // Hardware LOG_UART on PA7 (TX) and PA8 (RX)
  Serial.println(F("[UART] LOG_UART initialized on PA7/PA8 @ 115200"));
}

SystemMode getSystemMode() {
  return _currentMode;
}

void setSystemMode(SystemMode mode) {
  _currentMode = mode;
}

void uartSendPong() {
  uartFlashLed(); // Visual confirmation: GREEN blink = ping received!
  Serial.print("RAONE_READY\n");
  Serial.print("PONG_RAONE_SLAVE_READY\n");
  Serial.flush();
}

void uartSendStatus(const char *status, uint32_t val) {
  char buf[64];
  if (val > 0) {
    snprintf(buf, sizeof(buf), "STATUS:%s:%lu\n", status, (unsigned long)val);
  } else {
    snprintf(buf, sizeof(buf), "STATUS:%s\n", status);
  }
  Serial.print(buf);
}

void uartSendLiveStats(uint8_t ch, uint32_t sent, uint32_t fail, uint16_t pps) {
  char buf[64];
  snprintf(buf, sizeof(buf), "STATUS:LIVE:%u,%lu,%lu,%u\n", ch, (unsigned long)sent, (unsigned long)fail, pps);
  Serial.print(buf);

  // Discrete fallback lines
  char txBuf[32], chBuf[32], ppsBuf[32];
  snprintf(txBuf, sizeof(txBuf), "STATUS:TX:%lu\n", (unsigned long)sent);
  snprintf(chBuf, sizeof(chBuf), "STATUS:CH:%u\n", ch);
  snprintf(ppsBuf, sizeof(ppsBuf), "STATUS:PPS:%u\n", pps);

  Serial.print(txBuf);
  Serial.print(chBuf);
  Serial.print(ppsBuf);
}

static void strToLower(char *s) {
  for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static bool checkBufferMatch(char *buf, UartCommand &cmd) {
  char lower[128];
  strncpy(lower, buf, sizeof(lower) - 1);
  lower[sizeof(lower) - 1] = '\0';
  strToLower(lower);

  // Instant keyword match (no newline required)
  if (strstr(lower, "ping_raone") || strstr(lower, "handshake_req") || strstr(lower, "master_tetrax") || strstr(lower, "ping")) {
    uartSendPong();
    _currentMode = SYS_MODE_SLAVE;
    cmd.type = UART_CMD_PING;
    return true;
  }
  if (strstr(buf, "CMD:NAV") || strstr(buf, "NAV")) {
    cmd.type = UART_CMD_NAV;
    return true;
  }
  if (strstr(buf, "CMD:OK") || strstr(buf, "OK")) {
    cmd.type = UART_CMD_OK;
    return true;
  }
  if (strstr(buf, "CMD:BACK") || strstr(buf, "BACK")) {
    cmd.type = UART_CMD_BACK;
    return true;
  }
  if (strstr(buf, "DEAUTH_ALL_24") || strstr(buf, "DEAUTH_24") || strstr(buf, "AT+DEAUTH24")) {
    cmd.type = UART_CMD_DEAUTH_ALL_24;
    return true;
  }
  if (strstr(buf, "DEAUTH_ALL_5G") || strstr(buf, "DEAUTH_5G") || strstr(buf, "AT+DEAUTH5G")) {
    cmd.type = UART_CMD_DEAUTH_ALL_5G;
    return true;
  }
  if (strstr(buf, "BEACON_24") || strstr(buf, "AT+BEACON24")) {
    cmd.type = UART_CMD_BEACON_24;
    return true;
  }
  if (strstr(buf, "BEACON_5G") || strstr(buf, "AT+BEACON5G")) {
    cmd.type = UART_CMD_BEACON_5G;
    return true;
  }
  if (strstr(buf, "CMD:STOP") || strstr(buf, "STOP") || strstr(buf, "AT+STOP")) {
    cmd.type = UART_CMD_STOP;
    return true;
  }
  char *targetPos = strstr(buf, "DEAUTH_TARGET:");
  if (targetPos) {
    cmd.type = UART_CMD_DEAUTH_TARGET;
    strncpy(cmd.payload, targetPos + 14, sizeof(cmd.payload) - 1);
    cmd.payload[sizeof(cmd.payload) - 1] = '\0';
    return true;
  }

  return false;
}

bool uartCheckMasterHandshake(uint32_t timeoutMs) {
  uint32_t start = millis();
  Serial.println(F("SLAVE_ANNOUNCE:RAONE_5G"));
  
  while (millis() - start < timeoutMs) {
    UartCommand cmd = uartPollCommand();
    if (cmd.type == UART_CMD_PING || _currentMode == SYS_MODE_SLAVE) {
      return true;
    }
    delay(10);
  }
  
  _currentMode = SYS_MODE_STANDALONE;
  return false;
}

UartCommand uartPollCommand() {
  UartCommand cmd;
  cmd.type = UART_CMD_NONE;
  cmd.payload[0] = '\0';

  while (Serial.available()) {
    char c = (char)Serial.read();
    
    // Ignore leading nulls/garbage
    if (_rxIdx == 0 && (c == '\0' || c == '\r' || c == '\n' || (uint8_t)c > 127)) {
      continue;
    }

    if (c == '\n' || c == '\r') {
      if (_rxIdx > 0) {
        _rxBuf[_rxIdx] = '\0';
        bool matched = checkBufferMatch(_rxBuf, cmd);
        _rxIdx = 0;
        if (matched) return cmd;
      }
    } else if (_rxIdx < sizeof(_rxBuf) - 1) {
      _rxBuf[_rxIdx++] = c;
      _rxBuf[_rxIdx] = '\0';

      // Check on-the-fly keyword matching without waiting for newline
      if (_rxIdx >= 4 && checkBufferMatch(_rxBuf, cmd)) {
        _rxIdx = 0;
        return cmd;
      }
    } else {
      // Buffer full without match -> reset
      _rxIdx = 0;
    }
  }

  // ── Heartbeat beacon: announce ourselves every 3 seconds ──
  if (millis() - _lastBeaconMs >= 3000) {
    _lastBeaconMs = millis();
    Serial.print("RAONE_READY\n");
    Serial.flush();
  }

  return cmd;
}
