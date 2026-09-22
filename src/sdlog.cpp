#include "reception_debug.hpp"
#include "recording_health.hpp"
//
// Created by FLN1021 on 2023/9/5.
//

#include "sdlog.hpp"
#include "time_service.hpp"
#include "use_mode.hpp"

#include "debug_log.hpp"
#include <fcntl.h>
#include <unistd.h>

namespace {
// File::flush() returns void in this framework. Ask VFS for a checked sync too.
bool syncLogPath(const String &path) {
    const String absolute = String("/sd") + path;
    const int fd = open(absolute.c_str(), O_WRONLY);
    if (fd < 0) return false;
    const bool synced = fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    return synced && closed;
}
}

// Initialize static variables.
// File SD_LOG::log;
// File SD_LOG::csv;
// bool SD_LOG::is_newfile = false;
// bool SD_LOG::is_newfile_csv = false;
// char SD_LOG::filename[32] = "";
// char SD_LOG::filename_csv[32] = "";
// bool SD_LOG::sd_log = false;
// bool SD_LOG::sd_csv = false;
// struct tm SD_LOG::timein{};
// fs::FS *SD_LOG::filesys;
// int SD_LOG::log_count = 0;
// const char *SD_LOG::log_directory{};
// const char *SD_LOG::csv_directory{};
// String SD_LOG::log_path;
// String SD_LOG::csv_path;
// bool SD_LOG::is_startline = true;
// bool SD_LOG::is_startline_csv = true;
// bool SD_LOG::size_checked = false;
// String SD_LOG::large_buffer;
// String SD_LOG::large_buffer_csv;

// SD_LOG::SD_LOG(fs::FS &fs) {
//     filesys = &fs;
// }

SD_LOG::SD_LOG() :
        filesys{},
        log_count{},
        filename{},
        filename_csv{},
        sd_log(false),
        sd_csv(false),
        sd_cd(false),
        is_newfile(false),
        is_newfile_csv(false),
        is_startline(true),
        is_startline_csv(true),
        size_checked(false),
        log_directory{},
        csv_directory{} {}

void SD_LOG::setFS(fs::FS &fs) {
    filesys = &fs;
}

void SD_LOG::getFilename(const char *path) {
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            debugLogError("[SDLOG] Failed to open log directory!\n");
            debugLogError("[SDLOG] Will not write log to SD card.\n");
            sd_log = false; reportRecordingFailure(RecordChannel::Log);
            return;
        }
    }
    if (!cwd.isDirectory()) {
        debugLogError("[SDLOG] log directory error!\n");
        sd_log = false; reportRecordingFailure(RecordChannel::Log);
        return;
    }
    char last[32];
    // read index
    int counter = readIndex(cwd);
    // int counter = 0;
    // while (cwd.openNextFile()) {
    //     counter++;
    // }
    sprintf(last, "LOG_%04d.txt", counter - 1);
    String last_path = String(String(path) + "/" + String(last));
    if (!filesys->exists(last_path)) {
        sprintf(last, "LOG_%04d.txt", counter - 1);
        last_path = String(String(path) + "/" + String(last));
    }
    File last_log = filesys->open(last_path);

    if (last_log && last_log.size() <= MAX_LOG_SIZE && counter > 0) {
        sprintf(filename, "%s", last_log.name());
        log_count = counter;
    } else {
        sprintf(filename, "LOG_%04d.txt", counter);
        is_newfile = true;
        log_count = counter;
        // update index
        updateIndex(String(path),counter + 1);
    }
    debugLogInfo("[SDLOG] %d log files, using %s \n", counter, filename);
    sd_log = true;
}

void SD_LOG::getFilenameCSV(const char *path) {
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            debugLogError("[SDLOG] Failed to open csv directory!\n");
            debugLogError("[SDLOG] Will not write csv to SD card.\n");
            sd_csv = false; reportRecordingFailure(RecordChannel::Csv);
            return;
        }
    }
    if (!cwd.isDirectory()) {
        debugLogError("[SDLOG] csv directory error!\n");
        sd_csv = false; reportRecordingFailure(RecordChannel::Csv);
        return;
    }
    char last[32];
    int counter = readIndex(cwd);
    // int counter = 0;
    // while (cwd.openNextFile()) {
    //     counter++;
    // }
    sprintf(last, "CSV_%04d.csv", counter - 1);
    String last_path = String(String(path) + "/" + String(last));
    if (!filesys->exists(last_path)) {
        sprintf(last, "CSV_%04d.csv", counter - 1);
        last_path = String(String(path) + "/" + String(last));
    }
    File last_csv = filesys->open(last_path);

    if (last_csv.size() <= MAX_LOG_SIZE && counter > 0 && last_csv) {
        sprintf(filename_csv, "%s", last_csv.name());
    } else {
        sprintf(filename_csv, "CSV_%04d.csv", counter);
        is_newfile_csv = true;
        updateIndex(String(path),counter + 1);
    }
    debugLogInfo("[SDLOG] %d csv files, using %s \n", counter, filename_csv);
    sd_csv = true;
}

