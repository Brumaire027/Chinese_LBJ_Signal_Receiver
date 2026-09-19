#pragma once

#include <cstdint>
#include <ctime>

namespace rtc_calendar {
constexpr int daysInMonth(int year, int month) {
    return month == 2 ? 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) :
           (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);
}

constexpr bool validDate(int year, int month, int day, int hour, int minute, int second) {
    // Reject the uninitialised 2000-era clock; DS3231/RTClib use 2000..2099.
    return year >= 2020 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 &&
           day <= daysInMonth(year, month) && hour >= 0 && hour <= 23 &&
           minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

constexpr int bcd(uint8_t value) {
    return ((value & 0x0f) > 9 || (value >> 4) > 9) ? -1 :
           (value >> 4) * 10 + (value & 0x0f);
}

constexpr int decodeHour(uint8_t value) {
    return (value & 0x80) ? -1 : (value & 0x40) ?
           ((bcd(value & 0x1f) < 1 || bcd(value & 0x1f) > 12) ? -1 :
            bcd(value & 0x1f) % 12 + ((value & 0x20) ? 12 : 0)) :
           ((bcd(value) > 23) ? -1 : bcd(value));
}

inline bool decode(const uint8_t *r, tm &value) {
    // Reserved bits and century are rejected, not silently masked into a date.
    if ((r[0] & 0x80) || (r[1] & 0x80) || (r[2] & 0x80) ||
        r[3] < 1 || r[3] > 7 || (r[4] & 0xc0) || (r[5] & 0xe0)) return false;
    int hour = decodeHour(r[2]);
    int year = bcd(r[6]), month = bcd(r[5]), day = bcd(r[4]);
    int minute = bcd(r[1]), second = bcd(r[0]);
    if (year < 0 || !validDate(2000 + year, month, day, hour, minute, second)) return false;
    value = {};
    value.tm_year = 100 + year;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = 0;
    return true;
}
}  // namespace rtc_calendar
