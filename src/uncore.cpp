//
// Created by victoryang00 on 1/12/23.
//

#include "uncore.h"
Uncore::Uncore(const uint32_t unc_idx, PerfConfig *perf_config) {
    unsigned long value;
    int r;
    char path[64], buf[32];
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path) - 1, perf_config->path_format_cha_type, unc_idx);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG(ERROR) << fmt::format("open {} failed", path);
        throw std::runtime_error("open");
    }

    memset(buf, 0, sizeof(buf));
    r = read(fd, buf, sizeof(buf) - 1);
    if (r < 0) {
        LOG(ERROR) << fmt::format("read {} failed", fd);
        close(fd);
        throw std::runtime_error("read");
    }
    close(fd);

    value = strtoul(buf, nullptr, 10);
    if (value == ULONG_MAX) {
        LOG(ERROR) << fmt::format("strtoul {} failed", fd);
        throw std::runtime_error("strtoul");
    }
    //    this->perf[0] = init_uncore_perf(-1, unc_idx, std::get<1>(perf_config->cpu_1),
    //    std::get<2>(perf_config->cpu_1), value);
    for (auto const &[k, v] : this->perf | enumerate) {
        v = init_uncore_perf(-1, unc_idx, std::get<1>(perf_config->cpu[k]), std::get<2>(perf_config->cpu[k]), value);
    }
    //    this->perf[1] = init_uncore_perf(-1, unc_idx, std::get<1>(perf_config->cpu_2),
    //    std::get<2>(perf_config->cpu_2), value); this->perf[2] = init_uncore_perf(-1, unc_idx,
    //    std::get<1>(perf_config->cpu_3), std::get<2>(perf_config->cpu_3), value); this->perf[3] = init_uncore_perf(-1,
    //    unc_idx, std::get<1>(perf_config->cpu_4), std::get<2>(perf_config->cpu_4), value);
}

int Uncore::read_cha_elems(struct CHAElem *elem) {
    int r;

    //    r = this->perf[0]->read_pmu(&elem->cpu_llc_hits);
    //    if (r < 0) {
    //        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    //    }
    //
    //    LOG(DEBUG) << fmt::format("llc_hits:{}\n", elem->cpu_llc_hits);
    //    r = this->perf[1]->read_pmu(&elem->cpu_llc_miss);
    //    if (r < 0) {
    //        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    //    }
    //
    //    LOG(DEBUG) << fmt::format("llc_miss:{}\n", elem->cpu_llc_miss);
    //    r = this->perf[2]->read_pmu(&elem->cpu_read_bandwidth);
    //    if (r < 0) {
    //        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    //    }
    //
    //    LOG(DEBUG) << fmt::format("read_bandwidth:{}\n", elem->cpu_read_bandwidth);
    //    r = this->perf[3]->read_pmu(&elem->cpu_llc_wb);
    //    if (r < 0) {
    //        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    //    }
    //
    //    LOG(DEBUG) << fmt::format("llc_wb:{}\n", elem->cpu_llc_wb);
    // for loop get out
    for (auto const &[k, v] : this->perf | enumerate) {
        v->read_pmu(&elem->cha[k]);
    }
    return r;
}
