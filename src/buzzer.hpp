#ifndef LBJ_BUZZER_HPP
#define LBJ_BUZZER_HPP

#include <Arduino.h>

class BuzzerController {
public:
    void begin(uint8_t pin, uint8_t idleLevel = LOW);
    void notifySignal(uint32_t now = millis());
    void update(uint32_t now = millis());
    void beep(uint8_t times, uint16_t onMs, uint16_t offMs);
    void hold(uint16_t durationMs);
    void stop();

private:
    static constexpr uint32_t SIGNAL_TIMEOUT_MS = 3000;
    static constexpr uint32_t SIGNAL_PERIOD_MS = 1000;
    static constexpr uint32_t SIGNAL_ON_MS = 500;

    uint8_t pin_ = 0;
    uint8_t idleLevel_ = LOW;
    uint8_t activeLevel_ = HIGH;
    bool initialized_ = false;
    volatile bool signalActive_ = false;
    volatile uint32_t lastSignalMs_ = 0;
};

extern BuzzerController buzzer;

#endif
