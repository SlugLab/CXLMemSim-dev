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

#ifndef CXLMEMSIM_CXLENDPOINT_H
#define CXLMEMSIM_CXLENDPOINT_H

#include "cxlcounter.h"
#include "helper.h"
#include <list>
#include <queue>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#define ROB_SIZE 512

struct occupation_info {
    uint64_t timestamp;
    uint64_t address;
    uint64_t access_count;
};
struct rob_info {
    std::map<int, int64_t> m_bandwidth, m_count;
    int64_t llcm_base, llcm_count, ins_count;
};
struct thread_info {
    rob_info rob;
    std::queue<int> llcm_type;
    std::queue<int> llcm_type_rob;
};
// Forward declarations
class CXLController;
class CXLEndPoint {
public:
    virtual ~CXLEndPoint() = default;

private:
    virtual void set_epoch(int epoch) = 0;
    virtual void free_stats(double size) = 0;
    virtual void delete_entry(uint64_t addr, uint64_t length) = 0;
    virtual double calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem,
                                     double dramlatency) = 0; // traverse the tree to calculate the latency
    virtual double calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) = 0;
    virtual int insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr,
                       int index) = 0; // 0 not this endpoint, 1 store, 2 load, 3 prefetch
    virtual std::vector<std::tuple<uint64_t, uint64_t>> get_access(uint64_t timestamp) = 0;
};

class CXLMemExpander : public CXLEndPoint {
public:
    EmuCXLBandwidth bandwidth{};
    EmuCXLLatency latency{};
    uint64_t capacity;

    std::vector<occupation_info> occupation; // timestamp, pa
    CXLMemExpanderEvent counter{};
    CXLMemExpanderEvent last_counter{};
    mutable std::shared_mutex occupationMutex_; // 使用共享互斥锁允许多个读取者
    // LRUCache lru_cache;
    // tlb map and paging map -> invalidate
    int last_read = 0;
    int last_write = 0;
    double last_latency = 0.;
    int epoch = 0;
    uint64_t last_timestamp = 0;
    int id = -1;
    CXLMemExpander(int read_bw, int write_bw, int read_lat, int write_lat, int id, int capacity);
    std::vector<std::tuple<uint64_t, uint64_t>> get_access(uint64_t timestamp) override;
    void set_epoch(int epoch) override;
    void free_stats(double size) override;
    int insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) override;
    double calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem,
                             double dramlatency) override; // traverse the tree to calculate the latency
    double calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) override;
    void delete_entry(uint64_t addr, uint64_t length) override;
};
class CXLSwitch : public CXLEndPoint {
public:
    std::vector<CXLMemExpander *> expanders{};
    std::vector<CXLSwitch *> switches{};
    CXLSwitchEvent counter{};
    int id = -1;
    int epoch = 0;
    uint64_t last_timestamp = 0;
    // TODO get the approximate congestion and target done time
    std::unordered_map<uint64_t, uint64_t> timeseries_map;

    double congestion_latency = 90; // 200ns is the latency of the switch
    explicit CXLSwitch(int id);
    std::vector<std::tuple<uint64_t, uint64_t>> get_access(uint64_t timestamp) override;
    double calculate_latency(const std::vector<std::tuple<uint64_t, uint64_t>> &elem,
                             double dramlatency) override; // traverse the tree to calculate the latency
    double calculate_bandwidth(const std::vector<std::tuple<uint64_t, uint64_t>> &elem) override;
    double get_endpoint_rob_latency(CXLMemExpander* endpoint,
                                  const std::vector<std::tuple<uint64_t, uint64_t>>& accesses,
                                  const thread_info& t_info,
                                  double dramlatency);
    int insert(uint64_t timestamp, uint64_t tid, uint64_t phys_addr, uint64_t virt_addr, int index) override;
    void delete_entry(uint64_t addr, uint64_t length) override;
    virtual std::tuple<double, std::vector<uint64_t>> calculate_congestion();
    void set_epoch(int epoch) override;
    void free_stats(double size) override;
};

#endif // CXLMEMSIM_CXLENDPOINT_H
