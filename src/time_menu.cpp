#include "menu_result_timer.hpp"
#include "time_menu.hpp"
#include "time_service.hpp"
#include "time_edit.hpp"
#include "status_display.hpp"
#include "task_state.hpp"
#include <cstdio>

namespace {
MenuResultTimer resultTimer;
tm draft{};
uint8_t selected = 0;
bool editing = false;
bool confirming = false;
bool saveSelected = false;
const char *result = nullptr;
const char *const names[] = {"年份", "月份", "日期", "小时", "分钟", "秒钟"};
const char *const editTitles[] = {"调整年份", "调整月份", "调整日期", "调整小时", "调整分钟", "调整秒钟"};
using time_edit::wrap;
void adjust(int delta) {
    switch (selected) {
        case 0: draft.tm_year = wrap(draft.tm_year + delta, 120, time_edit::maxYear() - 1900); break;
        case 1: draft.tm_mon = wrap(draft.tm_mon + delta, 0, 11); break;
        case 2: draft.tm_mday = wrap(draft.tm_mday + delta, 1, rtc_calendar::daysInMonth(draft.tm_year + 1900, draft.tm_mon + 1)); break;
        case 3: draft.tm_hour = wrap(draft.tm_hour + delta, 0, 23); break;
        case 4: draft.tm_min = wrap(draft.tm_min + delta, 0, 59); break;
        case 5: draft.tm_sec = wrap(draft.tm_sec + delta, 0, 59); break;
    }
    draft.tm_mday = time_edit::clampDay(draft.tm_year + 1900, draft.tm_mon + 1, draft.tm_mday);
}
}

bool timeMenuShouldExit() { return result && resultTimer.ready(millis()); }

void resetTimeMenu() {
    resultTimer.arm(false);
    if (!getValidLocalTime(&draft)) {
        draft = {};
        draft.tm_year = 126; draft.tm_mon = 0; draft.tm_mday = 1;
    }
    selected = 0; editing = confirming = saveSelected = false; result = nullptr;
}

void renderTimeMenu() {
    if (result) {
        const char *lines[] = {result};
        showMenuScreen("时间调整", lines, 1, 0, false);
        resultTimer.shown(millis());
        return;
    }
    if (confirming) {
        char date[32], time[32];
        snprintf(date, sizeof(date), "%04d-%02d-%02d", draft.tm_year + 1900, draft.tm_mon + 1, draft.tm_mday);
        snprintf(time, sizeof(time), "%02d:%02d:%02d", draft.tm_hour, draft.tm_min, draft.tm_sec);
        const char *lines[] = {date, time, "取消", "保存"};
        showMenuScreen("保存北京时间", lines, 4, saveSelected ? 3 : 2, true);
        return;
    }
    const int values[] = {draft.tm_year + 1900, draft.tm_mon + 1, draft.tm_mday, draft.tm_hour, draft.tm_min, draft.tm_sec};
    const uint8_t first = selected >= 4 ? selected - 3 : 0;
    char buffers[4][32]; const char *lines[4];
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t index = first + i;
        if (index == 6) snprintf(buffers[i], sizeof(buffers[i]), "保存时间");
        else snprintf(buffers[i], sizeof(buffers[i]), "%s:%0*d", names[index], index == 0 ? 4 : 2, values[index]);
        lines[i] = buffers[i];
    }
    showMenuScreen(editing ? editTitles[selected] : "时间调整", lines, 4, selected - first, true);
}

bool handleTimeButton(ButtonId key) {
    if (result && resultTimer.active()) return false;
    if (result) {
        if (key == ButtonId::Key1 || key == ButtonId::Key4) result = nullptr;
        return false;
    }
    if (confirming) {
        if (key == ButtonId::Key4) confirming = false;
        if (key == ButtonId::Key2 || key == ButtonId::Key3) saveSelected = !saveSelected;
        if (key == ButtonId::Key1) {
            confirming = false;
            if (saveSelected) {
                if (fd_state.load() != TASK_INIT) result = "接收忙，请稍后保存";
                else {
                    switch (setManualLocalTime(draft)) {
                        case ManualTimeResult::Saved: result = "时间已保存"; resultTimer.arm(true); break;
                        case ManualTimeResult::Invalid: result = "日期或时间无效"; break;
                        case ManualTimeResult::RtcFailed: result = "时钟写入失败"; break;
                        case ManualTimeResult::SystemFailed: result = "系统校时失败"; break;
                    }
                }
            }
        }
        return false;
    }
    if (key == ButtonId::Key4) {
        if (editing) editing = false;
        else return true; // draft is discarded when reopening the page
    } else if (key == ButtonId::Key1) {
        if (selected == 6) { confirming = true; saveSelected = false; }
        else editing = !editing;
    } else if (key == ButtonId::Key2 || key == ButtonId::Key3) {
        if (editing) adjust(key == ButtonId::Key2 ? 1 : -1);
        else selected = wrap(selected + (key == ButtonId::Key2 ? -1 : 1), 0, 6);
    }
    return false;
}
