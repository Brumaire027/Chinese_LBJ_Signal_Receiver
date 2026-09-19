#include "use_mode.hpp"
#include "recording_health.hpp"
#include "train_queue.hpp"
#include "train_identity.hpp"
#include "status_display.hpp"
#include "display_power.hpp"
#include "history_viewer.hpp"
#include "runtime_settings.hpp"
#include "task_state.hpp"
#include "indicator_led.hpp"
#include "buzzer.hpp"
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

extern bool oled_off;
namespace {
TrainQueue queue;
train_identity::Resolver identityResolver;
lbj_data records[TrainQueue::Capacity];
float signals[TrainQueue::Capacity] = {};
bool ride = false, haveRideData = false;
char locked[9] = {}, displayed[9] = {};
lbj_data rideData;
uint32_t lastRide = 0, lastRideDraw = 0;
File rideFile;
String ridePath;
std::atomic<bool> recordFailed{false};
enum class View { List, Pick, Confirm, Result };
View view = View::List;
uint8_t selected = 0, choice = 0, choiceCount = 0;
char choices[21][9] = {};
char candidate[9] = {};
bool confirm = false;
const char *message = "";
bool resultAutoExit = false, resultShown = false;
uint32_t resultShownAt = 0;

bool busy() { return fd_state.load() != TASK_INIT; }
void alert() {
    triggerTrainArrivalLed();
    if (isTrainArrivalBuzzerEnabled()) buzzer.notifySignal();
}
String csvQuote(const String &value) {
    String clean = value;
    clean.replace("\"", "\"\"");
    clean.replace("\r", " "); clean.replace("\n", " ");
    return String("\"") + clean + "\"";
}
bool checkedSync(File &file, const String &path) {
    if (!file) return false;
    file.flush();
    if (file.getWriteError()) return false;
    const String full = String("/sd") + path;
    const int descriptor = open(full.c_str(), O_WRONLY);
    if (descriptor < 0) return false;
    const bool ok = fsync(descriptor) == 0;
    const bool closed = close(descriptor) == 0;
    return ok && closed;
}

const char *startRide(const char *key, bool &started) {
    started = false;
    if (busy()) return "正在写入，请稍后";
    if (!validTrainKey(key)) return "请先接收本车信号";
    if (!have_sd) return "无卡，未开始记录";
    if (!flushRideRecord()) return "写入失败，请检查卡";
    if (!SD.exists("/RIDES") && !SD.mkdir("/RIDES")) { reportRecordingFailure(RecordChannel::Ride); return "无法建立记录目录"; }
    char stamp[32]; tm time{};
    if (getValidLocalTime(&time)) strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &time);
    else snprintf(stamp, sizeof(stamp), "TIME_UNKNOWN_%lu", static_cast<unsigned long>(millis()));
    String path;
    bool available = false;
    for (unsigned n = 0; n < 10000; ++n) {
        path = String("/RIDES/") + key + "_" + stamp + "_" + String(n) + ".csv";
        if (!SD.exists(path)) { available = true; break; }
    }
    if (!available) return "文件重名，创建失败";
    File next = SD.open(path, FILE_WRITE);
    if (!next) { reportRecordingFailure(RecordChannel::Ride); return "未能开始随车记录"; }
    const char *header = "日期时间,运行毫秒,类型,车次,方向,速度,公里标,机车,线路,纬度,经度,RSSI,FER,PPM,扩展报文,原始报文,接收类别,接收车次,身份依据\n";
    if (next.print(header) != strlen(header) || !checkedSync(next, path)) {
        reportRecordingFailure(RecordChannel::Ride); next.close(); return "未能开始随车记录";
    }
    // Creating a replacement must succeed before closing the previous session.
    rideFile.close(); rideFile = next; ridePath = path;
    recordFailed = false;
    clearRecordingFailure(RecordChannel::Ride);
    ride = true; strncpy(locked, key, sizeof(locked));
    haveRideData = false; lastRideDraw = 0;
    queue = TrainQueue{};
    started = true;
    return "已开始随车记录";
}

