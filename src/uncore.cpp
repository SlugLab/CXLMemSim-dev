//
// Created by victoryang00 on 1/12/23.
//

#include "uncore.h"
Uncore::Uncore(const uint32_t unc_idx, PerfConfig *perf_config) {
    this->perf[0] = init_uncore_perf(-1, unc_idx, perf_config->cha_llc_write_back_config,
                                     perf_config->cha_llc_write_back_config1, perf_config->path_format_cha_type);
    this->perf[1] =
        init_uncore_perf(-1, unc_idx, perf_config->cpu_bandwidth_read_config, 0, perf_config->path_format_cha_type);
    this->perf[2] =
        init_uncore_perf(-1, unc_idx, perf_config->cpu_bandwidth_write_config, 0, perf_config->path_format_cha_type);
    this->perf[3] =
        init_uncore_perf(-1, unc_idx, perf_config->cpu_llcl_hits_config, 0, perf_config->path_format_cha_type);
}

int Uncore::read_cha_elems(struct CHAElem *elem) {
    int r;

    r = this->perf[0].read_pmu(&elem->cpu_llc_hits);
    if (r < 0) {
        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    }

    LOG(DEBUG) << fmt::format("llc_hits:{}\n", elem->cpu_llc_hits);
    r = this->perf[1].read_pmu(&elem->cpu_llc_miss);
    if (r < 0) {
        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    }

    LOG(DEBUG) << fmt::format("llc_miss:{}\n", elem->cpu_llc_miss);
    r = this->perf[2].read_pmu(&elem->cpu_read_bandwidth);
    if (r < 0) {
        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    }

    LOG(DEBUG) << fmt::format("read_bandwidth:{}\n", elem->cpu_read_bandwidth);
    r = this->perf[3].read_pmu(&elem->cpu_llc_wb);
    if (r < 0) {
        LOG(ERROR) << fmt::format("perf_read_pmu failed.\n");
    }

    LOG(DEBUG) << fmt::format("llc_wb:{}\n", elem->cpu_llc_wb);

    return r;
}
