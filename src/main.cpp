//
// Created by victoryang00 on 1/12/23.
//
#include "cxlendpoint.h"
#include "helper.h"
#include "logging.h"
#include "monitor.h"
#include "policy.h"
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cxxopts.hpp>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/cxl_mem_simulator.sock"
extern struct ModelContext model_ctx[];
int main(int argc, char *argv[]) {

    cxxopts::Options options("CXLMemSim", "For simulation of CXL.mem Type 3 on Sapphire Rapids");
    options.add_options()("t,target", "The script file to execute",
                          cxxopts::value<std::string>()->default_value("./microbench/ld"))(
        "h,help", "Help for CXLMemSim", cxxopts::value<bool>()->default_value("false"))(
        "i,interval", "The value for epoch value", cxxopts::value<int>()->default_value("5"))(
        "s,source", "Collection Phase or Validation Phase", cxxopts::value<bool>()->default_value("false"))(
        "c,cpuset", "The CPUSET for CPU to set affinity on and only run the target process on those CPUs",
        cxxopts::value<std::vector<int>>()->default_value("0,1,2,3,4,5,6,7"))(
        "d,dramlatency", "The current platform's dram latency", cxxopts::value<double>()->default_value("110"))(
        "p,pebsperiod", "The pebs sample period", cxxopts::value<int>()->default_value("100"))(
        "m,mode", "Page mode or cacheline mode", cxxopts::value<std::string>()->default_value("p"))(
        "o,topology", "The newick tree input for the CXL memory expander topology",
        cxxopts::value<std::string>()->default_value("(1,(2,3))"))(
        "e,capacity", "The capacity vector of the CXL memory expander with the firsgt local",
        cxxopts::value<std::vector<int>>()->default_value("0,20,20,20"))(
        "f,frequency", "The frequency for the running thread", cxxopts::value<double>()->default_value("4000"))(
        "l,latency", "The simulated latency by epoch based calculation for injected latency",
        cxxopts::value<std::vector<int>>()->default_value("100,150,100,150,100,150"))(
        "b,bandwidth", "The simulated bandwidth by linear regression",
        cxxopts::value<std::vector<int>>()->default_value("50,50,50,50,50,50"))(
        "n,pmu", "The input for Collected PMU", cxxopts::value<std::vector<int>>()->default_value("0,0,0,0"));

    auto result = options.parse(argc, argv);
    if (result["help"].as<bool>()) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    auto target = result["target"].as<std::string>();
    auto interval = result["interval"].as<int>();
    auto cpuset = result["cpuset"].as<std::vector<int>>();
    auto pebsperiod = result["pebsperiod"].as<int>();
    auto latency = result["latency"].as<std::vector<int>>();
    auto bandwidth = result["bandwidth"].as<std::vector<int>>();
    auto frequency = result["frequency"].as<double>();
    auto topology = result["topology"].as<std::string>();
    auto capacity = result["capacity"].as<std::vector<int>>();
    auto dramlatency = result["dramlatency"].as<double>();
    auto pmu_counter = result["pmu"].as<std::vector<int>>();
    auto mode = result["mode"].as<std::string>() == "p";
    auto source = result["source"].as<bool>();

    Helper helper{};
    auto *policy = new InterleavePolicy();
    CXLController *controller;

    if (pmu_counter[0]!=0){
        memcpy(&model_ctx[0].perf_conf,pmu_counter.data(), pmu_counter.size()) ;
    }
    uint64_t use_cpus = 0;
    cpu_set_t use_cpuset;
    CPU_ZERO(&use_cpuset);
    for (int i = 0; i < helper.num_of_cpu(); i++) {
        if (!use_cpus || use_cpus & 1UL << i) {
            CPU_SET(i, &use_cpuset);
            LOG(DEBUG) << fmt::format("use cpuid: {}{}\n", i, use_cpus);
        }
    }
    auto tnum = CPU_COUNT(&use_cpuset);
    auto cur_processes = 0;
    auto ncpu = helper.num_of_cpu();
    auto ncha = helper.num_of_cha();
    LOG(DEBUG) << fmt::format("tnum:{}, intrval:{}\n", tnum, interval);
    /** Hardcoded weights for different latency */
    std::vector<int> weight_vec = {400, 800, 1200, 1600, 2000, 2400, 3000};
    std::vector<double> weight = {88, 88, 88, 88, 88, 88, 88};
    for (auto const &[idx, value] : weight | enumerate) {
        LOG(DEBUG) << fmt::format("weight[{}]:{}\n", weight_vec[idx], value);
    }

    for (auto const &[idx, value] : capacity | enumerate) {
        if (idx == 0) {
            LOG(DEBUG) << fmt::format("local_memory_region capacity:{}\n", value);
            controller = new CXLController(policy, capacity[0], mode, interval);
        } else {
            LOG(DEBUG) << fmt::format("memory_region:{}\n", (idx - 1) + 1);
            LOG(DEBUG) << fmt::format(" capacity:{}\n", capacity[(idx - 1) + 1]);
            LOG(DEBUG) << fmt::format(" read_latency:{}\n", latency[(idx - 1) * 2]);
            LOG(DEBUG) << fmt::format(" write_latency:{}\n", latency[(idx - 1) * 2 + 1]);
            LOG(DEBUG) << fmt::format(" read_bandwidth:{}\n", bandwidth[(idx - 1) * 2]);
            LOG(DEBUG) << fmt::format(" write_bandwidth:{}\n", bandwidth[(idx - 1) * 2 + 1]);
            auto *ep = new CXLMemExpander(bandwidth[(idx - 1) * 2], bandwidth[(idx - 1) * 2 + 1],
                                          latency[(idx - 1) * 2], latency[(idx - 1) * 2 + 1], (idx - 1), capacity[idx]);
            controller->insert_end_point(ep);
        }
    }
    controller->construct_topo(topology);
    LOG(INFO) << controller->output() << "\n";
    int sock;
    struct sockaddr_un addr {};

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);
    remove(addr.sun_path);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        LOG(ERROR) << "Failed to execute. Can't bind to a socket.\n";
        exit(1);
    }
    LOG(DEBUG) << fmt::format("cpu_freq:{}\n", frequency);
    LOG(DEBUG) << fmt::format("num_of_cha:{}\n", ncha);
    LOG(DEBUG) << fmt::format("num_of_cpu:{}\n", ncpu);
    Monitors monitors{tnum, &use_cpuset, helper};

    // https://stackoverflow.com/questions/24796266/tokenizing-a-string-to-pass-as-char-into-execve
    char cmd_buf[1024] = {0};
    strncpy(cmd_buf, target.c_str(), sizeof(cmd_buf));

    /* This strtok_r() call puts '\0' after the first token in the buffer,
     * It saves the state to the strtok_state and subsequent calls resume from that point. */
    char *strtok_state = nullptr;
    char *filename = strtok_r(cmd_buf, " ", &strtok_state);

    /* Allocate an array of pointers.
     * We will make them point to certain locations inside the cmd_buf. */
    char *args[32] = {nullptr};
    args[0] = filename;
    /* loop the strtok_r() call while there are tokens and free space in the array */
    size_t current_arg_idx;
    for (current_arg_idx = 1; current_arg_idx < 32; ++current_arg_idx) {
        /* Note that the first argument to strtok_r() is nullptr.
         * That means resume from a point saved in the strtok_state. */
        char *current_arg = strtok_r(nullptr, " ", &strtok_state);
        if (current_arg == nullptr) {
            break;
        }

        args[current_arg_idx] = current_arg;
        LOG(INFO) << fmt::format("args[{}] = {}\n", current_arg_idx, args[current_arg_idx]);
    }

    /* zombie avoid */
    Helper::detach_children();
    /* create target process */
    auto t_process = fork();
    if (t_process < 0) {
        LOG(ERROR) << "Fork: failed to create target process";
        exit(1);
    } else if (t_process == 0) {
        execv(filename, args);
        /* We do not need to check the return value */
        LOG(ERROR) << "Exec: failed to create target process\n";
        exit(1);
    }
    // In case of process, use SIGSTOP.
    auto res = monitors.enable(t_process, t_process, true, pebsperiod, tnum, mode);
    if (res == -1) {
        LOG(ERROR) << fmt::format("Failed to enable monitor\n");
        exit(0);
    } else if (res < 0) {
        // pid not found. might be already terminated.
        LOG(DEBUG) << fmt::format("pid({}) not found. might be already terminated.\n", t_process);
    }
    cur_processes++;
    LOG(DEBUG) << fmt::format("pid of CXLMemSim = {}, cur process={}\n", t_process, cur_processes);

    if (cur_processes >= ncpu) {
        LOG(ERROR) << fmt::format(
            "Failed to execute. The number of processes/threads of the target application is more than "
            "physical CPU cores.\n");
        exit(0);
    }

    // Wait all the target processes until emulation process initialized.
    monitors.stop_all(cur_processes);

    /* get CPU information */
    if (!get_cpu_info(&monitors.mon[0].before->cpuinfo)) {
        LOG(DEBUG) << "Failed to obtain CPU information.\n";
    }

    /* check the CPU model */
    auto perf_config = helper.detect_model(monitors.mon[0].before->cpuinfo.cpu_model);

    PMUInfo pmu{t_process, &helper, &perf_config};

    /* Caculate epoch time */
    struct timespec waittime {};
    waittime.tv_sec = interval / 1000;
    waittime.tv_nsec = (interval % 1000) * 1000000;

    LOG(DEBUG) << "The target process starts running.\n";
    LOG(DEBUG) << fmt::format("set nano sec = {}\n", waittime.tv_nsec);

    /* read CHA params */
    for (auto mon : monitors.mon) {
        for (auto const &[idx, value] : pmu.chas | enumerate) {
            pmu.chas[idx].read_cha_elems(&mon.before->chas[idx]);
        }
        for (auto const &[idx, value] : pmu.cpus | enumerate) {
            pmu.cpus[idx].read_cpu_elems(&mon.before->cpus[idx]);
        }
    }

    uint32_t diff_nsec = 0;
    struct timespec start_ts {
    }, end_ts{};
    struct timespec sleep_start_ts {
    }, sleep_end_ts{};

    // Wait all the target processes until emulation process initialized.
    monitors.run_all(cur_processes);
    for (int i = 0; i < cur_processes; i++) {
        clock_gettime(CLOCK_MONOTONIC, &monitors.mon[i].start_exec_ts);
    }

    while (true) {
        /* wait for pre-defined interval */
        clock_gettime(CLOCK_MONOTONIC, &sleep_start_ts);

        /** Here was a definition for the multi process and thread to enable multiple monitor */

        struct timespec req = waittime;
        struct timespec rem = {0};
        while (true) {
            auto ret = nanosleep(&req, &rem);
            if (ret == 0) { // success
                break;
            } else { // ret < 0
                if (errno == EINTR) {
                    LOG(ERROR) << fmt::format("nanosleep: remain time {}.{}(sec)\n", (long)rem.tv_sec,
                                              (long)rem.tv_nsec);
                    // if the milisecs was set below 5, will trigger stop before the target process stop.
                    // The pause has been interrupted by a signal that was delivered to the thread.
                    req = rem; // call nanosleep() again with the remain time.
                    break;
                } else {
                    // fatal error
                    LOG(ERROR) << "Failed to wait nanotime";
                    exit(0);
                }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &sleep_end_ts);

        for (auto const &[i, mon] : monitors.mon | enumerate) {
            if (mon.status == MONITOR_DISABLE) {
                continue;
            }
            if (mon.status == MONITOR_ON) {
                clock_gettime(CLOCK_MONOTONIC, &start_ts);
                LOG(DEBUG) << fmt::format("[{}:{}:{}] start_ts: {}.{}\n", i, mon.tgid, mon.tid, start_ts.tv_sec,
                                          start_ts.tv_nsec);
                mon.stop();
                /* read CHA values */
                uint64_t wb_cnt = 0;
                for (int j = 0; j < ncha; j++) {
                    pmu.chas[j].read_cha_elems(&mon.after->chas[j]);
                    wb_cnt += mon.after->chas[j].cpu_llc_wb - mon.before->chas[j].cpu_llc_wb;
                }
                LOG(INFO) << fmt::format("[{}:{}:{}] LLC_WB = {}\n", i, mon.tgid, mon.tid, wb_cnt);

                /* read CPU params */
                uint64_t read_config = 0;
                uint64_t target_l2stall = 0, target_llcmiss = 0, target_llchits = 0;
                for (int j = 0; j < ncpu; ++j) {
                    pmu.cpus[j].read_cpu_elems(&mon.after->cpus[j]);
                    read_config += mon.after->cpus[j].cpu_bandwidth - mon.before->cpus[j].cpu_bandwidth;
                }
                /* read PEBS sample */
                if (mon.pebs_ctx->read(controller, &mon.after->pebs) < 0) {
                    LOG(ERROR) << fmt::format("[{}:{}:{}] Warning: Failed PEBS read\n", i, mon.tgid, mon.tid);
                }
                target_llcmiss = mon.after->pebs.total - mon.before->pebs.total;

                // target_l2stall =
                //     mon.after->cpus[mon.cpu_core].cpu_l2stall_t - mon.before->cpus[mon.cpu_core].cpu_l2stall_t;
                // target_llchits =
                //     mon.after->cpus[mon.cpu_core].cpu_llcl_hits - mon.before->cpus[mon.cpu_core].cpu_llcl_hits;
                for (auto const &[idx, value] : pmu.cpus | enumerate) {
                    target_l2stall += mon.after->cpus[idx].cpu_l2stall_t - mon.before->cpus[idx].cpu_l2stall_t;
                    target_llchits += mon.after->cpus[idx].cpu_llcl_hits - mon.before->cpus[idx].cpu_llcl_hits;
                }

                uint64_t llcmiss_wb = 0;
                // To estimate the number of the writeback-involving LLC
                // misses of the CPU core (llcmiss_wb), the total number of
                // writebacks observed in L3 (wb_cnt) is devided
                // proportionally, according to the number of the ratio of
                // the LLC misses of the CPU core (target_llcmiss) to that
                // of the LLC misses of all the CPU cores and the
                // prefetchers (cpus_dram_rds).
                llcmiss_wb = wb_cnt * std::lround(((double)target_llcmiss) / ((double)read_config));

                uint64_t llcmiss_ro = 0;
                if (target_llcmiss < llcmiss_wb) {
                    LOG(ERROR) << fmt::format("[{}:{}:{}] cpus_dram_rds {}, llcmiss_wb {}, target_llcmiss {}\n", i,
                                              mon.tgid, mon.tid, read_config, llcmiss_wb, target_llcmiss);
                    llcmiss_wb = target_llcmiss;
                    llcmiss_ro = 0;
                } else {
                    llcmiss_ro = target_llcmiss - llcmiss_wb;
                }
                LOG(DEBUG) << fmt::format("[{}:{}:{}]llcmiss_wb={}, llcmiss_ro={}\n", i, mon.tgid, mon.tid, llcmiss_wb,
                                          llcmiss_ro);

                uint64_t mastall_wb = 0;
                uint64_t mastall_ro = 0;
                // If both target_llchits and target_llcmiss are 0, it means that hit in L2.
                // Stall by LLC misses is 0.
                // choose by vector
                // mastall_wb = (double)(target_l2stall / frequency) *
                //              ((double)(weight * llcmiss_wb) / (double)(target_llchits + (weight * target_llcmiss))) *
                //              1000;
                // mastall_ro = (double)(target_l2stall / frequency) *
                //              ((double)(weight * llcmiss_ro) / (double)(target_llchits + (weight * target_llcmiss))) *
                //              1000;
                LOG(DEBUG) << fmt::format(
                    "l2stall={}, mastall_wb={}, mastall_ro={}, target_llchits={}, target_llcmiss={}\n", target_l2stall,
                    mastall_wb, mastall_ro, target_llchits, target_llcmiss);

                auto ma_wb = (double)mastall_wb / dramlatency;
                auto ma_ro = (double)mastall_ro / dramlatency;

                uint64_t emul_delay = 0;

                LOG(DEBUG) << fmt::format("[{}:{}:{}] pebs: total={}, \n", i, mon.tgid, mon.tid, mon.after->pebs.total);

                /** TODO: calculate latency construct the passing value and use interleaving policy and counter to get
                 * the sample_prop */
                auto all_access = controller->get_all_access();
                LatencyPass lat_pass = {
                    .all_access = all_access,
                    .dramlatency = dramlatency,
                    .ma_ro = ma_ro,
                    .ma_wb = ma_wb,
                };
                BandwidthPass bw_pass = {
                    .all_access = all_access,
                    .read_config = read_config,
                    .write_config = read_config,
                };
                emul_delay += std::lround(controller->calculate_latency(lat_pass));
                //                emul_delay += controller->calculate_bandwidth(bw_pass);
                //                emul_delay += std::get<0>(controller->calculate_congestion());

                mon.before->pebs.total = mon.after->pebs.total;

                LOG(DEBUG) << fmt::format("ma_wb={}, ma_ro={}, delay={}\n", ma_wb, ma_ro, emul_delay);

                /* compensation of delay END(1) */
                clock_gettime(CLOCK_MONOTONIC, &end_ts);
                diff_nsec += (end_ts.tv_sec - start_ts.tv_sec) * 1000000000 + (end_ts.tv_nsec - start_ts.tv_nsec);
                LOG(DEBUG) << fmt::format("dif:{}\n", diff_nsec);

                uint64_t calibrated_delay = (diff_nsec > emul_delay) ? 0 : emul_delay - diff_nsec;
                // uint64_t calibrated_delay = emul_delay;
                mon.total_delay += (double)calibrated_delay / 1000000000;
                diff_nsec = 0;

                /* insert emulated NVM latency */
                mon.injected_delay.tv_sec += std::lround(calibrated_delay / 1000000000);
                mon.injected_delay.tv_nsec += std::lround(calibrated_delay % 1000000000);
                LOG(DEBUG) << fmt::format("[{}:{}:{}]delay:{} , total delay:{}\n", i, mon.tgid, mon.tid,
                                          calibrated_delay, mon.total_delay);
                auto swap = mon.before;
                mon.before = mon.after;
                mon.after = swap;

                /* continue suspended processes: send SIGCONT */
                // unfreeze_counters_cha_all(fds.msr[0]);
                // start_pmc(&fds, i);
                if (calibrated_delay == 0) {
                    Monitor::clear_time(&mon.wasted_delay);
                    Monitor::clear_time(&mon.injected_delay);
                    mon.run();
                }
            } else if (mon.status == MONITOR_OFF) {
                // Wasted epoch time
                clock_gettime(CLOCK_MONOTONIC, &start_ts);
                uint64_t sleep_diff = (sleep_end_ts.tv_sec - sleep_start_ts.tv_sec) * 1000000000 +
                                      (sleep_end_ts.tv_nsec - sleep_start_ts.tv_nsec);
                struct timespec sleep_time {};
                sleep_time.tv_sec = std::lround(sleep_diff / 1000000000);
                sleep_time.tv_nsec = std::lround(sleep_diff % 1000000000);
                mon.wasted_delay.tv_sec += sleep_time.tv_sec;
                mon.wasted_delay.tv_nsec += sleep_time.tv_nsec;
                LOG(DEBUG) << fmt::format("[{}:{}:{}][OFF] total: {}| wasted : {}| waittime : {}| squabble : {}\n", i,
                                          mon.tgid, mon.tid, mon.injected_delay.tv_nsec, mon.wasted_delay.tv_nsec,
                                          waittime.tv_nsec, mon.squabble_delay.tv_nsec);
                if (monitors.check_continue(i, sleep_time)) {
                    Monitor::clear_time(&mon.wasted_delay);
                    Monitor::clear_time(&mon.injected_delay);
                    mon.run();
                }
                clock_gettime(CLOCK_MONOTONIC, &end_ts);
                diff_nsec += (end_ts.tv_sec - start_ts.tv_sec) * 1000000000 + (end_ts.tv_nsec - start_ts.tv_nsec);
            }

            if (mon.status == MONITOR_OFF && mon.injected_delay.tv_nsec != 0) {
                long remain_time = mon.injected_delay.tv_nsec - mon.wasted_delay.tv_nsec;
                /* do we need to get squabble time ? */
                if (mon.wasted_delay.tv_sec >= waittime.tv_sec && remain_time < waittime.tv_nsec) {
                    mon.squabble_delay.tv_nsec += remain_time;
                    if (mon.squabble_delay.tv_nsec < 40000000) {
                        LOG(DEBUG) << fmt::format("[SQ]total: {}| wasted : {}| waittime : {}| squabble : {}\n",
                                                  mon.injected_delay.tv_nsec, mon.wasted_delay.tv_nsec,
                                                  waittime.tv_nsec, mon.squabble_delay.tv_nsec);
                        Monitor::clear_time(&mon.wasted_delay);
                        Monitor::clear_time(&mon.injected_delay);
                        mon.run();
                    } else {
                        mon.injected_delay.tv_nsec += mon.squabble_delay.tv_nsec;
                        Monitor::clear_time(&mon.squabble_delay);
                    }
                }
            }
        } // End for-loop for all target processes
        if (monitors.check_all_terminated(tnum)) {
            break;
        }
        LOG(TRACE)<<fmt::format("{}\n", monitors);;
    } // End while-loop for emulation

    return 0;
}
