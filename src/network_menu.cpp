#include "menu_result_timer.hpp"
#include "network_menu.hpp"
#include "networks.hpp"
#include "runtime_settings.hpp"
#include "status_display.hpp"
#include "task_state.hpp"
#include <WiFiManager.h>

extern WiFiManager wm;
extern bool is_ap_active;
extern uint64_t net_timer;
namespace {
MenuResultTimer resultTimer;
enum class View { List, Status, Portal, Result };
View view = View::List;
uint8_t selected = 0;
uint32_t portalStarted = 0;
uint32_t connectionStarted = 0;
bool connecting = false;
bool credentialsSaved = false;
bool portalSaved = false;
const char *message = "";
const char *portalResult = "配网已关闭";
void reconnect() {
    if (telnet_online) { telnet.stop(); telnet_online = false; }
    no_wifi = false;
    net_timer = millis64();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false); // retain saved credentials
    WiFi.begin();
    connecting = true;
    connectionStarted = millis();
}
void closePortal() {
    wm.stopConfigPortal();
    is_ap_active = false;
    WiFi.mode(WIFI_STA);
    net_timer = millis64();
}
}
void initNetworkSettings() {
    wm.setConfigPortalBlocking(false);
    wm.setConnectTimeout(3);
    wm.setSaveConnectTimeout(1);
    wm.setSaveConfigCallback([] { credentialsSaved = true; });
    wm.setSaveConnect(false); // Save first, connect in the background afterward.
    wm.setClass("invert");
    wm.setTitle("列车接收器");
    const char *menu[] = {"wifi", "exit"};
    wm.setMenu(menu, 2);
    is_ap_active = false;
    reconnect(); // Never auto-start the configuration portal on boot failure.
}
void processNetworkSettings() {
    if (is_ap_active && fd_state.load() == TASK_INIT) {
        wm.process();
        if (credentialsSaved) {
            credentialsSaved = false;
            closePortal();
            portalResult = "配置已保存";
            portalSaved = true;
            reconnect();
        } else if (!wm.getConfigPortalActive() || uint32_t(millis() - portalStarted) >= 300000UL) {
            closePortal();
            portalResult = "配网已关闭";
            reconnect();
        }
    }
    if (isConnected() || (connecting && uint32_t(millis() - connectionStarted) >= 20000UL)) connecting = false;
}
bool networkMenuShouldExit() {
    if (view != View::Portal || is_ap_active || !portalSaved || !isConnected()) {
        resultTimer.arm(false);
        return false;
    }
    return resultTimer.ready(millis());
}
void resetNetworkMenu() { view = View::List; selected = 0; resultTimer.arm(false); }
void renderNetworkMenu() {
    if (view == View::List) {
        const char *items[] = {"连接状态", "手机配网", "重新连接", isRemoteConnectionEnabled() ? "远程连接:开" : "远程连接:关"};
        showMenuScreen("网络设置", items, 4, selected, true);
    } else if (view == View::Status) {
        const String ssid = WiFi.SSID();
        const String ipText = isConnected() ? WiFi.localIP().toString() : "暂无地址";
        const char *state = isConnected() ? "已连接" : is_ap_active ? "配网中" : no_wifi ? "网络已休眠" : connecting ? "正在连接" : "未连接";
        const char *lines[] = {state, ssid.length() ? ssid.c_str() : "尚未配置网络", ipText.c_str(), telnet_online ? "远程服务:运行" : "远程服务:关闭"};
        showMenuScreen("连接状态", lines, 4, 0, false);
    } else if (view == View::Portal) {
        const String address = WiFi.softAPIP().toString();
        const char *active[] = {"手机连接此热点", "LBJ-Receiver", address.c_str(), "关闭热点"};
        const char *closed[] = {portalResult, isConnected() ? "网络已连接" : connecting ? "正在连接网络" : "未连接，请重新配网"};
        showMenuScreen("手机配网", is_ap_active ? active : closed, is_ap_active ? 4 : 2, 3, is_ap_active);
        if (!is_ap_active && portalSaved && isConnected()) {
            if (!resultTimer.active()) resultTimer.arm(true);
            resultTimer.shown(millis());
        } else resultTimer.arm(false);
    } else {
        const char *lines[] = {message};
        showMenuScreen("网络设置", lines, 1, 0, false);
    }
}
bool handleNetworkButton(ButtonId key) {
    if (view == View::Portal && resultTimer.active() && isConnected()) return false;
    if (key == ButtonId::Key4) {
        if (view == View::List) return true;
        view = View::List;
        return false;
    }
    if (view == View::Portal && key == ButtonId::Key1) {
        if (fd_state.load() != TASK_INIT) { message = "接收忙，请稍后操作"; view = View::Result; return false; }
        if (is_ap_active) {
            closePortal(); portalResult = "配网已关闭"; portalSaved = false; resultTimer.arm(false); reconnect();
        } else view = View::List;
        return false;
    }
    if (view == View::Result && key == ButtonId::Key1) { view = View::List; return false; }
    if (view != View::List) return false;
    if (key == ButtonId::Key2) selected = (selected + 3) % 4;
    if (key == ButtonId::Key3) selected = (selected + 1) % 4;
    if (key != ButtonId::Key1) return false;
    if (selected == 0) { view = View::Status; return false; }
    if (fd_state.load() != TASK_INIT) { message = "接收忙，请稍后操作"; view = View::Result; return false; }
    if (selected == 1) {
        if (!is_ap_active) {
            no_wifi = false;
            net_timer = millis64();
            credentialsSaved = false;
            portalSaved = false; resultTimer.arm(false);
            wm.startConfigPortal("LBJ-Receiver");
            is_ap_active = wm.getConfigPortalActive() && (WiFi.getMode() & WIFI_AP);
            if (!is_ap_active) {
                wm.stopConfigPortal();
                message = "热点启动失败"; view = View::Result; return false;
            }
            portalStarted = millis();
        }
        view = View::Portal;
    } else if (selected == 2) {
        if (is_ap_active) closePortal();
        reconnect(); view = View::Status;
    } else {
        if (!toggleRemoteConnectionEnabled()) { message = "设置保存失败"; view = View::Result; }
        else if (!isRemoteConnectionEnabled()) { telnet.stop(); telnet_online = false; }
    }
    return false;
}
