//
// DIY Board Adapter by Gemini
// Fixed to match modified boards.hpp
//

#include "boards.hpp"

// === 1. 全局变量实例化 ===

// 显示屏对象
#ifdef HAS_DISPLAY
    DISPLAY_MODEL *u8g2 = nullptr;
#endif

// 电池与电压 (DIY板通常没有分压电路，给个模拟值)
ESP32AnalogRead battery;
float voltage = 4.20; 

// SD卡对象
#ifdef HAS_SDCARD
    SPIClass SDSPI(VSPI); // 使用 VSPI 总线
    bool have_sd = false;
#endif

// RTC 对象 (必须与 hpp 中的类型一致)
#ifdef HAS_RTC
    RTC_DS1307 rtc; 
#endif

// === 2. 辅助函数 ===
uint64_t millis64() {
    return esp_timer_get_time() / 1000ULL;
}

// === 3. 初始化函数 ===
void initBoard() {
    Serial.begin(115200);
    Serial.println("\n[Board] Init Started (DIY Version)...");

    // --- LED 初始化 ---
    #ifdef BOARD_LED
        pinMode(BOARD_LED, OUTPUT);
        digitalWrite(BOARD_LED, LOW); 
    #endif

    // --- 总线初始化 ---
    // I2C (OLED & RTC)
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // SPI (LoRa & SD)
    // 使用 platformio.ini 中定义的引脚宏
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    // --- OLED 初始化 ---
    #ifdef HAS_DISPLAY
        Serial.println("[Display] Init...");
        // 这里的 OLED_RST 在 ini 里定义为 -1
        u8g2 = new DISPLAY_MODEL(U8G2_R0, OLED_RST, I2C_SCL, I2C_SDA);
        
        if (u8g2->begin()) {
            u8g2->clearBuffer();
            u8g2->setFont(u8g2_font_ncenB08_tr);
            u8g2->drawStr(0, 12, "LBJ Receiver");
            u8g2->drawStr(0, 26, "System Init...");
            u8g2->sendBuffer();
        } else {
            Serial.println("[Display] Failed!");
        }
    #endif

    // --- SD 卡初始化 ---
    #ifdef HAS_SDCARD
        Serial.println("[SD] Init...");
        // 注意：SD卡使用 SDSPI 实例，引脚来自 platformio.ini
        SDSPI.begin(SDCARD_SCLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
        
        if (!SD.begin(SDCARD_CS, SDSPI)) {
            Serial.println("[SD] Mount Failed!");
            if(u8g2) {
                u8g2->drawStr(0, 40, "SD: Fail");
                u8g2->sendBuffer();
            }
        } else {
            Serial.println("[SD] Mounted Successfully");
            have_sd = true;
            if(u8g2) {
                u8g2->drawStr(0, 40, "SD: OK");
                u8g2->sendBuffer();
            }
        }
    #endif

    // --- RTC 初始化 ---
    #ifdef HAS_RTC
        Serial.println("[RTC] Init...");
        if (!rtc.begin()) {
            Serial.println("[RTC] Not Found!");
        } else {
            if (!rtc.isrunning()) {
                Serial.println("[RTC] Not running, setting time...");
                rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
            // 打印当前时间测试
            DateTime now = rtc.now();
            Serial.printf("[RTC] Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
        }
    #endif
    
    // --- 电池虚拟初始化 ---
    // 防止 main.cpp 调用 battery.readVoltage() 时崩溃
    // 随意绑定一个未使用的引脚
    #ifdef ADC_PIN
        battery.attach(ADC_PIN);
    #endif

    Serial.println("[Board] Init Done.\n");
}