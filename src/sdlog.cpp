//
// Created by FLN1021 on 2023/9/5.
//

#include "sdlog.hpp"

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
            Serial.println("[SDLOG] Failed to open log directory!");
            Serial.println("[SDLOG] Will not write log to SD card.");
            sd_log = false;
            return;
        }
    }
    if (!cwd.isDirectory()) {
        Serial.println("[SDLOG] log directory error!");
        sd_log = false;
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

    if (last_log.size() <= MAX_LOG_SIZE && counter > 0) {
        sprintf(filename, "%s", last_log.name());
        log_count = counter;
    } else {
        sprintf(filename, "LOG_%04d.txt", counter);
        is_newfile = true;
        log_count = counter;
        // update index
        updateIndex(String(path),counter + 1);
    }
    Serial.printf("[SDLOG] %d log files, using %s \n", counter, filename);
    sd_log = true;
}

void SD_LOG::getFilenameCSV(const char *path) {
    File cwd = filesys->open(path, FILE_READ, false);
    if (!cwd) {
        filesys->mkdir(path);
        cwd = filesys->open(path, FILE_READ, false);
        if (!cwd) {
            Serial.println("[SDLOG] Failed to open csv directory!");
            Serial.println("[SDLOG] Will not write csv to SD card.");
            sd_csv = false;
            return;
        }
    }
    if (!cwd.isDirectory()) {
        Serial.println("[SDLOG] csv directory error!");
        sd_csv = false;
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
    Serial.printf("[SDLOG] %d csv files, using %s \n", counter, filename_csv);
    sd_csv = true;
}

int SD_LOG::begin(const char *path) {
    log_directory = path;
    getFilename(path);
    if (!sd_log)
        return -1;
    log_path = String(String(path) + '/' + filename);
    log = filesys->open(log_path, "a", true);
    if (!log) {
        Serial.println("[SDLOG] Failed to open log file!");
        Serial.println("[SDLOG] Will not write log to SD card.");
        sd_log = false;
        return -1;
    }
    writeHeader();
    sd_log = true;
    return 0;
}

int SD_LOG::beginCSV(const char *path) {
    csv_directory = path;
    getFilenameCSV(path);
    if (!sd_csv)
        return -1;
    csv_path = String(String(path) + '/' + filename_csv);
    csv = filesys->open(csv_path, "a", true);
    if (!csv) {
        Serial.println("[SDLOG] Failed to open csv file!");
        Serial.println("[SDLOG] Will not write csv to SD card.");
        sd_csv = false;
        return -1;
    }
    // csv.close();
    // csv = filesys->open(csv_path, "a", true);
    writeHeaderCSV();

    // Serial.printf("csvsize = %d\n",csv.size());
    // if (csv.size() == 0)
    //     writeHeaderCSV();
    sd_csv = true;
    return 0;
}

void SD_LOG::writeHeader() {
    log.println("-------------------------------------------------");
    if (is_newfile) {
        log.printf("ESP32 DEV MODULE LOG FILE %s \n", filename);
    }
    log.printf("BEGIN OF SYSTEM LOG, STARTUP TIME %llu MS.\n", millis64());
    if (getLocalTime(&timein, 0))
        log.printf("CURRENT TIME %d-%02d-%02d %02d:%02d:%02d\n",
                   timein.tm_year + 1900, timein.tm_mon + 1, timein.tm_mday, timein.tm_hour, timein.tm_min,
                   timein.tm_sec);
    log.println("-------------------------------------------------");
    log.flush();
}

void SD_LOG::writeHeaderCSV() { // TODO: needs more confirmation about title.
    csv.close();
    csv = filesys->open(csv_path, "a");
    // Serial.printf("csvfile %s, csv size %d, is_newfile_csv = %d\n",csv.path(),csv.size(),is_newfile_csv);
    if (is_newfile_csv && csv.size() < 200) {
        csv.printf("# ESP32 DEV MODULE CSV FILE %s \n", filename_csv);
        // csv.printf(
        //         "电压,系统时间,日期,时间,LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,原始数据,错误,错误率\n");
        csv.printf(
                "温度,电压,系统时间,日期,时间,LBJ时间,方向,级别,车次,速度,公里标,机车编号,线路,纬度,经度,HEX,RSSI,FER,PPM(FER),PPM(CURRENT),原始数据,错误,错误率\n");
        if (sd_log) {
            append("[SDLOG][D] Writing CSV Headers, is_newfile = %d, filesize = %d\n", is_newfile_csv, csv.size());
        }
        Serial.printf("[SDLOG][D] Writing CSV Headers, is_newfile = %d, filesize = %d\n", is_newfile_csv, csv.size());
    }
    // csv.flush();
    csv.close();
    csv = filesys->open(csv_path, "a");
    // Serial.printf("Write hdr end\n");
}

void SD_LOG::appendCSV(const char *format, ...) { // TODO: maybe implement item based csv append?
    if (!sd_csv) {
        return;
    }
    if (!filesys->exists(csv_path)) {
        Serial.println("[SDLOG] CSV file unavailable!");
        sd_csv = false;
        SD.end();
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE && !size_checked) {
        csv.close();
        beginCSV(csv_directory);
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline_csv) {
        csv.printf("%1.2f,", battery.readVoltage() * 2);
        csv.printf("%llu,", millis64());
        if (getLocalTime(&timein, 0)) {
            csv.printf("%d-%02d-%02d,%02d:%02d:%02d,", timein.tm_year + 1900, timein.tm_mon + 1,
                       timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
        } else {
            csv.printf("null,null,");
        }
        is_startline_csv = false;
    }
    if (nullptr != strchr(format, '\n')) /* detect end of line in stream */
        is_startline_csv = true;
    csv.print(buffer);
    csv.flush();
}

void SD_LOG::append(const char *format, ...) {
    // Serial.printf("[D] Using log %s\n", log_path.c_str());
    if (!sd_log) {
        // Serial.println("[D] sd_log false.");
        return;
    }
    if (!filesys->exists(log_path)) {
        Serial.printf("[SDLOG] Log file %s unavailable!\n", log_path.c_str());
        sd_log = false;
        SD.end();
        return;
    }
    if (log.size() >= MAX_LOG_SIZE && !size_checked) {
        log.close();
        begin(log_directory);
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline) {
        if (getLocalTime(&timein, 0)) {
            log.printf("%d-%02d-%02d %02d:%02d:%02d > ", timein.tm_year + 1900, timein.tm_mon + 1,
                       timein.tm_mday, timein.tm_hour, timein.tm_min, timein.tm_sec);
        } else {
            log.printf("[%6llu.%03llu] > ", millis64() / 1000, millis64() % 1000);
        }
        is_startline = false;
    }
    if (nullptr != strchr(format, '\n')) /* detect end of line in stream */
        is_startline = true;
    log.print(buffer);
    log.flush();
//    Serial.printf("[D] Using log %s \n", log_path.c_str());
}

void SD_LOG::append(int level, const char *format, ...) {
    if (level > LOG_VERBOSITY)
        return;
    appendBuffer("[DEBUG-%d] ", level);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer(buffer);
    sendBufferLOG();
}

void SD_LOG::appendBuffer(const char *format, ...) {
    if (!sd_log)
        return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline) {
        char *time_buffer = new char[128];
        if (getLocalTime(&timein, 1)) {
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
    if (nullptr != strchr(format, '\n')) /* detect end of line in stream */
        is_startline = true;
    large_buffer += buffer;
}

void SD_LOG::appendBuffer(int level, const char *format, ...) {
    if (level > LOG_VERBOSITY)
        return;
    appendBuffer("[DEBUG-%d] ", level);
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    appendBuffer(buffer);
}

void SD_LOG::sendBufferLOG() {
    if (!sd_log)
        return;
    if (!filesys->exists(log_path)) {
        Serial.println("[SDLOG] Log file unavailable!");
        sd_log = false;
        SD.end();
        return;
    }
    if (log.size() >= MAX_LOG_SIZE && !size_checked) {
        log.close();
        begin(log_directory);
    }
    if (log.size() >= MAX_LOG_SIZE && !size_checked) {
        log.close();
        begin(log_directory);
    }
    log.print(large_buffer);
    log.flush();
    large_buffer = "";
}

void SD_LOG::appendBufferCSV(const char *format, ...) {
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
        if (getLocalTime(&timein, 1)) {
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
    if (nullptr != strchr(format, '\n')) /* detect end of line in stream */
        is_startline_csv = true;
    large_buffer_csv += buffer;
}

void SD_LOG::sendBufferCSV() {
    if (!sd_csv) {
        return;
    }
    if (!filesys->exists(csv_path)) {
        Serial.println("[SDLOG] CSV file unavailable!");
        sd_csv = false;
        SD.end();
        return;
    }
    if (csv.size() >= MAX_CSV_SIZE && !size_checked) {
        csv.close();
        beginCSV(csv_directory);
    }
    csv.print(large_buffer_csv);
    csv.flush();
    large_buffer_csv = "";
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
        Serial.println("[SDLOG] seek failed!");
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
        Serial.println(log_last_path);
        log_r = filesys->open(log_last_path, "r");
        if (left < log_r.size())
            pos = log_r.size() - left;
        else
            pos = 0;
        if (!log_r.seek(pos)) {
            Serial.println("[SDLOG] seek failed!");
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
        begin(log_directory);
    }
    if (csv.size() >= MAX_CSV_SIZE) {
        csv.close();
        beginCSV(csv_directory);
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
            Serial.println("[SDLOG] Failed to open coredump directory!");
            Serial.println("[SDLOG] Will not write coredump to SD card.");
            return -1;
        }
    }
    if (!cwd.isDirectory()) {
        Serial.println("[SDLOG] coredump directory error!");
        return -2;
    }
    int counter = 0;
    while (cwd.openNextFile()) {
        counter++;
    }
    sprintf(filename_cd, "COREDUMP_%04d.bin", counter);
    Serial.printf("[SDLOG] %d coredump files, using %s \n", counter, filename_cd);

    cd_path = String(String(path) + '/' + filename_cd);
    cd = filesys->open(cd_path, "a", true);
    if (!cd) {
        Serial.println("[SDLOG] Failed to open coredump file!");
        Serial.println("[SDLOG] Will not write coredump to SD card.");
        return -3;
    }
    sd_cd = true;
    return 0;
}

void SD_LOG::appendCD(const uint8_t *data, size_t size) {
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

void SD_LOG::end() {
    if (!sd_log)
        return;
    SD.end();
    log.close();
    csv.close();
    sd_csv = false;
    sd_log = false;
}

void SD_LOG::reopenSD() {
    SD.begin(SDCARD_CS, SDSPI);
    // sd_csv = true;
    // sd_log = true;
}

int SD_LOG::createIndex(File cwd, const String& index_path) {
    File index = filesys->open(index_path,FILE_WRITE);
    // Write Header
    index.println("-------------------------------------------------");
    index.println("ESP32 DEV MODULE INDEX FILE");
    index.println("PROGRAM GENERATED, DO NOT EDIT.");
    index.println("-------------------------------------------------");
    index.printf("DIRECTORY: %s\n",cwd.path());
    index.close();
    // Count files
    int counter = -1;
    while (cwd.openNextFile()) {
        counter++;
    }
    // Write count
    index = filesys->open(index_path,FILE_APPEND);
    index.printf("FILE COUNTER: %d\n",counter);
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
    File index = filesys->open(index_path,FILE_WRITE);
    index.println("-------------------------------------------------");
    index.println("ESP32 DEV MODULE INDEX FILE");
    index.println("PROGRAM GENERATED, DO NOT EDIT.");
    index.println("-------------------------------------------------");
    index.printf("DIRECTORY: %s\n",path.c_str());
    index.printf("FILE COUNTER: %d\n",counter);
    index.close();
}