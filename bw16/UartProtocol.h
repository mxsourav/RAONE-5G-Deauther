#pragma once

#include <Arduino.h>

enum SystemMode {
  SYS_MODE_STANDALONE = 0,
  SYS_MODE_SLAVE      = 1
};

enum UartCommandType {
  UART_CMD_NONE = 0,
  UART_CMD_PING,
  UART_CMD_NAV,
  UART_CMD_OK,
  UART_CMD_BACK,
  UART_CMD_DEAUTH_TARGET,
  UART_CMD_DEAUTH_ALL_24,
  UART_CMD_DEAUTH_ALL_5G,
  UART_CMD_BEACON_24,
  UART_CMD_BEACON_5G,
  UART_CMD_STOP,
  UART_CMD_STATUS_REQ
};

struct UartCommand {
  UartCommandType type;
  char payload[64];
};

void uartProtocolBegin(unsigned long baud = 115200);
bool uartCheckMasterHandshake(uint32_t timeoutMs = 2000);
UartCommand uartPollCommand();
void uartSendStatus(const char *status, uint32_t val = 0);
void uartSendLiveStats(uint8_t ch, uint32_t sent, uint32_t fail, uint16_t pps);
void uartSendPong();
SystemMode getSystemMode();
void setSystemMode(SystemMode mode);
