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

#include "cxlcontroller.h"
#include <linux/bpf.h>
#include <string>
#include <sys/types.h>

class BpfTimeRuntime {
public:
    BpfTimeRuntime(std::string program_location, pid_t pid, std::string inject_path, std::string arg);
    ~BpfTimeRuntime();

    int read(CXLController *, BPFTimeRuntimeElem *);

private:
    std::string program_location;
    pid_t pid;
    std::string inject_path;
    std::string arg;
};

#endif
