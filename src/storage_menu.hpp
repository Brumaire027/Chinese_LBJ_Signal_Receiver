#pragma once
#include "buttons.hpp"

// These APIs are main-loop only, as are creation of formatter tasks and SD readers.
const char *unmountStorage(bool *success = nullptr);
const char *remountStorage(bool *success = nullptr);
void resetStorageMenu();
void renderStorageMenu();
bool handleStorageButton(ButtonId key); // true: return to main menu

bool storageMenuShouldExit();
