# RAONE-5G Deauther

Dual-band (2.4GHz + 5GHz) Wi-Fi auditor, scanner, and deauther built for the RTL8720DN (BW16 / NiceMCU V1).

## Status: UNDER PROCESS (WIP)

Currently stabilizing the 5GHz scanning and AP logic. The core dual-band unlock is completed. Next steps involve refining packet injection and the UI workflows.

### Hardware Note (NiceMCU RTL8720DN)
If you are using the NiceMCU RTL8720DN board, the factory often populates the 0-ohm resistor for the internal PCB antenna AND the IPEX connector simultaneously. To get a usable 5GHz signal, you MUST remove the top 0-ohm resistor routing to the PCB antenna so 100% of the RF power goes to the IPEX connector.

---

## Version History

### v1.3.0 (Current)
* **[RESOLVED] 5GHz Hardware & Software Bug:** The notorious 5GHz issue is finally fixed! 
  1. **Software:** Bypassed the AmebaD SDK weak symbol bug. Forced channel plan `0x25` (FCC dual-band) directly after `wifi_on` to prevent IPC deadlocks.
  2. **Hardware:** Identified the factory defect (0-ohm resistor splitting the RF signal). Removing it allows full 5GHz range on the external antenna.
* **Dual-Band Scanning:** Swapped standard scan for `wifi_scan_networks_mcc()` to successfully hit all 38 channels across 2.4GHz and 5GHz.

### v1.2.0
* **Boot Deadlock Fixed:** Patched the `#calibration_ok` hang by properly initializing the UART before the Wi-Fi driver attempts to log to it.
* **I2C Crash Fix:** Prevented the Adafruit SSD1306 library from calling `Wire.begin()` internally to stop random hardware resets on AmebaD.
* **[Remaining Bug]:** 5GHz scanning is still failing. The radio remains stubbornly locked to 2.4GHz channels, and hardware RF signal is heavily degraded.

### v1.1.0
* **SH1106 OLED Fix:** Implemented custom bit-bang display flush to fix the 2-pixel column wrap glitch common on 1.3-inch SH1106 displays.
* **Input Mapping:** Consolidated navigation to a single-button/touch-sensor input scheme.
* **[Remaining Bug]:** 5GHz scanning is still failing. No 5GHz networks are visible due to SDK and hardware trace issues.

### v1.0.0
* Ported baseline Wi-Fi scanner and deauther logic to the RTL8720DN Ameba SDK.
* Implemented the 128x64 OLED UI and basic status LEDs.
* **[Known Issue]:** 5GHz is completely broken. Scanning fails due to default regulatory domain restrictions locking the radio to 2.4GHz channels, compounded by the hardware IPEX trace splitting the signal.
