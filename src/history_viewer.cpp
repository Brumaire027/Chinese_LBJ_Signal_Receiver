#include "history_viewer.hpp"

#include "boards.hpp"
#include "status_display.hpp"
#include "utilities.h"
#include "use_mode.hpp"
#include "storage_paths.hpp"
#include "history_policy.hpp"
#include "history_stream.hpp"
#include "task_state.hpp"

extern SX1276 radio;
extern PagerClient pager;

#ifdef HAS_SDCARD
#include <SD.h>
#include <FS.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {

bool historyActive = false;
bool historyDirty = false;
bool historyEnterUp = false;

constexpr uint8_t MAX_HISTORY_RECORDS = 20;
constexpr size_t MAX_HISTORY_SCAN_BYTES = 32UL * 1024UL;
constexpr size_t MAX_CSV_LINE_LENGTH = 768;
constexpr size_t HISTORY_TIME_LENGTH = 64;
constexpr size_t HISTORY_LINE_LENGTH = 64;
constexpr size_t CSV_FIELD_LENGTH = 32;

constexpr uint8_t CSV_FIELD_SYSTEM_MS = 2;
constexpr uint8_t CSV_FIELD_DATE = 3;
constexpr uint8_t CSV_FIELD_TIME = 4;
constexpr uint8_t CSV_FIELD_LBJ_TIME = 5;
constexpr uint8_t CSV_FIELD_CLASS = 7;
constexpr uint8_t CSV_FIELD_TRAIN = 8;
constexpr uint8_t CSV_FIELD_SPEED = 9;
constexpr uint8_t CSV_FIELD_POSITION = 10;

struct HistoryRecord {
    char trainKey[9];
    char timestamp[HISTORY_TIME_LENGTH];
    char trainLine[HISTORY_LINE_LENGTH];
    char detailLine[HISTORY_LINE_LENGTH];
    char locoLine[HISTORY_LINE_LENGTH];
};

enum class HistoryLoadStatus : uint8_t {
    NotLoaded,
    Loading,
    Ready,
    NoSd,
    NoCsvFile,
    NoRecords,
    ReadError,
};

HistoryRecord historyRecords[MAX_HISTORY_RECORDS];
uint8_t historyRecordCount = 0;
uint8_t historySelectedIndex = 0;
HistoryLoadStatus historyLoadStatus = HistoryLoadStatus::NotLoaded;

void clearHistoryRecords(HistoryLoadStatus status) {
    historyRecordCount = 0;
    historySelectedIndex = 0;
    historyLoadStatus = status;
}

void copyCleanText(char *dest, size_t destSize, const char *src) {
    if (destSize == 0)
        return;

    size_t out = 0;
    bool lastWasSpace = false;
    while (*src && out + 1 < destSize) {
        char c = *src++;
        if (c == ',' || c == '"') {
            c = ' ';
        }
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
        if (c == ' ') {
            if (lastWasSpace)
                continue;
            lastWasSpace = true;
        } else {
            lastWasSpace = false;
        }
        dest[out++] = c;
    }
    dest[out] = '\0';
}

void trimText(char *text) {
    char *start = text;
    while (*start == ' ') {
        ++start;
    }

    if (start != text) {
        std::memmove(text, start, std::strlen(start) + 1);
    }

    size_t len = std::strlen(text);
    while (len > 0 && text[len - 1] == ' ') {
        text[--len] = '\0';
    }
}

bool extractCsvField(const char *line, uint8_t targetField, char *dest, size_t destSize) {
    if (destSize == 0)
        return false;

    dest[0] = '\0';
    uint8_t field = 0;
    bool quoted = false;
    size_t out = 0;

    for (const char *cursor = line; *cursor; ++cursor) {
        const char c = *cursor;
        if (c == '"') {
            if (quoted && cursor[1] == '"') {
                if (field == targetField && out + 1 < destSize) {
                    dest[out++] = '"';
                }
                ++cursor;
                continue;
            }
            quoted = !quoted;
            continue;
        }
        if (c == ',' && !quoted) {
            if (field == targetField)
                break;
            ++field;
            continue;
        }
        if (field == targetField && out + 1 < destSize) {
            dest[out++] = c;
        }
    }

    dest[out] = '\0';
    trimText(dest);
    return field >= targetField && out > 0;
}

