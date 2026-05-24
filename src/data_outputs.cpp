#include "data_outputs.hpp"
#include "debug_log.hpp"
#include "runtime_config.hpp"

enum class DecodedStorageFlushPolicy {
    FlushAfterEachWrite,
    BatchCsvFlush,
};

#if ENABLE_BATCHED_CSV_FLUSH
static uint32_t pendingDecodedCsvWrites = 0;
static uint64_t lastDecodedCsvWriteMs = 0;
#endif

#if ENABLE_DECODED_TELNET_RATE_LIMIT
static bool hasDecodedTelnetOutput = false;
static uint32_t lastDecodedTelnetOutputMs = 0;
#endif

static bool shouldFlushCsvAfterWrite(DecodedStorageFlushPolicy policy) {
    return policy == DecodedStorageFlushPolicy::FlushAfterEachWrite;
}

static bool isDecodedTelnetRateLimitSatisfied() {
#if ENABLE_DECODED_TELNET_RATE_LIMIT
    if (!hasDecodedTelnetOutput)
        return true;

    const uint32_t nowMs = millis();
    return static_cast<uint32_t>(nowMs - lastDecodedTelnetOutputMs) >= DECODED_TELNET_MIN_INTERVAL_MS;
#else
    return true;
#endif
}

static void noteDecodedTelnetOutputWritten() {
#if ENABLE_DECODED_TELNET_RATE_LIMIT
    lastDecodedTelnetOutputMs = millis();
    hasDecodedTelnetOutput = true;
#endif
}

static bool shouldWriteDecodedSerialOutput(const struct DecodedOutputContext &context) {
    (void) context;
    return ENABLE_DECODED_SERIAL_OUTPUT;
}

static bool shouldWriteDecodedNetworkOutput(const struct DecodedOutputContext &context) {
    (void) context;
    if (!ENABLE_DECODED_TELNET_OUTPUT)
        return false;

#if SKIP_DECODED_TELNET_WHEN_OFFLINE
    if (!isTelnetOutputAvailable())
        return false;
#else
    // Default keeps the old call path; telPrintf() still guards offline Telnet writes.
    const bool telnetAvailable = isTelnetOutputAvailable();
    (void) telnetAvailable;
#endif

    return isDecodedTelnetRateLimitSatisfied();
}

void flushDecodedCsvOutputIfDue() {
#if ENABLE_BATCHED_CSV_FLUSH
    if (pendingDecodedCsvWrites == 0 || lastDecodedCsvWriteMs == 0)
        return;

    const uint64_t nowMs = millis64();
    if (pendingDecodedCsvWrites >= CSV_FLUSH_EVERY_N_WRITES ||
        nowMs - lastDecodedCsvWriteMs >= CSV_FLUSH_INTERVAL_MS) {
        sd1.flushCSV();
        pendingDecodedCsvWrites = 0;
        lastDecodedCsvWriteMs = 0;
    }
#endif
}

static void noteDecodedCsvWriteForBatchFlush(DecodedStorageFlushPolicy policy) {
#if ENABLE_BATCHED_CSV_FLUSH
    if (policy != DecodedStorageFlushPolicy::BatchCsvFlush)
        return;

    if (pendingDecodedCsvWrites < CSV_FLUSH_EVERY_N_WRITES)
        ++pendingDecodedCsvWrites;

    lastDecodedCsvWriteMs = millis64();
    flushDecodedCsvOutputIfDue();
#else
    (void) policy;
#endif
}

static void writeDecodedSerialOutput(const struct DecodedOutputContext &context) {
    printDataSerial(context.bond.pocsagData, context.bond.lbjData, context.info);
}

static void measureDecodedSerialOutput(const struct DecodedOutputContext &context) {
    if (!shouldWriteDecodedSerialOutput(context))
        return;

    writeDecodedSerialOutput(context);
    debugLogStageTimingSd(2, "串口输出完成，用时[%llu]\n", millis64() - context.runtimeStartMs);
    debugLogStageTiming("SPRINT complete.[%llu]", millis64() - context.runtimeStartMs);
}

static void writeDecodedLogOutput(const struct DecodedOutputContext &context, bool flushAfterWrite) {
    appendDataLog(context.bond.pocsagData, context.bond.lbjData, context.info, flushAfterWrite);
    debugLogStageTiming("sdprint complete.[%llu]", millis64() - context.runtimeStartMs);
}

static void writeDecodedCsvOutput(const struct DecodedOutputContext &context, bool flushAfterWrite) {
    appendDataCSV(context.bond.pocsagData, context.bond.lbjData, context.info, flushAfterWrite);
    noteDecodedCsvWriteForBatchFlush(flushAfterWrite ? DecodedStorageFlushPolicy::FlushAfterEachWrite
                                                     : DecodedStorageFlushPolicy::BatchCsvFlush);
    debugLogStageTiming("csvprint complete.[%llu]", millis64() - context.runtimeStartMs);
}

static void writeDecodedStorageOutput(const struct DecodedOutputContext &context) {
#if ENABLE_BATCHED_CSV_FLUSH
    constexpr DecodedStorageFlushPolicy csvFlushPolicy = DecodedStorageFlushPolicy::BatchCsvFlush;
#else
    constexpr DecodedStorageFlushPolicy csvFlushPolicy = DecodedStorageFlushPolicy::FlushAfterEachWrite;
#endif
    const bool flushCsvAfterWrite = shouldFlushCsvAfterWrite(csvFlushPolicy);

    writeDecodedLogOutput(context, true);
    writeDecodedCsvOutput(context, flushCsvAfterWrite);
}

static void writeDecodedNetworkOutput(const struct DecodedOutputContext &context) {
    printDataTelnet(context.bond.pocsagData, context.bond.lbjData, context.info);
}

static void measureDecodedNetworkOutput(const struct DecodedOutputContext &context) {
    if (!shouldWriteDecodedNetworkOutput(context))
        return;

    writeDecodedNetworkOutput(context);
    noteDecodedTelnetOutputWritten();
    debugLogStageTiming("telprint complete.[%llu]", millis64() - context.runtimeStartMs);
}

void dispatchDecodedOutputs(struct data_bond &bond, const struct rx_info &info, uint64_t runtimeStartMs) {
    const DecodedOutputContext context{bond, info, runtimeStartMs};

    measureDecodedSerialOutput(context);
    writeDecodedStorageOutput(context);
    measureDecodedNetworkOutput(context);
}