void writeRide(const data_bond &bond, const rx_info &info, bool inferred) {
    if (recordFailed || !rideFile || !have_sd) { recordFailed = true; reportRecordingFailure(RecordChannel::Ride); return; }
    const auto &d = bond.lbjData;
    tm time{}; char stamp[32] = "时间无效";
    if (getValidLocalTime(&time)) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &time);
    String raw;
    for (const auto &p : bond.pocsagData) if (!p.is_empty)
        raw += String("[") + String(p.addr) + "/" + String(p.func) + ":" + p.str + "]";
    String line = csvQuote(stamp) + "," + String(millis()) + "," + String(d.type) + "," + csvQuote(locked) + "," +
        String(d.direction) + "," + csvQuote(d.speed) + "," + csvQuote(d.position) + "," + csvQuote(d.loco) + "," +
        csvQuote(d.route_utf8) + "," + csvQuote(d.pos_lat) + "," + csvQuote(d.pos_lon) + "," +
        String(info.rssi, 1) + "," + String(info.fer, 2) + "," + String(info.ppm, 2) + "," +
        csvQuote(d.info2_hex) + "," + csvQuote(raw) + "," + csvQuote(d.lbj_class) + "," +
        csvQuote(d.train) + "," + csvQuote(inferred ? "近期关联" : "完整车次") + "\n";
    if (rideFile.print(line) != line.length() || !checkedSync(rideFile, ridePath)) { recordFailed = true; reportRecordingFailure(RecordChannel::Ride); }
}

void drawRide() {
    if (!u8g2) return;
    char title[32], speed[48], position[48], age[48];
    snprintf(title, sizeof(title), "随车 %s", locked);
    snprintf(speed, sizeof(speed), "速度:%s km/h", haveRideData ? rideData.speed : "--");
    snprintf(position, sizeof(position), "公里标:%s", haveRideData ? rideData.position : "--");
    if (haveRideData) {
        const uint32_t seconds = uint32_t(millis() - lastRide) / 1000;
        if (seconds < 1000) snprintf(age, sizeof(age), "上次接收:%lu秒前", static_cast<unsigned long>(seconds));
        else snprintf(age, sizeof(age), "上次接收:%lu分前", static_cast<unsigned long>(seconds / 60));
    } else snprintf(age, sizeof(age), "等待本车新信号");
    const char *lines[] = {speed, position, age, recordFailed ? "本车记录失败" : "本车记录中"};
    showPassiveScreen(title, lines, 4);
}
}

bool validTrainKey(const char *key) {
    if (key[0] >= 'A' && key[0] <= 'Z' && key[0] != 'X') ++key;
    return train_identity::digits(key, 1, 5);
}
void makeTrainKey(const char *train, const char *category, char *out, bool extended) {
    const auto identity = train_identity::parse(train, category, extended);
    train_identity::copy(out, identity.key);
}
bool isRideMode() { return ride; }
bool flushRideRecord() {
    if (!rideFile) return true;
    const bool ok = checkedSync(rideFile, ridePath) && !recordFailed.load();
    if (!ok) { recordFailed = true; reportRecordingFailure(RecordChannel::Ride); }
    return ok;
}
void closeRideRecord() {
    if (rideFile) { rideFile.close(); recordFailed = true; }
}

// Main loop only, after formatter DONE and before its bond/radio stats are cleared.
void acceptModeReception(const data_bond &bond, const rx_info &info) {
    const auto &data = bond.lbjData;
    if (data.type < 0 || data.type > 1) {
        if (!ride && queue.current < 0 && data.type >= 0)
            requestDecodedDisplayUpdate(data, millis64(), info.rssi);
        return;
    }
    const auto identity = identityResolver.resolve(data.train, data.lbj_class, data.type == 1,
        data.direction, data.loco, millis());
    const char *key = identity.key;
    if (!validTrainKey(key)) return; // Uncertain packets remain in the unmodified total CSV.
    if (ride && !strcmp(key, locked)) {
        const bool fresh = !haveRideData || uint32_t(millis() - lastRide) >= 600000U;
        rideData = data; haveRideData = true; lastRide = millis(); lastRideDraw = 0;
        writeRide(bond, info, identity.inferred);
        displayActivity(runtimeSettings().display.wakeOnArrival);
        if (fresh) alert();
        return;
    }
    auto arrival = queue.receive(key, millis());
    if (arrival.slot < 0) return; // Total CSV already saved; only presentation has a bounded queue.
    records[arrival.slot] = data; signals[arrival.slot] = info.rssi;
    if (identity.inferred) {
        records[arrival.slot].lbj_class[0] = key[0] >= 'A' && key[0] <= 'Z' ? key[0] : ' ';
        records[arrival.slot].lbj_class[1] = 0;
    }
    if (arrival.fresh && (!ride || runtimeSettings().otherTrainAlerts)) alert();
    if (!ride) displayActivity(runtimeSettings().display.wakeOnArrival);
    else { queue.entries[arrival.slot].queued = false; queue.count = 0; }
}

