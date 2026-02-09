# SX1276_Receive_LBJ
A LBJ Message Receiver Based on TTGO LoRa 32 v1.6.1 (ESP32 + SX1276 868MHz)

The SX1276_Receive_LBJ project is modified from [RadioLib](https://github.com/jgromes/RadioLib)'s Pager_Receive.ino and 
[LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series)'s templates,
based on the TTGO LoRa 32 v1.6.1 dev board.
This project aims to provide an alternative solution to the expensive programmable 
pagers commonly used in receiving the Chinese Railway Train Proximity Alarm
(aka LBJ, on 821.2375MHz following POCSAG protocol) messages often transmitted 
by on board LBJ systems through CIR.

## Notice
This is an experimental project to learn embedded device programming and sub-GHz signal
receives, thus have bad coding quality and may have unknown issues. Use it at your own risk.

## Hardware
Based on [TTGO LoRa 32 v1.6.1 dev board](https://www.lilygo.cc/products/lora3), utilizes SX1276's FSK modem to 
receive LBJ messages.
The schematic of this board can be found [here](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/schematic/T3_V1.6.1.pdf).

### Additional Functions
***Note: This is optional and requires additional hardware to be connected to the board.***

Additional functions can be enabled by connecting specific peripheral to the specified pin as following:

#### 1. DS3231 RTC
Share IIC Bus with on board SSD1306 OLED.
```c++
#define I2C_SDA                     21
#define I2C_SCL                     22
```
Keeps time while power off. If not used, the time will be acquired from NTP server every time after reboot.
If connected uncomment the following line in [utilities.h](src/utilities.h) while building.
```c++
#define HAS_RTC // soldered an external DS3231 module.
```

#### 2. AD Buttons (WIP)
Due to lack of GPIO a four key analog button is used. 
```c++
#define BUTTON_PIN                  34
```
Provides input to interactive functions including logs inspection and device settings on OLED(WIP). If connected use
the ***button_equipped*** branch instead of master while building.

More information about pin definitions can be found in [utilities.h](src/utilities.h)

## Features
- Shows formatted LBJ messages on OLED display.
- Save Received messages to TF card in plain text and CSV format.
- Host Telnet server and send formatted messages to client.
- BCH error correction migrated from MMDVM_HS_Hat's [BCH3121.cpp](https://github.com/phl0/POCSAG_HS/blob/master/BCH3121.cpp)

## Known Issues
- Takes a very long time to startup if a large number of files are in TF card.
- TF card hot-plug is **NOT** supported. Unplug while power on will cause crash after next message receive.
- Unstable WiFi connection.
- No multi-client support for Telnet service.
- No buzzers for alert.
- Partially decoded or corrupted message will show on display.
- Documentation and license are preliminary, needs to clean up and complete.

## Details
### 1. About LBJ extend message
It does not appear on `TB/T 3504-2018` standard.
I received this kind of messages using SDR while listening to 821.2375MHz. Some train transmits them on
POCSAG address `1234002`. The formal name and structure of these messages is unknown, the contents are currently
identified by guessing. Current identified information in one typical LBJ extend info message is listed in the following
table.

| Nibbles(4bit) | Encode  | Meaning                                                                         |
|---------------|---------|---------------------------------------------------------------------------------|
| 0-3           | ASCII   | Type                                                                            |
| 4-11          | Decimal | 8 digit locomotive registry number                                              |
| 12-13         | Unknown | Locomotive ends, 31 for A end, 32 for B end, 30 for no A/B end or unregistered. |
| 14-29         | GB2312  | Route                                                                           |
| 30-38         | Decimal | Longitude (XXX°XX.XXXX′ E)                                                      |
| 39-46         | Decimal | Latitude (XX°XX.XXXX′ N)                                                        |
| 47-49         | Unknown | Unknown, usually 000                                                            |

In total of 50 nibbles / 200 bits, transmitted in 10 POCSAG frames.

### 2. SX1276 Configuration
- Freq = 821.2375MHz + 6 ppm (default)
- Mode = FSK, RxDirect (DIO2)
- Gain = 001 + LnaBoostHf (AGC off)
- RxBW = 12.5 kHz

Due to lack of TCXO on the SX1276 module used by the dev board, an automatic frequency adjustment
mechanism is implemented by measuring the frequency error of the carrier and preamble received using SX1276's frequency
error indicator (FEI). It tries to lock on the signal after receiving a carrier or preamble, thus compensate for the
frequency error caused by crystal or the transmitter. This mechanism can be disabled via serial command `afc off`.

### 3. Telnet/Serial Commands
#### Serial
- `ping` Serial state test command, returns pong.
- `time` Returns system time.
- `sd end` Unmount TF card.
- `sd begin` Mount TF card.
- `mem` Returns memory left. (by `esp_get_free_heap_size()`)
- `rst` Returns reset reason.
- `ppm read` Returns current ppm.
- `afc off` Disable automatic frequency correction.
- `afc on` Enable automatic frequency correction.

#### Telnet
- `ping` Telnet state test command, returns pong.
- `read` Returns 1000 bytes of data from log on TF card.
- `log read [int]` Returns `[int]` bytes of data from log on TF card.
- `log status` Returns if TF log is enabled.
- `afc off` Disable automatic frequency correction.
- `afc on` Enable automatic frequency correction.
- `ppm [float]` Set ppm to `[float]`.
- `bat` Returns battery voltage.
- `rssi` Returns current RSSI from SX1276 module.
- `gain` Returns current gain of SX1276 module.
- `time` Returns system time.
- `bye` Disconnect from Telnet.

### 4. Reception Failure
Sometimes the received messages may be corrupt, partially decoded or wrongly corrected, thus may display unreliable results.
If `<NUL>`, `NA`, `********` or part of these characters shows up, it means this part of the message is corrupted.

## Libraries and codes used/referenced
[//]: # (todo: add links)
- [RadioLib](https://github.com/jgromes/RadioLib) (Modified)
- [U8G2](https://github.com/olikraus/u8g2)
- [ESP32-Arduino](https://github.com/espressif/arduino-esp32)
- [PlatformIO](https://platformio.org/)
- [ESP32AnalogRead](https://github.com/madhephaestus/ESP32AnalogRead.git) (for battery voltage checking)
- BCH3121.cpp/.h from [POCSAG_HS](https://github.com/phl0/POCSAG_HS)
- [LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series)
- [ESPTelnet](https://github.com/LennartHennigs/ESPTelnet)
- [RTClib](https://github.com/adafruit/RTClib.git)
- [Multimon-ng](https://github.com/EliasOenal/multimon-ng)
- Project inspired by [GoRail_Pager](https://github.com/killeder/GoRail_Pager).
- More if I can remember, I apologize for that.

Huge thanks to all authors and contributors.