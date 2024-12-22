//
// Created by victoryang00 on 12/20/24.
//

#ifndef CXLMEMSIM_LBR_H
#define CXLMEMSIM_LBR_H

#include "cxlcontroller.h"
#include "helper.h"
#include "pebs.h"

class LBR {
public:
    int fd;
    int pid;
    uint64_t sample_period;
    uint32_t seq{};
    size_t rdlen{};
    size_t mplen{};
    perf_event_mmap_page *mp;
    LBR(pid_t, uint64_t);
    ~LBR();
    int read(CXLController *, PEBSElem *);
    int start();
    int stop();
};



#endif //CXLMEMSIM_LBR_H
