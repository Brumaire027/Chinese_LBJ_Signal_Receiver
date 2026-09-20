# Chinese LBJ Signal Receiver

An **ESP32 + SX1276** railway LBJ signal receiver with train message decoding, SD card recording, and stationary and onboard operating modes.

This project is adapted from FLN1021's `SX1276_Receive_LBJ` project.

## Overview

The receiver uses the SX1276 in **FSK direct mode** to receive LBJ / POCSAG messages at approximately **821.2375 MHz**, with a data rate of **1200 bps**.

Depending on the message type and signal quality, decoded information may include:

- Train number, direction, speed, and kilometer post
- Locomotive number, route name, and position data
- Time information carried in the message

Received messages may be corrupted, partially decoded, or incorrectly corrected, resulting in unreliable information. Placeholders such as `<NUL>`, `NA`, or `********`, or fragments of these placeholders, indicate that the corresponding field is unavailable or has not been successfully decoded. This can result from missing data or reception errors.

## Features

- Four-button operation
- Stationary and onboard modes
- SD card reception records and operating logs
- Separate recording files for onboard sessions
- Recent reception history
- DS3231 real-time clock, manual time adjustment, and NTP synchronization
- Phone-based Wi-Fi setup and Telnet output
- LED and buzzer controls
- Display brightness, automatic screen sleep, and wake-on-arrival settings
- Optional low-battery alerts, disabled by default
- SD card error alerts
- Reception diagnostics page

## Hardware and Pin Mapping

The current firmware targets the V2 PCB, using:

- ESP32-WROOM-32-E with 4 MB flash
- E32-900M20S (SX1276) radio module
- 128 × 64 SSD1306 OLED
- DS3231 RTC
- SD card
- Four buttons, an LED, a buzzer, and a TP4056 charging module

Pin assignments are defined in `src/utilities.h`.

| Function | ESP32 GPIO |
| --- | --- |
| E32 CS | 13 |
| SPI SCK | 14 |
| SPI MOSI | 27 |
| SPI MISO | 39 |
| E32 RST | 26 |
| E32 DIO0 | 33 |
| E32 DIO1 | 35 |
| E32 DIO2 | 32 |
| SD card CS | 16 |
| OLED / DS3231 SDA | 21 |
| OLED / DS3231 SCL | 22 |
| KEY1: Confirm | 23 |
| KEY2: Up | 19 |
| KEY3: Down | 18 |
| KEY4: Back | 17 |
| LED | 2 |
| Buzzer | 25 |
| Battery voltage sensing | 34 |
| Serial TX / RX | 1 / 3 |

The radio module and SD card share the SPI bus, with separate chip-select pins. The OLED and DS3231 share the I²C bus.

## Button Controls

| Button | Function |
| --- | --- |
| KEY1 | Open the menu; confirm |
| KEY2 | Move up or adjust an option |
| KEY3 | Move down or adjust an option |
| KEY4 | Return to the previous screen; hold to exit the menu |

From the main screen, KEY2 and KEY3 open reception history. When the display is asleep, the first button press wakes it.

## Menu

The device interface uses Chinese labels. English names below describe the corresponding menu items.

| Order | Menu | Function |
| --- | --- | --- |
| 1 | Operating Mode (使用模式) | Switch between stationary and onboard modes; select your train |
| 2 | Sound and Light (声光设置) | Enable or disable the arrival LED and buzzer separately |
| 3 | Display Settings (显示设置) | Brightness, automatic sleep, wake on arrival, and restore display defaults |
| 4 | Time Adjustment (时间调整) | Set the date and time manually |
| 5 | Network Settings (网络设置) | Connection status, phone-based setup, reconnect, and remote connection toggle |
| 6 | Battery Alert (电量提示) | Enable or disable low-battery alerts; disabled by default |
| 7 | Logs and Storage (日志存储) | Card status, log status, safe unmount, and remount |
| 8 | History (历史记录) | View up to 20 recent records from the latest reception CSV file |
| 9 | System Status (系统状态) | Uptime, memory, and related status; press KEY1 to open reception diagnostics |
| 10 | About (关于设备) | Firmware version, author, and build date |

## Operating Modes

### Stationary Mode

For observing nearby trains from a fixed location.

Repeated messages from the same train update its information, while duplicate suppression reduces repeated alerts. When several trains are received, a display queue reduces frequent switching between them.

### Onboard Mode

For monitoring the train you are riding.

After selecting your train and confirming that recording should begin:

- The main screen prioritizes the selected train.
- Other trains are still received in the background and included in the general reception records.
- A separate CSV file is created for each onboard session.
- If reception from your train stops, the last data remains visible along with the elapsed time since reception. The device does not automatically switch to another train.

