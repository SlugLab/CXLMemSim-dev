#define BPF_NO_GLOBAL_DATA
/*
 * CXLMemSim bpfhook
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *      Brian Zhao
 *  SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "../include/bpftimeruntime.h"

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 100000);
    __type(key, u64);
    __type(value, struct alloc_info);
} allocs_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u32);
    __type(value, struct mem_stats);
} stats_map SEC(".maps");

// 存储线程信息的 map
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 102400);
    __type(key, u32); // tid as key
    __type(value, struct proc_info);
} thread_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u32);
    __type(value, struct proc_info);
} process_map SEC(".maps");

// 用于处理多线程同步的自旋锁映射
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, u32);
    __type(value, u32);
} locks SEC(".maps");
// 钩住OpenMP并行区域创建函数
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_parallel")
int uprobe_omp_parallel(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    void *fn = (void *)PT_REGS_PARM1(ctx);  // 并行区域函数指针
    void *data = (void *)PT_REGS_PARM2(ctx); // 用户数据指针
    unsigned num_threads = (unsigned)PT_REGS_PARM3(ctx); // 线程数量

    bpf_printk("OMP parallel region start: pid=%u, threads=%u\n",
               pid, num_threads);

    // 更新进程信息中的线程计数
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info) {
        proc_info->thread_count += num_threads;
        bpf_map_update_elem(&process_map, &pid, proc_info, BPF_ANY);
    }

    return 0;
}

// 钩住OpenMP并行区域结束函数
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_parallel")
int uretprobe_omp_parallel(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    bpf_printk("OMP parallel region end: pid=%u\n", pid);

    return 0;
}

// 钩住GOMP_single_start函数 - 用于single构造
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_single_start")
int uprobe_omp_single_start(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    bpf_printk("OMP single construct start: pid=%u, tid=%u\n", pid, tid);

    // 返回值表示该线程是否执行single区域
    return 0;
}

// 钩住GOMP_single_start的返回值
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_single_start")
int uretprobe_omp_single_start(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;
    bool is_selected = (bool)PT_REGS_RC(ctx);

    bpf_printk("OMP single construct: pid=%u, tid=%u, selected=%d\n",
               pid, tid, is_selected);

    return 0;
}

// OpenMP临界区钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_critical_start")
int uprobe_omp_critical_start(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    bpf_printk("OMP critical section start: pid=%u, tid=%u\n", pid, tid);

    // 记录线程进入临界区
    u32 locked = 1;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
    if (item) {
        item->is_locked = 1;
        bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
    }

    return 0;
}

SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_critical_end")
int uprobe_omp_critical_end(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    bpf_printk("OMP critical section end: pid=%u, tid=%u\n", pid, tid);

    // 记录线程离开临界区
    u32 locked = 0;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
    if (item) {
        item->is_locked = 0;
        bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
    }

    return 0;
}

// OpenMP屏障同步点
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:GOMP_barrier")
int uprobe_omp_barrier(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    bpf_printk("OMP barrier reached: pid=%u, tid=%u\n", pid, tid);

    return 0;
}

// 线程同步：获取OpenMP锁
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:omp_set_lock")
int uprobe_omp_set_lock(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;
    void *lock = (void *)PT_REGS_PARM1(ctx);

    bpf_printk("OMP set lock: pid=%u, tid=%u, lock=%p\n",
               pid, tid, lock);

    // 记录线程获取锁
    u32 locked = 1;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    return 0;
}

// 线程同步：释放OpenMP锁
SEC("uprobe//usr/lib/x86_64-linux-gnu/libgomp.so.1:omp_unset_lock")
int uprobe_omp_unset_lock(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;
    void *lock = (void *)PT_REGS_PARM1(ctx);

    bpf_printk("OMP unset lock: pid=%u, tid=%u, lock=%p\n",
               pid, tid, lock);

    // 记录线程释放锁
    u32 locked = 0;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    return 0;
}
// C++标准库线程创建函数的钩子
SEC("uprobe//usr/lib/gcc/x86_64-linux-gnu/14/libstdc++.so:_ZNSt6thread15_M_start_threadESt10unique_ptrINS_6_StateESt14default_deleteIS1_EEPFvvE")
int uprobe_cxx_thread_start(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    // 获取函数参数（注意：在BPF中我们不能直接访问complex C++对象的内部）
    // 我们只能获取到原始指针
    void *thread_state_ptr = (void *)PT_REGS_PARM1(ctx);  // unique_ptr参数
    void *thread_func_ptr = (void *)PT_REGS_PARM2(ctx);   // 函数指针参数

    bpf_printk("C++ std::thread start detected in process %u (tid %u)\n", pid, tid);

    // 记录线程创建事件到临时映射，供返回探针使用
    struct {
        u32 parent_pid;
        u64 create_time;
    } thread_info = {
        .parent_pid = pid,
        .create_time = bpf_ktime_get_ns(),
    };

    bpf_map_update_elem(&allocs_map, &pid_tgid, &thread_info, BPF_ANY);

    return 0;
}

// C++标准库线程创建函数的返回钩子
SEC("uretprobe//usr/lib/gcc/x86_64-linux-gnu/14/libstdc++.so:_ZNSt6thread15_M_start_threadESt10unique_ptrINS_6_StateESt14default_deleteIS1_EEPFvvE")
int uretprobe_cxx_thread_start(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 获取之前保存的信息
    void *thread_info_ptr = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!thread_info_ptr) {
        return 0;
    }

    // 注意：C++线程创建函数不直接返回线程ID
    // 需要在后续步骤中捕获（比如通过pthread_create的返回值）

    // 在这里，我们可以增加进程的线程计数
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info) {
        proc_info->thread_count++;
        bpf_map_update_elem(&process_map, &pid, proc_info, BPF_ANY);
        bpf_printk("Incremented thread count for process %u to %u\n",
                  pid, proc_info->thread_count);
    }

    // 清理临时信息
    bpf_map_delete_elem(&allocs_map, &pid_tgid);

    return 0;
}
// mmap的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:mmap")
int uprobe_mmap(struct pt_regs *ctx) {
    u64 size = PT_REGS_PARM2(ctx);  // 第二个参数是大小
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 更新统计信息
    struct mem_stats *stats, zero_stats = {};
    stats = bpf_map_lookup_elem(&stats_map, &pid);
    if (!stats) {
        bpf_map_update_elem(&stats_map, &pid, &zero_stats, BPF_ANY);
        stats = bpf_map_lookup_elem(&stats_map, &pid);
        if (!stats)
            return 0;
    }

    // 记录请求的大小
    struct alloc_info info = {
        .size = size,
    };
    bpf_map_update_elem(&allocs_map, &pid_tgid, &info, BPF_ANY);
    return 0;
}

// mmap的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:mmap")
int uretprobe_mmap(struct pt_regs *ctx) {
    void *address = (void *)PT_REGS_RC(ctx);  // 返回值是映射地址
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!info) {
        return 0;
    }

    if (address && (unsigned long)address != -1) {  // 检查mmap是否成功
        struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
        if (stats) {
            stats->total_allocated += info->size;
            stats->current_usage += info->size;
            stats->allocation_count += 1;

            info->address = (u64)address;
            bpf_map_update_elem(&allocs_map, &address, info, BPF_ANY);
        }
    }

    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// munmap的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:munmap")
int uprobe_munmap(struct pt_regs *ctx) {
    void *address = (void *)PT_REGS_PARM1(ctx);  // 第一个参数是地址
    u64 size = PT_REGS_PARM2(ctx);              // 第二个参数是大小
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    if (!address)
        return 0;

    // 查找分配信息
    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &address);
    if (!info) {
        // 如果找不到详细信息，使用参数中的大小
        struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
        if (stats) {
            stats->total_freed += size;
            stats->current_usage -= size;
            stats->free_count += 1;
        }
        return 0;
    }

    struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
    if (stats) {
        stats->total_freed += info->size;
        stats->current_usage -= info->size;
        stats->free_count += 1;
    }

    bpf_map_delete_elem(&allocs_map, &address);
    return 0;
}

// malloc的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:malloc")
int uprobe_malloc(struct pt_regs *ctx) {
    u64 size = PT_REGS_PARM1(ctx);  // 第一个参数是大小
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 记录请求的大小
    struct alloc_info info = {
        .size = size,
    };
    bpf_map_update_elem(&allocs_map, &pid_tgid, &info, BPF_ANY);
    return 0;
}

// malloc的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:malloc")
int uretprobe_malloc(struct pt_regs *ctx) {
    void *address = (void *)PT_REGS_RC(ctx);  // 返回值是分配的地址
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!info) {
        return 0;
    }

    if (address) {  // malloc成功
        struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
        if (stats) {
            stats->total_allocated += info->size;
            stats->current_usage += info->size;
            stats->allocation_count += 1;

            info->address = (u64)address;
            bpf_map_update_elem(&allocs_map, &address, info, BPF_ANY);
        }
    }

    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// free的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:free")
int uprobe_free(struct pt_regs *ctx) {
    void *address = (void *)PT_REGS_PARM1(ctx);  // 第一个参数是地址
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    if (!address)
        return 0;

    // 查找分配信息
    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &address);
    if (!info) {
        return 0;  // 无法确定大小，跳过
    }

    struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
    if (stats) {
        stats->total_freed += info->size;
        stats->current_usage -= info->size;
        stats->free_count += 1;
    }

    bpf_map_delete_elem(&allocs_map, &address);
    return 0;
}

// calloc的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:calloc")
int uprobe_calloc(struct pt_regs *ctx) {
    u64 nmemb = PT_REGS_PARM1(ctx);  // 第一个参数是元素数量
    u64 size = PT_REGS_PARM2(ctx);   // 第二个参数是每个元素的大小
    u64 total_size = nmemb * size;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 记录请求的大小
    struct alloc_info info = {
        .size = total_size,
    };
    bpf_map_update_elem(&allocs_map, &pid_tgid, &info, BPF_ANY);
    return 0;
}

// calloc的uretprobe钩子（与malloc返回类似）
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:calloc")
int uretprobe_calloc(struct pt_regs *ctx) {
    void *address = (void *)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!info) {
        return 0;
    }

    if (address) {
        struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
        if (stats) {
            stats->total_allocated += info->size;
            stats->current_usage += info->size;
            stats->allocation_count += 1;

            info->address = (u64)address;
            bpf_map_update_elem(&allocs_map, &address, info, BPF_ANY);
        }
    }

    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// realloc的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:realloc")
int uprobe_realloc(struct pt_regs *ctx) {
    void *ptr = (void *)PT_REGS_PARM1(ctx);  // 第一个参数是原指针
    u64 size = PT_REGS_PARM2(ctx);           // 第二个参数是新大小
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 如果ptr不为NULL，记录原指针的信息
    if (ptr) {
        struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &ptr);
        if (info) {
            // 记录旧信息和新大小
            struct alloc_info new_info = {
                .size = size,
                .address = (u64)ptr,  // 临时存储旧地址
            };
            bpf_map_update_elem(&allocs_map, &pid_tgid, &new_info, BPF_ANY);
            return 0;
        }
    }

    // 如果ptr为NULL或找不到原记录，相当于malloc
    struct alloc_info info = {
        .size = size,
    };
    bpf_map_update_elem(&allocs_map, &pid_tgid, &info, BPF_ANY);
    return 0;
}

// realloc的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:realloc")
int uretprobe_realloc(struct pt_regs *ctx) {
    void *new_addr = (void *)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!info) {
        return 0;
    }

    struct mem_stats *stats = bpf_map_lookup_elem(&stats_map, &pid);
    if (!stats) {
        bpf_map_delete_elem(&allocs_map, &pid_tgid);
        return 0;
    }

    // 如果原地址存在（即不是NULL），需要先释放
    if (info->address) {
        void *old_addr = (void *)info->address;
        struct alloc_info *old_info = bpf_map_lookup_elem(&allocs_map, &old_addr);

        if (old_info) {
            stats->total_freed += old_info->size;
            stats->current_usage -= old_info->size;
            stats->free_count += 1;

            bpf_map_delete_elem(&allocs_map, &old_addr);
        }
    }

    // 如果新地址有效，记录新的分配
    if (new_addr) {
        stats->total_allocated += info->size;
        stats->current_usage += info->size;
        stats->allocation_count += 1;

        info->address = (u64)new_addr;
        bpf_map_update_elem(&allocs_map, &new_addr, info, BPF_ANY);
    }

    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// 处理sbrk作为brk的用户空间接口
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:sbrk")
int uprobe_sbrk(struct pt_regs *ctx) {
    long increment = (long)PT_REGS_PARM1(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    // 获取或创建进程信息
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (!proc_info) {
        // 如果不存在，创建新的进程信息
        struct proc_info new_proc_info = {
            .mem_info = {.current_brk = 0, .total_allocated = 0, .total_freed = 0},
            .current_pid = pid,
            .current_tid = tid,
        };
        bpf_map_update_elem(&process_map, &pid, &new_proc_info, BPF_ANY);
        proc_info = bpf_map_lookup_elem(&process_map, &pid);
        if (!proc_info)
            return 0;
    }

    // 记录增量信息用于返回探针
    bpf_map_update_elem(&allocs_map, &pid_tgid, &increment, BPF_ANY);
    return 0;
}

// sbrk的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:sbrk")
int uretprobe_sbrk(struct pt_regs *ctx) {
    void *brk = (void *)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 获取增量信息
    long *increment = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!increment) {
        return 0;
    }

    // 获取进程信息
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info && brk != (void *)-1) {
        // 如果是首次调用，记录初始brk
        if (proc_info->mem_info.current_brk == 0) {
            proc_info->mem_info.current_brk = (u64)brk;
        } else {
            // 更新内存统计
            if (*increment > 0) {
                // 分配内存
                proc_info->mem_info.total_allocated += *increment;
            } else if (*increment < 0) {
                // 释放内存
                proc_info->mem_info.total_freed += -(*increment);
            }
            proc_info->mem_info.current_brk = (u64)brk;
        }
    }

    // 清理临时状态
    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// fork的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:fork")
int uprobe_fork(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 记录当前进程信息供返回探针使用
    struct proc_info temp_info = {
        .parent_pid = pid,
        .create_time = bpf_ktime_get_ns(),
    };
    bpf_map_update_elem(&allocs_map, &pid_tgid, &temp_info, BPF_ANY);
    return 0;
}

// fork的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:fork")
int uretprobe_fork(struct pt_regs *ctx) {
    u32 child_pid = (u32)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 parent_pid = pid_tgid >> 32;

    // 获取前面保存的信息
    struct proc_info *temp_info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!temp_info) {
        return 0;
    }

    if (child_pid > 0) {
        // 为子进程创建新的进程信息
        struct proc_info proc_info = {
            .parent_pid = parent_pid,
            .create_time = temp_info->create_time,
            .thread_count = 1,  // 初始只有主线程
            .current_pid = child_pid,
            .current_tid = child_pid,
        };

        // 更新进程映射
        bpf_map_update_elem(&process_map, &child_pid, &proc_info, BPF_ANY);

        // 为子进程创建新的内存统计
        struct mem_stats new_stats = {};
        bpf_map_update_elem(&stats_map, &child_pid, &new_stats, BPF_ANY);

        bpf_printk("fork: parent=%u created child=%u\n", parent_pid, child_pid);
    }

    // 清理临时状态
    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// clone的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:clone")
int uprobe_clone(struct pt_regs *ctx) {
    unsigned long flags = PT_REGS_PARM1(ctx);  // 第一个参数是标志
    void *stack = (void *)PT_REGS_PARM2(ctx);  // 第二个参数是栈
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 记录当前进程信息和标志供返回探针使用
    struct {
        struct proc_info info;
        unsigned long flags;
    } clone_data = {
        .info = {
            .parent_pid = pid,
            .create_time = bpf_ktime_get_ns(),
        },
        .flags = flags,
    };

    bpf_map_update_elem(&allocs_map, &pid_tgid, &clone_data, BPF_ANY);
    return 0;
}

// clone的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:clone")
int uretprobe_clone(struct pt_regs *ctx) {
    u32 child_id = (u32)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 parent_pid = pid_tgid >> 32;

    // 获取前面保存的信息和标志
    void *clone_data_ptr = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!clone_data_ptr) {
        return 0;
    }

    // 假设我们能够访问clone_data结构
    // 这里是个简化，在真实代码中可能需要通过读取相应偏移量来获取数据
    struct {
        struct proc_info info;
        unsigned long flags;
    } *clone_data = clone_data_ptr;

    if (child_id > 0) {
        // 确定是线程还是进程
        // CLONE_THREAD标志表示创建线程
        int is_thread = clone_data->flags & 0x00010000;  // CLONE_THREAD

        if (is_thread) {
            // 这是一个线程，更新线程映射
            struct proc_info thread_info = clone_data->info;
            thread_info.current_tid = child_id;
            thread_info.current_pid = parent_pid;

            bpf_map_update_elem(&thread_map, &child_id, &thread_info, BPF_ANY);

            // 更新父进程的线程计数
            struct proc_info *parent_info = bpf_map_lookup_elem(&process_map, &parent_pid);
            if (parent_info) {
                parent_info->thread_count++;
                bpf_map_update_elem(&process_map, &parent_pid, parent_info, BPF_ANY);
            }

            bpf_printk("clone created thread: pid=%u, tid=%u\n", parent_pid, child_id);
        } else {
            // 这是一个进程，更新进程映射
            struct proc_info proc_info = clone_data->info;
            proc_info.current_pid = child_id;
            proc_info.current_tid = child_id;
            proc_info.thread_count = 1;

            bpf_map_update_elem(&process_map, &child_id, &proc_info, BPF_ANY);

            // 为新进程创建内存统计
            struct mem_stats new_stats = {};
            bpf_map_update_elem(&stats_map, &child_id, &new_stats, BPF_ANY);

            bpf_printk("clone created process: parent=%u, child=%u\n", parent_pid, child_id);
        }
    }

    // 清理临时状态
    bpf_map_delete_elem(&allocs_map, &pid_tgid);
    return 0;
}

// execve的uprobe钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:execve")
int uprobe_execve(struct pt_regs *ctx) {
    const char *filename = (const char *)PT_REGS_PARM1(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    // 创建新的进程信息
    struct proc_info proc_info = {
        .parent_pid = pid,
        .create_time = bpf_ktime_get_ns(),
        .thread_count = 1,
        .current_pid = pid,
        .current_tid = tid,
        .mem_info = {.total_allocated = 0, .total_freed = 0, .current_brk = 0}
    };

    // 保存进程信息
    bpf_map_update_elem(&process_map, &pid, &proc_info, BPF_ANY);

    // 清理旧的统计信息
    bpf_map_delete_elem(&stats_map, &pid);

    // 初始化新的统计信息
    struct mem_stats new_stats = {};
    bpf_map_update_elem(&stats_map, &pid, &new_stats, BPF_ANY);

    return 0;
}

// execve的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:execve")
int uretprobe_execve(struct pt_regs *ctx) {
    int ret = (int)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // execve成功返回0
    if (ret == 0) {
        // 获取进程信息
        struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
        if (proc_info) {
            // 更新执行时间
            proc_info->create_time = bpf_ktime_get_ns();
        }
    } else {
        // execve失败，清理之前创建的信息
        bpf_map_delete_elem(&process_map, &pid);
        bpf_map_delete_elem(&stats_map, &pid);
    }

    return 0;
}

// 进程/线程退出的探针
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:exit")
int uprobe_exit(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    u32 pid = pid_tgid >> 32;

    bpf_printk("exit: tid=%u, pid=%u\n", tid, pid);

    // 查找并删除线程信息
    struct proc_info *thread_info = bpf_map_lookup_elem(&thread_map, &tid);
    if (thread_info) {
        // 更新父进程的线程计数
        struct proc_info *parent_info = bpf_map_lookup_elem(&process_map, &thread_info->parent_pid);
        if (parent_info && parent_info->thread_count > 0) {
            parent_info->thread_count--;
            bpf_map_update_elem(&process_map, &thread_info->parent_pid, parent_info, BPF_ANY);
        }
        // 删除线程信息
        bpf_map_delete_elem(&thread_map, &tid);
    }

    // 如果这是主线程，则也需要从process_map中删除进程信息
    if (tid == pid) {
        bpf_map_delete_elem(&process_map, &pid);
        bpf_map_delete_elem(&stats_map, &pid);
    }

    return 0;
}

// 进程组退出的探针
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:_exit")
int uprobe_exit_group(struct pt_regs *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    u32 pid = pid_tgid >> 32;

    bpf_printk("exit_group: pid=%u\n", pid);

    // 获取退出状态码
    int exit_code = (int)PT_REGS_PARM1(ctx);

    // 查找进程信息
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info) {
        // 记录进程退出信息
        bpf_printk("Process %u exiting with code %d, had %d threads\n",
                  pid, exit_code, proc_info->thread_count);

        // 删除进程信息
        bpf_map_delete_elem(&process_map, &pid);
        bpf_map_delete_elem(&stats_map, &pid);
    }

    // 删除调用线程的信息
    bpf_map_delete_elem(&thread_map, &tid);

    return 0;
}

// pthread_mutex_lock的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:pthread_mutex_lock")
int uretprobe_pthread_mutex_lock(struct pt_regs *ctx) {
    int ret = (int)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    // 如果获取锁失败，更新状态
    if (ret != 0) {
        u32 not_locked = 0;
        bpf_map_update_elem(&locks, &tid, &not_locked, BPF_ANY);

        struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
        if (item) {
            item->is_locked = 0;
            bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
        }
    }

    return 0;
}
// pthread_mutex_lock 的 uprobe 钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:pthread_mutex_lock")
int uprobe_pthread_mutex_lock(struct pt_regs *ctx) {
    void *mutex = (void *)PT_REGS_PARM1(ctx);  // 使用 void* 代替 pthread_mutex_t*
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    // 记录尝试获取锁
    u32 locked = 1;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
    if (item) {
        item->is_locked = 1;
        bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
    }

    return 0;
}

// pthread_mutex_unlock 的 uprobe 钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:pthread_mutex_unlock")
int uprobe_pthread_mutex_unlock(struct pt_regs *ctx) {
    void *mutex = (void *)PT_REGS_PARM1(ctx);  // 使用 void* 代替 pthread_mutex_t*
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    // 记录释放锁
    u32 locked = 0;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
    if (item) {
        item->is_locked = 0;
        bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
    }

    return 0;
}

// pthread_mutex_trylock 的 uprobe 钩子
SEC("uprobe//usr/lib/x86_64-linux-gnu/libc.so.6:pthread_mutex_trylock")
int uprobe_pthread_mutex_trylock(struct pt_regs *ctx) {
    void *mutex = (void *)PT_REGS_PARM1(ctx);  // 使用 void* 代替 pthread_mutex_t*
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    // 记录尝试获取锁
    u32 locked = 1;
    bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

    return 0;
}

// pthread_mutex_trylock的uretprobe钩子
SEC("uretprobe//usr/lib/x86_64-linux-gnu/libc.so.6:pthread_mutex_trylock")
int uretprobe_pthread_mutex_trylock(struct pt_regs *ctx) {
    int ret = (int)PT_REGS_RC(ctx);
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    // 获取当前的锁状态
    u32 *locked = bpf_map_lookup_elem(&locks, &tid);
    if (locked) {
        // 如果trylock返回非0，表示获取锁失败
        if (*locked == 1 && ret != 0) {
            u32 not_locked = 0;
            bpf_map_update_elem(&locks, &tid, &not_locked, BPF_ANY);

            struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
            if (item) {
                item->is_locked = 0;
                bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
            }
        } else if (ret == 0) {
            // 获取锁成功，更新线程信息
            struct proc_info *item = bpf_map_lookup_elem(&thread_map, &tid);
            if (item) {
                item->is_locked = 1;
                bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
            }
        }
    }

    return 0;
}
char _license[] SEC("license") = "GPL";