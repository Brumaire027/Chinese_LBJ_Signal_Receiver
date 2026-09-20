#include "menu_system.hpp"

#include "history_viewer.hpp"
#include "networks.hpp"
#include "runtime_settings.hpp"
#include "status_display.hpp"
#include "task_state.hpp"
#include "storage_menu.hpp"
#include "time_menu.hpp"
#include "network_menu.hpp"
#include "display_menu.hpp"
#include "use_mode.hpp"
#include "menu_result_timer.hpp"
#include "receiver_control.hpp"

extern bool low_volt_warned;

#include <cstdio>
#include <esp_system.h>

namespace {

enum class MenuView : uint8_t {
    Inactive = 0,
    TopLevel,
    Page,
};

constexpr uint8_t MENU_VISIBLE_LINES = 4;
constexpr uint8_t MENU_PAGE_COUNT = 10;
enum MenuPage : uint8_t {
    MODE_PAGE = 0, SOUND_LIGHT_PAGE, DISPLAY_PAGE, TIME_PAGE, NETWORK_PAGE,
    BATTERY_PAGE, STORAGE_PAGE, HISTORY_PAGE, SYSTEM_PAGE, ABOUT_PAGE,
};

const char *const MENU_TITLES[MENU_PAGE_COUNT] = {
    "使用模式",
    "声光设置",
    "显示设置",
    "时间调整",
    "网络设置",
    "电量提示",
    "日志存储",
    "历史记录",
    "系统状态",
    "关于设备",
};

MenuView currentView = MenuView::Inactive;
uint8_t selectedPage = 0;
uint8_t selectedPromptSetting = 0;
bool menuDirty = false;
MenuResultTimer batteryResultTimer;
const char *batteryResult = nullptr;
bool diagnosticsOpen = false;
uint32_t diagnosticsRefresh = 0;

void enterMenu() {
    diagnosticsOpen = false;
    currentView = MenuView::TopLevel;
    selectedPage = 0;
    selectedPromptSetting = 0;
    menuDirty = true;
    setMenuDisplayActive(true);
}

void exitMenu() {
    diagnosticsOpen = false;
    currentView = MenuView::Inactive;
    menuDirty = false;
    setMenuDisplayActive(false);
}

void enterHistoryFromMenu() {
    exitMenu();
    enterHistoryViewer(HistoryEntryDirection::Next);
}

void moveSelection(int8_t delta) {
    diagnosticsOpen = false;
    const int8_t pageCount = static_cast<int8_t>(MENU_PAGE_COUNT);
    int8_t next = static_cast<int8_t>(selectedPage) + delta;
    if (next < 0) {
        next = pageCount - 1;
    } else if (next >= pageCount) {
        next = 0;
    }
    selectedPage = static_cast<uint8_t>(next);
    resetStorageMenu();
    resetTimeMenu();
    resetNetworkMenu();
    resetDisplayMenu();
    resetModeMenu();
    menuDirty = true;
}

const char *rxStateText() {
    return fd_state == TASK_INIT ? "闲" : "忙";
}

const char *statusText(bool value) {
    return value ? "开" : "关";
}

void renderTopLevel() {
    uint8_t first = 0;
    if (selectedPage >= MENU_VISIBLE_LINES) {
        first = selectedPage - MENU_VISIBLE_LINES + 1;
    }

    const char *visible[MENU_VISIBLE_LINES] = {};
    uint8_t count = 0;
    for (uint8_t i = 0; i < MENU_VISIBLE_LINES && first + i < MENU_PAGE_COUNT; ++i) {
        visible[i] = MENU_TITLES[first + i];
        count++;
    }

    showMenuScreen("主菜单", visible, count, selectedPage - first, true);
}

void formatDiagnosticCount(char *out, size_t size, uint32_t value) {
    // Compact large counts to keep both counters on one OLED line.
    if (value >= 100000000U) snprintf(out, size, "%lu亿", static_cast<unsigned long>(value / 100000000U));
    else if (value >= 10000U) snprintf(out, size, "%lu万", static_cast<unsigned long>(value / 10000U));
    else snprintf(out, size, "%lu", static_cast<unsigned long>(value));
}

void renderReceiverDiagnostics() {
    const auto &d = receiverDiagnostics();
    const uint64_t now = millis64();
    char frequency[48], signal[48], counts[48], recent[48], received[20], decoded[20];
    snprintf(frequency, sizeof(frequency), "频率:%.4fMHz", actual_frequency);
    if (!d.haveRssi || now - d.sampledMs >= 2000)
        snprintf(signal, sizeof(signal), "信号:等待采样");
    else snprintf(signal, sizeof(signal), "信号:%.0fdBm", d.rssi);
    formatDiagnosticCount(received, sizeof(received), d.received);
    formatDiagnosticCount(decoded, sizeof(decoded), d.decoded);
    snprintf(counts, sizeof(counts), "收:%s 成:%s", received, decoded);
    if (!d.received) snprintf(recent, sizeof(recent), "最近:尚未收到");
    else {
        const uint64_t seconds = (now - d.lastReceivedMs) / 1000;
        if (seconds < 3600) snprintf(recent, sizeof(recent), "最近:%lu秒前", static_cast<unsigned long>(seconds));
        else if (seconds < 86400) snprintf(recent, sizeof(recent), "最近:%lu分前", static_cast<unsigned long>(seconds / 60));
        else snprintf(recent, sizeof(recent), "最近:%lu天前", static_cast<unsigned long>(seconds / 86400));
    }
    const char *lines[] = {frequency, signal, counts, recent};
    showMenuScreen("接收诊断", lines, 4, 0, false);
}

void renderSystemStatus() {
    if (diagnosticsOpen) { renderReceiverDiagnostics(); return; }
    char uptime[48], memory[48], storage[48], receiver[48];
    const uint64_t seconds = millis64() / 1000ULL;
    if (seconds < 86400ULL)
        snprintf(uptime, sizeof(uptime), "运行:%lu秒 %uMHz", static_cast<unsigned long>(seconds), ets_get_cpu_frequency());
    else
        snprintf(uptime, sizeof(uptime), "运行:%lu天 %uMHz", static_cast<unsigned long>(seconds / 86400ULL), ets_get_cpu_frequency());
    snprintf(memory, sizeof(memory), "可用内存:%lu千字节", static_cast<unsigned long>(esp_get_free_heap_size() / 1024));
    snprintf(storage, sizeof(storage), "卡:%s 网络:%s", statusText(sd1.status()), statusText(isConnected()));
    snprintf(receiver, sizeof(receiver), "远程:%s 接收:%s", statusText(telnet_online), rxStateText());
    const char *lines[] = {uptime, memory, storage, receiver};
    showMenuScreen(MENU_TITLES[selectedPage], lines, 4, 0, false);
}

void renderPlaceholderPage(const char *description) {
    const char *lines[] = {description, "功能尚未开放", "当前不能修改", ""};
    showMenuScreen(MENU_TITLES[selectedPage], lines, 4, 0, false);
}

void renderAboutPage() {
    // RTClib parses the compiler date; display a numeric date, without an English month.
    const DateTime buildDate(__DATE__, __TIME__);
    char buildLine[48];
    snprintf(buildLine, sizeof(buildLine), "编译:%04u-%02u-%02u", buildDate.year(), buildDate.month(), buildDate.day());
    const char *lines[] = {"固件版本:2.3.7","Created By Brumaire", buildLine, ""};
    showMenuScreen(MENU_TITLES[selectedPage], lines, 4, 0, false);
}

const char *onOffText(bool enabled) {
    return enabled ? "开" : "关";
}

void renderSoundLightPage() {
    char lightLine[48], buzzerLine[48];
    snprintf(lightLine, sizeof(lightLine), "来车灯光:%s", onOffText(isTrainArrivalLedEnabled()));
    snprintf(buzzerLine, sizeof(buzzerLine), "来车声音:%s", onOffText(isTrainArrivalBuzzerEnabled()));
    const char *lines[] = {lightLine, buzzerLine, "", ""};
    showMenuScreen(MENU_TITLES[selectedPage], lines, 4, selectedPromptSetting, true);
}

void movePromptSetting(int8_t delta) {
    int8_t next = static_cast<int8_t>(selectedPromptSetting) + delta;
    if (next < 0) {
        next = 1;
    } else if (next > 1) {
        next = 0;
    }
    selectedPromptSetting = static_cast<uint8_t>(next);
    menuDirty = true;
}

void togglePromptSetting() {
    if (selectedPromptSetting == 0) {
        toggleTrainArrivalLedEnabled();
    } else {
        toggleTrainArrivalBuzzerEnabled();
    }
    menuDirty = true;
}

void renderBatteryPage() {
    if (batteryResult) {
        const char *lines[] = {batteryResult};
        showMenuScreen("电量提示", lines, 1, 0, false);
        batteryResultTimer.shown(millis());
    } else {
        const char *lines[] = {runtimeSettings().lowBatteryAlertEnabled ? "低电量提示:开" : "低电量提示:关"};
        showMenuScreen("电量提示", lines, 1, 0, true);
    }
}

void renderCurrentPage() {
    switch (selectedPage) {
        case SYSTEM_PAGE:
            renderSystemStatus();
            break;
        case SOUND_LIGHT_PAGE:
            renderSoundLightPage();
            break;
        case TIME_PAGE:
            renderTimeMenu();
            break;
        case DISPLAY_PAGE:
            renderDisplayMenu();
            break;
        case BATTERY_PAGE:
            renderBatteryPage();
            break;
        case STORAGE_PAGE:
            renderStorageMenu();
            break;
        case NETWORK_PAGE:
            renderNetworkMenu();
            break;
        case HISTORY_PAGE:
            {
                const char *lines[] = {"查看近期接收记录", "", "", ""};
                showMenuScreen(MENU_TITLES[selectedPage], lines, 4, 0, false);
            }
            break;
        case ABOUT_PAGE:
            renderAboutPage();
            break;
        case MODE_PAGE:
            renderModeMenu();
            break;
        default:
            renderTopLevel();
            break;
    }
}

}  // namespace