int SD_LOG::begin(const char *path) {
    clearRecordingFailure(RecordChannel::Log);
    log_directory = path;
    getFilename(path);
    if (!sd_log)
        return -1;
    log_path = String(String(path) + '/' + filename);
    log = filesys->open(log_path, "a", true);
    if (!log) {
        debugLogError("[SDLOG] Failed to open log file!\n");
        debugLogError("[SDLOG] Will not write log to SD card.\n");
        sd_log = false; reportRecordingFailure(RecordChannel::Log);
        return -1;
    }
    writeHeader();
    if (!log || log.getWriteError() || recordingFailed(RecordChannel::Log) || !syncLogPath(log_path)) { sd_log = false; reportRecordingFailure(RecordChannel::Log); return -1; }
    sd_log = true;
    return 0;
}

int SD_LOG::beginCSV(const char *path) {
    clearRecordingFailure(RecordChannel::Csv);
    csv_directory = path;
    getFilenameCSV(path);
    if (!sd_csv)
        return -1;
    csv_path = String(String(path) + '/' + filename_csv);
    csv = filesys->open(csv_path, "a", true);
    if (!csv) {
        debugLogError("[SDLOG] Failed to open csv file!\n");
        debugLogError("[SDLOG] Will not write csv to SD card.\n");
        sd_csv = false; reportRecordingFailure(RecordChannel::Csv);
        return -1;
    }
    // csv.close();
    // csv = filesys->open(csv_path, "a", true);
    writeHeaderCSV();
    if (!csv || csv.getWriteError() || recordingFailed(RecordChannel::Csv) || !syncLogPath(csv_path)) { sd_csv = false; reportRecordingFailure(RecordChannel::Csv); return -1; }

    // Serial.printf("csvsize = %d\n",csv.size());
    // if (csv.size() == 0)
    //     writeHeaderCSV();
    sd_csv = true;
    return 0;
}

void SD_LOG::writeHeader() {
    CheckedRecordPrint(log, RecordChannel::Log).println("-------------------------------------------------");
    if (is_newfile) {
        CheckedRecordPrint(log, RecordChannel::Log).printf("ESP32 DEV MODULE LOG FILE %s \n", filename);
    }
    CheckedRecordPrint(log, RecordChannel::Log).printf("BEGIN OF SYSTEM LOG, STARTUP TIME %llu MS.\n", millis64());
    if (getValidLocalTime(&timein))
        CheckedRecordPrint(log, RecordChannel::Log).printf("CURRENT TIME %d-%02d-%02d %02d:%02d:%02d\n",
                   timein.tm_year + 1900, timein.tm_mon + 1, timein.tm_mday, timein.tm_hour, timein.tm_min,
                   timein.tm_sec);
    CheckedRecordPrint(log, RecordChannel::Log).println("-------------------------------------------------");
    log.flush();
    if (!syncLogPath(log_path) || log.getWriteError()) reportRecordingFailure(RecordChannel::Log);
}

void SD_LOG::writeHeaderCSV() { // TODO: needs more confirmation about title.
    csv.close();
    csv = filesys->open(csv_path, "a");
    // Serial.printf("csvfile %s, csv size %d, is_newfile_csv = %d\n",csv.path(),csv.size(),is_newfile_csv);
    if (is_newfile_csv && csv.size() < 200) {
        CheckedRecordPrint(csv, RecordChannel::Csv).printf("# ESP32 DEV MODULE CSV FILE %s \n", filename_csv);
        // CheckedRecordPrint(csv, RecordChannel::Csv).printf(
        //         "电压,系统时间,日期,时间,LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,原始数据,错误,错误率\n");
        CheckedRecordPrint(csv, RecordChannel::Csv).printf(
                "温度,电压,系统时间,日期,时间,LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,PPM(FER),PPM(CURRENT),原始数据,错误,错误率\n");
        if (sd_log) {
            append("[SDLOG][D] Writing CSV Headers, is_newfile = %d, filesize = %d\n", is_newfile_csv, csv.size());
        }
        debugLogVerbose("[SDLOG][D] Writing CSV Headers, is_newfile = %d, filesize = %d\n", is_newfile_csv, csv.size());
    }
    csv.flush();
    if (!syncLogPath(csv_path) || csv.getWriteError()) reportRecordingFailure(RecordChannel::Csv);
    csv.close();
    csv = filesys->open(csv_path, "a");
    // Serial.printf("Write hdr end\n");
}

