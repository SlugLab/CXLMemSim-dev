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

#include "policy.h"
#include <numeric>
MigrationPolicy::MigrationPolicy() = default;
PagingPolicy::PagingPolicy() = default;
CachingPolicy::CachingPolicy() = default;
AllocationPolicy::AllocationPolicy() = default;
InterleavePolicy::InterleavePolicy() = default;
NUMAPolicy::NUMAPolicy() = default;
// If the number is -1 for local, else it is the index of the remote server
int InterleavePolicy::compute_once(CXLController *controller) {
    int per_size;
    switch (controller->page_type_) {
    case CACHELINE:
        per_size = 64;
        break;
    case PAGE:
        per_size = 4096;
        break;
    case HUGEPAGE_2M:
        per_size = 2 * 1024 * 1024;
        break;
    case HUGEPAGE_1G:
        per_size = 1024 * 1024 * 1024;
        break;
    };
    if (controller->occupation.size() * per_size / 1024 / 1024 < controller->capacity * 0.9) {
        return -1;
    } else {
        if (this->percentage.empty()) {
            // Here to compute the distributor statically using geometry average of write latency
            std::vector<double> to_store;
            for (auto &i : controller->cur_expanders) {
                to_store.push_back(1 / i->latency.write);
            }
            for (auto &i : to_store) {
                this->percentage.push_back(int(i / std::accumulate(to_store.begin(), to_store.end(), 0.0) * 10));
            }
            this->all_size = std::accumulate(this->percentage.begin(), this->percentage.end(), 0);
        }
    next:
        last_remote = (last_remote + 1) % all_size;
        int sum, index;
        for (index = 0, sum = 0; sum <= last_remote; index++) { // 5 2 2 to get the next
            sum += this->percentage[index];
            if (sum > last_remote) {
                if (controller->cur_expanders[index]->occupation.size() * per_size / 1024 / 1024 <
                    controller->cur_expanders[index]->capacity) {
                    break;
                } else {
                    /** TODO: capacity bound */
                    goto next;
                }
            }
        }
        return index;
    }
}
int NUMAPolicy::compute_once(CXLController *controller) {
    int per_size;
    // 确定页面大小
    switch (controller->page_type_) {
    case CACHELINE:
        per_size = 64;
        break;
    case PAGE:
        per_size = 4096;
        break;
    case HUGEPAGE_2M:
        per_size = 2 * 1024 * 1024;
        break;
    case HUGEPAGE_1G:
        per_size = 1024 * 1024 * 1024;
        break;
    };

    // 检查本地内存是否有足够空间
    if (controller->occupation.size() * per_size / 1024 / 1024 < controller->capacity * 0.9) {
        return -1;  // 返回-1表示使用本地内存
    }

    // 初始化延迟评分
    if (this->latency_scores.empty()) {
        for (size_t i = 0; i < controller->cur_expanders.size(); i++) {
            // 计算综合延迟分数：考虑读写延迟的加权平均
            double read_weight = 0.7;  // 读操作权重
            double write_weight = 0.3;  // 写操作权重
            double latency_score = 1.0 / (
                read_weight * controller->cur_expanders[i]->latency.read +
                write_weight * controller->cur_expanders[i]->latency.write
            );
            latency_scores.push_back(latency_score);
        }
    }

    // 寻找最佳节点
    int best_node = -1;
    double best_score = -1;

    for (size_t i = 0; i < controller->cur_expanders.size(); i++) {
        // 检查节点是否有足够容量
        if (controller->cur_expanders[i]->occupation.size() * per_size / 1024 / 1024 >=
            controller->cur_expanders[i]->capacity) {
            continue;  // 跳过已满的节点
        }

        // 计算节点评分
        double current_score = latency_scores[i] * (1.0 -
            static_cast<double>(controller->cur_expanders[i]->occupation.size() * per_size) /
            (controller->cur_expanders[i]->capacity * 1024 * 1024));

        // 更新最佳节点
        if (current_score > best_score) {
            best_score = current_score;
            best_node = i;
        }
    }

    // 如果找不到合适的节点，返回第一个未满的节点
    if (best_node == -1) {
        for (size_t i = 0; i < controller->cur_expanders.size(); i++) {
            if (controller->cur_expanders[i]->occupation.size() * per_size / 1024 / 1024 <
                controller->cur_expanders[i]->capacity) {
                return i;
            }
        }
    }

    return best_node;
}