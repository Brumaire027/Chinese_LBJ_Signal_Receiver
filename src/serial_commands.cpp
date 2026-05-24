#include "serial_commands.hpp"

#include "coredump.h"
#include "networks.hpp"
#include "receiver_control.hpp"
#include "task_state.hpp"

#include <RadioLib.h>

extern SX1276 radio;
extern PagerClient pager;
extern uint64_t runtime_timer;
extern uint32_t car_count;
extern uint64_t car_timer;

String printResetReason(esp_reset_reason_t reset);

static void getCoreFreq(void *pVoid) {
    Serial.printf("Core %d Frequency %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
    vTaskDelete(nullptr);
}

void handleSerialInput() {
    if (Serial.available()) {
        String in = Serial.readStringUntil('\r');
        if (in == "ping")
            Serial.println("$ Pong");
        else if (in == "task state")
            Serial.println("$ Task state " + String(fd_state));
        else if (in == "rtc") {
#ifdef HAS_RTC
            time_info = rtcLibtoC(rtc.now());
            Serial.print(&time_info, "$ [eRTC] %Y-%m-%d %H:%M:%S ");
#endif
        } else if (in == "time") {
            getLocalTime(&time_info, 1);
            Serial.printf("$ SYS Time %s, Up time %llu ms (%s)\n", fmtime(time_info), millis64(), fmtms(millis64()));
        } else if (in == "cd") {
            if (have_cd)
                Serial.println("$ Core dump exported.");
            else
                Serial.println("$ No core dump.");
        } else if (in == "sd end") {
            if (!sd1.status())
                Serial.println("$ [SDLOG] No SD.");
            else {
                sd1.append("[SDLOG] SD卡将被卸载\n");
                sd1.end();
                Serial.println("$ [SDLOG] SD end.");
            }
        } else if (in == "sd begin") {
            if (sd1.status())
                Serial.println("$ End SD First.");
            else {
                SD_LOG::reopenSD();
                sd1.begin("/LOGTEST");
                sd1.beginCSV("/CSVTEST");
                sd1.append("[SDLOG] SD卡已重新挂载\n");
                Serial.println("$ [SDLOG] SD reopen.");
            }
        } else if (in == "mem") {
            Serial.printf("$ Mem left: %d Bytes\n", esp_get_free_heap_size());
        } else if (in == "rst") {
            esp_reset_reason_t reason = esp_reset_reason();
            Serial.printf("$ RST: %s\n", printResetReason(reason).c_str());
        } else if (in == "ppm") {
            if (runtime_timer == 0 && !pager.gotSyncState()) {
                ppm = 3;
                int16_t state = radio.setFrequency(actualFreq(ppm));
                if (state == RADIOLIB_ERR_NONE)
                    Serial.printf("$ Actual Frequency %f MHz\n", actualFreq(ppm));
                else
                    Serial.printf("$ Failure, Code %d\n", state);
            } else {
                Serial.println("$ Unable to change frequency due to occupation");
                if (pager.available())
                    Serial.println("$ pager.available == true");
                if (runtime_timer)
                    Serial.printf("$ runtime_timer = %llu, running %llu\n", runtime_timer, millis64() - runtime_timer);
            }
        } else if (in == "ppm read") {
            Serial.printf("$ ppm %.1f\n", ppm);
        } else if (in == "afc off") {
            prb_count = 0;
            prb_timer = 0;
            car_count = 0;
            car_timer = 0;
            freq_correction = false;
            Serial.println("$ Frequency Correction Disabled");
        } else if (in == "afc on") {
            freq_correction = true;
            Serial.println("$ Frequency Correction Enabled");
        } else if (in == "rssi") {
            Serial.printf("$ RSSI %3.2f dBm.\n", radio.getRSSI(false, true));
        } else if (in == "gain") {
            Serial.printf("$ Gain Pos %d \n", radio.getGain());
        } else if (in == "cpu") {
            xTaskCreatePinnedToCore(getCoreFreq, "get_freq", 2048, nullptr, 1, nullptr, 0);
            Serial.printf("Core %d Frequency %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
        }
    }
}
