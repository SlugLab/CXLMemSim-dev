/*
 * CXLMemSim controller
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#ifndef CXLMEMSIM_BPFTIME_RUNTIME_H
#define CXLMEMSIM_BPFTIME_RUNTIME_H

#include <cstdint>
#include <linux/bpf.h>

class BpfTimeRuntime {
public:
    BpfTimeRuntime(std::string program_location);
    ~BpfTimeRuntime();

    void start();
    void stop();
};

#endif