//
// Created by victoryang00 on 1/12/23.
//
#include "helper.h"
#include "logging.h"

const struct ModelContext model_ctx[] = {{CPU_MDL_BDX,
                                          {"/sys/bus/event_source/devices/uncore_cbox_%u/type",
                                           /*
                                            * cha_config:
                                            *    unc_c_llc_victims.m_state
                                            *    umask=0x1,event=0x37
                                            */
                                           0x0137,
                                           /*
                                            * all_dram_rds_config:
                                            *   offcore_response.all_reads.llc_miss.local_dram
                                            *   cpu/umask=0x1,event=0xb7,offcore_rsp=0x40007f7/
                                            */
                                           0x01b7, 0x6040007f7,
                                           /*
                                            * cpu_l2stall_config:
                                            *   cycle_activity.stalls_l2_pending
                                            *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                            */
                                           0x50005a3,
                                           /*
                                            * cpu_llcl_hits_config:
                                            *   mem_load_uops_l3_hit_retired.xsnp_none
                                            *   cpu/umask=0x8,event=0xd2/
                                            */
                                           0x08d2,
                                           /*
                                            * cpu_llcl_miss_config:
                                            *   mem_load_uops_l3_miss_retired.local_dram
                                            *   cpu/umask=0x1,event=0xd3/
                                            */
                                           0x01d3,
                                           /*
                                            * cpu_bandwidth_read_config:
                                            *   UNC_M_CAS_COUNT.RD * 64
                                            *   cpu/umask=0x03,event=0x04/
                                            */
                                           0x0304,
                                           /*
                                            * cpu_bandwidth_write_config:
                                            *   UNC_M_CAS_COUNT.WR * 64
                                            *   cpu/umask=0x0c,event=0x04/
                                            */
                                           0x0c04}},
                                         {CPU_MDL_SKX,
                                          {"/sys/bus/event_source/devices/uncore_cha_%u/type",
                                           /*
                                            * cha_config:
                                            *   UNC_CHA_LLC_VICTIMS
                                            *   umask=0x21,event=37
                                            */
                                           0x2137,
                                           /*
                                            * all_dram_rds_config:
                                            *   OCR.ALL_READS.L3_MISS.SNOOP_NONE
                                            *   cpu/umask=0x1,event=0xb7,offcore_rsp=0xBC408000/
                                            */
                                           0x01b7, 0xBC408000,
                                           /*
                                            * cpu_l2stall_config:
                                            *   cycle_activity.stalls_l2_miss
                                            *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                            */
                                           0x50005a3,
                                           /*
                                            * cpu_llcl_hits_config:
                                            *   mem_load_l3_hit_retired.xsnp_none
                                            *   cpu/umask=0x8,event=0xd2/
                                            */
                                           0x08d2,
                                           /*
                                            * cpu_llcl_miss_config:
                                            *   mem_load_l3_miss_retired.local_dram
                                            *   cpu/umask=0x1,event=0xd3/
                                            */
                                           0x01d3,
                                           /*
                                            * cpu_bandwidth_read_config:
                                            *   UNC_M_CAS_COUNT.RD * 64
                                            *   cpu/umask=0x03,event=0x04/
                                            */
                                           0x0304,
                                           /*
                                            * cpu_bandwidth_write_config:
                                            *   UNC_M_CAS_COUNT.WR * 64
                                            *   cpu/umask=0x0c,event=0x04/
                                            */
                                           0x0c04}},
                                         {CPU_MDL_SPR,
                                          {"/sys/bus/event_source/devices/uncore_cha_%u/type",
                                           /*
                                            * cha_config:
                                            *   UNC_CHA_LLC_VICTIMS
                                            *   umask=0x10,event=b0
                                            */
                                           0x10b0,
                                           /*
                                            * all_dram_rds_config:
                                            *   OCR.ALL_READS.L3_MISS.SNOOP_NONE => L3_MISS.SNOOP_MISS_OR_NO_FWD
                                            *   cpu/umask=0x1,event=0xb7,offcore_rsp=0x63FC00491/
                                            */
                                           0x01b7, 0x63FC00491,
                                           /*
                                            * cpu_l2stall_config:
                                            *   cycle_activity.stalls_l2_miss
                                            *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                            */
                                           0x50005a3,
                                           /*
                                            * cpu_llcl_hits_config:
                                            *   mem_load_l3_hit_retired.xsnp_none
                                            *   cpu/umask=0x8,event=0xd2/
                                            */
                                           0x08d2,
                                           /*
                                            * cpu_llcl_miss_config:
                                            *   mem_load_l3_miss_retired.local_dram
                                            *   cpu/umask=0x1,event=0xd3/
                                            */
                                           0x01d3,
                                           /*
                                            * cpu_bandwidth_read_config:
                                            *   UNC_M_CAS_COUNT.RD * 64
                                            *   cpu/umask=0xcf,event=0x05/
                                            */
                                           0xcf05,
                                           /*
                                            * cpu_bandwidth_write_config:
                                            *   UNC_M_CAS_COUNT.WR * 64
                                            *   cpu/umask=0xf0,event=0x05/
                                            */
                                           0xf005}},
                                         {CPU_MDL_ADL,
                                          {"/sys/bus/event_source/devices/uncore_cbox_%u/type",
                                           /*
                                            * cha_config:
                                            *   UNC_C_LLC_VICTIMS => OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_RD
                                            *   umask=0x21,event=10
                                            */
                                           0x2110, // no use
                                           /*
                                            * all_dram_rds_config:
                                            *   OCR.ALL_READS.L3_MISS.SNOOP_NONE => OCR.DEMAND_DATA_RD.L3_MISS
                                            *   cpu/umask=0x1,event=0x2A,offcore_rsp=0x3FBFC00001/
                                            */
                                           0x012a, 0x3fbfc00001,
                                           /*
                                            * cpu_l2stall_config:
                                            *   cycle_activity.stalls_l2_miss
                                            *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                            */
                                           0x50005a3,
                                           /*
                                            * cpu_llcl_hits_config:
                                            *   mem_load_l3_hit_retired.xsnp_none
                                            *   cpu/umask=0x8,event=0xd2/
                                            */
                                           0x08d2,
                                           /*
                                            * cpu_llcl_miss_config:
                                            *   mem_load_l3_miss_retired.local_dram
                                            *   cpu/umask=0x1,event=0xd3/
                                            */
                                           0x01d3,
                                           /*
                                            * cpu_bandwidth_read_config:
                                            *   UNC_M_CAS_COUNT.RD * 64
                                            *   cpu/umask=0xcf,event=0x05/
                                            */
                                           0xcf05,
                                           /*
                                            * cpu_bandwidth_write_config:
                                            *   UNC_M_CAS_COUNT.WR * 64
                                            *   cpu/umask=0xf0,event=0x05/
                                            */
                                           0xf005}},
                                         {CPU_MDL_END, {0}}};

