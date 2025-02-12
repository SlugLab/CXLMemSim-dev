/*
 * CXLMemSim rob
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *      Brian Zhao
 *  SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#ifndef CXLMEMSIM_ROB_H
#define CXLMEMSIM_ROB_H

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cxlcontroller.h"

class Rob {
    public:
        Rob();
        ~Rob();
        void add_request(uint64_t addr, bool is_write);
        void evict_lru();
        CXLMemController *controller;
        std::deque<uint64_t> lru;
        std::unordered_map<uint64_t, uint64_t> addr_to_lru_idx;
        std::unordered_map<uint64_t, uint64_t> lru_idx_to_addr;
        std::unordered_map<uint64_t, uint64_t> addr_to_req_type;
};


#endif // CXLMEMSIM_ROB_H