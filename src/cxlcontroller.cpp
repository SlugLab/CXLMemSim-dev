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

#include "cxlcontroller.h"
#include "bpftimeruntime.h"
#include "lbr.h"
#include "monitor.h"

void CXLController::insert_end_point(CXLMemExpander *end_point) { this->cur_expanders.emplace_back(end_point); }

void CXLController::construct_topo(std::string_view newick_tree) {
    auto tokens = tokenize(newick_tree);
    std::vector<CXLSwitch *> stk;
    stk.push_back(this);
    for (const auto &token : tokens) {
        if (token == "(" && num_switches == 0) {
            num_switches++;
        } else if (token == "(") {
            /** if is not on the top level */
            auto cur = new CXLSwitch(num_switches++);
            stk.back()->switches.push_back(cur);
            stk.push_back(cur);
        } else if (token == ")") {
            if (!stk.empty()) {
                stk.pop_back();
            } else {
                throw std::invalid_argument("Unbalanced number of parentheses");
            }
        } else if (token == ",") {
        } else {
            stk.back()->expanders.emplace_back(this->cur_expanders[atoi(token.c_str()) - 1]);
            num_end_points++;
        }
    }
}

CXLController::CXLController(std::array<Policy *, 4> p, int capacity, page_type page_type_, int epoch,
                             double dramlatency)
    : CXLSwitch(0), capacity(capacity), allocation_policy(dynamic_cast<AllocationPolicy *>(p[0])),
      migration_policy(dynamic_cast<MigrationPolicy *>(p[1])), paging_policy(dynamic_cast<PagingPolicy *>(p[2])),
      caching_policy(dynamic_cast<CachingPolicy *>(p[3])), page_type_(page_type_), dramlatency(dramlatency) {
    for (auto switch_ : this->switches) {
        switch_->set_epoch(epoch);
    }
    for (auto expander : this->expanders) {
        expander->set_epoch(epoch);
    }
    // TODO get LRU wb
    // TODO BW type series
    // TODO cache
    // TODO back invalidation
    // deferentiate R/W for multi reader multi writer
}

double CXLController::calculate_latency(const std::vector<std::tuple<int, int>> &elem, double dramlatency) {
    return CXLSwitch::calculate_latency(elem, dramlatency);
}

double CXLController::calculate_bandwidth(const std::vector<std::tuple<int, int>> &elem) {
    return CXLSwitch::calculate_bandwidth(elem);
}

std::string CXLController::output() {
    std::string res;
    if (!this->switches.empty()) {
        res += "(";
        res += this->switches[0]->output();
        for (size_t i = 1; i < this->switches.size(); ++i) {
            res += ",";
            res += this->switches[i]->output();
        }
        res += ")";
    }
    if (!this->expanders.empty()) {
        res += "(";
        res += this->expanders[0]->output();
        for (size_t i = 1; i < this->expanders.size(); ++i) {
            res += ",";
            res += this->expanders[i]->output();
        }
        res += ")";
    }
    return res;
}

void CXLController::set_stats(mem_stats stats) {
    // SPDLOG_INFO("stats: {} {} {} {} {}", stats.total_allocated, stats.total_freed, stats.current_usage,
    // stats.allocation_count, stats.free_count);
    if (stats.total_allocated < 100000000000) {
        for (auto switch_ : this->switches) {
            switch_->free_stats((stats.total_freed - this->freed) / num_end_points);
        }
        for (auto expander_ : this->expanders) {
            expander_->free_stats((stats.total_freed - this->freed) / num_end_points);
        }
    }
    if (stats.total_freed > this->freed)
        this->freed = stats.total_freed;
}

void CXLController::set_process_info(const proc_info &process_info) {
    monitors->enable(process_info.current_pid, process_info.current_tid, true, 1000, 0);
}

void CXLController::set_thread_info(const proc_info &thread_info) {
    monitors->enable(thread_info.current_pid, thread_info.current_tid, false, 0, 0);
}

void CXLController::delete_entry(uint64_t addr, uint64_t length) { CXLSwitch::delete_entry(addr, length); }