void SD_LOG::appendCSV(const char *format, ...) {
    if (!rxDebugAllowCsv()) return;
    RxDebugTimer timing(RxDebugStage::Csv); // TODO: maybe implement item based csv append?
    if (!sd_csv) {
        return;
    }
    if (!filesys->exists(csv_path)) {
        debugLogError("[SDLOG] CSV file unavailable!\n");
        sd_csv = false; reportRecordingFailure(RecordChannel::Csv);
        end();
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE && !size_checked) {
        csv.close();
        if (beginCSV(csv_directory) != 0) return;
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline_csv) {
        CheckedRecordPrint(csv, RecordChannel::Csv).printf("%1.2f,", battery.readVoltage() * 2);
        CheckedRecordPrint(csv, RecordChannel::Csv).printf("%llu,", millis64());
        if (getValidLocalTime(&timein)) {
            CheckedRecordPrint(csv, RecordChannel::Csv).printf("%d-%02d-%02d,%02d:%02d:%02d,", timein.tm_year + 1900, timein.tm_mon + 1,
                       timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
        } else {
            CheckedRecordPrint(csv, RecordChannel::Csv).printf("null,null,");
        }
        is_startline_csv = false;
    }
    if (nullptr != strchr(buffer, '\n')) /* detect end of line in stream */
        is_startline_csv = true;
    CheckedRecordPrint(csv, RecordChannel::Csv).print(buffer);
    csv.flush();
    if (!syncLogPath(csv_path) || csv.getWriteError()) reportRecordingFailure(RecordChannel::Csv);
}

void SD_LOG::append(const char *format, ...) {
    if (!rxDebugAllowLog()) return;
    RxDebugTimer timing(RxDebugStage::Log);
    // Serial.printf("[D] Using log %s\n", log_path.c_str());
    if (!sd_log) {
        // Serial.println("[D] sd_log false.");
        return;
    }
    if (!filesys->exists(log_path)) {
        debugLogError("[SDLOG] Log file %s unavailable!\n", log_path.c_str());
        sd_log = false; reportRecordingFailure(RecordChannel::Log);
        end();
        return;
    }
    if (log.size() >= MAX_LOG_SIZE && !size_checked) {
        log.close();
        if (begin(log_directory) != 0) return;
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline) {
        if (getValidLocalTime(&timein)) {
            CheckedRecordPrint(log, RecordChannel::Log).printf("%d-%02d-%02d %02d:%02d:%02d > ", timein.tm_year + 1900, timein.tm_mon + 1,
                       timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
        } else {
            CheckedRecordPrint(log, RecordChannel::Log).printf("[%6llu.%03llu] > ", millis64() / 1000, millis64() % 1000);
        }
        is_startline = false;
    }
    if (nullptr != strchr(buffer, '\n')) /* detect end of line in stream */
        is_startline = true;
    CheckedRecordPrint(log, RecordChannel::Log).print(buffer);
    log.flush();
    if (!syncLogPath(log_path) || log.getWriteError()) reportRecordingFailure(RecordChannel::Log);
//    Serial.printf("[D] Using log %s \n", log_path.c_str());
}

void SD_LOG::append(int level, const char *format, ...) {
    if (!rxDebugAllowLog()) return;
    if (level > LOG_VERBOSITY)
        return;
    appendBuffer("[DEBUG-%d] ", level);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer("%s", buffer);
    sendBufferLOG();
}

void SD_LOG::appendBuffer(const char *format, ...) {
    if (!rxDebugAllowLog()) return;
    if (!sd_log)
        return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline) {
        char *time_buffer = new char[128];
        if (getValidLocalTime(&timein)) {
            sprintf(time_buffer, "%d-%02d-%02d %02d:%02d:%02d > ", timein.tm_year + 1900, timein.tm_mon + 1,
                    timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
            large_buffer += time_buffer;
        } else {
            sprintf(time_buffer, "[%6llu.%03llu] > ", millis64() / 1000, millis64() % 1000);
            large_buffer += time_buffer;
        }
        delete[] time_buffer;
        is_startline = false;
    }
    if (nullptr != strchr(buffer, '\n')) /* detect end of line in stream */
        is_startline = true;
    large_buffer += buffer;
}

void SD_LOG::appendBuffer(int level, const char *format, ...) {
    if (!rxDebugAllowLog()) return;
    if (level > LOG_VERBOSITY)
        return;
    appendBuffer("[DEBUG-%d] ", level);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer("%s", buffer);
}

void SD_LOG::sendBufferLOG(bool flushAfterWrite) {
    if (!rxDebugAllowLog()) return;
    RxDebugTimer timing(RxDebugStage::Log);
    if (!sd_log)
        return;
    if (!filesys->exists(log_path)) {
        debugLogError("[SDLOG] Log file unavailable!\n");
        sd_log = false; reportRecordingFailure(RecordChannel::Log);
        end();
        return;
    }
    if (log.size() >= MAX_LOG_SIZE && !size_checked) {
        log.close();
        if (begin(log_directory) != 0) return;
    }
    const size_t written = CheckedRecordPrint(log, RecordChannel::Log).print(large_buffer);
    large_buffer.remove(0, written);
    // Stop accumulating new records after a short write; keep the suffix for
    // safeEnd to retry instead of exhausting RAM on a full/failing card.
    if (large_buffer.length()) { sd_log = false; reportRecordingFailure(RecordChannel::Log); }
    if (flushAfterWrite) {
        log.flush();
        if (!syncLogPath(log_path) || log.getWriteError()) reportRecordingFailure(RecordChannel::Log);
    }
}

void SD_LOG::appendBufferCSV(const char *format, ...) {
    if (!rxDebugAllowCsv()) return;
    if (!sd_csv)
        return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline_csv) {
        char *headers = new char[128];
#ifdef HAS_RTC
        sprintf(headers, "%.2f,", 0.00);
        large_buffer_csv += headers;
#else
        sprintf(headers, "null,");
        large_buffer_csv += headers;
#endif
        sprintf(headers, "%1.2f,%llu,", battery.readVoltage() * 2, millis64());
        large_buffer_csv += headers;
        if (getValidLocalTime(&timein)) {
            sprintf(headers, "%d-%02d-%02d,%02d:%02d:%02d,", timein.tm_year + 1900, timein.tm_mon + 1,
                    timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
            large_buffer_csv += headers;
        } else {
            sprintf(headers, "null,null,");
            large_buffer_csv += headers;
        }
        delete[] headers;
        is_startline_csv = false;
    }
    if (nullptr != strchr(buffer, '\n')) /* detect end of line in stream */
        is_startline_csv = true;
    large_buffer_csv += buffer;
}

void SD_LOG::sendBufferCSV(bool flushAfterWrite) {
    if (!rxDebugAllowCsv()) return;
    RxDebugTimer timing(RxDebugStage::Csv);
    if (!sd_csv) {
        return;
    }
    if (!filesys->exists(csv_path)) {
        debugLogError("[SDLOG] CSV file unavailable!\n");
        sd_csv = false; reportRecordingFailure(RecordChannel::Csv);
        end();
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE && !size_checked) {
        csv.close();
        if (beginCSV(csv_directory) != 0) return;
    }
    const size_t written = CheckedRecordPrint(csv, RecordChannel::Csv).print(large_buffer_csv);
    large_buffer_csv.remove(0, written);
    if (large_buffer_csv.length()) { sd_csv = false; reportRecordingFailure(RecordChannel::Csv); }
    if (flushAfterWrite) {
        csv.flush();
        if (!syncLogPath(csv_path) || csv.getWriteError()) reportRecordingFailure(RecordChannel::Csv);
    }
}

void SD_LOG::flushCSV() {
    if (!rxDebugAllowCsv()) return;
    RxDebugTimer timing(RxDebugStage::Csv);
    if (!sd_csv || !csv)
        return;
    csv.flush();
    if (!syncLogPath(csv_path) || csv.getWriteError()) reportRecordingFailure(RecordChannel::Csv);
}

File SD_LOG::logFile(char op) {
    log.close();
    switch (op) {
        case 'r': {
            log = filesys->open(log_path, "r");
            break;
        }
        case 'w': {
            log = filesys->open(log_path, "w");
            break;
        }
        case 'a': {
            log = filesys->open(log_path, "a");
            break;
        }
        default:
            log = filesys->open(log_path, "a");
    }
    return log;
}

void SD_LOG::printTel(unsigned int chars, ESPTelnet &tel) {
    log.close();
    uint32_t pos, left{};
    File log_r = filesys->open(log_path, "r");
    if (chars < log_r.size())
        pos = log_r.size() - chars;
    else {
        left = chars - log_r.size();
        pos = 0;
    }
    // Serial.println("LEFT = " + String(chars - log_r.size()));
    String line;
    if (!log_r.seek(pos))
        debugLogError("[SDLOG] seek failed!\n");
    while (log_r.available()) {
        line = log_r.readStringUntil('\n');
        if (line) {
            tel.print(line);
            tel.print("\n");
        } else
            tel.printf("[SDLOG] Read failed!\n");
    }
    if (left) {
        // Serial.printf("SEEK LAST LEFT %u\n", left);
        char last_file_name[32];
        sprintf(last_file_name, "LOG_%04d.txt", log_count - 2);
        String log_last_path = String(log_directory) + '/' + String(last_file_name);
    debugLogInfoPrintln(log_last_path);
        log_r = filesys->open(log_last_path, "r");
        if (left < log_r.size())
            pos = log_r.size() - left;
        else
            pos = 0;
        if (!log_r.seek(pos)) {
            debugLogError("[SDLOG] seek failed!\n");
        }
        while (log_r.available()) {
            line = log_r.readStringUntil('\n');
            if (line) {
                tel.print(line);
                tel.print("\n");
            } else
                tel.print("[SDLOG] Read failed!\n");
        }
    }
    log = filesys->open(log_path, "a");
}

void SD_LOG::disableSizeCheck() {
    if (log.size() >= MAX_LOG_SIZE) {
        log.close();
        if (begin(log_directory) != 0) return;
    }
    if (csv.size() >= MAX_CSV_SIZE) {
        csv.close();
        if (beginCSV(csv_directory) != 0) return;
    }
    size_checked = true;
}

void SD_LOG::enableSizeCheck() {
    size_checked = false;
}

void SD_LOG::reopen() {
    log.close();
    log = filesys->open(log_path, "a");
}

bool SD_LOG::status() const {
    return sd_log;
}

int SD_LOG::beginCD(const char *path) {
    char filename_cd[32];
    String cd_path;
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            debugLogError("[SDLOG] Failed to open coredump directory!\n");
            debugLogError("[SDLOG] Will not write coredump to SD card.\n");
            return -1;
        }
    }
    if (!cwd.isDirectory()) {
        debugLogError("[SDLOG] coredump directory error!\n");
        return -2;
    }
    int counter = 0;
    while (cwd.openNextFile()) {
        counter++;
    }
    sprintf(filename_cd, "COREDUMP_%04d.bin", counter);
    debugLogInfo("[SDLOG] %d coredump files, using %s \n", counter, filename_cd);

    cd_path = String(String(path) + '/' + filename_cd);
    cd = filesys->open(cd_path, "a", true);
    if (!cd) {
        debugLogError("[SDLOG] Failed to open coredump file!\n");
        debugLogError("[SDLOG] Will not write coredump to SD card.\n");
        return -3;
    }
    sd_cd = true;
    return 0;
}

