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

class MGLRUPolicy : public MigrationPolicy {
public:
    MGLRUPolicy() = default;
    int compute_once(CXLController *) override;
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
private:
    std::unordered_map<uint64_t, uint64_t> access_count;  // 地址到访问计数的映射
    uint64_t access_threshold;  // 访问阈值
    uint64_t last_cleanup;      // 上次清理时间戳
    uint64_t cleanup_interval;  // 清理间隔

public:
    explicit FrequencyBasedInvalidationPolicy(uint64_t threshold = 100, uint64_t interval = 10000000)
        : access_threshold(threshold), last_cleanup(0), cleanup_interval(interval) {}

    bool should_cache(uint64_t addr, uint64_t timestamp) {
        // 记录访问
        access_count[addr]++;
        return true; // 总是缓存
    }

    bool should_invalidate(uint64_t addr, uint64_t timestamp) {
        // 根据访问频率决定是否应该失效
        auto it = access_count.find(addr);
        if (it != access_count.end()) {
            return it->second < access_threshold;
        }
        return false;
    }

    std::vector<uint64_t> get_invalidation_list(CXLController* controller) {
        std::vector<uint64_t> to_invalidate;

        // 遍历缓存查找低频访问的地址
        for (const auto& [addr, entry] : controller->lru_cache.cache) {
            if (should_invalidate(addr, 0)) {
                to_invalidate.push_back(addr);
            }
        }

        // 清理访问计数（周期性）
        uint64_t current_time = controller->last_timestamp;
        if (current_time - last_cleanup > cleanup_interval) {
            access_count.clear();
            last_cleanup = current_time;
        }

        return to_invalidate;
    }

    int compute_once(CXLController* controller) override {
        // 如果有需要失效的地址，返回正数
        return !get_invalidation_list(controller).empty() ? 1 : 0;
    }
};
#endif // CXLMEMSIM_POLICY_H
