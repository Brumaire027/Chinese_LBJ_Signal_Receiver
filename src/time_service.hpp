#pragma once

#include <ctime>
#include <sys/time.h>

// RTC stores local CST-8 (UTC+8), matching existing firmware. No UTC migration.
constexpr const char *LOCAL_TIME_ZONE = "CST-8";

void initTimeService();
void processTimeSync();  // setup/loop task only: shares Wire with the OLED
void notifyNtpTime(const timeval *value);  // SNTP callback: no I/O or logging
bool getValidLocalTime(tm *value);
const char *systemTimeSource();
enum class ManualTimeResult { Saved, Invalid, RtcFailed, SystemFailed };
ManualTimeResult setManualLocalTime(const tm &value); // main loop only
void printRtcStatus();  // loop task only