void initMenuSystem() {
    diagnosticsOpen = false;
    currentView = MenuView::Inactive;
    selectedPage = 0;
    menuDirty = false;
    setMenuDisplayActive(false);
}

void handleMenuButtonEvent(const ButtonEvent &event) {
    if (event.type == ButtonEventType::Released) {
        return;
    }

    if (event.id == ButtonId::Key4 && event.type == ButtonEventType::LongPress) {
        exitMenu();
        return;
    }

    if (currentView == MenuView::Inactive) {
        if (event.id == ButtonId::Key1 && event.type == ButtonEventType::ShortPress) {
            enterMenu();
        }
        return;
    }

    if (event.type != ButtonEventType::ShortPress) {
        return;
    }

    if (currentView == MenuView::TopLevel) {
        switch (event.id) {
            case ButtonId::Key2:
                moveSelection(-1);
                break;
            case ButtonId::Key3:
                moveSelection(1);
                break;
            case ButtonId::Key1:
                if (selectedPage == HISTORY_PAGE) {
                    enterHistoryFromMenu();
                    break;
                }
                currentView = MenuView::Page;
                batteryResult = nullptr;
                batteryResultTimer.arm(false);
                resetStorageMenu();
                resetTimeMenu();
                resetNetworkMenu();
                resetDisplayMenu();
                resetModeMenu();
                selectedPromptSetting = 0;
                menuDirty = true;
                break;
            case ButtonId::Key4:
                exitMenu();
                break;
        }
        return;
    }

    if (currentView == MenuView::Page) {
        if (selectedPage == SYSTEM_PAGE) {
            if (diagnosticsOpen) {
                if (event.id == ButtonId::Key4) { diagnosticsOpen = false; menuDirty = true; }
                return;
            }
            if (event.id == ButtonId::Key1) {
                diagnosticsOpen = true;
                diagnosticsRefresh = millis();
                menuDirty = true;
                return;
            }
        }
        if (selectedPage == BATTERY_PAGE) {
            if (batteryResultTimer.active()) return;
            if (batteryResult) {
                if (event.id == ButtonId::Key1 || event.id == ButtonId::Key4) batteryResult = nullptr;
            } else if (event.id == ButtonId::Key4) {
                currentView = MenuView::TopLevel;
            } else if (event.id == ButtonId::Key1) {
                const bool saved = toggleLowBatteryAlert();
                batteryResultTimer.arm(saved);
                if (saved) {
                    low_volt_warned = false;
                    batteryResult = runtimeSettings().lowBatteryAlertEnabled ? "低电量提示已开启" : "低电量提示已关闭";
                } else batteryResult = "保存失败，设置未改";
            }
            menuDirty = true;
            return;
        }
        if (selectedPage == MODE_PAGE) {
            if (handleModeButton(event.id)) currentView = MenuView::TopLevel;
            menuDirty = true;
            return;
        }
        if (selectedPage == DISPLAY_PAGE) {
            if (handleDisplayButton(event.id)) currentView = MenuView::TopLevel;
            menuDirty = true;
            return;
        }
        if (selectedPage == NETWORK_PAGE) {
            if (handleNetworkButton(event.id)) currentView = MenuView::TopLevel;
            menuDirty = true;
            return;
        }
        if (selectedPage == TIME_PAGE) {
            if (handleTimeButton(event.id)) currentView = MenuView::TopLevel;
            menuDirty = true;
            return;
        }
        if (selectedPage == STORAGE_PAGE) {
            if (handleStorageButton(event.id)) currentView = MenuView::TopLevel;
            menuDirty = true;
            return;
        }
        if (selectedPage == SOUND_LIGHT_PAGE) {
            switch (event.id) {
                case ButtonId::Key2:
                    movePromptSetting(-1);
                    break;
                case ButtonId::Key3:
                    movePromptSetting(1);
                    break;
                case ButtonId::Key1:
                    togglePromptSetting();
                    break;
                case ButtonId::Key4:
                    currentView = MenuView::TopLevel;
                    menuDirty = true;
                    break;
            }
            return;
        }

        switch (event.id) {
            case ButtonId::Key2:
                moveSelection(-1);
                break;
            case ButtonId::Key3:
                moveSelection(1);
                break;
            case ButtonId::Key1:
                if (selectedPage == HISTORY_PAGE) {
                    enterHistoryFromMenu();
                    break;
                }
                menuDirty = true;
                break;
            case ButtonId::Key4:
                currentView = MenuView::TopLevel;
                menuDirty = true;
                break;
        }
    }
}

