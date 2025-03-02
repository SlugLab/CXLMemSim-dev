/*
 * CXLMemSim policy
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *      Brian Zhao
 *  SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#ifndef CXLMEMSIM_POLICY_H
#define CXLMEMSIM_POLICY_H
#include "cxlcontroller.h"
#include "cxlendpoint.h"
#include "helper.h"
#include <map>

// Saturate Local 90% and start interleave accrodingly the remote with topology
// Say 3 remote, 2 200ns, 1 400ns, will give 40% 40% 20%
class InterleavePolicy : public AllocationPolicy {

public:
    InterleavePolicy() = default;
    int last_remote = 0;
    int all_size = 0;
    std::vector<double> percentage;
    int compute_once(CXLController *) override;
};

class NUMAPolicy : public AllocationPolicy {

public:
    NUMAPolicy() = default;
    std::vector<double> latency_scores; // 存储每个节点的延迟评分
    int compute_once(CXLController *) override;
};

class HeatAwareMigrationPolicy : public MigrationPolicy {
public:
    std::unordered_map<uint64_t, uint64_t> access_count;  // 地址到访问次数的映射
    uint64_t hot_threshold;  // 热点数据阈值

    HeatAwareMigrationPolicy(uint64_t threshold = 100) : hot_threshold(threshold) {}

    // 记录访问以跟踪热点数据
    void record_access(uint64_t addr) {
        access_count[addr]++;
    }

    int compute_once(CXLController* controller) override {
        // 更新访问计数
        for (const auto& [timestamp, info] : controller->occupation) {
            record_access(info.address);
        }

        // 检查是否有热点数据需要迁移
        auto migration_list = get_migration_list(controller);
        return migration_list.empty() ? 0 : 1;
    }

    std::vector<std::tuple<uint64_t, uint64_t>> get_migration_list(CXLController* controller) {
        std::vector<std::tuple<uint64_t, uint64_t>> to_migrate;

        // 定义页面大小
        int per_size;
        switch (controller->page_type_) {
        case CACHELINE: per_size = 64; break;
        case PAGE: per_size = 4096; break;
        case HUGEPAGE_2M: per_size = 2 * 1024 * 1024; break;
        case HUGEPAGE_1G: per_size = 1024 * 1024 * 1024; break;
        };

        // 检查热点数据
        for (const auto& [addr, count] : access_count) {
            if (count > hot_threshold) {
                // 添加到迁移列表
                to_migrate.emplace_back(addr, per_size);
            }
        }
        return to_migrate;
    }
};

class HugePagePolicy : public PagingPolicy {
public:
    HugePagePolicy() = default;
    int compute_once(CXLController *) override;
};

class FIFOPolicy : public CachingPolicy {
public:
    FIFOPolicy() = default;
    int compute_once(CXLController *) override;
};

// 基于访问频率的后向失效策略
class FrequencyBasedInvalidationPolicy : public CachingPolicy {
public:
    std::unordered_map<uint64_t, uint64_t> access_count;  // 地址到访问计数的映射
    uint64_t access_threshold;  // 访问阈值
    uint64_t last_cleanup;      // 上次清理时间戳
    uint64_t cleanup_interval;  // 清理间隔

    explicit FrequencyBasedInvalidationPolicy(uint64_t threshold = 100, uint64_t interval = 10000000)
        : access_threshold(threshold), last_cleanup(0), cleanup_interval(interval) {}

    bool should_cache(uint64_t addr, uint64_t timestamp) ;
    bool should_invalidate(uint64_t addr, uint64_t timestamp);

    std::vector<uint64_t> get_invalidation_list(CXLController* controller) ;

    int compute_once(CXLController* controller) override;
};
#endif // CXLMEMSIM_POLICY_H
