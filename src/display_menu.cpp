#include "menu_result_timer.hpp"
#include "display_menu.hpp"
#include "display_power.hpp"
#include "runtime_settings.hpp"
#include "status_display.hpp"
#include <cstdio>

namespace {
MenuResultTimer resultTimer;
enum class View { Settings, ConfirmReset, Result };
View view = View::Settings;
uint8_t selected = 0;
bool confirmReset = false;
const char *result = "";
const char *const brightnessNames[] = {"低", "中", "高"};
const char *const timeoutNames[] = {"常亮", "30秒", "1分钟", "3分钟", "5分钟"};
}

bool updateDisplayMenu() {
    if (view != View::Result || !resultTimer.ready(millis())) return false;
    resultTimer.arm(false);
    view = View::Settings;
    return true;
}

void resetDisplayMenu() {
    resultTimer.arm(false);
    view = View::Settings;
    selected = 0;
    confirmReset = false;
}

void renderDisplayMenu() {
    if (view == View::ConfirmReset) {
        const char *lines[] = {"仅恢复显示设置", "取消", "确认恢复", ""};
        showMenuScreen("恢复显示默认值", lines, 4, confirmReset ? 2 : 1, true);
    } else if (view == View::Result) {
        const char *lines[] = {result, "", "", ""};
        showMenuScreen("显示设置", lines, 4, 0, false);
        resultTimer.shown(millis());
    } else {
        const auto &value = runtimeSettings().display;
        char brightness[48], timeout[48], wake[48];
        snprintf(brightness, sizeof(brightness), "屏幕亮度:%s", brightnessNames[value.brightness]);
        snprintf(timeout, sizeof(timeout), "自动息屏:%s", timeoutNames[value.timeout]);
        snprintf(wake, sizeof(wake), "来车唤醒:%s", value.wakeOnArrival ? "开" : "关");
        const char *lines[] = {brightness, timeout, wake, "恢复显示默认值"};
        showMenuScreen("显示设置", lines, 4, selected, true);
    }
}

bool handleDisplayButton(ButtonId button) {
    if (view == View::Result && resultTimer.active()) return false;
    if (view == View::Result) {
        if (button == ButtonId::Key1 || button == ButtonId::Key4) view = View::Settings;
        return false;
    }
    if (view == View::ConfirmReset) {
        if (button == ButtonId::Key4) view = View::Settings;
        else if (button == ButtonId::Key2 || button == ButtonId::Key3) confirmReset = !confirmReset;
        else if (button == ButtonId::Key1) {
            if (!confirmReset) view = View::Settings;
            else {
                const bool saved = saveDisplaySettings(DisplaySettings{});
                if (saved) applyDisplayBrightness();
                resultTimer.arm(saved);
                result = saved ? "已恢复显示默认值" : "保存失败，设置未改";
                view = View::Result;
            }
        }
        return false;
    }
    if (button == ButtonId::Key4) return true;
    if (button == ButtonId::Key2) selected = (selected + 3) % 4;
    else if (button == ButtonId::Key3) selected = (selected + 1) % 4;
    else if (button == ButtonId::Key1) {
        if (selected == 3) {
            confirmReset = false;
            view = View::ConfirmReset;
        } else {
            DisplaySettings next = runtimeSettings().display;
            if (selected == 0) next.brightness = (next.brightness + 1) % 3;
            else if (selected == 1) next.timeout = (next.timeout + 1) % 5;
            else next.wakeOnArrival = !next.wakeOnArrival;
            if (saveDisplaySettings(next)) applyDisplayBrightness();
            else {
                result = "保存失败，设置未改";
                view = View::Result;
            }
        }
    }
    return false;
}
