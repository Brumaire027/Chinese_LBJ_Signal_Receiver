#include "menu_result_timer.hpp"
#include "storage_menu.hpp"
#include "networks.hpp"
#include "status_display.hpp"
#include "task_state.hpp"
#include "storage_paths.hpp"
#include "recording_health.hpp"

namespace {
MenuResultTimer resultTimer;
enum class View { List, Card, Logs, Confirm, Result };
View view = View::List;
uint8_t selected = 0;
bool confirm = false;
const char *message = "";
const char *const items[] = {"存储卡状态", "日志状态", "安全卸载", "重新挂载"};
bool busy() { return fd_state.load() != TASK_INIT; }
}

const char *unmountStorage(bool *success) {
    if (success) *success = false;
    // Main loop cannot create another worker until this synchronous call returns.
    // DONE/TERMINATED are deliberately not considered idle.
    if (busy()) return "正在写入，请稍后";
    if (!have_sd) return "存储卡未挂载";
    const bool ok = sd1.safeEnd();
    if (success) *success = ok;
    return ok ? "可以取卡" : "写入失败，请勿取卡";
}

const char *remountStorage(bool *success) {
    if (success) *success = false;
    if (busy()) return "正在写入，请稍后";
    if (have_sd) return "请先安全卸载";
    if (!SD_LOG::reopenSD()) { reportRecordingFailure(RecordChannel::Card); return "挂载失败，请检查卡"; }
    clearRecordingFailure(RecordChannel::Card);
    clearRecordingFailure(RecordChannel::Index);
    const int logResult = sd1.begin(StoragePaths::Logs);
    const int csvResult = sd1.beginCSV(StoragePaths::Records);
    // Keep a partial mount visible so the user can safely unmount it.
    if (logResult && csvResult) return "两种日志打开失败";
    if (logResult) return "文本日志打开失败";
    if (csvResult) return "接收记录打开失败";
    if (success) *success = true;
    return "挂载成功，日志恢复";
}

bool storageMenuShouldExit() { return view == View::Result && resultTimer.ready(millis()); }

void resetStorageMenu() { resultTimer.arm(false); view = View::List; selected = 0; confirm = false; }

void renderStorageMenu() {
    if (view == View::List) {
        showMenuScreen("日志存储", items, 4, selected, true);
    } else if (view == View::Confirm) {
        const char *lines[] = {"卸载后停止保存", "取消", "确认卸载", ""};
        showMenuScreen("安全卸载", lines, 4, confirm ? 2 : 1, true);
    } else if (view == View::Result) {
        const char *lines[] = {message, "", "", ""};
        showMenuScreen("操作结果", lines, 4, 0, false);
        resultTimer.shown(millis());
    } else if (busy()) {
        const char *lines[] = {"正在写入，请稍后", "", "", ""};
        showMenuScreen(items[view == View::Card ? 0 : 1], lines, 4, 0, false);
    } else if (view == View::Card) {
        char total[48], used[48], free[48];
        const uint64_t capacity = have_sd ? SD.totalBytes() : 0;
        const uint64_t occupied = have_sd ? SD.usedBytes() : 0;
        snprintf(total, sizeof(total), "总量:%llu兆字节", capacity / 1048576);
        snprintf(used, sizeof(used), "已用:%llu兆字节", occupied / 1048576);
        snprintf(free, sizeof(free), "剩余:%llu兆字节", (capacity >= occupied ? capacity - occupied : 0) / 1048576);
        const char *alert = recordingAlertText();
        const char *lines[] = {alert ? alert : have_sd ? "已挂载" : "未挂载", total, used, free};
        showMenuScreen("存储卡状态", lines, 4, 0, false);
    } else {
        const char *lines[] = {sd1.status() ? "文本日志:开" : "文本日志:关",
            sd1.status() ? sd1.logName() : "无文件", sd1.csvStatus() ? "接收记录:开" : "接收记录:关",
            sd1.csvStatus() ? sd1.csvName() : "无文件"};
        showMenuScreen("日志状态", lines, 4, 0, false);
    }
}

bool handleStorageButton(ButtonId key) {
    if (view == View::Result && resultTimer.active()) return false;
    if (key == ButtonId::Key4) {
        if (view == View::List) return true;
        view = View::List;
    } else if (view == View::List) {
        if (key == ButtonId::Key2) selected = (selected + 3) % 4;
        if (key == ButtonId::Key3) selected = (selected + 1) % 4;
        if (key == ButtonId::Key1) {
            if (selected == 0) view = View::Card;
            if (selected == 1) view = View::Logs;
            if (selected == 2) { view = View::Confirm; confirm = false; }
            if (selected == 3) { bool success; message = remountStorage(&success); resultTimer.arm(success); view = View::Result; }
        }
    } else if (view == View::Confirm) {
        if (key == ButtonId::Key2 || key == ButtonId::Key3) confirm = !confirm;
        if (key == ButtonId::Key1) {
            if (confirm) { bool success; message = unmountStorage(&success); resultTimer.arm(success); view = View::Result; }
            else view = View::List;
        }
    } else if (view == View::Result && key == ButtonId::Key1) view = View::List;
    return false;
}
