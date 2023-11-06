//
// Created by victoryang00 on 1/12/23.
//
#include "helper.h"
#include "logging.h"
#include <string>

struct ModelContext model_ctx = {CPU_MDL_SPR, {"/sys/bus/event_source/devices/uncore_cha_%u/type",
                                                     /*
                                                                        * cha_llc_write_back_config:
                                                                        *   UNC_CHA_LLC_VICTIMS
                                                                        *   umask=0x10,event=b0
                                                      */
                                                     //{
                                                     //    {"llc_write_back", 0x10b0, 0x1},
                                                     //    /*
                                                     //                       * all_dram_rds_config:
                                                     //                       *   OCR.ALL_READS.L3_MISS.SNOOP_NONE => L3_MISS.SNOOP_MISS_OR_NO_FWD
                                                     //                       *   cpu/umask=0x1,event=0xb7,offcore_rsp=0x63FC00491/
                                                     //     */
                                                     //    {"all_dram_rds", 0x01b7, 0x63FC00491},
                                                     //    /*
                                                     //                       * cpu_l2stall_config:
                                                     //                       *   cycle_activity.stalls_l2_miss
                                                     //                       *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                                     //     */
                                                     //    {"l2stall", 0x50005a3, 0},
                                                     //    /*
                                                     //                       * cpu_l2stall_config:
                                                     //                       *   cycle_activity.stalls_l2_miss
                                                     //                       *   cpu/umask=0x5,cmask=0x5,event=0xa3/
                                                     //     */
                                                     //    {"l2stall", 0x50005a3, 0},
                                                     //},
                                                     //{    /*
                                                     //                          * cpu_llcl_hits_config:
                                                     //                          *   mem_load_l3_hit_retired.xsnp_none
                                                     //                          *   cpu/umask=0x8,event=0xd2/
                                                     //      */
                                                     //     {"llcl_hits", 0x08d2, 0},
                                                     //     /*
                                                     //                        * cpu_llcl_miss_config:
                                                     //                        *   mem_load_l3_miss_retired.local_dram
                                                     //                        *   cpu/umask=0x1,event=0xd3/
                                                     //      */
                                                     //     {"llcl_miss", 0x01d3, 0},
                                                     //     /*
                                                     //                        * cha_bandwidth_config:
                                                     //                        *   UNC_M_CAS_COUNT.ALL * 64
                                                     //                        *   cpu/umask=0xcf,event=0x05/
                                                     //      */
                                                     //     {"bandwidth_all", 0xff05, 0},
                                                     //     /*
                                                     //                        * cha_bandwidth_write_config:
                                                     //                        *   UNC_M_CAS_COUNT.WR * 64
                                                     //                        *   cpu/umask=0xf0,event=0x05/
                                                     //      */
                                                     //     {"bandwidth_write", 0xf005, 0}
                                                     // }

                                                    }
                                                  };
                                   //{CPU_MDL_END, {nullptr}}};

int Helper::num_of_cpu() {
    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    ncpu = 24;
    if (ncpu < 0) {
        LOG(ERROR) << "sysconf";
    }
    return ncpu;
}

int Helper::num_of_cha() {
    int ncha = 0;
    for (; ncha < 24; ++ncha) {
        std::string path = fmt::format("/sys/bus/event_source/devices/uncore_cha_{}/type", ncha);
        //         LOG(DEBUG) << path;
        if (!std::filesystem::exists(path)) {
            break;
        }
    }
    LOG(DEBUG) << fmt::format("num_of_cha={}\n", ncha);
    return ncha;
}

double Helper::cpu_frequency() {
    int i, c = 0;
    double cpu_mhz = 0.0;
    double max_cpu_mhz = 0.0;
    std::ifstream fp("/proc/cpuinfo");

    for (std::string line; c != this->num_of_cpu() - 1; std::getline(fp, line)) {
        // LOG(DEBUG) << fmt::format("line: {}\n", line);
        i = std::sscanf(line.c_str(), "cpu MHz : %lf", &cpu_mhz);
        max_cpu_mhz = i == 1 ? std::max(max_cpu_mhz, cpu_mhz) : max_cpu_mhz;
        std::sscanf(line.c_str(), "processor : %d", &c);
    }
    LOG(DEBUG) << fmt::format("cpu MHz: {}\n", cpu_mhz);

    return cpu_mhz;
}
PerfConfig Helper::detect_model(uint32_t model) {
   //int i = 0;
   //LOG(INFO) << fmt::format("Detecting model...{}\n", model);
   //while (model_ctx[i].model != CPU_MDL_END) {
   //    if (model_ctx[i].model == model) {
   //        this->perf_conf = model_ctx[i].perf_conf;
   //        return model_ctx[i].perf_conf;
   //    }
   //    i++;
   //}
   //LOG(ERROR) << "Failed to execute. This CPU model is not supported. Refer to perfmon or pcm to add support\n";
    throw;
}
Helper::Helper() : perf_conf({}) {}
void Helper::noop_handler(int sig) { ; }
void Helper::detach_children() {
    struct sigaction sa {};

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
    //    Uncore *incore = new Uncore(0, perf_config);
    //    incore->perf[0].start();
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
        for (int j : {0, 1, 2, 3}) {
            r = this->chas[i].perf[j]->start();
            if (r < 0) {
                LOG(ERROR) << fmt::format("perf_start failed. cha:{}\n", i);
                return r;
            }
        }
    }
    return 0;
}
int PMUInfo::freeze_counters_cha_all() {
    int i, r;

    for (i = 0; i < helper->num_of_cha(); i++) {
        for (int j : {0, 1, 2, 3}) {
            r = this->chas[i].perf[j]->stop();
            if (r < 0) {
                LOG(ERROR) << fmt::format("perf_stop failed. cha:{}\n", i);
                return r;
            }
        }
    }
    return 0;
}
PMUInfo::~PMUInfo() {
    this->cpus.clear();
    this->chas.clear();
    stop_all_pmcs();
}