void updateUseMode() {
    const bool visible = !oled_off && !isMenuDisplayActive() && !isHistoryDisplayActive();
    if (ride) {
        if (!visible) { lastRideDraw = 0; return; }
        if (!lastRideDraw || uint32_t(millis() - lastRideDraw) >= 1000) {
            drawRide(); lastRideDraw = millis();
        }
        return;
    }
    if (queue.advance(millis(), visible)) {
        const int slot = queue.current;
        strcpy(displayed, queue.entries[slot].key);
        requestDecodedDisplayUpdate(records[slot], millis64(), signals[slot], false, displayed);
    }
}

void resetModeMenu() {
    view = View::List; selected = 0; confirm = true;
    resultAutoExit = false; resultShown = false;
}
bool modeMenuShouldExit() {
    return view == View::Result && resultAutoExit && resultShown &&
        uint32_t(millis() - resultShownAt) >= 2000U;
}
void renderModeMenu() {
    if (view == View::Result) {
        const char *lines[] = {message}; showMenuScreen("使用模式", lines, 1, 0, false);
        if (resultAutoExit && !resultShown) { resultShownAt = millis(); resultShown = true; }
    }
    else if (view == View::Confirm) {
        char train[32]; snprintf(train, sizeof(train), "锁定本车:%s", candidate);
        const char *lines[] = {train, "确认并开始记录", "取消"};
        showMenuScreen("开始随车", lines, 3, confirm ? 1 : 2, true);
    } else if (view == View::Pick) {
        const char *lines[4] = {}; uint8_t first = choice > 3 ? choice - 3 : 0;
        uint8_t count = 0; for (; count < 4 && first + count < choiceCount; ++count) lines[count] = choices[first + count];
        showMenuScreen("选择本车", lines, count, choice - first, true);
    } else {
        const char *lines[] = {ride ? "当前模式:随车" : "当前模式:驻守", ride ? "更换本车" : "选择本车",
            runtimeSettings().otherTrainAlerts ? "其他列车提醒:开" : "其他列车提醒:关", ride && recordFailed ? "本车记录失败" : ""};
        showMenuScreen("使用模式", lines, 4, selected, true);
    }
}
bool handleModeButton(ButtonId key) {
    if (view == View::Result && resultAutoExit) return false;
    if (key == ButtonId::Key4) { if (view == View::List) return true; view = View::List; return false; }
    if (view == View::Result) { if (key == ButtonId::Key1) view = View::List; return false; }
    if (view == View::Confirm) {
        if (key == ButtonId::Key2 || key == ButtonId::Key3) confirm = !confirm;
        if (key == ButtonId::Key1) {
            if (!confirm) view = View::List;
            else { message = startRide(candidate, resultAutoExit); resultShown = false; view = View::Result; }
        }
        return false;
    }
    if (view == View::Pick) {
        if (key == ButtonId::Key2) choice = (choice + choiceCount - 1) % choiceCount;
        if (key == ButtonId::Key3) choice = (choice + 1) % choiceCount;
        if (key == ButtonId::Key1) { strcpy(candidate, choices[choice]); confirm = true; view = View::Confirm; }
        return false;
    }
    if (key == ButtonId::Key2) selected = (selected + 2) % 3;
    if (key == ButtonId::Key3) selected = (selected + 1) % 3;
    if (key != ButtonId::Key1) return false;
    if (selected == 2) {
        if (!toggleOtherTrainAlerts()) { message = "保存失败，设置未改"; view = View::Result; }
        return false;
    }
    if (busy()) { message = "正在写入，请稍后"; view = View::Result; return false; }
    if (selected == 0 && ride) {
        const bool saved = flushRideRecord() && !recordFailed.load();
        rideFile.close(); ride = false; locked[0] = 0;
        queue = TrainQueue{}; displayed[0] = 0;
        resetDecodedDisplay();
        message = saved ? "已切换驻守模式" : "已驻守，记录有错误";
        resultAutoExit = true; resultShown = false;
        view = View::Result;
    } else if (selected == 0 && validTrainKey(displayed)) {
        strcpy(candidate, displayed); confirm = true; view = View::Confirm;
    } else {
        choiceCount = loadHistoryTrainChoices(choices, 21);
        choice = 0;
        if (!choiceCount) { message = "请先接收本车信号"; view = View::Result; }
        else view = View::Pick;
    }
    return false;
}
