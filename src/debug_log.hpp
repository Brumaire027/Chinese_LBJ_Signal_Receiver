#ifndef LBJ_DEBUG_LOG_HPP
#define LBJ_DEBUG_LOG_HPP

#include "runtime_config.hpp"
#include "sdlog.hpp"

extern SD_LOG sd1;

#if DEBUG_LOG_LEVEL >= DEBUG_LOG_ERROR
#define debugLogError(...) Serial.printf(__VA_ARGS__)
#define debugLogErrorPrint(...) Serial.print(__VA_ARGS__)
#define debugLogErrorPrintln(...) Serial.println(__VA_ARGS__)
#else
#define debugLogError(...) do {} while (0)
#define debugLogErrorPrint(...) do {} while (0)
#define debugLogErrorPrintln(...) do {} while (0)
#endif

#if DEBUG_LOG_LEVEL >= DEBUG_LOG_INFO
#define debugLogInfo(...) Serial.printf(__VA_ARGS__)
#define debugLogInfoPrint(...) Serial.print(__VA_ARGS__)
#define debugLogInfoPrintln(...) Serial.println(__VA_ARGS__)
#else
#define debugLogInfo(...) do {} while (0)
#define debugLogInfoPrint(...) do {} while (0)
#define debugLogInfoPrintln(...) do {} while (0)
#endif

#if DEBUG_LOG_LEVEL >= DEBUG_LOG_VERBOSE
#define debugLogVerbose(...) Serial.printf(__VA_ARGS__)
#define debugLogVerbosePrint(...) Serial.print(__VA_ARGS__)
#define debugLogVerbosePrintln(...) Serial.println(__VA_ARGS__)
#define debugLogVerboseSd(...) sd1.append(__VA_ARGS__)
#define debugLogVerboseSdLevel(level, ...) sd1.append(level, __VA_ARGS__)
#else
#define debugLogVerbose(...) do {} while (0)
#define debugLogVerbosePrint(...) do {} while (0)
#define debugLogVerbosePrintln(...) do {} while (0)
#define debugLogVerboseSd(...) do {} while (0)
#define debugLogVerboseSdLevel(level, ...) do {} while (0)
#endif

#if ENABLE_STAGE_TIMING_LOGS
#define debugLogStageTiming(...) Serial.printf(__VA_ARGS__)
#define debugLogStageTimingSd(level, ...) sd1.append(level, __VA_ARGS__)
#else
#define debugLogStageTiming(...) do {} while (0)
#define debugLogStageTimingSd(level, ...) do {} while (0)
#endif

#endif