bool isUsefulField(const char *value) {
    return value[0] != '\0' &&
           std::strcmp(value, "null") != 0 &&
           std::strcmp(value, "NUL") != 0 &&
           std::strcmp(value, "<NUL>") != 0 &&
           std::strcmp(value, "-----") != 0 &&
           std::strcmp(value, "----.-") != 0;
}

bool isLikelyDataLine(const char *line) {
    if (line[0] == '\0' || line[0] == '#')
        return false;
    if (std::strncmp(line, "null,", 5) == 0)
        return true;
    return (line[0] >= '0' && line[0] <= '9') || line[0] == '-';
}

bool isFieldStartCandidate(char c) {
    return (c >= '0' && c <= '9') || c == '-';
}

const char *findDataStart(const char *line) {
    if (isLikelyDataLine(line))
        return line;

    for (const char *cursor = line; *cursor; ++cursor) {
        if (!isFieldStartCandidate(*cursor))
            continue;
        if (cursor != line && ((cursor[-1] >= '0' && cursor[-1] <= '9') || cursor[-1] == '.'))
            continue;

        const char *firstComma = std::strchr(cursor, ',');
        if (!firstComma || !isFieldStartCandidate(firstComma[1]))
            continue;

        return cursor;
    }

    return nullptr;
}

void buildRecordTimestamp(const char *line, char *dest, size_t destSize) {
    char date[CSV_FIELD_LENGTH];
    char time[CSV_FIELD_LENGTH];
    char lbjTime[CSV_FIELD_LENGTH];
    char systemMs[CSV_FIELD_LENGTH];
    extractCsvField(line, CSV_FIELD_DATE, date, sizeof(date));
    extractCsvField(line, CSV_FIELD_TIME, time, sizeof(time));
    extractCsvField(line, CSV_FIELD_LBJ_TIME, lbjTime, sizeof(lbjTime));
    extractCsvField(line, CSV_FIELD_SYSTEM_MS, systemMs, sizeof(systemMs));

    if (isUsefulField(date) && isUsefulField(time)) {
        std::snprintf(dest, destSize, "%s %s", date, time);
    } else if (isUsefulField(lbjTime)) {
        std::snprintf(dest, destSize, "报文时间:%s", lbjTime);
    } else if (isUsefulField(systemMs)) {
        std::snprintf(dest, destSize, "运行:%s毫秒", systemMs);
    } else {
        copyCleanText(dest, destSize, "时间未知");
    }
}

bool buildRecordLines(const char *line, HistoryRecord &record) {
    char lbjClass[CSV_FIELD_LENGTH], train[CSV_FIELD_LENGTH], speed[CSV_FIELD_LENGTH];
    char position[CSV_FIELD_LENGTH], loco[CSV_FIELD_LENGTH], route[CSV_FIELD_LENGTH];
    char lat[CSV_FIELD_LENGTH], lon[CSV_FIELD_LENGTH], hex[CSV_FIELD_LENGTH];
    extractCsvField(line, CSV_FIELD_CLASS, lbjClass, sizeof(lbjClass));
    extractCsvField(line, CSV_FIELD_TRAIN, train, sizeof(train));
    extractCsvField(line, CSV_FIELD_SPEED, speed, sizeof(speed));
    extractCsvField(line, CSV_FIELD_POSITION, position, sizeof(position));
    extractCsvField(line, 11, loco, sizeof(loco));
    extractCsvField(line, 12, route, sizeof(route));
    extractCsvField(line, 13, lat, sizeof(lat));
    extractCsvField(line, 14, lon, sizeof(lon));
    extractCsvField(line, 15, hex, sizeof(hex));
    if (!history_policy::useful(train, speed, position, loco, route, lat, lon)) return false;
    makeTrainKey(train, lbjClass, record.trainKey, isUsefulField(hex));
    const bool classValid = lbjClass[0] >= 'A' && lbjClass[0] <= 'Z' &&
        lbjClass[0] != 'X' && !lbjClass[1];
    const bool speedValid = display_fields::number(speed);
    const bool positionValid = display_fields::number(position, true);
    std::snprintf(record.trainLine, sizeof(record.trainLine), "车:%s%s 速:%s",
        classValid ? lbjClass : "", display_fields::number(train) ? train : "--",
        speedValid ? speed : "--");
    const bool routeValid = history_policy::route(route);
    std::snprintf(record.detailLine, sizeof(record.detailLine), "线:%s %sK",
        routeValid ? route : "--", positionValid ? position : "--");
    // If coordinates are the only decoded content, make that reason for retaining the record visible.
    if (!routeValid && !positionValid) {
        if (history_policy::coordinate(lat, 8))
            std::snprintf(record.detailLine, sizeof(record.detailLine), "纬:%s", lat);
        else if (history_policy::coordinate(lon, 9))
            std::snprintf(record.detailLine, sizeof(record.detailLine), "经:%s", lon);
    }
    // Keep the full locomotive number on its own row instead of squeezing it beside the route.
    std::snprintf(record.locoLine, sizeof(record.locoLine), "机车:%s",
        display_fields::digits(loco, 8) ? loco : "--");
    if (!display_fields::digits(loco, 8) && history_policy::coordinate(lon, 9))
        std::snprintf(record.locoLine, sizeof(record.locoLine), "经:%s", lon);
    return true;
}