int Helper::num_of_cpu() {
    int ncpu;
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu < 0) {
        LOG(ERROR) << "sysconf";
    }
    LOG(DEBUG) << fmt::format("num_of_cpu={}\n", ncpu);
    return ncpu;
}

int Helper::num_of_cha() {
    int ncha = 0;
    for (; ncha < 128; ++ncha) {
        std::string path = fmt::format("/sys/bus/event_source/devices/uncore_cha_{}/type", ncha);
//         LOG(DEBUG) << path;
        if (!std::filesystem::exists(path)) {
            break;
        }
    }
    LOG(DEBUG) << fmt::format("num_of_cha={}\n", ncha);
    return ncha;
}

double Helper::cpu_frequency() const {
    int i;
    int cpu = 0;
    double cpu_mhz = 0.0;
    double max_cpu_mhz = 0.0;
    std::ifstream fp("/proc/cpuinfo");

    for (std::string line; cpu != this->cpu - 1; std::getline(fp, line)) {
        // LOG(DEBUG) << fmt::format("line: {}\n", line);
        i = std::sscanf(line.c_str(), "cpu MHz : %lf", &cpu_mhz);
        max_cpu_mhz = i == 1 ? std::max(max_cpu_mhz, cpu_mhz) : max_cpu_mhz;
        std::sscanf(line.c_str(), "processor : %d", &cpu);
    }
    LOG(DEBUG) << fmt::format("cpu MHz: {}\n", cpu_mhz);

    return cpu_mhz;
}
PerfConfig Helper::detect_model(uint32_t model) {
    int i = 0;
    LOG(INFO) << fmt::format("Detecting model...{}\n", model);
    while (model_ctx[i].model != CPU_MDL_END) {
        if (model_ctx[i].model == model) {
            this->perf_conf = model_ctx[i].perf_conf;
            return model_ctx[i].perf_conf;
        }
        i++;
    }
    LOG(ERROR) << "Failed to execute. This CPU model is not supported. Refer to perfmon or pcm to add support\n";
    throw;
}
Helper::Helper() : perf_conf({}) {
    cpu = num_of_cpu();
    LOG(DEBUG) << cpu;
    cha = num_of_cha();
    cpu_freq = cpu_frequency();
}
void Helper::noop_handler(int sig) { ; }
void Helper::detach_children() {
    struct sigaction sa;

    sa.sa_handler = noop_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDWAIT;
    if (sigaction(SIGCHLD, &sa, nullptr) < 0) {
        LOG(ERROR) << fmt::format("Failed to sigaction: %s", strerror(errno));
    }
}
int PMUInfo::start_all_pmcs() {
    /* enable all pmcs to count */
    int i, r;
    for (i = 0; i < helper->num_of_cpu(); i++) {
        r = this->cpus[i].start();
        if (r < 0) {
            LOG(ERROR) << fmt::format("start failed. cpu:{}\n", i);
            return r;
        }
    }
    return 0;
}
PMUInfo::PMUInfo(pid_t pid, Helper *helper, struct PerfConfig *perf_config) : helper(helper) {
    int i, r, n;

    n = helper->num_of_cha();

    for (i = 0; i < n; i++) {
        this->chas.emplace_back(i, perf_config);
    }

    // unfreeze counters
    r = this->unfreeze_counters_cha_all();
    if (r < 0) {
        LOG(DEBUG) << fmt::format("unfreeze_counters_cha_all failed.\n");
        throw;
    }

    n = helper->num_of_cpu();

    for (i = 0; i < n; i++) {
        this->cpus.emplace_back(pid, i, perf_config);
    }

    r = this->start_all_pmcs();
    if (r < 0) {
        LOG(ERROR) << fmt::format("start_all_pmcs failed\n");
    }
}
int PMUInfo::stop_all_pmcs() {
    /* disable all pmcs to count */
    int i, r;

    for (i = 0; i < helper->num_of_cpu(); i++) {
        r = this->cpus[i].stop();
        if (r < 0) {
            LOG(ERROR) << fmt::format("stop failed. cpu:{}\n", i);
            return r;
        }
    }
    return 0;
}

int PMUInfo::unfreeze_counters_cha_all() {
    int i, r;

    for (i = 0; i < helper->num_of_cha(); i++) {
        r = this->chas[i].perf->start();
        if (r < 0) {
            LOG(ERROR) << fmt::format("perf_start failed. cha:{}\n", i);
            return r;
        }
    }
    return 0;
}
int PMUInfo::freeze_counters_cha_all() {
    int i, r;

    for (i = 0; i < helper->num_of_cha(); i++) {
        r = this->chas[i].perf->stop();
        if (r < 0) {
            LOG(ERROR) << fmt::format("perf_stop failed. cha:{}\n", i);
            return r;
        }
    }
    return 0;
}
PMUInfo::~PMUInfo() {
    this->cpus.clear();
    this->chas.clear();
    stop_all_pmcs();
}
