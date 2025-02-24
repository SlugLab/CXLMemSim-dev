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
struct lbr;
struct cntr;
enum page_type { CACHELINE, PAGE, HUGEPAGE_2M, HUGEPAGE_1G };

class Policy {
public:
    virtual ~Policy() = default;
    Policy() = default;
    virtual int compute_once(CXLController *) = 0; // reader writer
};

class AllocationPolicy : public Policy {
public:
    AllocationPolicy();
};

class MigrationPolicy : public Policy {
public:
    MigrationPolicy();
    std::vector<std::tuple<uint64_t, uint64_t>> get_migration_list(CXLController *controller);
    // migration related
    // 1. get the migration list
    // 2. do the migration
    // 3. update the counter
    // 4. update the occupation map
    // 5. update the latency and bandwidth
    // 6. update the ring buffer
    // 7. update the rob info
    // 8. update the thread info
};

// need to give a timeout and will be added latency later,
class PagingPolicy : public Policy {
public:
    PagingPolicy();
    // paging related
};

class CachingPolicy : public Policy {
public:
    CachingPolicy();
    // paging related
};

class CXLController : public CXLSwitch {
public:
    std::vector<CXLMemExpander *> cur_expanders{};
    int capacity; // GB
    AllocationPolicy *allocation_policy{};
    MigrationPolicy *migration_policy{};
    PagingPolicy *paging_policy{};
    CachingPolicy *caching_policy{};
    CXLCounter counter;
    std::map<uint64_t, occupation_info> occupation;
    page_type page_type_; // percentage
    // no need for va pa map because v-indexed will not caught by us
    int num_switches = 0;
    int num_end_points = 0;
    int last_index = 0;
    uint64_t freed = 0;
    uint64_t latency_lat{};
    uint64_t bandwidth_lat{};
    double dramlatency;
    // ring buffer
    std::queue<lbr> ring_buffer;
    // rob info
    std::unordered_map<uint64_t, thread_info> thread_map;

    explicit CXLController(std::array<Policy *, 4> p, int capacity, page_type page_type_, int epoch,
                           double dramlatency);
    void construct_topo(std::string_view newick_tree);
    void insert_end_point(CXLMemExpander *end_point);
    std::vector<std::string> tokenize(const std::string_view &s);
    std::tuple<double, std::vector<uint64_t>> calculate_congestion() override;
    void set_epoch(int epoch) override;
    std::vector<std::tuple<uint64_t, uint64_t>> get_access(uint64_t timestamp) override;
    double calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem,
                             double dramlatency) override; // traverse the tree to calculate the latency
    double calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) override;
    void insert_one(thread_info &t_info, lbr &lbr);
    int insert(uint64_t timestamp, uint64_t tid, lbr lbrs[32], cntr counters[32]);
    int insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) override;
    void delete_entry(uint64_t addr, uint64_t length) override;
    void set_stats(mem_stats stats);
    static void set_process_info(const proc_info &process_info);
    static void set_thread_info(const proc_info &thread_info);
};

template <> struct std::formatter<CXLController> {
    // Parse function to handle any format specifiers (if needed)
    constexpr auto parse(std::format_parse_context &ctx) -> decltype(ctx.begin()) {
        // If you have specific format specifiers, parse them here
        // For simplicity, we'll ignore them and return the end iterator
        return ctx.end();
    }

    // Format function to output the Monitors data

    template <typename FormatContext>
    auto format(const CXLController &controller, FormatContext &ctx) const -> decltype(ctx.out()) {
        std::string result;

        // 首先打印控制器自身的计数器信息
        result += std::format("CXLController:\n");
        // iterate through the topology map
        uint64_t total_capacity = 0;

        std::function<void(const CXLSwitch *)> dfs_capacity = [&](const CXLSwitch *node) {
            if (!node)
                return;

            // Traverse expanders and sum their capacity
            for (const auto *expander : node->expanders) {
                if (expander) {
                    total_capacity += expander->capacity;
                }
            }

            // Recur for all connected switches
            for (const auto *sw : node->switches) {
                dfs_capacity(sw); // Proper recursive call
            }
        };
        dfs_capacity(&controller);

        result += std::format("Total system memory capacity: {}GB\n", total_capacity);

        result += std::format("  Page Type: {}\n", [](page_type pt) {
            switch (pt) {
            case CACHELINE:
                return "CACHELINE";
            case PAGE:
                return "PAGE";
            case HUGEPAGE_2M:
                return "HUGEPAGE_2M";
            case HUGEPAGE_1G:
                return "HUGEPAGE_1G";
            default:
                return "UNKNOWN";
            }
        }(controller.page_type_));

        // 打印全局计数器
        result += std::format("  Global Counter:\n");
        result += std::format("    Local: {}\n", controller.counter.local);
        result += std::format("    Remote: {}\n", controller.counter.remote);
        result += std::format("    HITM: {}\n", controller.counter.hitm);

        // 打印拓扑结构（交换机和端点）
        result += "Topology:\n";

        // 递归打印每个交换机
        std::function<void(const CXLSwitch *, int)> print_switch = [&result, &print_switch](const CXLSwitch *sw,
                                                                                            int depth) {
            std::string indent(depth * 2, ' ');

            // 打印交换机事件计数
            result += std::format("{}Switch:\n", indent);
            result += std::format("{}  Events:\n", indent);
            result += std::format("{}    Load: {}\n", indent, sw->counter.load);
            result += std::format("{}    Store: {}\n", indent, sw->counter.store);
            result += std::format("{}    Conflict: {}\n", indent, sw->counter.conflict);

            // 递归打印子交换机
            for (const auto &child : sw->switches) {
                print_switch(child, depth + 1);
            }

            // 打印端点
            for (const auto &endpoint : sw->expanders) {
                result += std::format("{}Expander:\n", indent + "  ");
                result += std::format("{}  Events:\n", indent + "  ");
                result += std::format("{}    Load: {}\n", indent + "  ", endpoint->counter.load);
                result += std::format("{}    Store: {}\n", indent + "  ", endpoint->counter.store);
                result += std::format("{}    Migrate: {}\n", indent + "  ", endpoint->counter.migrate);
                result += std::format("{}    Hit Old: {}\n", indent + "  ", endpoint->counter.hit_old);
            }
        };

        // 从控制器开始递归打印
        print_switch(&controller, 0);

        // 打印额外的统计信息
        result += "\nStatistics:\n";
        result += std::format("  Number of Switches: {}\n", controller.num_switches);
        result += std::format("  Number of Endpoints: {}\n", controller.num_end_points);
        result += std::format("  Memory Freed: {} bytes\n", controller.freed);

        return format_to(ctx.out(), "{}", result);
    }
};

extern CXLController *controller;
#endif // CXLMEMSIM_CXLCONTROLLER_H
