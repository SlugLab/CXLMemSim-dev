/*
 * CXLMemSim bpftime runtime
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "bpf_attach_ctx.hpp"
#include "bpftimeruntime.h"
#include "bpftime_helper_group.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm_internal.hpp"
#include "handler/prog_handler.hpp"
#include <bpftime_shm.hpp>
#include <bpf/bpf.h>

BpfTimeRuntime::BpfTimeRuntime(std::string program_location) {
    
}

BpfTimeRuntime::~BpfTimeRuntime() {
}

void BpfTimeRuntime::start() {
}

void BpfTimeRuntime::stop() {
}