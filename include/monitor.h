//
// Created by victoryang00 on 1/11/23.
//

#ifndef SLUGALLOCATOR_MONITOR_H
#define SLUGALLOCATOR_MONITOR_H

#include "cxlcontroller.h"
#include "helper.h"
#include "pebs.h"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <sched.h>
#include <unistd.h>
#include <vector>

enum MONITOR_STATUS {
    MONITOR_OFF = 0,
    MONITOR_ON = 1,
    MONITOR_TERMINATED = 2,
    MONITOR_NOPERMISSION = 3,
    MONITOR_DISABLE = 4,
    MONITOR_UNKNOWN = 0xff
};

class Monitor;
class Monitors {
public:
    std::vector<Monitor> mon;
    Monitors(int tnum, cpu_set_t *use_cpuset, Helper h);
    ~Monitors() = default;

    void stop_all(const int);
    void run_all(const int);
    int enable(const uint32_t, const uint32_t, bool, uint64_t, const int32_t, bool is_page);
    void disable(const uint32_t target);
    int terminate(const uint32_t, const uint32_t, const int32_t);
    bool check_all_terminated(const uint32_t);
    bool check_continue(const uint32_t, const struct timespec);
};

class Monitor {
public:
    pid_t tgid;
    pid_t tid;
    uint32_t cpu_core;
    char status;
    struct timespec injected_delay; // recorded time for injected
    struct timespec wasted_delay; // recorded time for calling between continue and calculation
    struct timespec squabble_delay; // inj-was
    struct Elem elem[2];
    struct Elem *before, *after;
    double total_delay;
    struct timespec start_exec_ts, end_exec_ts;
    bool is_process;
    struct PEBS *pebs_ctx;

    Monitor(Helper h);

    void stop();
    void run();
    static void clear_time(struct timespec *);
};

#endif // SLUGALLOCATOR_MONITOR_H