void appendHistoryRecord(const char *line) {
    HistoryRecord record{};
    // Empty receptions must not consume one of the twenty history slots.
    if (!buildRecordLines(line, record)) return;
    buildRecordTimestamp(line, record.timestamp, sizeof(record.timestamp));
    if (historyRecordCount == MAX_HISTORY_RECORDS) {
        for (uint8_t i = 1; i < MAX_HISTORY_RECORDS; ++i) {
            historyRecords[i - 1] = historyRecords[i];
        }
        historyRecordCount = MAX_HISTORY_RECORDS - 1;
    }

    historyRecords[historyRecordCount++] = record;
}

void processCsvLine(char *line) {
    const char *dataStart = findDataStart(line);
    if (!dataStart || !history_stream::completeRecord(dataStart))
        return;
    appendHistoryRecord(dataStart);
}

#ifdef HAS_SDCARD
bool parseCsvFileNumber(const char *name, uint16_t &number) {
    const char *baseName = std::strrchr(name, '/');
    baseName = baseName ? baseName + 1 : name;

    if (std::strncmp(baseName, "CSV_", 4) != 0)
        return false;
    if (std::strlen(baseName) != 12 || std::strcmp(baseName + 8, ".csv") != 0)
        return false;

    char digits[5];
    std::memcpy(digits, baseName + 4, 4);
    digits[4] = '\0';
    for (uint8_t i = 0; i < 4; ++i) {
        if (digits[i] < '0' || digits[i] > '9')
            return false;
    }

    number = static_cast<uint16_t>(std::strtoul(digits, nullptr, 10));
    return true;
}

enum class LoadPhase { Idle, OpenDirectory, Directory, OpenCsv, Read };
LoadPhase loadPhase = LoadPhase::Idle;
File historyDirectory, historyCsv;
uint16_t highestNumber = 0;
bool foundCsv = false;
size_t remainingBytes = 0;
history_stream::Line<MAX_CSV_LINE_LENGTH> historyLine;
int previousReceiveBytes = -1;
uint32_t quietSince = 0;

void closeHistoryFiles() {
    historyCsv.close(); historyDirectory.close(); loadPhase = LoadPhase::Idle;
}
void finishHistoryLoad(HistoryLoadStatus status) {
    closeHistoryFiles();
    historyLoadStatus = status;
    historySelectedIndex = history_policy::entry(historyRecordCount, historyEnterUp);
    historyDirty = true;
}

