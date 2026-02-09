#ifndef BOARDS_H
#define BOARDS_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "utilities.h"
#include <ESP32AnalogRead.h>
#include "patterns.h"

// === SD 卡部分 ===
#ifdef HAS_SDCARD
#include <SD.h>
#include <FS.h>
// 声明外部变量，让其他文件能用
extern SPIClass SDSPI;
extern bool have_sd;
#endif

// === 显示屏部分 ===
#ifdef HAS_DISPLAY
#include <U8g2lib.h>
#ifndef DISPLAY_MODEL
// 定义屏幕驱动模型
#define DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C
#endif
extern DISPLAY_MODEL *u8g2;
#endif

// === RTC 时钟部分 (关键修改) ===
#ifdef HAS_RTC
#include <RTClib.h>
// [修改] 将原版的 RTC_DS3231 改为你的 RTC_DS1307
extern RTC_DS1307 rtc;
#endif

#ifndef OLED_WIRE_PORT
#define OLED_WIRE_PORT Wire
#endif

// === 电源管理 (PMU) 部分 [修改] ===
// DIY 板子没有 PMU，为了防止 main.cpp 报错，我们定义空宏
#define initPMU()
#define disablePeripherals()

// === 变量声明 ===
extern ESP32AnalogRead battery;
extern float voltage;

void initBoard();
uint64_t millis64(); 

#endif // BOARDS_H