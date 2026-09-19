#include "history_viewer.hpp"

#include "boards.hpp"
#include "status_display.hpp"
#include "utilities.h"
#include "use_mode.hpp"
#include "storage_paths.hpp"

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
    char packetLine[HISTORY_LINE_LENGTH];
};

enum class HistoryLoadStatus : uint8_t {
    NotLoaded,
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

bool extractFirstPacketId(const char *line, char *dest, size_t destSize) {
    if (destSize == 0)
        return false;

    dest[0] = '\0';
    const char *packet = std::strchr(line, '[');
    while (packet) {
        const char *cursor = packet + 1;
        if (*cursor >= '0' && *cursor <= '9') {
            size_t out = 0;
            while (*cursor && *cursor != ':' && *cursor != ']' && out + 1 < destSize) {
                dest[out++] = *cursor++;
            }
            dest[out] = '\0';
            return out > 0;
        }
        packet = std::strchr(packet + 1, '[');
    }

    return false;
}

void buildRecordLines(const char *line, HistoryRecord &record) {
    char lbjClass[CSV_FIELD_LENGTH];
    char train[CSV_FIELD_LENGTH];
    char speed[CSV_FIELD_LENGTH];
    char position[CSV_FIELD_LENGTH];
    char packet[CSV_FIELD_LENGTH];
    extractCsvField(line, CSV_FIELD_CLASS, lbjClass, sizeof(lbjClass));
    extractCsvField(line, CSV_FIELD_TRAIN, train, sizeof(train));
    char hex[CSV_FIELD_LENGTH];
    extractCsvField(line, 15, hex, sizeof(hex));
    makeTrainKey(train, lbjClass, record.trainKey, isUsefulField(hex));
    extractCsvField(line, CSV_FIELD_SPEED, speed, sizeof(speed));
    extractCsvField(line, CSV_FIELD_POSITION, position, sizeof(position));

    const char *displayClass = isUsefulField(lbjClass) ? lbjClass : "-";
    const char *displayTrain = isUsefulField(train) ? train : "-";
    const char *displaySpeed = isUsefulField(speed) ? speed : "-";
    const char *displayPosition = isUsefulField(position) ? position : "-";

    std::snprintf(record.trainLine, sizeof(record.trainLine), "级:%s 车:%s", displayClass, displayTrain);
    std::snprintf(record.detailLine, sizeof(record.detailLine), "速:%s 公里:%s", displaySpeed, displayPosition);

    if (extractFirstPacketId(line, packet, sizeof(packet))) {
        std::snprintf(record.packetLine, sizeof(record.packetLine), "报文:%s", packet);
        return;
    }

    copyCleanText(record.packetLine, sizeof(record.packetLine), "报文:未知");
}

void appendHistoryRecord(const char *line) {
    if (historyRecordCount == MAX_HISTORY_RECORDS) {
        for (uint8_t i = 1; i < MAX_HISTORY_RECORDS; ++i) {
            historyRecords[i - 1] = historyRecords[i];
        }
        historyRecordCount = MAX_HISTORY_RECORDS - 1;
    }

    HistoryRecord &record = historyRecords[historyRecordCount++];
    buildRecordTimestamp(line, record.timestamp, sizeof(record.timestamp));
    buildRecordLines(line, record);
}

void processCsvLine(char *line) {
    const char *dataStart = findDataStart(line);
    if (!dataStart)
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

bool findLatestCsvPath(const char *directory, char *path, size_t pathSize) {
    File dir = SD.open(directory, FILE_READ);
    if (!dir || !dir.isDirectory()) {
        clearHistoryRecords(HistoryLoadStatus::NoCsvFile);
        return false;
    }

    bool found = false;
    uint16_t highestNumber = 0;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry)
            break;

        if (!entry.isDirectory()) {
            uint16_t number = 0;
            if (parseCsvFileNumber(entry.name(), number) && (!found || number > highestNumber)) {
                highestNumber = number;
                found = true;
            }
        }
        entry.close();
    }
    dir.close();

    if (!found) {
        clearHistoryRecords(HistoryLoadStatus::NoCsvFile);
        return false;
    }

    std::snprintf(path, pathSize, "%s/CSV_%04u.csv", directory, highestNumber);
    return true;
}

