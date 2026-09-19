#pragma once
#include "buttons.hpp"

void initDisplayPower();
void applyDisplayBrightness();
void displayActivity(bool allowWake);
bool consumeDisplayButton(const ButtonEvent &event);
void updateDisplayPower();
