#ifndef LBJ_MENU_SYSTEM_HPP
#define LBJ_MENU_SYSTEM_HPP

#include "buttons.hpp"

void initMenuSystem();
void handleMenuButtonEvent(const ButtonEvent &event);
void updateMenuSystem();
bool isMenuSystemActive();

#endif
