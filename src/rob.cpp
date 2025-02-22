//
// Created by root on 2/19/25.
//

#include "rob.h"
// 发射指令到ROB
bool Rob::issue(const InstructionGroup &ins) {
    if (queue_.size() >= maxSize_) {
        stallCount_++;
        return false; // ROB已满,停顿
    }

    // 将指令加入ROB尾部
    queue_.push_back(ins);

    // 对于内存访问指令,通知控制器
    if (ins.address != 0) {
        controller_->insert(ins.retireTimestamp, 0, ins.address, 0, 0);
    }

    return true;
}

// 检查指令是否可以提交
bool Rob::canRetire(const InstructionGroup &ins) {
    if (ins.address == 0) {
        return true; // 非内存指令可以直接提交
    }

    // 检查内存访问是否完成
    if (cur_latency == 0) {
        auto allAccess = controller_->get_access(currentCycle_);
        cur_latency = controller_->calculate_latency(allAccess, 80.);
    }
    // SPDLOG_INFO("{}",cur_latency);
    if (currentCycle_ - ins.cycleCount >= cur_latency) {
        cur_latency = 0;
        return true;
    }
    return false;
}

// 提交最老的指令
void Rob::retire() {
    if (queue_.empty()) {
        return;
    }

    auto &oldestIns = queue_.front();
    if (!canRetire(oldestIns)) {
        stallCount_++; // 无法提交,增加停顿
        return;
    }

    // 计算这条指令的实际延迟
    if (oldestIns.address != 0) {
        auto allAccess = controller_->get_access(currentCycle_);
        uint64_t latency = controller_->calculate_latency(allAccess, 80.); // also delete the latency
        totalLatency_ += latency;
    }

    // 提交指令
    queue_.pop_front();
}

// 时钟周期推进
void Rob::tick() {
    currentCycle_++;
    // 尝试提交指令
    retire();
}