void stepHistoryLoad() {
    if (loadPhase == LoadPhase::Idle) return;
    // One bounded slice per main-loop turn, only after the receiver has had
    // first opportunity to consume a batch. A single SD operation can still stall.
    const int bytes = radio.available();
    const uint32_t now = millis();
    if (fd_state.load() != TASK_INIT || pager.available() >= 2 || bytes != previousReceiveBytes) {
        previousReceiveBytes = bytes; quietSince = now; return;
    }
    if (uint32_t(now - quietSince) < 120) return;
    if (!have_sd) { finishHistoryLoad(HistoryLoadStatus::NoSd); return; }
    if (loadPhase == LoadPhase::OpenDirectory) {
        historyDirectory = SD.open(StoragePaths::Records, FILE_READ);
        if (!historyDirectory || !historyDirectory.isDirectory()) {
            finishHistoryLoad(HistoryLoadStatus::NoCsvFile); return;
        }
        loadPhase = LoadPhase::Directory;
        return;
    }
    if (loadPhase == LoadPhase::Directory) {
        File entry = historyDirectory.openNextFile();
        if (entry) {
            uint16_t number = 0;
            if (!entry.isDirectory() && parseCsvFileNumber(entry.name(), number) &&
                (!foundCsv || number > highestNumber)) {
                foundCsv = true; highestNumber = number;
            }
            entry.close(); return;
        }
        historyDirectory.close();
        if (!foundCsv) { finishHistoryLoad(HistoryLoadStatus::NoCsvFile); return; }
        loadPhase = LoadPhase::OpenCsv;
        return;
    }
    if (loadPhase == LoadPhase::OpenCsv) {
        char path[40];
        std::snprintf(path, sizeof(path), "%s/CSV_%04u.csv", StoragePaths::Records, highestNumber);
        historyCsv = SD.open(path, FILE_READ);
        if (!historyCsv) { finishHistoryLoad(HistoryLoadStatus::ReadError); return; }
        const size_t fileSize = historyCsv.size();
        const size_t start = fileSize > MAX_HISTORY_SCAN_BYTES ? fileSize - MAX_HISTORY_SCAN_BYTES : 0;
        if (start && !historyCsv.seek(start)) { finishHistoryLoad(HistoryLoadStatus::ReadError); return; }
        // Freeze the end offset so concurrent appends cannot extend this scan.
        remainingBytes = fileSize - start;
        historyLine.reset(start != 0);
        loadPhase = LoadPhase::Read;
        return;
    }
    if (remainingBytes) {
        uint8_t chunk[256];
        const size_t requested = remainingBytes < sizeof(chunk) ? remainingBytes : sizeof(chunk);
        const int count = historyCsv.read(chunk, requested);
        if (count <= 0) { finishHistoryLoad(HistoryLoadStatus::ReadError); return; }
        remainingBytes -= static_cast<size_t>(count);
        for (int i = 0; i < count; ++i)
            if (historyLine.push(static_cast<char>(chunk[i]))) processCsvLine(historyLine.data);
    }
    if (!remainingBytes)
        finishHistoryLoad(historyRecordCount ? HistoryLoadStatus::Ready : HistoryLoadStatus::NoRecords);
}
#endif

void loadRecentHistoryRecords() {
#ifdef HAS_SDCARD
    closeHistoryFiles();
#endif
    clearHistoryRecords(HistoryLoadStatus::Loading);
    historyDirty = true;
#ifndef HAS_SDCARD
    historyLoadStatus = HistoryLoadStatus::NoSd;
#else
    if (!have_sd) { historyLoadStatus = HistoryLoadStatus::NoSd; return; }
    foundCsv = false; highestNumber = 0;
    previousReceiveBytes = -1; quietSince = millis();
    loadPhase = LoadPhase::OpenDirectory;
#endif
}

void moveHistory(HistoryEntryDirection direction) {
    if (historyLoadStatus == HistoryLoadStatus::Loading || historyRecordCount == 0) {
        historyDirty = true;
        return;
    }

    historySelectedIndex = history_policy::move(historyRecordCount, historySelectedIndex,
        direction == HistoryEntryDirection::Previous);
    historyDirty = true;
}

