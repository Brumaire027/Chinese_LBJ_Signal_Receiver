#ifndef LBJ_STATUS_DISPLAY_HPP
#define LBJ_STATUS_DISPLAY_HPP

#include "networks.hpp"

#ifdef HAS_DISPLAY
void showInitComp();
void updateInfo();
void showSTR(const String &str);
void showLBJ0(const struct lbj_data &l);
void showLBJ1(const struct lbj_data &l);
void showLBJ2(const struct lbj_data &l);
void requestDecodedDisplayUpdate(const struct lbj_data &data, uint64_t runtimeStartMs);
void processPendingDisplayUpdate();
#else
inline void showInitComp() {}
inline void updateInfo() {}
inline void showSTR(const String &) {}
inline void showLBJ0(const struct lbj_data &) {}
inline void showLBJ1(const struct lbj_data &) {}
inline void showLBJ2(const struct lbj_data &) {}
inline void requestDecodedDisplayUpdate(const struct lbj_data &, uint64_t) {}
inline void processPendingDisplayUpdate() {}
#endif

#endif
