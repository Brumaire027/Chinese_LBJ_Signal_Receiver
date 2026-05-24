#include "status_display.hpp"

#ifdef HAS_DISPLAY

#include "buzzer.hpp"
#include "customfont.h"
#include "debug_log.hpp"
#include "runtime_config.hpp"
#include "task_state.hpp"

#include <esp_sleep.h>

extern bool low_volt_warned;
extern bool oled_off;
extern struct rx_info rxInfo;
extern task_states fd_state;

static volatile bool pendingDecodedDisplayUpdate = false;
static struct lbj_data pendingDecodedDisplayData;
static uint64_t pendingDecodedDisplayRuntimeStartMs = 0;
static bool hasDecodedDisplayRefresh = false;
static uint32_t lastDecodedDisplayRefreshMs = 0;

void requestDecodedDisplayUpdate(const struct lbj_data &data, uint64_t runtimeStartMs) {
    pendingDecodedDisplayData = data;
    pendingDecodedDisplayRuntimeStartMs = runtimeStartMs;
    pendingDecodedDisplayUpdate = true;
}

void processPendingDisplayUpdate() {
    if (!pendingDecodedDisplayUpdate)
        return;

    const uint32_t nowMs = millis();
    if (hasDecodedDisplayRefresh &&
        static_cast<uint32_t>(nowMs - lastDecodedDisplayRefreshMs) < OLED_DECODED_DISPLAY_MIN_INTERVAL_MS) {
        return;
    }

    pendingDecodedDisplayUpdate = false;

    const task_states previousState = fd_state;
    if (fd_state != TASK_TERMINATED) {
        fd_state = TASK_RUNNING_SCREEN;
    }

    if (u8g2) {
#ifdef HAS_OLED_TIMEOUT
        if (oled_off) {
            oled_off = false;
            u8g2->setPowerSave(false);
            u8g2->clearBuffer();
            updateInfo();
        }
#endif
        if (pendingDecodedDisplayData.type == 0)
            showLBJ0(pendingDecodedDisplayData);
        else if (pendingDecodedDisplayData.type == 1) {
            showLBJ1(pendingDecodedDisplayData);
        } else if (pendingDecodedDisplayData.type == 2) {
            showLBJ2(pendingDecodedDisplayData);
        }
        debugLogStageTiming("Complete u8g2 [%llu]\n", millis64() - pendingDecodedDisplayRuntimeStartMs);
        lastDecodedDisplayRefreshMs = millis();
        hasDecodedDisplayRefresh = true;
    }

    if (previousState != TASK_TERMINATED) {
        fd_state = previousState;
    }
}

static void pword(const char *msg, int xloc, int yloc) {
    int dspW = u8g2->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += u8g2->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += u8g2->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += u8g2->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += u8g2->drawStr(xloc, yloc, glyph);
    }
}

void showInitComp() {
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_squeezed_b7_tr);

    String ipa = WiFi.localIP().toString();
    u8g2->drawStr(0, 64, ipa.c_str());
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "D");
    else if (have_sd)
        u8g2->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "N");

    char buffer[32];
    sprintf(buffer, "%2u", ets_get_cpu_frequency() / 10);
    u8g2->drawStr(96, 64, buffer);
    sprintf(buffer, "%1.2f", battery.readVoltage() * 2);
    u8g2->drawStr(108, 64, buffer);

    if (!getLocalTime(&time_info, 0))
        u8g2->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }
    u8g2->sendBuffer();
}

void updateInfo() {
    char buffer[32];
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    u8g2->drawBox(0, 0, 97, 8);
    u8g2->setDrawColor(1);
    if (!getLocalTime(&time_info, 0))
        u8g2->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }

    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 56, 128, 8);
    u8g2->setDrawColor(1);
    if (!no_wifi) {
        String ipa = WiFi.localIP().toString();
        u8g2->drawStr(0, 64, ipa.c_str());
    } else {
        u8g2->drawStr(0, 64, "WIFI OFF");
    }

    sprintf(buffer, "%.1f", getBias(actual_frequency));
    u8g2->drawStr(73, 64, buffer);
    if (sd1.status() && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "D");
    else if (sd1.status())
        u8g2->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "N");

    sprintf(buffer, "%2u", ets_get_cpu_frequency() / 10);
    u8g2->drawStr(96, 64, buffer);
    voltage = battery.readVoltage() * 2;
    sprintf(buffer, "%1.2f", voltage);

    if (voltage < 3.4 && !low_volt_warned) {
        debugLogError("Warning! Low Voltage detected, %1.2fV\n", voltage);
        sd1.append("低压警告，电池电压%1.2fV\n", voltage);
        low_volt_warned = true;

        u8g2->setDrawColor(0);
        u8g2->drawBox(20, 20, 88, 24);
        u8g2->setDrawColor(1);
        u8g2->drawFrame(20, 20, 88, 24);
        u8g2->setFont(u8g2_font_wqy12_t_gb2312);
        u8g2->setCursor(35, 38);
        u8g2->print("电量不足!");
        u8g2->sendBuffer();
        delay(2000);

        buzzer.beep(3, 300, 300);
    }

    if (voltage < 3.10) {
        debugLogErrorPrintln("Critical Voltage! System Halted.");
        u8g2->clearBuffer();
        u8g2->setDrawColor(0);
        u8g2->drawBox(20, 20, 88, 24);
        u8g2->setDrawColor(1);
        u8g2->drawFrame(20, 20, 88, 24);
        u8g2->setFont(u8g2_font_wqy12_t_gb2312);
        u8g2->setCursor(35, 38);
        u8g2->print("电量耗尽!");
        u8g2->sendBuffer();

        sd1.end();
        buzzer.hold(3000);
        esp_deep_sleep_start();
    }

    u8g2->drawStr(108, 64, buffer);
    u8g2->sendBuffer();
}

