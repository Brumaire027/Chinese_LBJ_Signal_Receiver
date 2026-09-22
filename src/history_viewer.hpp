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
// Loading advances in bounded slices after receiver processing in the main loop.
void requestHistoryLoad();
bool historyLoadPending();
void cancelHistoryLoad();
void processHistoryLoad();
// Returns identifiers from the completed snapshot; never performs SD reads.
uint8_t loadHistoryTrainChoices(char (*keys)[9], uint8_t capacity);

#endif
