// Created by victoryang00 on 1/14/23.
//

#ifndef CXL_MEM_SIMULATOR_INCORE_H
#define CXL_MEM_SIMULATOR_INCORE_H
#include "helper.h"
#include "perf.h"
#include <sys/types.h>

class CXLController;
union CPUID_INFO {
    int array[4];
    struct {
        unsigned int eax, ebx, ecx, edx;
    } reg;
};
/** This is a per cha metrics*/
class Incore {
public:
    PerfInfo *perf[4]; // should only be 4 counters
    struct PerfConfig *perf_config;
    Incore(pid_t pid, int cpu, struct PerfConfig *perf_config);
    ~Incore() = default;
    int start();
    int stop();
    void init_cpu_l2stall(pid_t pid,int cpu);
    void init_cpu_llcl_hits(pid_t pid,int cpu);
    void init_cpu_llcl_miss(pid_t pid,int cpu);
    void init_cpu_all_read(pid_t pid, int cpu);

    int read_cpu_elems(struct CPUElem *cpu_elem);
};

void pcm_cpuid(unsigned leaf, CPUID_INFO *info);
bool get_cpu_info(struct CPUInfo *);

#endif // CXL_MEM_SIMULATOR_INCORE_H
