#include "status_display.hpp"

#ifdef HAS_DISPLAY

#include "buzzer.hpp"
#include "customfont.h"
#include "debug_log.hpp"
#include "runtime_config.hpp"
#include "task_state.hpp"
#include "display_power.hpp"
#include "runtime_settings.hpp"
#include "use_mode.hpp"
#include "recording_health.hpp"
#include <atomic>

#include <esp_sleep.h>

extern bool low_volt_warned;
extern bool oled_off;
extern struct rx_info rxInfo;

static volatile bool pendingDecodedDisplayUpdate = false;
static struct lbj_data pendingDecodedDisplayData;
static uint64_t pendingDecodedDisplayRuntimeStartMs = 0;
static bool hasDecodedDisplayRefresh = false;
static uint32_t lastDecodedDisplayRefreshMs = 0;
static bool menuDisplayActive = false;
static bool historyDisplayActive = false;
static bool mainScreenDirty = false;
static std::atomic<bool> receivedDisplayActivity{false};
static float displayedRssi = 0;
static char displayedTrainKey[9] = {};

// 9x8 warning triangle with an exclamation mark; bottom-row icon slot x=18..26.
static void sendStatusBuffer() {
    if (!u8g2) return;
    uint8_t saved[9];
    uint8_t *pixels = u8g2->getBufferPtr() + 7 * 128 + 18;
    const bool warning = !menuDisplayActive && !historyDisplayActive && recordingAlertVisible();
    if (warning) {
        static const uint16_t rows[8] = {0x010, 0x028, 0x028, 0x054, 0x054, 0x082, 0x092, 0x1ff};
        for (uint8_t x = 0; x < 9; ++x) {
            saved[x] = pixels[x]; pixels[x] = 0;
            for (uint8_t y = 0; y < 8; ++y) if (rows[y] & (1U << x)) pixels[x] |= 1U << y;
        }
    }
    u8g2->sendBuffer();
    if (warning) for (uint8_t x = 0; x < 9; ++x) pixels[x] = saved[x];
}
void refreshRecordingAlert() {
    static bool previousVisible = false;
    if (!u8g2 || oled_off || menuDisplayActive || historyDisplayActive) { previousVisible = false; return; }
    const bool visible = recordingAlertVisible();
    if (visible != previousVisible) { sendStatusBuffer(); previousVisible = visible; }
}

void requestMainDisplayRefresh() { mainScreenDirty = true; }
void resetDecodedDisplay() {
    pendingDecodedDisplayUpdate = false;
    hasDecodedDisplayRefresh = false;
    receivedDisplayActivity = false;
    mainScreenDirty = true;
}

void setMenuDisplayActive(bool active) {
    if (menuDisplayActive && !active) {
        mainScreenDirty = true;
        displayActivity(true);
    }
    menuDisplayActive = active;
}

bool isMenuDisplayActive() {
    return menuDisplayActive;
}

void setHistoryDisplayActive(bool active) {
    if (historyDisplayActive && !active) {
        mainScreenDirty = true;
        displayActivity(true);
    }
    historyDisplayActive = active;
}

bool isHistoryDisplayActive() {
    return historyDisplayActive;
}

void requestDecodedDisplayUpdate(const struct lbj_data &data, uint64_t runtimeStartMs, float rssi, bool arrival, const char *trainKey) {
    pendingDecodedDisplayData = data;
    pendingDecodedDisplayRuntimeStartMs = runtimeStartMs;
    pendingDecodedDisplayUpdate = true;
    displayedRssi = rssi;
    snprintf(displayedTrainKey, sizeof(displayedTrainKey), "%s", trainKey ? trainKey : "");
    // The mode queue owns its dwell timer; a previous frame must not delay a new queued car.
    if (!arrival) lastDecodedDisplayRefreshMs = millis() - OLED_DECODED_DISPLAY_MIN_INTERVAL_MS;
    if (arrival) receivedDisplayActivity.store(true);
}