Ending onboard mode returns the device to stationary mode. **Every restart also returns it to stationary mode.**

## Wi-Fi Setup

### Default Network

When no Wi-Fi configuration has been saved, the device uses these defaults:

```text
SSID: LBJ
Password: 123456789
```

The default credentials are configured in `src/networks.hpp`:

```cpp
#define WIFI_SSID       "LBJ"
#define WIFI_PASSWORD   "123456789"
```

If a configuration already exists, the saved network takes priority. A connection failure does not cause the device to switch back to the default network or overwrite the saved credentials.

### Setup Using a Phone

1. Open **Network Settings → Phone Setup** (网络设置 → 手机配网).
2. Connect your phone to the device hotspot, `LBJ-Receiver`.
3. If the setup page does not open automatically, visit `http://192.168.4.1` in a browser.
4. Select your router or phone hotspot, enter its password, and save.
5. Once connected, the device displays a confirmation and returns to the main screen.

Use a 2.4 GHz Wi-Fi network. The device does not automatically enter setup mode; reception and recording remain available offline.

The Telnet address is shown on the connection status page. The port is **23**.

## Timekeeping

The device uses a **DS3231** RTC. Displayed time is Beijing time, **UTC+8**.

- RTC time is checked for validity at startup.
- After a valid NTP synchronization, the system clock is updated and the firmware attempts to write the time back to the DS3231.
- Without a network connection, use the Time Adjustment menu to set the clock manually.
- Invalid time is not treated as a reliable timestamp.

## SD Card Recording

| Directory | Contents |
| --- | --- |
| `/LOGS` | Operating and debug logs |
| `/RECORDS` | General reception CSV records |
| `/RIDES` | Separate CSV files for individual onboard sessions |

General reception recording continues in onboard mode. The history page reads up to 20 recent records from the latest CSV file in `/RECORDS`.

Before removing the card, open **Logs and Storage → Safe Unmount** (日志存储 → 安全卸载) and wait for the “Safe to remove card” message (可以取卡). After reinserting the card, select **Remount** (重新挂载).

Write failures, capacity-query errors, or insufficient free space cause a blinking **triangle containing an exclamation mark** to appear in the main screen's bottom bar. Check the Logs and Storage pages for more information. The warning disappearing does not mean that previously missed records have been recovered.

## Reception Diagnostics

Open **System Status**, then press KEY1 to enter the reception diagnostics page.

The page displays:

- Current reception frequency
- Live RSSI
- Received batch count, labeled “收”
- Successfully decoded batch count, labeled “成”
- Time elapsed since the most recent reception

RSSI is sampled approximately every half second, and the page refreshes every second. Sampling is deferred while reception processing is busy. Counters accumulate from startup, remain unchanged when leaving the page, and reset on reboot.

These counters represent reception batches, not the number of distinct trains. RSSI includes background noise and other signals; a changing value does not necessarily indicate successful reception of a train message.

## Building and Flashing

Use VS Code with PlatformIO. Open the project root containing `platformio.ini` and select the `esp32dev` environment.

Build:

```bash
pio run -e esp32dev
```

Flash:

```bash
pio run -e esp32dev -t upload
```

Serial monitor:

```bash
pio device monitor -b 115200
```

The serial baud rate is **115200**.

This project includes modified local copies of RadioLib and the SD library. Keep the repository's `lib` directory when building.

## Frequently Asked Questions

### Why does the screen keep showing “Waiting for train signal”?

Reception depends on nearby transmitters, the antenna, location, frequency offset, and hardware connections.

Check the reception diagnostics page first:

- **Received count stays at zero:** no reception batch has become available for the program to read.
- **Received count increases but decoded count does not:** messages may contain errors, be incomplete, or be unrecognized.
- **RSSI changes but no records appear:** the signal may be noise or other radio traffic.

If the radio fails to initialize, inspect the startup log through the serial monitor.

### Why does the device still use the old Wi-Fi network after flashing?

Wi-Fi credentials are stored internally and are normally retained during firmware uploads. Use Phone Setup to save a new network.

### Why does the receiver work without a network connection?

Radio reception, OLED display, and SD recording do not depend on Wi-Fi. Networking is mainly used for setup, NTP time synchronization, and Telnet output.

## Acknowledgements

This project is adapted from FLN1021's `SX1276_Receive_LBJ`. Thanks to the original author and the contributors to these open-source projects:

- RadioLib
- LilyGo LoRa Series
- U8g2
- Arduino-ESP32
- WiFiManager
- ESP Telnet
- RTClib
- ESP32AnalogRead

## Usage Notice

This project is intended for learning and experimentation. It must not be used as a basis for railway operations, dispatching, or personal safety decisions. Reception and decoding results may be incomplete or inaccurate because of signal quality and message variations. Use it with care.
