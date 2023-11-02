//
// Created by victoryang00 on 1/12/23.
//

#ifndef CXL_MEM_SIMULATOR_HELPER_H
#define CXL_MEM_SIMULATOR_HELPER_H

#include "incore.h"
#include "logging.h"
#include "uncore.h"
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fnmatch.h>
#include <linux/perf_event.h>
#include <map>
#include <optional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

/* CPU Models */
enum { CPU_MDL_BDX = 63, CPU_MDL_SKX = 85, CPU_MDL_SPR = 143, CPU_MDL_ADL = 151, CPU_MDL_END = 0x0ffff };
class Incore;
class Uncore;
class Helper;

struct EmuCXLLatency {
    double read;
    double write;
};

struct EmuCXLBandwidth {
    double read;
    double write;
};

struct BandwidthPass {
    std::tuple<int, int> all_access;
    uint64_t read_config;
    uint64_t write_config;
};

struct LatencyPass {
    std::tuple<int, int> all_access;
    double dramlatency;
    double ma_ro;
    double ma_wb;
};

struct CHAElem {
    uint64_t cpu_llc_hits;
    uint64_t cpu_llc_miss;
    uint64_t cpu_read_bandwidth;
    uint64_t cpu_llc_wb;
};

struct CPUElem {
    uint64_t cpu_l2stall_t;
    uint64_t cpu_llcl_hits;
    uint64_t cpu_llcl_miss;
    uint64_t cpu_bandwidth;
};

struct PEBSElem {
    uint64_t total;
    uint64_t llcmiss;
};

struct CPUInfo {
    uint32_t max_cpuid;
    uint32_t cpu_family;
    uint32_t cpu_model;
    uint32_t cpu_stepping;
};

struct Elem {
    struct CPUInfo cpuinfo;
    std::vector<CHAElem> chas;
    std::vector<CPUElem> cpus;
    struct PEBSElem pebs;
};

class PMUInfo {
public:
    std::vector<Uncore> chas;
    std::vector<Incore> cpus;
    Helper *helper;
    PMUInfo(pid_t pid, Helper *h, struct PerfConfig *perf_config);
    ~PMUInfo();
    int start_all_pmcs();
    int stop_all_pmcs();
    int freeze_counters_cha_all();
    int unfreeze_counters_cha_all();
};

struct PerfConfig {
    const char *path_format_cha_type;
    uint64_t cha_llc_write_back_config;
    uint64_t cha_llc_write_back_config1;
    uint64_t all_dram_rds_config;
    uint64_t all_dram_rds_config1;
    uint64_t cpu_ldm_stalling_config;
    uint64_t cpu_llcl_hits_config;
    uint64_t cpu_llc_writeback_config;
    uint64_t cpu_bandwidth_read_config;
    uint64_t cpu_bandwidth_write_config;
};

struct ModelContext {
    uint32_t model{};
    struct PerfConfig perf_conf;
};

class Helper {
public:
    PerfConfig perf_conf;
    Helper();
    int num_of_cpu();
    int num_of_cha();
    static void detach_children();
    static void noop_handler(int signum);
    double cpu_frequency();
    PerfConfig detect_model(uint32_t);
};

#endif // CXL_MEM_SIMULATOR_HELPER_H
