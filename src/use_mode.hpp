#pragma once
#include "networks.hpp"
#include "buttons.hpp"
void acceptModeReception(const data_bond &bond, const rx_info &info);
void updateUseMode();
bool isRideMode();
bool flushRideRecord();
void closeRideRecord();
void resetModeMenu();
void renderModeMenu();
bool modeMenuShouldExit();
bool handleModeButton(ButtonId key);
bool validTrainKey(const char *key);
void makeTrainKey(const char *train, const char *category, char *out, bool extended = false);