void CXLController::insert_one(thread_info &t_info, lbr &lbr) {
    auto &rob = t_info.rob;
    auto llcm_count = (lbr.flags & LBR_DATA_MASK) >> LBR_DATA_SHIFT;
    auto ins_count = (lbr.flags & LBR_INS_MASK) >> LBR_INS_SHIFT;

    // ——在这里插入 ring_buffer，表示我们接收到了一个新的 lbr
    ring_buffer.push(lbr);

    for (int i = 0; i < llcm_count; i++) {
        rob.m_count[t_info.llcm_type.front()]++;
        t_info.llcm_type_rob.push(t_info.llcm_type.front());
        t_info.llcm_type.pop();
    }
    rob.llcm_count += llcm_count;
    rob.ins_count += ins_count;

    while (rob.ins_count > ROB_SIZE) {
        auto old_lbr = ring_buffer.front();
        llcm_count = (old_lbr.flags & LBR_DATA_MASK) >> LBR_DATA_SHIFT;
        ins_count = (old_lbr.flags & LBR_INS_MASK) >> LBR_INS_SHIFT;

        rob.ins_count -= ins_count;
        rob.llcm_count -= llcm_count;
        rob.llcm_base += llcm_count;

        for (int i = 0; i < llcm_count; i++) {
            rob.m_count[t_info.llcm_type_rob.front()]--;
            t_info.llcm_type_rob.pop();
        }
        ring_buffer.pop();
    }
}

int CXLController::insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) {
    auto res = true;
    auto &t_info = thread_map[tid];

    // 计算时间步长,使timestamp均匀分布
    uint64_t time_step = 0;
    if (index > last_index) {
        time_step = (timestamp - last_timestamp) / (index - last_index);
    }
    uint64_t current_timestamp = last_timestamp;

    for (int i = last_index; i < index; i++) {
        // 更新当前时间戳
        current_timestamp += time_step;

        auto numa_policy = allocation_policy->compute_once(this);
        if (numa_policy == -1) {
            this->occupation.emplace(current_timestamp, phys_addr);
            this->counter.inc_local();
            t_info.llcm_type.push(0);
            continue;
        }
        this->counter.inc_remote();
        for (auto switch_ : this->switches) {
            res &= switch_->insert(current_timestamp, tid, phys_addr, virt_addr, numa_policy);
        }
        for (auto expander_ : this->expanders) {
            res &= expander_->insert(current_timestamp, tid, phys_addr, virt_addr, numa_policy);
        }
        t_info.llcm_type.push(1);
    }

    // 更新最后的索引和时间戳
    last_index = index;
    last_timestamp = timestamp;
    return res; // 返回实际的结果而不是固定的true
}
int CXLController::insert(uint64_t timestamp, uint64_t tid, lbr lbrs[32], cntr counters[32]) {
    for (int i = 0; i < 32; i++) {
        if (!lbrs[i].from) {
            break;
        }
        insert_one(thread_map[tid], lbrs[i]);
        // TODO calculate delay
        // timestamp
    }
    auto all_access = get_access(timestamp); // get the current branch access?
    latency_lat += calculate_latency(all_access, dramlatency); // insert once
    bandwidth_lat += calculate_bandwidth(all_access); // insert once
    return 0;
}
std::vector<std::string> CXLController::tokenize(const std::string_view &s) {
    std::vector<std::string> res;
    std::string tmp;
    for (char c : s) {
        if (c == '(' || c == ')' || c == ':' || c == ',') {
            if (!tmp.empty()) {
                res.emplace_back(std::move(tmp));
            }
            res.emplace_back(1, c);
        } else {
            tmp += c;
        }
    }
    if (!tmp.empty()) {
        res.emplace_back(std::move(tmp));
    }
    return res;
}
std::vector<std::tuple<int, int>> CXLController::get_access(uint64_t timestamp) { return CXLSwitch::get_access(timestamp); }
std::tuple<double, std::vector<uint64_t>> CXLController::calculate_congestion() {
    return CXLSwitch::calculate_congestion();
}
void CXLController::set_epoch(int epoch) { CXLSwitch::set_epoch(epoch); }
