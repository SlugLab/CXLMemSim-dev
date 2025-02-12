#include "rob.h"

Rob::Rob() {
    controller = nullptr;
}


Rob::~Rob() {
    controller = nullptr;
}

void Rob::add_request(uint64_t addr, bool is_write) {
    lru.push_back(addr);
}

void Rob::evict_lru() {
    uint64_t addr = lru.front();
    lru.pop_front();
    addr_to_lru_idx.erase(addr);
    lru_idx_to_addr.erase(0);
}

void Rob::update_lru(uint64_t addr) {
    auto it = addr_to_lru_idx.find(addr);
    if (it != addr_to_lru_idx.end()) {
        lru.erase(it->second);
    }
    lru.push_back(addr);
}

void Rob::update_addr_to_lru_idx(uint64_t addr, uint64_t idx) {
    addr_to_lru_idx[addr] = idx;
}

void Rob::update_lru_idx_to_addr(uint64_t idx, uint64_t addr) {
    lru_idx_to_addr[idx] = addr;
} 

