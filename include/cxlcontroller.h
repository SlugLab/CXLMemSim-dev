//
// Created by victoryang00 on 1/14/23.
//

#ifndef CXL_MEM_SIMULATOR_CXLCONTROLLER_H
#define CXL_MEM_SIMULATOR_CXLCONTROLLER_H

#include "cxlcounter.h"
#include "cxlendpoint.h"
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

class CXLController;
class Policy {
public:
    Policy();
    virtual int compute_once(CXLController *) = 0;
};
class CXLController : CXLSwitch {
public:
    std::vector<CXLMemExpander *> cur_expanders{};
    int capacity; // GB
    Policy *policy;
    CXLCounter counter;
    std::map<uint64_t, uint64_t> occupation;
    std::map<uint64_t, uint64_t> va_pa_map;
    bool is_page; // percentage
    int num_switches = 0;
    /** LRU Cache for wb and timeseries map */
    std::list<uint64_t> lru_list;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lru_map;
    std::unordered_map<uint64_t, uint64_t> wb_map;
    std::unordered_map<uint64_t, uint64_t> timeseries_map;
    CXLController(Policy *policy, int capacity, bool is_page, int epoch);
    void construct_topo(std::string_view newick_tree);
    void insert_end_point(CXLMemExpander *end_point);
    std::vector<std::string> tokenize(const std::string_view &s);
    std::tuple<double, std::vector<uint64_t>> calculate_congestion() override;
    void set_epoch(int epoch) override;
    std::tuple<int, int> get_all_access() override;
    double calculate_latency(LatencyPass elem); // traverse the tree to calculate the latency
    double calculate_bandwidth(BandwidthPass elem);
    int insert(uint64_t timestamp, uint64_t phys_addr, uint64_t virt_addr, int index) override;
    void delete_entry(uint64_t addr, uint64_t length) override;
    std::string output() override;
};

#endif // CXL_MEM_SIMULATOR_CXLCONTROLLER_H
