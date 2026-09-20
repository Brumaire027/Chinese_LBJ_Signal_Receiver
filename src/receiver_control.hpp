#ifndef LBJ_RECEIVER_CONTROL_HPP
#define LBJ_RECEIVER_CONTROL_HPP

#include <stdint.h>

struct data_bond;
struct ReceiverDiagnostics {
    uint32_t received = 0;
    uint32_t decoded = 0;
    uint64_t lastReceivedMs = 0;
    uint64_t sampledMs = 0;
    float rssi = 0;
    bool haveRssi = false;
};

// All diagnostics access is on the main loop, including completed decode results.
const ReceiverDiagnostics &receiverDiagnostics();
void noteReceiverBatch();
void noteReceiverDecoded(const data_bond &bond);
void sampleReceiverDiagnostics();

float actualFreq(float bias);
void handleSync();
void handleCarrier();
void handlePreamble();
void revertFrequency();

#endif
