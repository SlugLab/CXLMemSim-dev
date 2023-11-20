//
// Created by victoryang00 on 1/12/23.
//
#include "uncore.h"
extern Helper helper;
Uncore::Uncore(const uint32_t unc_idx, PerfConfig *perf_config) {
    unsigned long value;
    int r;
    char path[64], buf[32];
    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path) - 1, perf_config->path_format_cha_type.c_str(), unc_idx);

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
        v = init_uncore_perf(-1, (int)unc_idx, std::get<1>(perf_config->cha[k]), std::get<2>(perf_config->cha[k]), value);
    }
    //    this->perf[1] = init_uncore_perf(-1, unc_idx, std::get<1>(perf_config->cpu_2),
    //    std::get<2>(perf_config->cpu_2), value); this->perf[2] = init_uncore_perf(-1, unc_idx,
    //    std::get<1>(perf_config->cpu_3), std::get<2>(perf_config->cpu_3), value); this->perf[3] = init_uncore_perf(-1,
    //    unc_idx, std::get<1>(perf_config->cpu_4), std::get<2>(perf_config->cpu_4), value);
}

int Uncore::read_cha_elems(struct CHAElem *elem) {
    ssize_t r;
    for (auto const &[idx, value] : this->perf | enumerate) {
        r = value->read_pmu(&elem->cha[idx]);
        if (r < 0) {
            LOG(ERROR) << fmt::format("read cha_elems[{}] failed.\n", std::get<0>(helper.perf_conf.cha[idx]));
            return r;
        }
        LOG(DEBUG) << fmt::format("read cha_elems[{}]:{}\n", std::get<0>(helper.perf_conf.cha[idx]), elem->cha[idx]);
    }

    return 0;
}
