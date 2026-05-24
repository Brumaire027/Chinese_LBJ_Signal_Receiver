#ifndef LBJ_DATA_FORMATTER_HPP
#define LBJ_DATA_FORMATTER_HPP

#include "data_outputs.hpp"
#include "networks.hpp"

void collectRawPagerData(struct data_bond &bond, uint64_t runtimeStartMs);
void decodeLbjData(struct data_bond &bond, uint64_t runtimeStartMs);

#endif
