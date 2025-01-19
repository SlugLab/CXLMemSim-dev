/*
 * CXLMemSim bpftime runtime
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */
#include "bpftimeruntime.h"
#include "bpftime_config.hpp"
#include "bpftime_logger.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <frida-core.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

BpfTimeRuntime::BpfTimeRuntime(std::string program_location, pid_t pid, std::string inject_path, std::string arg)
    : program_location(program_location), pid(pid), inject_path(inject_path), arg(arg) {
    bpftime_initialize_global_shm(bpftime::shm_open_type::SHM_CREATE_OR_OPEN);
    SPDLOG_INFO("GLOBAL memory initialized ");
    // load json program to shm
    bpftime_import_global_shm_from_json(program_location.c_str());
}

BpfTimeRuntime::~BpfTimeRuntime() { bpftime_remove_global_shm(); }

int BpfTimeRuntime::read(CXLController *controller, BPFTimeRuntimeElem *elem) {
    SPDLOG_INFO("Attaching runtime");
    auto item = bpftime_map_lookup_elem(10, &pid); // thread map
    if (item == nullptr) {
        SPDLOG_ERROR("Failed to find thread map");
        return -1;
    }
    // auto *thread_map = (bpftime::thread_map *)item;
    // bpftime_map_update_elem(fd, &pid, &res, 0);
    return 0;
}