void renderHistory() {
    char title[48];
    const char *lines[4];
    if (historyLoadStatus == HistoryLoadStatus::Ready && historyRecordCount > 0) {
        const HistoryRecord &record = historyRecords[historySelectedIndex];
        std::snprintf(title, sizeof(title), "历史 %u/%u", unsigned(history_policy::number(historyRecordCount, historySelectedIndex)), unsigned(historyRecordCount));
        lines[0] = record.timestamp;
        lines[1] = record.trainLine;
        lines[2] = record.detailLine;
        lines[3] = record.locoLine;
    } else {
        std::snprintf(title, sizeof(title), "历史记录");
        const char *statusLine = "尚未读取";
        switch (historyLoadStatus) {
            case HistoryLoadStatus::Loading: statusLine = "正在读取记录"; break;
            case HistoryLoadStatus::NoSd: statusLine = "存储卡不可用"; break;
            case HistoryLoadStatus::NoCsvFile: statusLine = "未找到记录文件"; break;
            case HistoryLoadStatus::NoRecords: statusLine = "暂无记录"; break;
            case HistoryLoadStatus::ReadError: statusLine = "读取失败"; break;
            default: break;
        }
        lines[0] = statusLine;
        lines[1] = historyLoadStatus == HistoryLoadStatus::Loading ? "接收优先，请稍候" :
            historyLoadStatus == HistoryLoadStatus::NoRecords ? "" : "请检查存储卡";
        lines[2] = "";
        lines[3] = "";
    }
    showHistoryRecordScreen(title, lines, 4);
}

}  // namespace

void initHistoryViewer() {
    historyActive = false;
    historyDirty = false;
    clearHistoryRecords(HistoryLoadStatus::NotLoaded);
    setHistoryDisplayActive(false);
}

void requestHistoryLoad() { loadRecentHistoryRecords(); }
bool historyLoadPending() { return historyLoadStatus == HistoryLoadStatus::Loading; }
void cancelHistoryLoad() {
#ifdef HAS_SDCARD
    closeHistoryFiles();
#endif
    if (historyLoadPending()) clearHistoryRecords(HistoryLoadStatus::NotLoaded);
}
void processHistoryLoad() {
#ifdef HAS_SDCARD
    stepHistoryLoad();
#endif
}
uint8_t loadHistoryTrainChoices(char (*keys)[9], uint8_t capacity) {
    if (historyLoadStatus != HistoryLoadStatus::Ready) return 0;
    uint8_t count = 0;
    for (int i = historyRecordCount - 1; i >= 0 && count < capacity; --i) {
        const char *key = historyRecords[i].trainKey;
        if (!validTrainKey(key)) continue;
        bool duplicate = false;
        for (uint8_t j = 0; j < count; ++j) if (!strcmp(key, keys[j])) duplicate = true;
        if (!duplicate) strcpy(keys[count++], key);
    }
    return count;
}

bool isHistoryViewerActive() {
    return historyActive;
}

void enterHistoryViewer(HistoryEntryDirection direction) {
    historyActive = true;
    setHistoryDisplayActive(true);
    historyEnterUp = direction == HistoryEntryDirection::Previous;
    loadRecentHistoryRecords();
    historySelectedIndex = history_policy::entry(historyRecordCount,
        direction == HistoryEntryDirection::Previous);
    historyDirty = true;
}

void exitHistoryViewer() {
    cancelHistoryLoad();
    historyActive = false;
    historyDirty = false;
    setHistoryDisplayActive(false);
}

void handleHistoryButtonEvent(const ButtonEvent &event) {
    if (event.type == ButtonEventType::Released) {
        return;
    }

    if (event.id == ButtonId::Key4) {
        exitHistoryViewer();
        return;
    }

    if (event.type != ButtonEventType::ShortPress) {
        return;
    }

    switch (event.id) {
        case ButtonId::Key1:
            historyEnterUp = false;
            loadRecentHistoryRecords();
            historyDirty = true;
            break;
        case ButtonId::Key2:
            moveHistory(HistoryEntryDirection::Previous);
            break;
        case ButtonId::Key3:
            moveHistory(HistoryEntryDirection::Next);
            break;
        case ButtonId::Key4:
            break;
    }
}

void updateHistoryViewer() {
    if (!historyActive || !historyDirty) {
        return;
    }

    renderHistory();
    historyDirty = false;
}