void processPendingDisplayUpdate() {
    if (isRideMode()) return;
    if (receivedDisplayActivity.exchange(false))
        displayActivity(runtimeSettings().display.wakeOnArrival);
    // Retain the newest record while asleep; cached redraws are not new arrivals.
    if (oled_off || menuDisplayActive || historyDisplayActive)
        return;

    if (mainScreenDirty && u8g2) {
        mainScreenDirty = false;
        showInitComp();
        if (hasDecodedDisplayRefresh) {
            pendingDecodedDisplayUpdate = true;
            hasDecodedDisplayRefresh = false; // Render the cached record immediately on return.
        } else if (!pendingDecodedDisplayUpdate) {
            showWaitingScreen();
        }
    }

    if (!pendingDecodedDisplayUpdate)
        return;

    const uint32_t nowMs = millis();
    if (hasDecodedDisplayRefresh &&
        static_cast<uint32_t>(nowMs - lastDecodedDisplayRefreshMs) < OLED_DECODED_DISPLAY_MIN_INTERVAL_MS) {
        return;
    }

    pendingDecodedDisplayUpdate = false;

    if (u8g2) {
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

static void drawStorageNetworkIcons(bool storage, bool network) {
    // Fixed symbols; a diagonal marks an unmounted card or disconnected Wi-Fi.
    u8g2->drawFrame(0, 57, 6, 7);
    u8g2->drawHLine(1, 59, 4);
    u8g2->drawVLine(9, 62, 2);
    u8g2->drawVLine(11, 60, 4);
    u8g2->drawVLine(13, 58, 6);
    for (uint8_t i = 0; i < 7; ++i) {
        if (!storage) u8g2->drawPixel(6 - i, 57 + i);
        if (!network) u8g2->drawPixel(8 + i, 57 + i);
    }
}

// Exclusive slots: icons [0,15), warning [18,27), bias [28,91), battery [98,128).
// Keep numeric fields intact; overflow is explicit rather than a clipped value.
static void drawStatusNumber(uint8_t left, uint8_t right, const char *value) {
    const char *fitted = u8g2->getStrWidth(value) <= right - left ? value : "--";
    u8g2->drawStr(right - u8g2->getStrWidth(fitted), 64, fitted);
}

static void drawMainStatusBar(float batteryVoltage) {
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    u8g2->setFontMode(1);
    u8g2->setFontPosBaseline();
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 56, 128, 8);
    u8g2->setDrawColor(1);
    drawStorageNetworkIcons(have_sd, WiFi.status() == WL_CONNECTED);
    char number[32];
    snprintf(number, sizeof(number), "%.1f", getBias(actual_frequency));
    drawStatusNumber(28, 91, number);
    snprintf(number, sizeof(number), "%.2fV", batteryVoltage);
    drawStatusNumber(98, 128, number);
}

void showWaitingScreen() {
    if (!u8g2) return;
    showInitComp();
    u8g2->setFont(FONT_12_GB2312);
    u8g2->drawUTF8(0, 52, "等待列车信号");
    sendStatusBuffer();
}

void showInitComp() {
    u8g2->setDrawColor(1);
    u8g2->setFontMode(1);
    u8g2->setFontPosBaseline();
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_squeezed_b7_tr);

    char buffer[32];
    drawMainStatusBar(battery.readVoltage() * 2);

    if (!getValidLocalTime(&time_info))
        u8g2->drawStr(0, 7, "---- -- -- --:--");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }
    sendStatusBuffer();
}

