#pragma once
#include "buttons.hpp"
void initNetworkSettings();
void processNetworkSettings();
void resetNetworkMenu();
void renderNetworkMenu();
bool handleNetworkButton(ButtonId key);

bool networkMenuShouldExit();