void SD_LOG::appendCD(const uint8_t *data, size_t size) {
    if (!rxDebugAllowCsv()) return;
    if (!sd_cd)
        return;
    cd.write(data, size);
}

void SD_LOG::endCD() {
    if (!sd_cd)
        return;
    append("内核转储文件已保存至 %s\n", cd.name());
    cd.close();
    sd_cd = false;
}

bool SD_LOG::safeEnd() {
    if (!flushRideRecord()) { reportRecordingFailure(RecordChannel::Card); return false; }
    // Do not call sendBuffer*: their error path forcibly ends the card.
    // Preserve unwritten suffixes on short writes; never announce safe removal.
    if (sd_cd || (sd_log && !log) || (sd_csv && !csv) ||
        (log && !filesys->exists(log_path)) || (csv && !filesys->exists(csv_path))) { reportRecordingFailure(RecordChannel::Card); return false; }
    if (large_buffer.length()) {
        if (!log) { reportRecordingFailure(RecordChannel::Card); return false; }
        large_buffer.remove(0, CheckedRecordPrint(log, RecordChannel::Log).print(large_buffer));
        if (large_buffer.length()) { reportRecordingFailure(RecordChannel::Card); return false; }
    }
    if (large_buffer_csv.length()) {
        if (!csv) { reportRecordingFailure(RecordChannel::Card); return false; }
        large_buffer_csv.remove(0, CheckedRecordPrint(csv, RecordChannel::Csv).print(large_buffer_csv));
        if (large_buffer_csv.length()) { reportRecordingFailure(RecordChannel::Card); return false; }
    }
    if (log) log.flush();

    if (csv) csv.flush();

    if ((log && log.getWriteError()) || (csv && csv.getWriteError())) { reportRecordingFailure(RecordChannel::Card); return false; }
    if ((log && !syncLogPath(log_path)) || (csv && !syncLogPath(csv_path))) { reportRecordingFailure(RecordChannel::Card); return false; }
    end();
    recordingSafelyUnmounted();
    return true;
}

