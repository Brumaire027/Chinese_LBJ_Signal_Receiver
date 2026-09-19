#include "time_service.hpp"

#include "boards.hpp"
#include "rtc_calendar.hpp"
#include <atomic>
#include <esp_sntp.h>

namespace {
enum class TimeSource { Unset, Rtc, Ntp, Manual };
std::atomic<TimeSource> source{TimeSource::Unset};
portMUX_TYPE ntpMux = portMUX_INITIALIZER_UNLOCKED;
bool ntpPending = false;
time_t ntpEpoch = 0;
bool rtcOnline = false;
bool rtcTimeValid = false;
bool rtcLostPower = false;
bool rtcStatusKnown = false;

bool validLocal(const tm &value) {
    return rtc_calendar::validDate(value.tm_year + 1900, value.tm_mon + 1,
                                  value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec);
}

#ifdef HAS_RTC
// RTClib 2.1.4 now()/lostPower() do not propagate I2C read failures.
// Read one checked snapshot so a disconnected chip cannot produce garbage time.
bool readRtc(tm &value) {
    rtcTimeValid = false;
    rtcStatusKnown = false;
    uint8_t regs[16] = {};
    Wire.beginTransmission(0x68);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(uint8_t(0x68), uint8_t(sizeof(regs))) != sizeof(regs)) {
        rtcOnline = false;
        return false;
    }
    for (auto &reg : regs) reg = Wire.read();
    rtcOnline = true;
    rtcStatusKnown = true;
    rtcLostPower = (regs[0x0f] & 0x80) != 0; // DS3231 lostPower()/OSF
    if (rtcLostPower || (regs[0x0e] & 0x80) || !rtc_calendar::decode(regs, value)) return false;
    // Reject dates outside this toolchain's time_t range as well.
    tm copy = value, roundtrip{};
    time_t epoch = mktime(&copy);
    if (epoch < 0 || !localtime_r(&epoch, &roundtrip) ||
        roundtrip.tm_year != value.tm_year || roundtrip.tm_mon != value.tm_mon ||
        roundtrip.tm_mday != value.tm_mday || roundtrip.tm_hour != value.tm_hour ||
        roundtrip.tm_min != value.tm_min || roundtrip.tm_sec != value.tm_sec) return false;
    rtcTimeValid = true;
    return true;
}

bool writeRtc(const tm &value) {
    // Only try a chip successfully initialized at boot; no hardware detection loop.
    if (!rtcOnline) return false;
    tm previous{};
    readRtc(previous);
    if (!rtcOnline) return false;
    // Enable the oscillator on backup power, preserving the other control bits.
    Wire.beginTransmission(0x68);
    Wire.write(0x0e);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(0x68), uint8_t(1)) != 1) {
        rtcOnline = rtcTimeValid = rtcStatusKnown = false;
        return false;
    }
    uint8_t control = Wire.read();
    Wire.beginTransmission(0x68);
    Wire.write(0x0e);
    Wire.write(control & 0x7f);
    if (Wire.endTransmission() != 0) {
        rtcOnline = rtcTimeValid = rtcStatusKnown = false;
        return false;
    }
    DateTime target(value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                    value.tm_hour, value.tm_min, value.tm_sec);
    rtc.adjust(target); // writes 24-hour local time and clears OSF
    tm verified{};
    if (!readRtc(verified)) return false;
    DateTime actual(verified.tm_year + 1900, verified.tm_mon + 1, verified.tm_mday,
                    verified.tm_hour, verified.tm_min, verified.tm_sec);
    const int64_t delta = int64_t(actual.unixtime()) - target.unixtime();
    rtcTimeValid = delta >= 0 && delta <= 2;
    return rtcTimeValid;
}
#endif
}  // namespace

const char *systemTimeSource() {
    switch (source.load()) {
        case TimeSource::Rtc: return "RTC";
        case TimeSource::Ntp: return "NTP";
        case TimeSource::Manual: return "MANUAL";
        default: return "UNSET";
    }
}

bool getValidLocalTime(tm *value) {
    if (!value) return false;
    *value = {};
    if (source.load() == TimeSource::Unset) return false;
    time_t epoch = time(nullptr);
    return localtime_r(&epoch, value) && validLocal(*value);
}

