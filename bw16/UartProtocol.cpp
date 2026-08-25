#include "UartProtocol.h"
#include <string.h>

static SystemMode _currentMode = SYS_MODE_STANDALONE;
static char _rxBuffer[128];
static uint8_t _rxIndex = 0;

void uartProtocolBegin(unsigned long baud) {
  Serial.begin(baud);
}

SystemMode getSystemMode() {
  return _currentMode;
}

void setSystemMode(SystemMode mode) {
  _currentMode = mode;
}

void uartSendPong() {
  Serial.println(F("PONG_RAONE_SLAVE_READY"));
}

void uartSendStatus(const char *status, uint32_t val) {
  Serial.print(F("STATUS:"));
  Serial.print(status);
  if (val > 0) {
    Serial.print(F(":"));
    Serial.print(val);
  }
  Serial.println();
}

bool uartCheckMasterHandshake(uint32_t timeoutMs) {
  uint32_t start = millis();
  
  // Announce presence on UART
  Serial.println(F("SLAVE_ANNOUNCE:RAONE_5G"));
  
  while (millis() - start < timeoutMs) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (_rxIndex > 0) {
          _rxBuffer[_rxIndex] = '\0';
          if (strstr(_rxBuffer, "PING_RAONE") || 
              strstr(_rxBuffer, "HANDSHAKE_REQ") || 
              strstr(_rxBuffer, "MASTER_TETRAX")) {
            uartSendPong();
            _currentMode = SYS_MODE_SLAVE;
            _rxIndex = 0;
            return true;
          }
          _rxIndex = 0;
        }
      } else if (_rxIndex < sizeof(_rxBuffer) - 1) {
        _rxBuffer[_rxIndex++] = c;
      }
    }
    delay(10);
  }
  
  _currentMode = SYS_MODE_STANDALONE;
  _rxIndex = 0;
  return false;
}

UartCommand uartPollCommand() {
  UartCommand cmd;
  cmd.type = UART_CMD_NONE;
  cmd.payload[0] = '\0';

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (_rxIndex > 0) {
        _rxBuffer[_rxIndex] = '\0';

        if (strcmp(_rxBuffer, "CMD:NAV") == 0 || strcmp(_rxBuffer, "NAV") == 0) {
          cmd.type = UART_CMD_NAV;
        } else if (strcmp(_rxBuffer, "CMD:OK") == 0 || strcmp(_rxBuffer, "OK") == 0) {
          cmd.type = UART_CMD_OK;
        } else if (strcmp(_rxBuffer, "CMD:BACK") == 0 || strcmp(_rxBuffer, "BACK") == 0) {
          cmd.type = UART_CMD_BACK;
        } else if (strcmp(_rxBuffer, "CMD:DEAUTH_ALL_24") == 0) {
          cmd.type = UART_CMD_DEAUTH_ALL_24;
        } else if (strcmp(_rxBuffer, "CMD:DEAUTH_ALL_5G") == 0) {
          cmd.type = UART_CMD_DEAUTH_ALL_5G;
        } else if (strcmp(_rxBuffer, "CMD:BEACON_24") == 0) {
          cmd.type = UART_CMD_BEACON_24;
        } else if (strcmp(_rxBuffer, "CMD:BEACON_5G") == 0) {
          cmd.type = UART_CMD_BEACON_5G;
        } else if (strcmp(_rxBuffer, "CMD:STOP") == 0 || strcmp(_rxBuffer, "STOP") == 0) {
          cmd.type = UART_CMD_STOP;
        } else if (strstr(_rxBuffer, "PING_RAONE") || strstr(_rxBuffer, "HANDSHAKE_REQ")) {
          uartSendPong();
          _currentMode = SYS_MODE_SLAVE;
        } else if (strncmp(_rxBuffer, "CMD:DEAUTH_TARGET:", 18) == 0) {
          cmd.type = UART_CMD_DEAUTH_TARGET;
          strncpy(cmd.payload, _rxBuffer + 18, sizeof(cmd.payload) - 1);
        }

        _rxIndex = 0;
        return cmd;
      }
    } else if (_rxIndex < sizeof(_rxBuffer) - 1) {
      _rxBuffer[_rxIndex++] = c;
    }
  }

  return cmd;
}
