#include "indicator_led.hpp"

#include <Arduino.h>

#include "runtime_settings.hpp"
#include "utilities.h"

namespace {

constexpr uint8_t TRAIN_ARRIVAL_FLASHES = 3;
constexpr uint32_t LED_ON_MS = 120;
constexpr uint32_t LED_OFF_MS = 120;

bool indicatorActive = false;
bool indicatorOn = false;
uint8_t flashesRemaining = 0;
uint32_t phaseStartedMs = 0;
volatile bool trainArrivalPending = false;

void writeIndicator(bool on) {
    digitalWrite(BOARD_LED, on ? LED_ON : LED_OFF);
    indicatorOn = on;
}

void startTrainArrivalFlash() {
    indicatorActive = true;
    flashesRemaining = TRAIN_ARRIVAL_FLASHES;
    phaseStartedMs = millis();
    writeIndicator(true);
}

}  // namespace

void initIndicatorLed() {
    pinMode(BOARD_LED, OUTPUT);
    indicatorActive = false;
    flashesRemaining = 0;
    trainArrivalPending = false;
    phaseStartedMs = millis();
    writeIndicator(false);
}

void triggerTrainArrivalLed() {
    if (!isTrainArrivalLedEnabled()) {
        return;
    }

    trainArrivalPending = true;
}

void updateIndicatorLed() {
    if (trainArrivalPending) {
        trainArrivalPending = false;
        startTrainArrivalFlash();
    }

    if (!indicatorActive) {
        return;
    }

    const uint32_t now = millis();
    const uint32_t interval = indicatorOn ? LED_ON_MS : LED_OFF_MS;
    if (static_cast<uint32_t>(now - phaseStartedMs) < interval) {
        return;
    }

    phaseStartedMs = now;
    if (indicatorOn) {
        writeIndicator(false);
        if (flashesRemaining > 0) {
            flashesRemaining--;
        }
        if (flashesRemaining == 0) {
            indicatorActive = false;
        }
    } else {
        writeIndicator(true);
    }
}

void stopIndicatorLed() {
    indicatorActive = false;
    flashesRemaining = 0;
    trainArrivalPending = false;
    writeIndicator(false);
}
