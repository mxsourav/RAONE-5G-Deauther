# Graph Report - D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16  (2026-08-24)

## Corpus Check
- 30 files · ~2,584,111 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 223 nodes · 428 edges · 28 communities detected
- Extraction: 90% EXTRACTED · 10% INFERRED · 0% AMBIGUOUS · INFERRED: 41 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]

## God Nodes (most connected - your core abstractions)
1. `oledFlush()` - 28 edges
2. `drawStatusBar()` - 24 edges
3. `drawFooter()` - 23 edges
4. `oledClear()` - 21 edges
5. `printTruncated()` - 13 edges
6. `uiRefreshWifiAnalyzer()` - 12 edges
7. `uiDrawNetworkList()` - 9 edges
8. `uiRefreshBleAnalyzer()` - 9 edges
9. `uiDrawNetworkDetails()` - 8 edges
10. `uiDrawTargetMonitor()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `beaconSpamSsidCount()` --calls--> `uiRefreshBeaconSpam()`  [INFERRED]
  D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\BeaconSpam.cpp → D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\DisplayUi.cpp
- `beaconSpamCurrentSsid()` --calls--> `uiRefreshBeaconSpam()`  [INFERRED]
  D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\BeaconSpam.cpp → D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\DisplayUi.cpp
- `bleSpamCount()` --calls--> `uiRefreshBleSpam()`  [INFERRED]
  D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\BleSpam.cpp → D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\DisplayUi.cpp
- `bleSpamCurrent()` --calls--> `uiRefreshBleSpam()`  [INFERRED]
  D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\BleSpam.cpp → D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\DisplayUi.cpp
- `bleKindLabel()` --calls--> `uiDrawBleDetails()`  [INFERRED]
  D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\BluetoothScanner.cpp → D:\Gaar Fata Project\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16-5Ghz-main\BWifiKill-BW16\bw16\DisplayUi.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.15
Nodes (42): bleCopyDevice(), bleCount(), drawFooter(), drawRssiBar(), drawStatusBar(), oledClear(), oledFlush(), printTruncated() (+34 more)

### Community 1 - "Community 1"
Cohesion: 0.1
Nodes (19): beginDeviceWrite(), bleAnalyzerReset(), bleAnalyzerStatus(), bleAvgRssi(), bleBaseline(), bleKindLabel(), blePps(), blePpsHistory() (+11 more)

### Community 2 - "Community 2"
Cohesion: 0.19
Nodes (22): uiBegin(), buzzerBeep(), buzzerBootMelody(), buzzerClick(), buzzerError(), buzzerScanDone(), buzzerSuccess(), hwBegin() (+14 more)

### Community 3 - "Community 3"
Cohesion: 0.13
Nodes (17): isLikelyPmfProtected(), labTestEvaluate(), labTestPrepare(), labTestSimulateDeauth(), missPercent(), wifiScannerBusiestChannel(), wifiScannerCount(), wifiScannerCountBand() (+9 more)

### Community 4 - "Community 4"
Cohesion: 0.15
Nodes (16): uiRefreshWifiAnalyzer(), channel5IndexOf(), promiscOnFrame(), sniffResetStats(), sniffStart(), wifiAnalyzerBand(), wifiAnalyzerBaseline(), wifiAnalyzerHistory() (+8 more)

### Community 5 - "Community 5"
Cohesion: 0.15
Nodes (7): beaconSpamCurrentSsid(), beaconSpamSsidCount(), beaconSpamTick(), buildBeacon(), wifi_tx_beacon_frame(), wifi_tx_deauth_frame(), wifi_tx_raw_frame()

### Community 6 - "Community 6"
Cohesion: 0.21
Nodes (9): applyAdvertAndAddr(), bleSpamCount(), bleSpamCurrent(), bleSpamStart(), bleSpamStop(), bleSpamTick(), genRandomMac(), bleActive() (+1 more)

### Community 7 - "Community 7"
Cohesion: 0.33
Nodes (8): clientScanPoll(), clientScanStart(), clientScanStop(), clientSniffCallback(), findClient(), isBroadcastMac(), isBssidMac(), macToStr()

### Community 8 - "Community 8"
Cohesion: 0.36
Nodes (6): irCodeCount(), irCodeName(), irMark(), irSendNec(), irSpace(), irTransmit()

### Community 9 - "Community 9"
Cohesion: 0.4
Nodes (2): labStatsAverageRssi(), labStatsPrintToSerial()

### Community 10 - "Community 10"
Cohesion: 0.5
Nodes (0): 

### Community 11 - "Community 11"
Cohesion: 0.67
Nodes (2): uiDrawSplashProgress(), splashDrawProgress()

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (0): 

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (0): 

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (0): 

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (0): 

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (0): 

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (0): 

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (0): 

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

### Community 20 - "Community 20"
Cohesion: 1.0
Nodes (0): 

### Community 21 - "Community 21"
Cohesion: 1.0
Nodes (0): 

### Community 22 - "Community 22"
Cohesion: 1.0
Nodes (0): 

### Community 23 - "Community 23"
Cohesion: 1.0
Nodes (0): 

### Community 24 - "Community 24"
Cohesion: 1.0
Nodes (0): 

### Community 25 - "Community 25"
Cohesion: 1.0
Nodes (0): 

### Community 26 - "Community 26"
Cohesion: 1.0
Nodes (0): 

### Community 27 - "Community 27"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **Thin community `Community 12`** (1 nodes): `BeaconSpam.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (1 nodes): `BleSpam.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (1 nodes): `BluetoothScanner.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (1 nodes): `ClientScanner.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (1 nodes): `Config.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (1 nodes): `DisplayUi.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `HardwareManager.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `IrBlaster.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 20`** (1 nodes): `LabStats.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 21`** (1 nodes): `LabTestEngine.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 22`** (1 nodes): `packet-injection.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 23`** (1 nodes): `PromiscuousSniffer.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 24`** (1 nodes): `SplashScreen.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 25`** (1 nodes): `TargetManager.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 26`** (1 nodes): `Theme.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 27`** (1 nodes): `WifiScanner.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `oledFlush()` connect `Community 0` to `Community 1`, `Community 2`, `Community 11`, `Community 4`?**
  _High betweenness centrality (0.174) - this node is a cross-community bridge._
- **Why does `uiBegin()` connect `Community 2` to `Community 0`?**
  _High betweenness centrality (0.142) - this node is a cross-community bridge._
- **Why does `uiRefreshWifiAnalyzer()` connect `Community 4` to `Community 0`?**
  _High betweenness centrality (0.133) - this node is a cross-community bridge._
- **Should `Community 1` be split into smaller, more focused modules?**
  _Cohesion score 0.1 - nodes in this community are weakly interconnected._
- **Should `Community 3` be split into smaller, more focused modules?**
  _Cohesion score 0.13 - nodes in this community are weakly interconnected._