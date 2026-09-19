#include "recording_health.hpp"
#include "boards.hpp"
#include "task_state.hpp"
#include <atomic>
namespace {
std::atomic<uint8_t> failures{0};
std::atomic<bool> checkRequested{true};
bool lowSpace = false;
bool previousMounted = false;
uint32_t lastCheck = 0;
}
void reportRecordingFailure(RecordChannel c) {
    failures.fetch_or(static_cast<uint8_t>(c));
    checkRequested = true;
}
void clearRecordingFailure(RecordChannel c) { failures.fetch_and(static_cast<uint8_t>(~static_cast<uint8_t>(c))); }
bool recordingFailed(RecordChannel c) { return (failures.load() & static_cast<uint8_t>(c)) != 0; }
const char *recordingAlertText() {
    if (lowSpace) return "空间不足";
    const uint8_t fault = failures.load();
    if (fault & (uint8_t(RecordChannel::Log) | uint8_t(RecordChannel::Csv) | uint8_t(RecordChannel::Index))) return "写入失败";
    if (fault & uint8_t(RecordChannel::Ride)) return "本车记录失败";
    return fault ? "存储卡异常" : nullptr;
}
bool recordingAlertVisible() { return recordingAlertText() && (millis() / 500U) % 2 == 0; }
void recordingSafelyUnmounted() {
    failures = 0; lowSpace = false; previousMounted = false;
}
void updateRecordingHealth() {
    if (fd_state.load() != TASK_INIT) return;
    if (!have_sd) { lowSpace = false; previousMounted = false; return; }
    const uint32_t now = millis();
    if (previousMounted && uint32_t(now - lastCheck) < (checkRequested.load() ? 5000U : 30000U)) return;
    previousMounted = true; lastCheck = now; checkRequested = false;
    uint64_t total = 0, free = 0;
    if (!SD.spaceInfo(total, free)) { failures.fetch_or(static_cast<uint8_t>(RecordChannel::Probe)); return; }
    clearRecordingFailure(RecordChannel::Probe);
    lowSpace = free < 1024ULL * 1024ULL; // Warn early; never stop or delete records here.
}
