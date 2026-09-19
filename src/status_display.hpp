#ifndef LBJ_STATUS_DISPLAY_HPP
#define LBJ_STATUS_DISPLAY_HPP

#include "networks.hpp"

#ifdef HAS_DISPLAY
void refreshRecordingAlert();
void showInitComp();
void showWaitingScreen();
void updateInfo();
void showSTR(const String &str);
void showLBJ0(const struct lbj_data &l);
void showLBJ1(const struct lbj_data &l);
void showLBJ2(const struct lbj_data &l);
void requestDecodedDisplayUpdate(const struct lbj_data &data, uint64_t runtimeStartMs, float rssi = 0, bool arrival = true, const char *trainKey = nullptr);
void resetDecodedDisplay();
void processPendingDisplayUpdate();
void requestMainDisplayRefresh();
void setMenuDisplayActive(bool active);
bool isMenuDisplayActive();
void setHistoryDisplayActive(bool active);
bool isHistoryDisplayActive();
void showMenuScreen(const char *title, const char *const *lines, uint8_t lineCount, uint8_t selectedLine,
                    bool showSelection, bool wake = true);
void showPassiveScreen(const char *title, const char *const *lines, uint8_t lineCount);
void showHistoryRecordScreen(const char *title, const char *const *lines, uint8_t lineCount);
#else
inline void refreshRecordingAlert() {}
inline void showInitComp() {}
inline void showWaitingScreen() {}
inline void updateInfo() {}
inline void showSTR(const String &) {}
inline void showLBJ0(const struct lbj_data &) {}
inline void showLBJ1(const struct lbj_data &) {}
inline void showLBJ2(const struct lbj_data &) {}
inline void requestDecodedDisplayUpdate(const struct lbj_data &, uint64_t, float = 0, bool = true, const char * = nullptr) {}
inline void resetDecodedDisplay() {}
inline void processPendingDisplayUpdate() {}
inline void requestMainDisplayRefresh() {}
inline void setMenuDisplayActive(bool) {}
inline bool isMenuDisplayActive() { return false; }
inline void setHistoryDisplayActive(bool) {}
inline bool isHistoryDisplayActive() { return false; }
inline void showMenuScreen(const char *, const char *const *, uint8_t, uint8_t, bool, bool = true) {}
inline void showPassiveScreen(const char *, const char *const *, uint8_t) {}
inline void showHistoryRecordScreen(const char *, const char *const *, uint8_t) {}
#endif

#endif
