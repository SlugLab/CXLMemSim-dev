//
// Created by victoryang00 on 1/12/23.
//

#ifndef CXL_MEM_SIMULATOR_UNCORE_H
#define CXL_MEM_SIMULATOR_UNCORE_H
#include "helper.h"
#include "perf.h"
#include <cstdint>

struct PerfConfig;
class Uncore {
public:
    uint32_t unc_idx;
    PerfInfo *perf;
    Uncore(uint32_t unc_idx, PerfConfig *perf_config);
    ~Uncore() = default;

    int read_cha_elems(struct CHAElem *elem);
};

#endif // CXL_MEM_SIMULATOR_UNCORE_H
