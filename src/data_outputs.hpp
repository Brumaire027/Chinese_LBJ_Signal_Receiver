#ifndef LBJ_DATA_OUTPUTS_HPP
#define LBJ_DATA_OUTPUTS_HPP

#include "networks.hpp"

struct DecodedOutputContext {
    struct data_bond &bond;
    const struct rx_info &info;
    uint64_t runtimeStartMs;
};

void dispatchDecodedOutputs(struct data_bond &bond, const struct rx_info &info, uint64_t runtimeStartMs);

void flushDecodedCsvOutputIfDue();

#endif
