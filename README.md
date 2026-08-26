# RAONE-5G Deauther

Dual-band (2.4GHz + 5GHz) Wi-Fi auditor, scanner, and deauther built for the RTL8720DN (BW16 / NiceMCU V1).

## Status: UNDER PROCESS (WIP)

Currently stabilizing the 5GHz scanning and AP logic. The core dual-band unlock is completed. Next steps involve refining packet injection and the UI workflows.

### Hardware Note (NiceMCU RTL8720DN)
If you are using the NiceMCU RTL8720DN board, the factory often populates the 0-ohm resistor for the internal PCB antenna AND the IPEX connector simultaneously. To get a usable 5GHz signal, you MUST remove the top 0-ohm resistor routing to the PCB antenna so 100% of the RF power goes to the IPEX connector.

---

## Version History

### v8.0 (Current)
* **[NEW] Cyber Glitch & Biometric Touch Security Lock:**
  * **Stealth Boot Lock:** Power-on triggers an audiovisual cyber glitch distortion on the bold `Ra-One` title with synchronized digital frequency bursts, followed by a tight single-line cycling subtitle animation (`github.com/mxsourav` and `instagram/mxsourav.dev`).
  * **Biometric 1.5s Touch Unlock:** The device runs in complete stealth with zero prompt text. The system only unlocks and boots when the TTP223 capacitive touch sensor (`PB_20`) is held continuously for 1.5 seconds.
  * **Escalating Audio & Reactive LED Feedback:** Features rising frequency synthesizer charging audio and a progressive **RED $\rightarrow$ YELLOW $\rightarrow$ GREEN** status LED charge-up sequence during biometric authentication.
  * **Ambient Noticeable Glitch:** In-place letter-corruption distortion with horizontal micro-tears and synchronized audio chirps occurs subtly during standby without moving the word position.
* **[NEW] Client Sniffing & Target Action Menu:**
  * Interactive 6-second Promiscuous Client Sniffer with dual-band channel plan (`0x7F`) to discover active station MAC addresses connected to target APs.
  * Dedicated interactive Client List UI with individual client targeting and real-time countdown timer.
* **[NEW] Multi-Reason Channel Hopping Engine:**
  * Hopping deauth across 2.4GHz & 5GHz channels sends dual-direction spoofed management frames (`AP` $\rightarrow$ `Broadcast` & `Client` $\rightarrow$ `AP`) across multi-vendor 802.11 reason codes (`0x0007`, `0x0002`, `0x0006`, `0x0001`) to forcefully drop all client device types (iOS, Android, Windows, IoT).
* **[NEW] System Settings Screen:**
  * Interactive toggles for Buzzer (`ON/OFF`) and Status LEDs (`ON/OFF`) on the fly.
* **[RESOLVED] Hardware Pin Isolation & Solid Purple Onboard RGB:**
  * Isolated onboard RGB PWM registers (`PA12`, `PA13`, `PA14`) from external status LEDs so the onboard diode stays 100% rock-solid deep purple (`#7b00ff`) without color shifting during scans, idle, or attacks.
  * Discrete external 3-LED repeating rhythm (**RED** $\rightarrow$ **GREEN** $\rightarrow$ **YELLOW**) during boot and scan melodies with zero bleed during rests.
* **[RESOLVED] NAV Button Smooth Hold Auto-Scroll:**
  * Added hardware auto-repeat scrolling (350ms initial delay + 200ms repeat interval) for smooth menu and network list navigation.
* **[RESOLVED] Network Scan Filtering:**
  * Re-scan cleans promiscuous radio state and strictly discards empty/non-printable `<hidden>` SSIDs.

### v7.0
* **[NEW] Dual-Mode Architecture (Standalone / Slave):**
  * **Auto-Detect Handshake:** Listens on `Serial` (115200 baud, 8-N-1) for 2.0 seconds during boot.
  * **Slave Mode (with TetraX Master):** Links with TetraX ESP32 as a dedicated 5GHz coprocessor. Accepts remote navigation (`CMD:NAV`, `CMD:OK`, `CMD:BACK`) and dual-band attack triggers over UART.
  * **Standalone Mode:** If no master is detected, smoothly enters standalone mode with onboard OLED, TTP223 touch sensor, and physical button controls.
* **[NEW] All-Channel Hopping Modes:**
  * Added direct standalone and remote support for 5GHz all-channel hopping (Ch 36-161) and 2.4GHz all-channel hopping.
* **[RESOLVED] 5GHz Raw TX Deadlock / Freeze:**
  * **Root Cause Fixed:** Resolved the ~96-packet CPU crash caused by Realtek `alloc_mgtxmitframe()` driver queue overflows during raw frame injection.
  * **Hardware Promiscuous Mode:** Enabled `wifi_set_promisc(RTW_PROMISC_ENABLE_2)` before all raw Wi-Fi transmission attacks to bypass MAC filtering, allowing 5GHz management frames to transmit OTA immediately.
  * **DMA Queue Throttling:** Implemented a strict 20-packet burst with 100ms yield to ensure the hardware DMA completely empties the descriptor ring.
* **Touch Sensor Debounce & Hold:**
  * Configured `BTN_LONG_PRESS_MS` to 2500ms (2.5s hold required for universal back/cancel) to eliminate accidental touch sensor triggers.
  * Short touch activates standard `OK` / selection without triggering accidental menu exits.
* **Beacon Mobile Compatibility:**
  * Integrated mandatory 802.11 TIM (IE 5) element in `buildBeacon()` for full Android and iOS beacon parser compatibility.

### v1.3.0
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
