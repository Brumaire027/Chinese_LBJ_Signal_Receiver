#include "receiver_control.hpp"

#include "debug_log.hpp"
#include "networks.hpp"

extern SX1276 radio;
extern PagerClient pager;
extern float rssi_cache;
extern float fers[32];
extern float freq_last;
extern float car_fer_last;
extern struct rx_info rxInfo;
extern uint64_t car_timer;
extern uint32_t car_count;

float actualFreq(float bias) {
    actual_frequency = (float) ((TARGET_FREQ * bias) / 1e6 + TARGET_FREQ);
    return actual_frequency;
}

void handleSync() {
    if (pager.gotSyncState()) {
        if (rxInfo.cnt < 5 && (rxInfo.timer == 0 || esp_timer_get_time() - rxInfo.timer < 11000)) {
            float rssi = radio.getRSSI(false, true);
            rxInfo.timer = esp_timer_get_time();
            rssi_cache += rssi;
            rxInfo.cnt++;
            debugLogVerbose("[D] RXI %.2f\n", rssi_cache / (float) rxInfo.cnt);
        }
        if (rxInfo.fer == 0)
            rxInfo.fer = radio.getFrequencyError();
    }
}

void revertFrequency() {
    if (actual_frequency != freq_last) {
        actual_frequency = freq_last;
        int state = radio.setFrequency(actual_frequency);
        if (state != RADIOLIB_ERR_NONE) {
            debugLogVerbose("[D] Revert freq failed %d\n", state);
        } else {
            debugLogVerbose("[D] Revert to last freq %f MHz, ppm %.2f\n", actual_frequency, getBias(actual_frequency));
        }
    }
}

void handleCarrier() {
    if (pager.gotCarrierState() && !pager.gotPreambleState() && !pager.gotSyncState() && freq_correction &&
        prb_timer == 0) {
        if (car_count == 0)
            car_timer = millis64();
        ++car_count;
        if (car_count < 64) {
            float fei = radio.getFrequencyError();
            if (abs(fei) > 1000.0 && car_count != 1 && abs(fei - car_fer_last) < 500) {
                auto target_freq = (float) (actual_frequency + fei * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    debugLogVerbose("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    debugLogVerboseSd("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    debugLogVerbose("[D][C] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency, fei,
                                    getBias(actual_frequency));
                }
            }
            car_fer_last = fei;
        }
    }
}

void handlePreamble() {
    if (pager.gotPreambleState() && !pager.gotSyncState() && freq_correction) {
        if (prb_count == 0)
            prb_timer = millis64();
        ++prb_count;
        if (prb_count < 32) {
            fers[prb_count - 1] = radio.getFrequencyError();
            if (abs(fers[prb_count - 1]) > 1000.0 && prb_count != 1 &&
                abs(fers[prb_count - 1] - fers[prb_count - 2]) < 500) {
                auto target_freq = (float) (actual_frequency + fers[prb_count - 1] * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    debugLogVerbose("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    debugLogVerboseSd("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    debugLogVerbose("[D][P] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency,
                                    fers[prb_count - 1], getBias(actual_frequency));
                }
            }
        }
    }
}
