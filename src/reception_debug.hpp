#pragma once
#include "reception_debug_policy.hpp"
#include "buttons.hpp"
#include <stdint.h>
struct data_bond;
enum class RxDebugStage : uint8_t { Raw, Parse, Log, Csv, Ride, Batch, Count };
bool rxDebugEnabled();
bool rxDebugAllowLog();
bool rxDebugAllowCsv();
void updateReceptionDebug();
void noteRxDebugBatch(int16_t status, const data_bond &bond);
void noteRxDebugDecoded(int type);
// Scoped timers include flush/fsync and early returns, but never write diagnostics to SD.
class RxDebugTimer {
    RxDebugStage stage; uint32_t started; bool active;
public:
    explicit RxDebugTimer(RxDebugStage value);
    ~RxDebugTimer();
};
void openReceptionDebug();
void closeReceptionDebug();
bool handleReceptionDebugButton(ButtonId key);
void renderReceptionDebug();