void readRecentRecordsFromCsv(const char *path) {
    File csv = SD.open(path, FILE_READ);
    if (!csv) {
        clearHistoryRecords(HistoryLoadStatus::ReadError);
        return;
    }

    const size_t fileSize = csv.size();
    const size_t scanStart = fileSize > MAX_HISTORY_SCAN_BYTES ? fileSize - MAX_HISTORY_SCAN_BYTES : 0;
    if (scanStart > 0) {
        if (!csv.seek(scanStart)) {
            csv.close();
            clearHistoryRecords(HistoryLoadStatus::ReadError);
            return;
        }

        while (csv.available()) {
            const char c = static_cast<char>(csv.read());
            if (c == '\n')
                break;
        }
    }

    char line[MAX_CSV_LINE_LENGTH];
    size_t lineLength = 0;
    while (csv.available()) {
        const char c = static_cast<char>(csv.read());
        if (c == '\r')
            continue;

        if (c == '\n') {
            line[lineLength] = '\0';
            processCsvLine(line);
            lineLength = 0;
            continue;
        }

        if (lineLength + 1 < sizeof(line)) {
            line[lineLength++] = c;
        }
    }

    if (lineLength > 0) {
        line[lineLength] = '\0';
        processCsvLine(line);
    }

    csv.close();

    if (historyRecordCount == 0) {
        historyLoadStatus = HistoryLoadStatus::NoRecords;
    } else {
        historySelectedIndex = historyRecordCount - 1;
        historyLoadStatus = HistoryLoadStatus::Ready;
    }
}
#endif

void loadRecentHistoryRecords() {
    clearHistoryRecords(HistoryLoadStatus::NotLoaded);

#ifndef HAS_SDCARD
    clearHistoryRecords(HistoryLoadStatus::NoSd);
#else
    if (!have_sd) {
        clearHistoryRecords(HistoryLoadStatus::NoSd);
        return;
    }

    char latestCsvPath[40];
    if (findLatestCsvPath(StoragePaths::Records, latestCsvPath, sizeof(latestCsvPath)))
        readRecentRecordsFromCsv(latestCsvPath);
#endif
}

void moveHistory(HistoryEntryDirection direction) {
    if (historyRecordCount == 0) {
        historyDirty = true;
        return;
    }

    if (direction == HistoryEntryDirection::Previous) {
        historySelectedIndex = historySelectedIndex == 0 ? historyRecordCount - 1 : historySelectedIndex - 1;
    } else {
        historySelectedIndex = (historySelectedIndex + 1) % historyRecordCount;
    }
    historyDirty = true;
}

void renderHistory() {
    char title[48];
    const char *lines[4];
    if (historyLoadStatus == HistoryLoadStatus::Ready && historyRecordCount > 0) {
        const HistoryRecord &record = historyRecords[historySelectedIndex];
        std::snprintf(title, sizeof(title), "历史 %u/%u", unsigned(historySelectedIndex + 1), unsigned(historyRecordCount));
        lines[0] = record.timestamp;
        lines[1] = record.trainLine;
        lines[2] = record.detailLine;
        lines[3] = record.packetLine;
    } else {
        std::snprintf(title, sizeof(title), "历史记录");
        const char *statusLine = "尚未读取";
        switch (historyLoadStatus) {
            case HistoryLoadStatus::NoSd: statusLine = "存储卡不可用"; break;
            case HistoryLoadStatus::NoCsvFile: statusLine = "未找到记录文件"; break;
            case HistoryLoadStatus::NoRecords: statusLine = "暂无记录"; break;
            case HistoryLoadStatus::ReadError: statusLine = "读取失败"; break;
            default: break;
        }
        lines[0] = statusLine;
        lines[1] = "请检查存储卡";
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

uint8_t loadHistoryTrainChoices(char (*keys)[9], uint8_t capacity) {
    loadRecentHistoryRecords();
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
    loadRecentHistoryRecords();
    if (historyRecordCount > 1 && direction == HistoryEntryDirection::Previous) {
        historySelectedIndex = historyRecordCount - 1;
    }
    historyDirty = true;
}

void exitHistoryViewer() {
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
