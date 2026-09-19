#pragma once

/*
 * Hardware pin map for the V2.0 New Function PCB.
 *
 * The project now targets this PCB directly. Do not add board auto-detection or
 * resistor-based hardware identification here; keep this file as the single
 * default firmware pin map.
 */

#define UNUSE_PIN                   (-1)

// LoRa / E32-900M20S (SX1276)
#define RADIO_CS_PIN                13
#define RADIO_SCLK_PIN              14
#define RADIO_MOSI_PIN              27
#define RADIO_MISO_PIN              39
#define RADIO_RST_PIN               26
#define RADIO_DIO0_PIN              33
#define RADIO_DIO1_PIN              35
#define RADIO_BUSY_PIN              32  // DIO2, used as the direct receive data pin.

// RXEN is tied to 3V3 and TXEN is tied to GND on the PCB.

// Buttons: follow the PCB labels confirmed by the user on 2026-09-18.
#define KEY1_PIN                    23
#define KEY2_PIN                    19
#define KEY3_PIN                    18
#define KEY4_PIN                    17
#define BUTTON_PIN                  KEY1_PIN
#define BUTTON_PIN_MASK             (1ULL << BUTTON_PIN)

// I2C (OLED & RTC)
#define I2C_SDA                     21
#define I2C_SCL                     22
#define OLED_RST                    UNUSE_PIN

// SD card, sharing the SPI bus pins with LoRa.
#define SDCARD_CS                   16
#define SDCARD_SCLK                 RADIO_SCLK_PIN
#define SDCARD_MOSI                 RADIO_MOSI_PIN
#define SDCARD_MISO                 RADIO_MISO_PIN

// UART
#define UART_TX_PIN                 1
#define UART_RX_PIN                 3

// Status outputs and battery sense
#define BOARD_LED                   2
#define LED_ON                      HIGH
#define LED_OFF                     LOW
#define BUZZER_PIN                  25
#define ADC_PIN                     34
#define BATTERY_ADC_PIN             ADC_PIN

// Enabled hardware
#define HAS_SDCARD
#define HAS_DISPLAY
#define HAS_RTC

// Display/runtime defaults used by the application.
#define FONT_12_GB2312              u8g2_font_wqy12_t_gb2312
#define OLED_TIMEOUT                60000  // ms

#ifndef INITIAL_PPM
#define INITIAL_PPM                 0
#endif

#ifndef AFC_ENABLE
#define AFC_ENABLE                  true
#endif