void showSTR(const String &str) {
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    pword(str.c_str(), 0, 19);
    u8g2->sendBuffer();
}

void showLBJ0(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 21);
    u8g2->printf("车  次");
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->printf("%s", l.train);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(u8g2->getCursorX() + 6, u8g2->getCursorY());
    if (l.direction == FUNCTION_UP) {
        u8g2->printf("上行");
    } else if (l.direction == FUNCTION_DOWN) {
        u8g2->printf("下行");
    } else {
        u8g2->printf("%d", l.direction);
    }
    u8g2->setCursor(0, 37);
    u8g2->printf("速  度");
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->printf(" %s ", l.speed);
    u8g2->setCursor(u8g2->getCursorX() + 7, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont15_mr);
    u8g2->printf("KM/H");
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 53);
    u8g2->printf("公里标");
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->printf("%s ", l.position);
    u8g2->setCursor(u8g2->getCursorX() + 4, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont15_mr);
    u8g2->printf("KM");

    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ1(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);

    u8g2->setCursor(0, 19);
    u8g2->printf("车:");
    u8g2->setCursor(u8g2->getCursorX() + 1, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    for (int i = 0, c = 0; i < 6; i++) {
        if (i == 5) {
            buffer[c] = 0;
            break;
        }
        if (l.train[i] == ' ')
            continue;
        buffer[c] = l.train[i];
        ++c;
    }
    u8g2->printf("%s%s", l.lbj_class, buffer);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setCursor(68, 19);
    u8g2->printf("速:");
    u8g2->setCursor(u8g2->getCursorX() + 2, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%s", l.speed);
    u8g2->setCursor(u8g2->getCursorX(), u8g2->getCursorY());
    u8g2->printf("KM/H");
    u8g2->setFont(FONT_12_GB2312);

    u8g2->setCursor(0, 31);
    u8g2->printf("线:");
    u8g2->setCursor(u8g2->getCursorX() + 2, u8g2->getCursorY());
    u8g2->printf("%s", l.route_utf8);
    u8g2->drawBox(67, 21, 13, 12);
    u8g2->setDrawColor(0);
    if (l.direction == FUNCTION_UP)
        u8g2->drawUTF8(68, 31, "上");
    else if (l.direction == FUNCTION_DOWN)
        u8g2->drawUTF8(68, 31, "下");
    else {
        sprintf(buffer, "%d", l.direction);
        u8g2->drawStr(71, 31, buffer);
    }
    u8g2->setDrawColor(1);
    u8g2->setCursor(84, 31);
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%s", l.position);
    u8g2->setCursor(u8g2->getCursorX(), u8g2->getCursorY());
    u8g2->printf("K");
    u8g2->setFont(FONT_12_GB2312);

    u8g2->setCursor(0, 43);
    u8g2->printf("号:");
    u8g2->setCursor(u8g2->getCursorX() + 1, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%s", l.loco);
    if (String(l.loco) != "<NUL>" && l.info2_hex.length() > 14 && l.info2_hex[12] == '3') {
        if (l.info2_hex[13] == '1')
            u8g2->printf("A");
        else if (l.info2_hex[13] == '2')
            u8g2->printf("B");
    }
    u8g2->setFont(FONT_12_GB2312);
    if (l.loco_type.length())
        u8g2->drawUTF8(72, 43, l.loco_type.c_str());

    String pos;
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lat);
        pos += String(buffer);
    }
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lon);
        pos += String(buffer);
    }
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->drawUTF8(0, 54, pos.c_str());

    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ2(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 23);
    u8g2->printf("当前时间");
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->setCursor(u8g2->getCursorX() + 3, u8g2->getCursorY() - 1);
    u8g2->printf("%s ", l.time);

    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

#endif
