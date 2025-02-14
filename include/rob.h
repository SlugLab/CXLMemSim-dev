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

#include "cxlcontroller.h"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Structure holding the minimal info we want.
struct InstructionGroup {
    unsigned long long address; // optional
    unsigned long long cycleCount = 0;
    unsigned long long fetchTimestamp = 0;
    unsigned long long retireTimestamp = 0;
    std::string instruction; // combined opcode and text
};

class Rob {
public:
    explicit Rob(CXLController *controller, size_t max_size);
    ~Rob();

    // Issue a new instruction. Returns true if issued, false if stalled (because full).
    bool issue(const InstructionGroup &ins);

    // Retire the oldest instruction. (In real usage, you'd call this when the 
    // oldest instruction is actually completed; you can also just pop it to simulate.)
    void retire();

    // For demonstration, evict the LRU or oldest entry.
    void evict_lru();

    // The total number of stalls due to a full ROB
    size_t stall_count() const { return stallCount_; }

private:
    // If you want to track requests, address mapping, etc.
    // For now, we just maintain a queue of InstructionGroup.
    std::deque<InstructionGroup> queue_;

    // Max size of the ROB
    size_t maxSize_;

    // The CXL controller pointer, used to update topology when we issue an instruction
    CXLController *controller_;

    // Count how many times we stall because the ROB is full
    size_t stallCount_ = 0;
};

#endif // CXLMEMSIM_ROB_H
