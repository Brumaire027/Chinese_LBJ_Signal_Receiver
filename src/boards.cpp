//
// DIY Board Adapter by Gemini
// Fixed to match modified boards.hpp
//

#include "boards.hpp"

#include "debug_log.hpp"

// 显示屏对象
#ifdef HAS_DISPLAY
    DISPLAY_MODEL *u8g2 = nullptr;
#endif

// 电池与电压
ESP32AnalogRead battery;
float voltage = 4.20; 

// SD卡对象
#ifdef HAS_SDCARD
    bool have_sd = false;
#endif

// RTC 对象
#ifdef HAS_RTC
    RTC_DS3231 rtc;
#endif

// === 2. 辅助函数 ===
#ifdef HAS_SDCARD
bool mountSdCard() {
    // Both clients use the same SPIClass and transaction lock. Never SPI.end().
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    digitalWrite(RADIO_CS_PIN, HIGH);
    digitalWrite(SDCARD_CS, HIGH);
    SPI.endTransaction();
    have_sd = SD.begin(SDCARD_CS, SPI, 4000000, "/sd", 5, false);
    return have_sd;
}
#endif

uint64_t millis64() {
    return esp_timer_get_time() / 1000ULL;
}

// === 3. 初始化函数 ===
void initBoard() {
    Serial.begin(115200);
    debugLogInfoPrintln("\n[Board] Init Started (DIY Version)...");

    // --- 总线初始化 ---
    // I2C (OLED & RTC)
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // SPI (LoRa & SD)
    // Deassert both chip selects before any clocks on the shared V2 bus.
    digitalWrite(RADIO_CS_PIN, HIGH);
    pinMode(RADIO_CS_PIN, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    // --- OLED 初始化 ---
    #ifdef HAS_DISPLAY
        debugLogInfoPrintln("[Display] Init...");
        // OLED_RST is defined in the central hardware pin map.
        u8g2 = new DISPLAY_MODEL(U8G2_R0, OLED_RST, I2C_SCL, I2C_SDA);
        
        if (u8g2->begin()) {
            u8g2->clearBuffer();
            u8g2->setFont(FONT_12_GB2312);
            u8g2->drawUTF8(0, 12, "列车接收器");
            u8g2->drawUTF8(0, 26, "系统初始化");
            u8g2->sendBuffer();
        } else {
            debugLogErrorPrintln("[Display] Failed!");
        }
    #endif

    // --- SD 卡初始化 ---
    #ifdef HAS_SDCARD
        debugLogInfoPrintln("[SD] Init...");
        if (!mountSdCard()) {
            debugLogErrorPrintln("[SD] Mount Failed!");
            if(u8g2) {
                u8g2->drawUTF8(0, 40, "存储卡不可用");
                u8g2->sendBuffer();
            }
        } else {
            debugLogInfoPrintln("[SD] Mounted Successfully");
            have_sd = true;
            if(u8g2) {
                u8g2->drawUTF8(0, 40, "存储卡正常");
                u8g2->sendBuffer();
            }
        }
    #endif

    // RTC initialization/time validation follows in initTimeService().
    
    // --- 电池虚拟初始化 ---
    #ifdef ADC_PIN
        battery.attach(ADC_PIN);
    #endif

    debugLogInfoPrintln("[Board] Init Done.\n");
}
