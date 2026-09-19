#ifndef LBJ_HISTORY_VIEWER_HPP
#define LBJ_HISTORY_VIEWER_HPP

#include "buttons.hpp"

enum class HistoryEntryDirection : uint8_t {
    Previous = 0,
    Next,
};

void initHistoryViewer();
bool isHistoryViewerActive();
void enterHistoryViewer(HistoryEntryDirection direction);
void exitHistoryViewer();
void handleHistoryButtonEvent(const ButtonEvent &event);
void updateHistoryViewer();
// Caller uses the idle gate; returns unique identifiers from the latest 20 CSV rows.
uint8_t loadHistoryTrainChoices(char (*keys)[9], uint8_t capacity);

#endif