void updateInfo() {
    if (menuDisplayActive || historyDisplayActive)
        return;

    char buffer[32];
    if (!isRideMode()) {
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    u8g2->drawBox(0, 0, 97, 8);
    u8g2->setDrawColor(1);
    if (!getValidLocalTime(&time_info))
        u8g2->drawStr(0, 7, "---- -- -- --:--");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    }

    }
    voltage = battery.readVoltage() * 2;

    if (runtimeSettings().lowBatteryAlertEnabled && voltage < 3.4 && !low_volt_warned) {
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
        sendStatusBuffer();
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
        sendStatusBuffer();

        sd1.end();
        buzzer.hold(3000);
        esp_deep_sleep_start();
    }

    if (!isRideMode()) {
        drawMainStatusBar(voltage);
        sendStatusBuffer();
    }
}

void showSTR(const String &str) {
    if (isRideMode() || oled_off || menuDisplayActive || historyDisplayActive)
        return;

    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    pword(str.c_str(), 0, 19);
    sendStatusBuffer();
}

void showLBJ0(const struct lbj_data &l) {
    if (menuDisplayActive || historyDisplayActive)
        return;

    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 21);
    u8g2->printf("车  次");
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->setCursor(50, u8g2->getCursorY());
    const char *label = displayedTrainKey[0] ? displayedTrainKey : l.train;
    if (strlen(label) > 5) u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%s", label);
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
    sprintf(buffer, "%3.1f", displayedRssi);
    u8g2->drawStr(99, 7, buffer);
    sendStatusBuffer();
}

void showLBJ1(const struct lbj_data &l) {
    if (menuDisplayActive || historyDisplayActive)
        return;

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
    sprintf(buffer, "%3.1f", displayedRssi);
    u8g2->drawStr(99, 7, buffer);
    sendStatusBuffer();
}

void showLBJ2(const struct lbj_data &l) {
    if (menuDisplayActive || historyDisplayActive)
        return;

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
    sprintf(buffer, "%3.1f", displayedRssi);
    u8g2->drawStr(99, 7, buffer);
    sendStatusBuffer();
}

// Keep UTF-8 characters intact when a data field is wider than the OLED.
static void drawUiLine(uint8_t x, uint8_t y, const char *text) {
    String fitted(text);
    if (u8g2->getUTF8Width(fitted.c_str()) <= 128 - x) {
        u8g2->drawUTF8(x, y, fitted.c_str());
        return;
    }
    do {
        size_t end = fitted.length() - 1;
        while (end > 0 && (static_cast<uint8_t>(fitted[end]) & 0xc0) == 0x80) --end;
        fitted.remove(end);
    } while (fitted.length() && u8g2->getUTF8Width((fitted + "...").c_str()) > 128 - x);
    fitted += "...";
    u8g2->drawUTF8(x, y, fitted.c_str());
}

void showMenuScreen(const char *title, const char *const *lines, uint8_t lineCount, uint8_t selectedLine,
                    bool showSelection, bool wake) {
    if (!u8g2) return;
    if (wake) displayActivity(true);
    u8g2->clearBuffer();
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setFontMode(1);
    u8g2->setFontPosBaseline();
    drawUiLine(0, 11, title);
    u8g2->drawHLine(0, 13, 128);
    const uint8_t visibleLines = lineCount > 4 ? 4 : lineCount;
    for (uint8_t i = 0; i < visibleLines; ++i) {
        const uint8_t y = 25 + i * 12;
        if (showSelection && i == selectedLine) {
            u8g2->drawBox(0, y - 11, 128, 12);
            u8g2->setDrawColor(0);
            drawUiLine(2, y, lines[i]);
            u8g2->setDrawColor(1);
        } else {
            drawUiLine(!wake && isRideMode() && i == 3 ? 30 : 2, y, lines[i]);
        }
    }
    sendStatusBuffer();
}

void showHistoryRecordScreen(const char *title, const char *const *lines, uint8_t lineCount) {
    showMenuScreen(title, lines, lineCount, 0, false);
}
void showPassiveScreen(const char *title, const char *const *lines, uint8_t lineCount) {
    showMenuScreen(title, lines, lineCount, 0, false, false);
}

#endif
