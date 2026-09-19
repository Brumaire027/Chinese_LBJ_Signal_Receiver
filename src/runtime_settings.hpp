#ifndef LBJ_RUNTIME_SETTINGS_HPP
#define LBJ_RUNTIME_SETTINGS_HPP
#include "display_settings.hpp"

struct RuntimeSettings {
    bool trainArrivalLedEnabled = true;
    bool trainArrivalBuzzerEnabled = true;
    bool telnetEnabled = true;
    bool otherTrainAlerts = false;
    bool lowBatteryAlertEnabled = false;
    DisplaySettings display;
};

RuntimeSettings &runtimeSettings();
void loadRuntimeSettings();
bool saveRuntimeSettings();
bool saveDisplaySettings(const DisplaySettings &value);
void resetRuntimeSettingsToDefaults();
bool isTrainArrivalLedEnabled();
bool isTrainArrivalBuzzerEnabled();
bool isRemoteConnectionEnabled();
bool toggleRemoteConnectionEnabled();
bool toggleOtherTrainAlerts();
bool toggleLowBatteryAlert();
void toggleTrainArrivalLedEnabled();
void toggleTrainArrivalBuzzerEnabled();

#endif