void SD_LOG::end() {
    closeRideRecord();
    log.close();
    csv.close();
    cd.close();
    SD.end();
    have_sd = false;
    sd_cd = false;
    sd_csv = false;
    sd_log = false;
    large_buffer = "";
    large_buffer_csv = "";
    is_startline = true;
    is_startline_csv = true;
}

bool SD_LOG::reopenSD() {
    return mountSdCard();
}

int SD_LOG::createIndex(File cwd, const String& index_path) {
    clearRecordingFailure(RecordChannel::Index);
    File index = filesys->open(index_path,FILE_WRITE);
    // Write Header
    CheckedRecordPrint(index, RecordChannel::Index).println("-------------------------------------------------");
    CheckedRecordPrint(index, RecordChannel::Index).println("ESP32 DEV MODULE INDEX FILE");
    CheckedRecordPrint(index, RecordChannel::Index).println("PROGRAM GENERATED, DO NOT EDIT.");
    CheckedRecordPrint(index, RecordChannel::Index).println("-------------------------------------------------");
    CheckedRecordPrint(index, RecordChannel::Index).printf("DIRECTORY: %s\n",cwd.path());
    index.flush();
    if (!index || index.getWriteError() || !syncLogPath(index_path)) reportRecordingFailure(RecordChannel::Index);
    index.close();
    // Count files
    int counter = -1;
    while (cwd.openNextFile()) {
        counter++;
    }
    // Write count
    index = filesys->open(index_path,FILE_APPEND);
    CheckedRecordPrint(index, RecordChannel::Index).printf("FILE COUNTER: %d\n",counter);
    index.flush();
    if (!index || index.getWriteError() || !syncLogPath(index_path)) reportRecordingFailure(RecordChannel::Index);
    index.close();
    return counter;
}

