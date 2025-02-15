/*
 * CXLMemSim controller
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *      Brian Zhao
 *  SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#ifndef CXLMEMSIM_CXLCONTROLLER_H
#define CXLMEMSIM_CXLCONTROLLER_H

#include "cxlendpoint.h"
#include "lbr.h"
#include <queue>
#include <string_view>

class Monitors;
struct mem_stats;
struct proc_info;
enum page_type { CACHELINE, PAGE, HUGEPAGE_2M, HUGEPAGE_1G };

class AllocationPolicy {
public:
    virtual ~AllocationPolicy() = default;
    AllocationPolicy();
    virtual int compute_once(CXLController *) = 0;
    // No write problem
};

class MigrationPolicy {
public:
    virtual ~MigrationPolicy() = default;
    MigrationPolicy();
    virtual int compute_once(CXLController *) = 0; // reader writer
    // paging related
    // switching related
};

// need to give a timeout and will be added latency later,
class PagingPolicy {
public:
    PagingPolicy();
    virtual int compute_once(CXLController *) = 0; // reader writer
    // paging related
};

class CachingPolicy {
public:
    CachingPolicy();
    virtual int compute_once(CXLController *) = 0; // reader writer
    // paging related
};

class CXLController : public CXLSwitch {
public:
    std::vector<CXLMemExpander *> cur_expanders{};
    int capacity; // GB
    AllocationPolicy *policy;
    CXLCounter counter;
    std::map<uint64_t, uint64_t> occupation;
    std::map<uint64_t, uint64_t> va_pa_map;
    page_type page_type_; // percentage
    int num_switches = 0;
    int num_end_points = 0;
    uint64_t freed = 0;
    size_t cache_size = 30 * 1024 * 1024; // 30MB
    std::queue<uint64_t> cache;

    explicit CXLController(AllocationPolicy *p, int capacity, page_type page_type_, int epoch);
    void construct_topo(std::string_view newick_tree);
    void insert_end_point(CXLMemExpander *end_point);
    std::vector<std::string> tokenize(const std::string_view &s);
    std::tuple<double, std::vector<uint64_t>> calculate_congestion() override;
    void set_epoch(int epoch) override;
    std::tuple<int, int> get_all_access() override;
    double calculate_latency(LatencyPass elem) override; // traverse the tree to calculate the latency
    double calculate_bandwidth(BandwidthPass elem) override;
    int insert(uint64_t timestamp, uint64_t tid, struct lbr *lbrs, struct cntr *counters) override;
    int insert(uint64_t timestamp, uint64_t phys_addr, uint64_t virt_addr, int index) override;
    void delete_entry(uint64_t addr, uint64_t length) override;
    std::string output() override;
    void set_stats(mem_stats stats);
    void set_process_info(proc_info process_info);
    void set_thread_info(proc_info thread_info);
};

template <> struct std::formatter<CXLController> {
    // Parse function to handle any format specifiers (if needed)
    constexpr auto parse(std::format_parse_context &ctx) -> decltype(ctx.begin()) {
        // If you have specific format specifiers, parse them here
        // For simplicity, we'll ignore them and return the end iterator
        return ctx.end();
    }

    // Format function to output the Monitors data
    template <typename FormatContext> auto format(const CXLController &p, FormatContext &ctx) const -> decltype(ctx.out()) {
        std::string result;

        // for (size_t core_idx = 0; core_idx < helper.used_cpu.size(); ++core_idx) {
        //     for (size_t cpu_idx = 0; cpu_idx < helper.perf_conf.cpu.size(); ++cpu_idx) {
        //         bool is_last_cpu = (cpu_idx == helper.perf_conf.cpu.size() - 1);
        //         bool is_last_core = (core_idx == helper.used_cpu.size() - 1);
        //         int cpu_diff = mon.after->cpus[core_idx].cpu[cpu_idx] - mon.before->cpus[core_idx].cpu[cpu_idx];
        //         if (is_last_cpu && is_last_core) {
        //             result += std::format("{}", cpu_diff);
        //         } else {
        //             result += std::format("{},", cpu_diff);
        //         }
        //     }
        // }

        // Write the accumulated result to the output iterator
        // return std::copy(result.begin(), result.end(), ctx.out());
        return format_to(ctx.out(), "[Monitor");
    }
};

template <> struct std::formatter<CXLSwitch> {
    constexpr auto parse(std::format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext> auto format(const CXLSwitch &switch_, FormatContext &ctx) {
        return format_to(ctx.out(), "[Monitor");
    }
};

template <> struct std::formatter<CXLEndPoint> {
    constexpr auto parse(std::format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext> auto format(const CXLEndPoint &end_point, FormatContext &ctx) {
        return format_to(ctx.out(), "[Monitor");
    }
};

extern CXLController *controller;
#endif // CXLMEMSIM_CXLCONTROLLER_H