void printRtcStatus() {
    tm value{};
#ifdef HAS_RTC
    // Refresh only an initialized device. Never call RTC APIs on missing hardware.
    if (rtcOnline) readRtc(value);
#endif
    Serial.printf("[RTC] DS3231 online=%s lostPower=%s timeValid=%s; system source=%s\n",
                  rtcOnline ? "yes" : "no", rtcStatusKnown ? (rtcLostPower ? "yes" : "no") : "unknown",
                  rtcTimeValid ? "yes" : "no", systemTimeSource());
    if (rtcTimeValid) Serial.println(&value, "[RTC] local %Y-%m-%d %H:%M:%S (UTC+8)");
}

void initTimeService() {
    setenv("TZ", LOCAL_TIME_ZONE, 1);
    tzset();
#ifdef HAS_RTC
    rtcOnline = rtc.begin(&Wire);
    tm value{};
    if (rtcOnline && readRtc(value)) {
        timeval stamp{};
        stamp.tv_sec = mktime(&value);
        if (settimeofday(&stamp, nullptr) == 0) source.store(TimeSource::Rtc);
    }
#endif
    printRtcStatus();
    if (source.load() == TimeSource::Unset)
        Serial.println("[Time] UNSET: waiting for NTP; reception continues, no calendar timestamps.");
}

void notifyNtpTime(const timeval *value) {
    if (!value) return;
    portENTER_CRITICAL(&ntpMux);
    ntpEpoch = value->tv_sec;
    ntpPending = true;
    portEXIT_CRITICAL(&ntpMux);
}

void processTimeSync() {
    time_t received = 0;
    portENTER_CRITICAL(&ntpMux);
    const bool pending = ntpPending;
    if (pending) { received = ntpEpoch; ntpPending = false; }
    portEXIT_CRITICAL(&ntpMux);
    if (!pending) return;
    tm value{};
    if (!localtime_r(&received, &value) || !validLocal(value)) {
        source.store(TimeSource::Unset);
        Serial.println("[SNTP] Invalid timestamp; calendar time disabled.");
        return;
    }
    // SNTP already set the system clock; use current time, not a delayed callback stamp.
    time_t now = time(nullptr);
    if (!localtime_r(&now, &value) || !validLocal(value)) {
        source.store(TimeSource::Unset);
        return;
    }
    source.store(TimeSource::Ntp);
    Serial.println(&value, "[SNTP] system synchronized %Y-%m-%d %H:%M:%S (UTC+8)");
#ifdef HAS_RTC
    if (!rtcOnline) Serial.println("[RTC] Write skipped: DS3231 offline; system source=NTP.");
    else if (writeRtc(value)) Serial.println("[RTC] NTP write/readback OK, lostPower=no, timeValid=yes.");
    else Serial.println("[RTC] NTP write/readback FAILED; system source=NTP remains valid.");
#endif
}

ManualTimeResult setManualLocalTime(const tm &input) {
    if (!validLocal(input)) return ManualTimeResult::Invalid;
    tm value = input, checked{};
    value.tm_isdst = 0;
    const time_t epoch = mktime(&value);
    if (epoch < 0 || !localtime_r(&epoch, &checked) ||
        checked.tm_year != input.tm_year || checked.tm_mon != input.tm_mon ||
        checked.tm_mday != input.tm_mday || checked.tm_hour != input.tm_hour ||
        checked.tm_min != input.tm_min || checked.tm_sec != input.tm_sec)
        return ManualTimeResult::Invalid;
    // Stop the network client while committing both clocks; editing itself has
    // no side effects. Restart afterward so automatic network time stays enabled.
    const bool restartNtp = esp_sntp_enabled();
    if (restartNtp) esp_sntp_stop();
    portENTER_CRITICAL(&ntpMux);
    ntpPending = false;
    portEXIT_CRITICAL(&ntpMux);
    ManualTimeResult result = ManualTimeResult::RtcFailed;
#ifdef HAS_RTC
    if (writeRtc(value)) {
        timeval stamp{};
        stamp.tv_sec = epoch;
        if (settimeofday(&stamp, nullptr) == 0) {
            source.store(TimeSource::Manual);
            result = ManualTimeResult::Saved;
        } else result = ManualTimeResult::SystemFailed;
    }
#endif
    if (restartNtp) esp_sntp_init();
    return result;
}
