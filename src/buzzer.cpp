#include "buzzer.hpp"

BuzzerController buzzer;

void BuzzerController::begin(uint8_t pin, uint8_t idleLevel) {
    pin_ = pin;
    idleLevel_ = idleLevel;
    activeLevel_ = (idleLevel_ == LOW) ? HIGH : LOW;
    initialized_ = true;

    pinMode(pin_, OUTPUT);
    stop();
}

void BuzzerController::notifySignal(uint32_t now) {
    if (!initialized_) {
        return;
    }

    lastSignalMs_ = now;
    signalActive_ = true;
}

void BuzzerController::update(uint32_t now) {
    if (!initialized_) {
        return;
    }

    if (signalActive_ && now - lastSignalMs_ > SIGNAL_TIMEOUT_MS) {
        signalActive_ = false;
    }

    if (!signalActive_) {
        digitalWrite(pin_, idleLevel_);
        return;
    }

    const uint32_t rhythm = now % SIGNAL_PERIOD_MS;
    digitalWrite(pin_, rhythm < SIGNAL_ON_MS ? activeLevel_ : idleLevel_);
}

void BuzzerController::beep(uint8_t times, uint16_t onMs, uint16_t offMs) {
    if (!initialized_) {
        return;
    }

    for (uint8_t i = 0; i < times; ++i) {
        digitalWrite(pin_, activeLevel_);
        delay(onMs);
        digitalWrite(pin_, idleLevel_);
        if (i + 1 < times) {
            delay(offMs);
        }
    }
}

void BuzzerController::hold(uint16_t durationMs) {
    if (!initialized_) {
        return;
    }

    digitalWrite(pin_, activeLevel_);
    delay(durationMs);
    digitalWrite(pin_, idleLevel_);
}

void BuzzerController::stop() {
    if (!initialized_) {
        return;
    }

    signalActive_ = false;
    digitalWrite(pin_, idleLevel_);
}
