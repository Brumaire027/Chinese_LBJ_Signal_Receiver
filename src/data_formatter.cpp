#include "data_formatter.hpp"
#include "debug_log.hpp"

void collectRawPagerData(struct data_bond &bond, uint64_t runtimeStartMs) {
    for (auto &i: bond.pocsagData) {
        if (i.is_empty)
            continue;
        debugLogVerbose("[D-pDATA] %d/%d: %s\n", i.addr, i.func, i.str.c_str());
        debugLogVerboseSdLevel(2, "[D-pDATA] %d/%d: %s\n", i.addr, i.func, i.str.c_str());
        bond.str += "  ";
        bond.str += i.str;
    }

    debugLogStageTimingSd(2, "原始数据输出完成，用时[%llu]\n", millis64() - runtimeStartMs);
    debugLogStageTiming("decode complete.[%llu]", millis64() - runtimeStartMs);
}

void decodeLbjData(struct data_bond &bond, uint64_t runtimeStartMs) {
    readDataLBJ(bond.pocsagData, &bond.lbjData);
    debugLogStageTimingSd(2, "LBJ读取完成，用时[%llu]\n", millis64() - runtimeStartMs);
    debugLogStageTiming("Read complete.[%llu]", millis64() - runtimeStartMs);
}
