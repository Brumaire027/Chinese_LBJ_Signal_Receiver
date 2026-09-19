#pragma once
#include "rtc_calendar.hpp"
namespace time_edit {
constexpr int wrap(int value, int low, int high) {
    return value < low ? high : value > high ? low : value;
}
constexpr int clampDay(int year, int month, int day) {
    return day > rtc_calendar::daysInMonth(year, month) ? rtc_calendar::daysInMonth(year, month) : day;
}
constexpr int maxYear() { return sizeof(time_t) <= 4 ? 2037 : 2099; }
}