int SD_LOG::readIndex(const File& cwd) {
    String index_path = String(cwd.path()) + "/INDEX";
    int counter = 0;
    if (!filesys->exists(index_path)) {
        counter = createIndex(cwd, index_path);
    } else {
        File index = filesys->open(index_path,FILE_READ, false);
        while (index.available()) {
            String str = index.readStringUntil('\n');
            if (str.substring(0, 13) == "FILE COUNTER:") {
                counter = std::stoi(str.substring(14).c_str());
                break;
            }
        }
    }
    // Serial.printf("[D] Index file counter: %d\n",counter);
    return counter;
}

void SD_LOG::updateIndex(const String &path, int counter) {
    String index_path = path + "/INDEX";
    clearRecordingFailure(RecordChannel::Index);
    File index = filesys->open(index_path,FILE_WRITE);
    CheckedRecordPrint(index, RecordChannel::Index).println("-------------------------------------------------");
    CheckedRecordPrint(index, RecordChannel::Index).println("ESP32 DEV MODULE INDEX FILE");
    CheckedRecordPrint(index, RecordChannel::Index).println("PROGRAM GENERATED, DO NOT EDIT.");
    CheckedRecordPrint(index, RecordChannel::Index).println("-------------------------------------------------");
    CheckedRecordPrint(index, RecordChannel::Index).printf("DIRECTORY: %s\n",path.c_str());
    CheckedRecordPrint(index, RecordChannel::Index).printf("FILE COUNTER: %d\n",counter);
    index.flush();
    if (!index || index.getWriteError() || !syncLogPath(index_path)) reportRecordingFailure(RecordChannel::Index);
    index.close();
}