void updateMenuSystem() {
    if (currentView == MenuView::Page && selectedPage == SYSTEM_PAGE && diagnosticsOpen) {
        sampleReceiverDiagnostics();
        if (uint32_t(millis() - diagnosticsRefresh) >= 1000) {
            diagnosticsRefresh = millis();
            menuDirty = true;
        }
    }
    if (currentView == MenuView::Page && selectedPage == BATTERY_PAGE && batteryResultTimer.ready(millis())) {
        batteryResultTimer.arm(false);
        batteryResult = nullptr;
        currentView = MenuView::TopLevel;
        menuDirty = true;
    }
    if (currentView == MenuView::Page) {
        const bool finished = (selectedPage == TIME_PAGE && timeMenuShouldExit()) ||
            (selectedPage == STORAGE_PAGE && storageMenuShouldExit()) ||
            (selectedPage == NETWORK_PAGE && networkMenuShouldExit());
        if (finished) { exitMenu(); return; }
        if (selectedPage == DISPLAY_PAGE && updateDisplayMenu()) menuDirty = true;
    }
    if (currentView == MenuView::Page && selectedPage == MODE_PAGE && modeMenuShouldExit()) {
        resetModeMenu();
        exitMenu();
        return;
    }
    static uint32_t networkRefresh = 0;
    if (currentView == MenuView::Page && selectedPage == NETWORK_PAGE && uint32_t(millis() - networkRefresh) >= 1000) {
        networkRefresh = millis();
        menuDirty = true;
    }
    if (currentView == MenuView::Inactive || !menuDirty) {
        return;
    }

    if (currentView == MenuView::TopLevel) {
        renderTopLevel();
    } else {
        renderCurrentPage();
    }
    menuDirty = false;
}

bool isMenuSystemActive() {
    return currentView != MenuView::Inactive;
}
