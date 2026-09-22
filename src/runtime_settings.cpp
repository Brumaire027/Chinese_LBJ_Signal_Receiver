#include "runtime_settings.hpp"
#include "indicator_led.hpp"
#include "buzzer.hpp"

#include <Preferences.h>

namespace {

constexpr const char *SETTINGS_NAMESPACE = "lbj_runtime";
constexpr const char *KEY_TRAIN_LED = "train_led";
constexpr const char *KEY_TRAIN_BUZZER = "train_buzz";

RuntimeSettings settings;

}  // namespace

RuntimeSettings &runtimeSettings() {
    return settings;
}

void loadRuntimeSettings() {
    resetRuntimeSettingsToDefaults();

    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, true)) {
        return;
    }

    settings.trainArrivalLedEnabled = preferences.getBool(KEY_TRAIN_LED, settings.trainArrivalLedEnabled);
    settings.trainArrivalBuzzerEnabled = preferences.getBool(KEY_TRAIN_BUZZER, settings.trainArrivalBuzzerEnabled);
    settings.telnetEnabled = preferences.getBool("telnet", settings.telnetEnabled);
    settings.otherTrainAlerts = preferences.getBool("other_train", false);
    settings.lowBatteryAlertEnabled = preferences.getBool("low_battery", false);
    const uint32_t display = preferences.getUInt("display", encodeDisplaySettings(1, 2, true));
    if (validDisplaySettings(display)) {
        settings.display.brightness = display & 0xff;
        settings.display.timeout = (display >> 8) & 0xff;
        settings.display.wakeOnArrival = (display >> 16) != 0;
    }
    preferences.end();
}

bool saveDisplaySettings(const DisplaySettings &value) {
    const uint32_t packed = encodeDisplaySettings(value.brightness, value.timeout, value.wakeOnArrival);
    if (!validDisplaySettings(packed)) return false;
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, false)) return false;
    // One NVS value keeps the three display options together, without touching other settings.
    const bool saved = preferences.putUInt("display", packed) == sizeof(uint32_t);
    preferences.end();
    if (saved) settings.display = value;
    return saved;
}

bool saveRuntimeSettings() {
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, false)) {
        return false;
    }

    const bool savedLed = preferences.putBool(KEY_TRAIN_LED, settings.trainArrivalLedEnabled) > 0;
    const bool savedBuzzer = preferences.putBool(KEY_TRAIN_BUZZER, settings.trainArrivalBuzzerEnabled) > 0;
    const bool savedTelnet = preferences.putBool("telnet", settings.telnetEnabled) > 0;
    preferences.end();
    return savedLed && savedBuzzer && savedTelnet;
}

void resetRuntimeSettingsToDefaults() {
    settings = RuntimeSettings{};
}

bool isTrainArrivalLedEnabled() {
    return settings.trainArrivalLedEnabled;
}

bool isTrainArrivalBuzzerEnabled() {
    return settings.trainArrivalBuzzerEnabled;
}

void toggleTrainArrivalLedEnabled() {
    settings.trainArrivalLedEnabled = !settings.trainArrivalLedEnabled;
    if (!settings.trainArrivalLedEnabled) stopIndicatorLed();
    saveRuntimeSettings();
}

void toggleTrainArrivalBuzzerEnabled() {
    settings.trainArrivalBuzzerEnabled = !settings.trainArrivalBuzzerEnabled;
    if (!settings.trainArrivalBuzzerEnabled) buzzer.stop();
    saveRuntimeSettings();
}

bool isRemoteConnectionEnabled() { return settings.telnetEnabled; }
bool toggleLowBatteryAlert() {
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, false)) return false;
    const bool next = !settings.lowBatteryAlertEnabled;
    const bool saved = preferences.putBool("low_battery", next) > 0;
    preferences.end();
    if (saved) settings.lowBatteryAlertEnabled = next;
    return saved;
}
bool toggleOtherTrainAlerts() {
    Preferences preferences;
    if (!preferences.begin(SETTINGS_NAMESPACE, false)) return false;
    const bool next = !settings.otherTrainAlerts;
    const bool saved = preferences.putBool("other_train", next) > 0;
    preferences.end();
    if (saved) settings.otherTrainAlerts = next;
    return saved;
}
bool toggleRemoteConnectionEnabled() {
    settings.telnetEnabled = !settings.telnetEnabled;
    if (saveRuntimeSettings()) return true;
    settings.telnetEnabled = !settings.telnetEnabled;
    return false;
}
