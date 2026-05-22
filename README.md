# Chinese LBJ Signal Receiver

A DIY ESP32 + SX1276 based receiver for Chinese Railway LBJ / POCSAG messages.

This project is modified from FLN1021's original `SX1276_Receive_LBJ` project. The original repository is no longer publicly available, but a public mirror can be found here:

https://github.com/sliverwolf233/SX1276_Receive_LBJ

The original project was designed for the TTGO LoRa 32 v1.6.1 board. This repository adapts the receiver to a generic ESP32 development board with an external SX1276 LoRa module, and adds several hardware and usability changes.


## What It Does

This project receives LBJ-related POCSAG messages around **821.2375 MHz** using an SX1276 radio module in FSK direct mode.

Currently supported functions include:

- Receiving and decoding LBJ / POCSAG messages
- Displaying decoded information on an OLED screen
- Saving received messages to SD card logs
- Outputting decoded messages through serial
- Optional Telnet output over WiFi
- Basic buzzer alert support

Decoded information may include train number, direction, speed, kilometer post, locomotive number, route name, and position data, depending on the received message type and signal quality.


## Hardware

The current version is intended for a DIY setup based on:

- ESP32 development board
- SX1276 LoRa module
- SSD1306 OLED display
- SD card module
- Optional RTC module
- Optional buzzer

Pin definitions and feature switches are mainly configured in `platformio.ini`.


## Build

This project uses PlatformIO.

Build:

```bash
pio run
```

Upload:

```bash
pio upload
```

Serial monitor:

```bash
pio device monitor
```

Default serial baud rate:

```text
115200
```


## WiFi

This version uses WiFiManager.

If no saved WiFi credentials are available, the device starts a configuration access point:

```text
LBJ-Receiver
```

Connect to it and open:

```text
http://192.168.4.1
```


## Notes

The project is still being cleaned up and actively modified.

Current limitations include:

- Code structure still contains legacy parts from the original TTGO-based project
- Some hardware definitions need further cleanup
- SD card hot-plug is not supported
- WiFi and Telnet behavior may be unstable
- Decoded messages may be incomplete or incorrect when signal quality is poor
- Documentation is still incomplete


## Roadmap

Planned work includes:

- Cleaning up the code structure
- Separating receiver, parser, display, network, and board support code
- Improving hardware documentation
- Adding new features and better device controls
- Reviewing license compatibility and adding a proper license file


## Credits

This project is based on the work of FLN1021's `SX1276_Receive_LBJ`.

Thanks to the maintainers and contributors of the following projects:

- RadioLib
- LilyGo LoRa Series
- U8g2
- ESP32 Arduino
- ESP Telnet
- RTClib
- ESP32AnalogRead

Thanks also to sliverwolf233 for preserving a public mirror of the original repository.


## Disclaimer

This project is for learning and experimentation only.

Radio regulations vary by country and region. Make sure your use complies with local laws.

Do not rely on this project for railway operation, dispatching, safety-related decisions, or any official use.

Use it at your own risk.