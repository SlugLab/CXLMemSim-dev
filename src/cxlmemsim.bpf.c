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

// malloc 入口探针
SEC("tracepoint/syscalls/sys_enter_mmap")
int mmap_enter(struct trace_event_raw_sys_enter *ctx) {
    u64 size = ctx->args[1];  // mmap 的第二个参数是大小
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

// malloc 返回探针
SEC("tracepoint/syscalls/sys_exit_mmap")
int mmap_exit(struct trace_event_raw_sys_exit *ctx) {
    void *address = (void *)ctx->ret;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    struct alloc_info *info = bpf_map_lookup_elem(&allocs_map, &pid_tgid);
    if (!info) {
        return 0;
    }

    if (address && (unsigned long)address != -1) {  // 检查 mmap 是否成功
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

// 内存释放探针
SEC("tracepoint/syscalls/sys_enter_munmap")
int munmap_enter(struct trace_event_raw_sys_enter *ctx) {
    void *address = (void *)ctx->args[0];
    u64 size = ctx->args[1];
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

// brk 系统调用入口
SEC("tracepoint/syscalls/sys_enter_brk")
int brk_enter(struct trace_event_raw_sys_enter *ctx) {
    u64 addr = ctx->args[0];
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    // 尝试获取锁
    u32 locked = 1;
    u32 *current = bpf_map_lookup_elem(&locks, &pid);
    if (!current) {
        bpf_map_update_elem(&locks, &pid, &locked, BPF_ANY);
    } else {
        // 已经有锁了，直接返回
        return 0;
    }

    // 获取或创建进程信息
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (!proc_info) {
        // 如果不存在，创建新的进程信息
        struct proc_info new_proc_info = {
            .mem_info = {.current_brk = addr, .total_allocated = 0, .total_freed = 0},
            .current_pid = pid,
            .current_tid = tid,
        };
        bpf_map_update_elem(&process_map, &pid, &new_proc_info, BPF_ANY);
    } else {
        // 如果存在，更新 brk 值
        proc_info->mem_info.current_brk = addr;
    }

    // 释放锁
    bpf_map_delete_elem(&locks, &pid);
    return 0;
}

// brk 系统调用返回
SEC("tracepoint/syscalls/sys_exit_brk")
int brk_exit(struct trace_event_raw_sys_exit *ctx) {
    u64 ret = ctx->ret;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 尝试获取锁
    u32 locked = 1;
    u32 *current = bpf_map_lookup_elem(&locks, &pid);
    if (!current) {
        bpf_map_update_elem(&locks, &pid, &locked, BPF_ANY);
    } else {
        return 0;
    }

    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info) {
        u64 current_brk = proc_info->mem_info.current_brk;
        if (ret > current_brk) {
            // 内存增加
            u64 increase = ret - current_brk;
            proc_info->mem_info.total_allocated += increase;
        } else if (ret < current_brk) {
            // 内存释放
            u64 decrease = current_brk - ret;
            proc_info->mem_info.total_freed += decrease;
        }
        proc_info->mem_info.current_brk = ret;
    }

    // 释放锁
    bpf_map_delete_elem(&locks, &pid);
    return 0;
}

// execve 系统调用入口
SEC("tracepoint/syscalls/sys_enter_execve")
int execve_enter(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    // 创建新的进程信息
    struct proc_info proc_info = {
        .parent_pid = pid,
        .create_time = bpf_ktime_get_ns(),
        .thread_count = 1, // execve 创建新进程时只有主线程
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

// execve 系统调用返回
SEC("tracepoint/syscalls/sys_exit_execve")
int execve_exit(struct trace_event_raw_sys_exit *ctx) {
    int ret = ctx->ret;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // execve 成功返回 0
    if (ret == 0) {
        // 获取进程信息
        struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
        if (proc_info) {
            // 更新执行时间
            proc_info->create_time = bpf_ktime_get_ns();
        }
    } else {
        // execve 失败，清理之前创建的信息
        bpf_map_delete_elem(&process_map, &pid);
        bpf_map_delete_elem(&stats_map, &pid);
    }

    return 0;
}

// clone 系统调用入口
SEC("tracepoint/syscalls/sys_enter_clone")
int clone_enter(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    // 获取 clone 参数
    unsigned long flags = ctx->args[0];
    void *stack = (void *)ctx->args[1];
    void *parent_tidptr = (void *)ctx->args[2];
    void *child_tidptr = (void *)ctx->args[3];

    bpf_printk("clone enter pid: %u, child_tidptr: %lx\n", pid, (unsigned long)child_tidptr);

    if (child_tidptr) {
        // 创建新的线程/进程信息
        struct proc_info thread_info = {
            .parent_pid = pid,
            .create_time = bpf_ktime_get_ns(),
        };

        // 更新线程计数
        struct proc_info *parent_info = bpf_map_lookup_elem(&process_map, &pid);
        if (parent_info) {
            parent_info->thread_count += 1;
        }

        // 保存父进程信息，稍后在返回探针中获取实际线程 ID
        bpf_map_update_elem(&thread_map, &child_tidptr, &thread_info, BPF_ANY);
    }

    return 0;
}

// clone 系统调用返回
SEC("tracepoint/syscalls/sys_exit_clone")
int clone_exit(struct trace_event_raw_sys_exit *ctx) {
    u32 child_pid = (u32)ctx->ret;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 parent_pid = pid_tgid >> 32;

    if (child_pid > 0) {
        bpf_printk("clone exit: parent=%u created child=%u\n", parent_pid, child_pid);

        // 创建新的进程/线程信息
        struct proc_info proc_info = {
            .parent_pid = parent_pid,
            .create_time = bpf_ktime_get_ns(),
            .current_pid = child_pid,
            .current_tid = child_pid,
        };

        // 检查标志位确定是进程还是线程
        // 如果是线程，更新线程计数

        // 更新映射
        bpf_map_update_elem(&process_map, &child_pid, &proc_info, BPF_ANY);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_enter_clone3")
int sys_enter_clone3(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;

    bpf_printk("clone3 syscall called by pid: %u\n", pid);

    // 记录基本信息，但不尝试访问复杂的参数结构
    // clone3 的第一个参数是指向 struct clone_args 的指针
    // 在这里我们只记录这个指针的地址，而不尝试访问其内容
    void *cl_args_ptr = (void *)ctx->args[0];

    // 更新线程计数
    struct proc_info *parent_info = bpf_map_lookup_elem(&process_map, &pid);
    if (parent_info) {
        parent_info->thread_count += 1;
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_clone3")
int sys_exit_clone3(struct trace_event_raw_sys_exit *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 parent_pid = pid_tgid >> 32;
    u32 child_pid = (u32)ctx->ret;

    if (child_pid > 0) {
        bpf_printk("clone3 created new thread/process: %u\n", child_pid);

        // 创建新的线程/进程信息
        struct proc_info proc_info = {
            .parent_pid = parent_pid,
            .create_time = bpf_ktime_get_ns(),
            .current_pid = child_pid,
            .current_tid = child_pid,
        };

        // 更新映射
        bpf_map_update_elem(&process_map, &child_pid, &proc_info, BPF_ANY);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_fork")
int fork_exit(struct trace_event_raw_sys_exit *ctx) {
    u32 child_pid = (u32)ctx->ret;  // 从返回值获取子进程 ID
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 parent_pid = pid_tgid >> 32;
    u32 tid = child_pid;

    if (child_pid > 0) {
        // 子进程创建成功，创建新的进程信息
        struct proc_info proc_info = {
            .parent_pid = parent_pid,
            .create_time = bpf_ktime_get_ns(),
            .thread_count = 1, // 初始只有主线程
            .current_pid = child_pid,
            .current_tid = tid,
        };

        // 更新进程 map
        bpf_map_update_elem(&process_map, &child_pid, &proc_info, BPF_ANY);

        bpf_printk("fork: parent=%u created child=%u\n", parent_pid, child_pid);
    }

    return 0;
}

SEC("tracepoint/syscalls/sys_enter_exit")
int exit_probe(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    u32 pid = pid_tgid >> 32;

    bpf_printk("exit: tid=%u, pid=%u\n", tid, pid);

    // 查找并删除线程信息
    struct proc_info *thread_info = bpf_map_lookup_elem(&thread_map, &tid);
    if (thread_info) {
        // 更新父进程的线程计数
        struct proc_info *parent_info = bpf_map_lookup_elem(&process_map, &thread_info->parent_pid);
        if (parent_info) {
            parent_info->thread_count -= 1;
        }
        // 删除线程信息
        bpf_map_delete_elem(&thread_map, &tid);
    }

    // 如果这是一个主线程，则也可能需要从 process_map 中删除进程信息
    if (tid == pid) {
        bpf_map_delete_elem(&process_map, &pid);
    }

    return 0;
}
SEC("tracepoint/syscalls/sys_enter_exit_group")
int exit_group_probe(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    u32 pid = pid_tgid >> 32;

    bpf_printk("exit_group: pid=%u\n", pid);

    // 获取退出状态码 (第一个参数)
    int exit_code = ctx->args[0];

    // 清理该进程组中的所有线程
    // 注意：由于 BPF 地图迭代限制，我们无法遍历地图来查找所有属于此进程的线程
    // 因此，我们依赖进程信息中的线程计数

    // 查找进程信息
    struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    if (proc_info) {
        // 记录进程退出信息
        bpf_printk("Process %u exiting with code %d, had %d threads\n",
                   pid, exit_code, proc_info->thread_count);

        // 删除进程信息
        bpf_map_delete_elem(&process_map, &pid);
    }

    // 删除调用线程的信息
    bpf_map_delete_elem(&thread_map, &tid);

    return 0;
}

// 跟踪 futex 系统调用
SEC("tracepoint/syscalls/sys_enter_futex")
int futex_enter(struct trace_event_raw_sys_enter *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    u32 pid = pid_tgid >> 32;

    // 获取 futex 参数
    u32 *uaddr = (u32 *)ctx->args[0];  // futex 地址
    int futex_op = (int)ctx->args[1];  // 操作码
    u32 val = (u32)ctx->args[2];       // 值

    // 解析 futex 操作类型
    int cmd = futex_op & 0xf;

    // FUTEX_LOCK_PI 和 FUTEX_WAIT 类似于互斥锁获取操作
    if (cmd == 6 /* FUTEX_LOCK_PI */ ||
        cmd == 0 /* FUTEX_WAIT */ ||
        cmd == 9 /* FUTEX_LOCK_PI2 */) {

        // 更新锁的状态
        u32 locked = 1;
        bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

        struct proc_info* item = bpf_map_lookup_elem(&thread_map, &tid);
        if (item) {
            item->is_locked = 1;
            bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
        }
    }
    // FUTEX_UNLOCK_PI 类似于互斥锁释放操作
    else if (cmd == 7 /* FUTEX_UNLOCK_PI */ ||
             cmd == 1 /* FUTEX_WAKE */) {

        // 更新锁的状态
        u32 locked = 0;
        bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);

        struct proc_info* item = bpf_map_lookup_elem(&thread_map, &tid);
        if (item) {
            item->is_locked = 0;
            bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);

            // 模拟延迟
            u64 delay_start = bpf_ktime_get_ns();
#pragma unroll
            for (int i = 0; i < 100; i++) {
                if ((bpf_ktime_get_ns() - delay_start) < item->sleep_time) {
                    break;
                }
            }
        }
    }
    // FUTEX_TRYLOCK_PI 类似于 trylock 操作
    else if (cmd == 8 /* FUTEX_TRYLOCK_PI */) {
        // 尝试获取锁
        u32 locked = 1;
        bpf_map_update_elem(&locks, &tid, &locked, BPF_ANY);
    }

    return 0;
}

// 跟踪 futex 系统调用返回
SEC("tracepoint/syscalls/sys_exit_futex")
int futex_exit(struct trace_event_raw_sys_exit *ctx) {
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;
    int ret = ctx->ret;

    // 获取当前的锁状态
    u32 *locked = bpf_map_lookup_elem(&locks, &tid);
    if (locked) {
        // 如果 trylock 失败，更新锁状态
        if (*locked == 1 && ret != 0) {
            u32 not_locked = 0;
            bpf_map_update_elem(&locks, &tid, &not_locked, BPF_ANY);

            struct proc_info* item = bpf_map_lookup_elem(&thread_map, &tid);
            if (item) {
                item->is_locked = 0;
                bpf_map_update_elem(&thread_map, &tid, item, BPF_ANY);
            }
        }
    }

    return 0;
}
char _license[] SEC("license") = "GPL";