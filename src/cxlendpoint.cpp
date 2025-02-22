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
double CXLMemExpander::calculate_latency(const std::vector<std::tuple<int, int>> &elem, double dramlatency) {

    return 60;
}
double CXLMemExpander::calculate_bandwidth(const std::vector<std::tuple<int, int>> &elem) {
    // Iterate the map within the last 20ms

    return this->bandwidth.read + this->bandwidth.write;
}
void CXLMemExpander::delete_entry(uint64_t addr, uint64_t length) {
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (it->second == addr) {
            it = occupation.erase(it);
        } else {
            ++it;
        }
    }
    this->counter.inc_load();
    // kernel mode access
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (it->second >= addr && it->second <= addr + length) {
            if (it->second == addr) {
                it = occupation.erase(it);
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
                if ((*it).second == phys_addr) {
                    this->occupation.erase(it);
                    this->occupation.emplace(timestamp, phys_addr);
                    this->counter.inc_load();
                    return 2;
                }
            }
            this->occupation.emplace(timestamp, phys_addr);
            this->counter.inc_store();
            return 1;
        } // kernel mode access
        for (auto it = this->occupation.cbegin(); it != this->occupation.cend(); it++) {
            if ((*it).second == virt_addr) {
                this->occupation.erase(it);
                this->occupation.emplace(timestamp, virt_addr);
                this->counter.inc_load();
                return 2;
            }
        }

        this->occupation.emplace(timestamp, virt_addr);
        this->counter.inc_store();
        return 1;
    }
    return 0;
}
std::string CXLMemExpander::output() { return std::format("CXLMemExpander {}", this->id); }
std::vector<std::tuple<int, int>> CXLMemExpander::get_access(uint64_t timestamp) {
    last_counter = CXLMemExpanderEvent(counter);
    std::vector<std::tuple<int, int>> res;
    // Iterate the map within the last 100ns
    for (auto it = occupation.begin(); it != occupation.end();) {
        if (it->first > timestamp - 100) {
            res.push_back(std::make_tuple(it->first, it->second));
        } else {
            ++it;
        }
    }
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

std::string CXLSwitch::output() {
    std::string res = std::format("CXLSwitch {} ", this->id);
    if (!this->switches.empty()) {
        res += "(";
        res += this->switches[0]->output();
        for (size_t i = 1; i < this->switches.size(); ++i) {
            res += ",";
            res += this->switches[i]->output();
        }
        res += ")";
    } else if (!this->expanders.empty()) {
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
void CXLSwitch::delete_entry(uint64_t addr, uint64_t length) {
    for (auto &expander : this->expanders) {
        expander->delete_entry(addr, length);
    }
    for (auto &switch_ : this->switches) {
        switch_->delete_entry(addr, length);
    }
}
CXLSwitch::CXLSwitch(int id) : id(id) {}
double CXLSwitch::calculate_latency(const std::vector<std::tuple<int, int>> &elem, double dramlatency) {
    double lat = 0.0;
    for (auto &expander : this->expanders) {
        lat += expander->calculate_latency(elem, dramlatency);
    }
    for (auto &switch_ : this->switches) {
        lat += switch_->calculate_latency(elem, dramlatency);
    }
    return lat;
}
double CXLSwitch::calculate_bandwidth(const std::vector<std::tuple<int, int>> &elem) {
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
            if (it.first > this->last_timestamp - epoch * 1e3) {
                congestion.push_back(it.first);
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
std::vector<std::tuple<int, int>> CXLSwitch::get_access(uint64_t timestamp) {
    auto res = std::vector<std::tuple<int, int>>();
    for (auto &expander : this->expanders) {
        auto tmp = expander->get_access(timestamp);
        res.insert(res.end(), tmp.begin(), tmp.end());
    }
    for (auto &switch_ : this->switches) {
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
