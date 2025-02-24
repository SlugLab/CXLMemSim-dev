/*
 * CXLMemSim endpoint
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *      Brian Zhao
 *  SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "cxlendpoint.h"
#include <random>

CXLMemExpander::CXLMemExpander(int read_bw, int write_bw, int read_lat, int write_lat, int id, int capacity)
    : capacity(capacity), id(id) {
    this->bandwidth.read = read_bw;
    this->bandwidth.write = write_bw;
    this->latency.read = read_lat;
    this->latency.write = write_lat;
}
// 修改CXLMemExpander的calculate_latency函数
double CXLMemExpander::calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem, double dramlatency) {
    if (elem.empty()) {
        return 0.0;
    }

    double total_latency = 0.0;
    size_t access_count = 0;

    for (const auto &[timestamp, addr] : elem) {
        // 检查是否是本endpoint的访问
        bool is_local_access = false;
        for (const auto &occ : occupation) {
            if (occ.address == addr) {
                is_local_access = true;
                break;
            }
        }

        if (!is_local_access)
            continue;

        // 基础延迟计算
        double current_latency = (this->latency.read + this->latency.write) / 2.0;

        // 考虑DRAM延迟影响
        current_latency += dramlatency * 0.1;

        total_latency += current_latency;
        access_count++;
    }

    return access_count > 0 ? total_latency / access_count : 0.0;
}
double CXLMemExpander::calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) {
    if (elem.empty()) {
        return 0.0;
    }

    // 获取20ms时间窗口内的访问
    uint64_t current_time = std::get<0>(elem.back());
    uint64_t window_start = current_time - 20000000; // 20ms = 20,000,000ns

    // 计算时间窗口内的访问次数
    size_t access_count = 0;
    uint64_t total_data = 0;
    constexpr uint64_t CACHE_LINE_SIZE = 64; // 假设缓存行大小为64字节

    for (const auto &[timestamp, addr] : elem) {
        if (timestamp >= window_start) {
            access_count++;
            total_data += CACHE_LINE_SIZE;
        }
    }

    // 计算带宽 (GB/s)
    // 带宽 = (总数据量 / 时间窗口)
    double time_window_seconds = 0.02; // 20ms = 0.02s
    double bandwidth_gbps = (total_data / time_window_seconds) / (1024.0 * 1024.0 * 1024.0);

    // 确保带宽不超过设备限制
    double max_bandwidth = this->bandwidth.read + this->bandwidth.write;
    return std::min(bandwidth_gbps, max_bandwidth);
}
void CXLMemExpander::delete_entry(uint64_t addr, uint64_t length) {
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (it->address == addr) {
            // it = occupation.erase(it);
            it->access_count++;
            it->timestamp = last_timestamp;
        } else {
            ++it;
        }
    }
    this->counter.inc_load();
    // kernel mode access
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (it->address >= addr && it->address <= addr + length) {
            if (it->address == addr) {
                // it = occupation.erase(it);
                it->access_count++;
                it->timestamp = last_timestamp;
            } else {
                ++it;
            }
        }
        this->counter.inc_load();
    }
}

int CXLMemExpander::insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) {
    if (index == this->id) {
        last_timestamp = last_timestamp > timestamp ? last_timestamp : timestamp; // Update the last timestamp
        // Check if the address is already in the map)
        if (phys_addr != 0) {
            for (auto it = this->occupation.cbegin(); it != this->occupation.cend(); it++) {
                if (it->address == phys_addr) {
                    this->occupation.erase(it);
                    this->occupation.emplace_back(timestamp, phys_addr, 0);
                    this->counter.inc_load();
                    return 2;
                }
            }
            this->occupation.emplace_back(timestamp, phys_addr, 0);
            this->counter.inc_store();
            return 1;
        }
        this->counter.inc_store();
        return 1;
    }
    return 0;
}
std::vector<std::tuple<uint64_t, uint64_t>> CXLMemExpander::get_access(uint64_t timestamp) {
    last_counter = CXLMemExpanderEvent(counter);
    // Iterate the map within the last 100ns
    auto res = occupation |
               std::views::filter([timestamp](const auto &it) { return it.timestamp > timestamp - 1000; }) |
               std::views::transform([](const auto &it) { return std::make_tuple(it.timestamp, it.address); }) |
               std::ranges::to<std::vector>();
    return res;
}
void CXLMemExpander::set_epoch(int epoch) { this->epoch = epoch; }
void CXLMemExpander::free_stats(double size) {
    // 随机删除
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (dis(gen) == 1) {
            it = occupation.erase(it);
        } else {
            ++it;
        }
    }
}

void CXLSwitch::delete_entry(uint64_t addr, uint64_t length) {
    for (auto &expander : this->expanders) {
        expander->delete_entry(addr, length);
    }
    for (auto &switch_ : this->switches) {
        switch_->delete_entry(addr, length);
    }
}
CXLSwitch::CXLSwitch(int id) : id(id) {}
double CXLSwitch::calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem, double dramlatency) {
    double lat = 0.0;
    for (auto &expander : this->expanders) {
        lat += expander->calculate_latency(elem, dramlatency);
    }
    for (auto &switch_ : this->switches) {
        lat += switch_->calculate_latency(elem, dramlatency);
    }
    return lat;
}
double CXLSwitch::calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) {
    double bw = 0.0;
    for (auto &expander : this->expanders) {
        bw += expander->calculate_bandwidth(elem);
    }
    for (auto &switch_ : this->switches) {
        bw += switch_->calculate_bandwidth(elem);
    }
    // time series
    return bw;
}
double CXLSwitch::get_endpoint_rob_latency(CXLMemExpander *endpoint,
                                           const std::vector<std::tuple<uint64_t, uint64_t>> &accesses,
                                           const thread_info &t_info, double dramlatency) {
    const auto &rob = t_info.rob; // 使用const引用

    // 计算当前endpoint的基础延迟
    double base_latency = (endpoint->latency.read + endpoint->latency.write) / 2.0;

    // 计算ROB相关指标
    double llc_miss_ratio = (rob.ins_count > 0) ? static_cast<double>(rob.llcm_count) / rob.ins_count : 0.0;

    // 使用find替代operator[]来访问const map
    double remote_ratio = 0.0;
    auto count_0 = rob.m_count.find(0);
    auto count_1 = rob.m_count.find(1);

    if (count_0 != rob.m_count.end() && count_1 != rob.m_count.end()) {
        int64_t local_count = count_0->second;
        int64_t remote_count = count_1->second;
        if (local_count + remote_count > 0) {
            remote_ratio = static_cast<double>(remote_count) / (local_count + remote_count);
        }
    }

    double total_latency = 0.0;
    size_t access_count = 0;

    for (const auto &[timestamp, addr] : accesses) {
        // 检查该访问是否属于这个endpoint
        bool is_endpoint_access = false;
        for (const auto &occ : endpoint->occupation) {
            if (occ.address == addr) {
                is_endpoint_access = true;
                break;
            }
        }

        if (!is_endpoint_access)
            continue;

        double current_latency = base_latency;
        access_count++;

        // ROB拥塞调整
        if (rob.ins_count >= ROB_SIZE * 0.8) {
            double rob_penalty = 1.0 + (llc_miss_ratio * 0.5);
            current_latency *= rob_penalty;
        }

        // 远程访问影响
        auto remote_count = rob.m_count.find(1);
        if (remote_count != rob.m_count.end() && remote_count->second > 0) {
            current_latency *= (1.0 + remote_ratio * 0.3);
        }

        // 考虑DRAM延迟
        current_latency += dramlatency * (remote_ratio + 0.1);

        total_latency += current_latency;
    }

    return access_count > 0 ? total_latency / access_count : 0.0;
}

std::tuple<double, std::vector<uint64_t>> CXLSwitch::calculate_congestion() {
    double latency = 0.0;
    std::vector<uint64_t> congestion;
    for (auto &switch_ : this->switches) {
        auto [lat, con] = switch_->calculate_congestion();
        latency += lat;
        congestion.insert(congestion.end(), con.begin(), con.end());
    }
    for (auto &expander : this->expanders) {
        for (auto &it : expander->occupation) {
            // every epoch
            if (it.timestamp > this->last_timestamp - epoch * 1000) {
                congestion.push_back(it.timestamp);
            }
        }
    }
    sort(congestion.begin(), congestion.end());
    for (auto it = congestion.begin(); it != congestion.end(); ++it) {
        if (*(it + 1) - *it < 2000) { // if less than 20ns
            latency += this->congestion_latency;
            this->counter.inc_conflict();
            if (it + 1 == congestion.end()) {
                break;
            }
            congestion.erase(it);
        }
    }
    return std::make_tuple(latency, congestion);
}
std::vector<std::tuple<uint64_t, uint64_t>> CXLSwitch::get_access(uint64_t timestamp) {
    std::vector<std::tuple<uint64_t, uint64_t>> res;
    size_t total_size = 0;
    for (const auto &expander : expanders) {
        total_size += expander->get_access(timestamp).size();
    }
    for (const auto &switch_ : switches) {
        total_size += switch_->get_access(timestamp).size();
    }
    res.reserve(total_size);

    // 直接使用 insert 合并结果
    for (auto &expander : expanders) {
        auto tmp = expander->get_access(timestamp);
        res.insert(res.end(), tmp.begin(), tmp.end());
    }
    for (auto &switch_ : switches) {
        auto tmp = switch_->get_access(timestamp);
        res.insert(res.end(), tmp.begin(), tmp.end());
    }

    return res;
}
void CXLSwitch::set_epoch(int epoch) { this->epoch = epoch; }
void CXLSwitch::free_stats(double size) {
    // 随机删除
    for (auto &expander : this->expanders) {
        expander->free_stats(size);
    }
}

int CXLSwitch::insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) {
    // 简单示例：依次调用下属的 expander 和 switch
    SPDLOG_DEBUG("CXLSwitch insert phys_addr={}, virt_addr={}, index={} for switch id:{}", phys_addr, virt_addr, index,
                 this->id);

    for (auto &expander : this->expanders) {
        // 在每个 expander 上尝试插入
        int ret = expander->insert(timestamp, tid, phys_addr, virt_addr, index);
        if (ret == 1) {
            this->counter.inc_store();
            return 1;
        }
        if (ret == 2) {
            this->counter.inc_load();
            return 2;
        }
    }
    // 如果没有合适的 expander，就尝试下属的 switch
    for (auto &sw : this->switches) {
        int ret = sw->insert(timestamp, tid, phys_addr, virt_addr, index);
        if (ret == 1) {
            this->counter.inc_store();
            return 1;
        }
        if (ret == 2) {
            this->counter.inc_load();
            return 2;
        }
    }
    // 如果都处理不了，就返回0
    return 0;
}
