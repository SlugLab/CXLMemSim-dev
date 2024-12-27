/*
 * CXLMemSim lbr
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#ifndef CXLMEMSIM_LBR_H
#define CXLMEMSIM_LBR_H

#include "cxlcontroller.h"
#include "helper.h"
#include "pebs.h"

class LBR {
public:
    int fd;
    int pid;
    uint32_t seq{};
    size_t rdlen{};
    size_t mplen{};
    perf_event_mmap_page *mp;
    LBR(pid_t);
    ~LBR();
    int read(CXLController *, LBRElem *);
    int start();
    int stop();
};



#endif //CXLMEMSIM_LBR_H
