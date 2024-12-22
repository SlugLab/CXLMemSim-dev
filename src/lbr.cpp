//
// Created by victoryang00 on 12/20/24.
//

#include "lbr.h"

struct lbr_sample {
    struct perf_event_header header;
    uint32_t pid;
    uint32_t tid;
    uint64_t timestamp;
    uint64_t addr;
    uint64_t value;
    uint64_t counters[4];
};