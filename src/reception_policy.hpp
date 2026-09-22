#pragma once
namespace reception_policy {
// No identity or elapsed-time gate in stationary mode.
constexpr bool trainReport(int type) { return type == 0 || type == 1; }
constexpr bool displayStation(int type, bool ride, bool haveTrain) {
    return !ride && (trainReport(type) || (type == 2 && !haveTrain));
}
constexpr bool alertTrain(int type, bool ride, bool matches, bool otherAlerts) {
    return trainReport(type) && (!ride || matches || otherAlerts);
}
constexpr bool attachExtension(bool previousBasic, int snapshotType, int previousDirection, int direction) {
    return previousBasic && snapshotType == 0 && previousDirection == direction;
}
}
