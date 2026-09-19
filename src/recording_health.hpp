#pragma once
#include <Arduino.h>
#include <FS.h>
enum class RecordChannel : uint8_t { Log = 1, Csv = 2, Ride = 4, Index = 8, Card = 16, Probe = 32 };
void reportRecordingFailure(RecordChannel channel);
void clearRecordingFailure(RecordChannel channel);
bool recordingFailed(RecordChannel channel);
const char *recordingAlertText();
bool recordingAlertVisible();
void updateRecordingHealth(); // Main loop only, idle gate inside.
void recordingSafelyUnmounted();
// Print adapter detects short writes, including formatted prefixes and headers.
class CheckedRecordPrint : public Print {
    File &file;
    RecordChannel channel;
public:
    CheckedRecordPrint(File &f, RecordChannel c) : file(f), channel(c) {}
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *data, size_t size) override {
        const size_t n = file.write(data, size);
        if (n != size || file.getWriteError()) { setWriteError(); reportRecordingFailure(channel); }
        return n;
    }
};
