#define BPF_NO_GLOBAL_DATA
/*
 * CXLMemSim bpfhook
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "vmlinux.h"

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

// 定义内存分配信息结构体
struct alloc_info {
	u64 size; // 分配的大小
	u64 address; // 分配的地址
};

// 存储每个进程的 malloc 分配信息
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 100000);
	__type(key, u64); // 使用地址作为 key
	__type(value, struct alloc_info);
} allocs SEC(".maps");

// 存储每个进程的总计内存使用情况
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32); // pid as key
	__type(value, struct mem_stats);
} stats SEC(".maps");

struct mem_stats {
	u64 total_allocated; // 总分配量
	u64 total_freed; // 总释放量
	u64 current_usage; // 当前使用量
	u64 allocation_count; // 分配次数
	u64 free_count; // 释放次数
};

// malloc probe
SEC("uprobe//opt/bwzhao/lib/libc.so.6:malloc")
int malloc_entry(struct pt_regs *ctx)
{
	u64 size;
	u32 pid = bpf_get_current_pid_tgid();
	u64 pid_tgid = bpf_get_current_pid_tgid();

	// 读取分配大小参数
	bpf_probe_read_user(&size, sizeof(size), (void *)&PT_REGS_PARM1(ctx));

	// 更新统计信息
	struct mem_stats *stats, zero_stats = {};
	stats = bpf_map_lookup_elem(&stats, &pid);
	if (!stats) {
		bpf_map_update_elem(&stats, &pid, &zero_stats, BPF_ANY);
		stats = bpf_map_lookup_elem(&stats, &pid);
		if (!stats)
			return 0;
	}

	// 记录请求的大小，用于在 return probe 中使用
	struct alloc_info info = {
		.size = size,
	};
	u64 id = pid_tgid;
	bpf_map_update_elem(&allocs, &id, &info, BPF_ANY);

	return 0;
}

// malloc return probe
SEC("uretprobe//opt/bwzhao/lib/libc.so.6:malloc")
int malloc_return(struct pt_regs *ctx)
{
	u64 address = PT_REGS_RC(ctx);
	u32 pid = bpf_get_current_pid_tgid();
	u64 pid_tgid = bpf_get_current_pid_tgid();

	// 获取之前保存的分配信息
	struct alloc_info *info = bpf_map_lookup_elem(&allocs, &pid_tgid);
	if (!info)
		return 0;

	// 分配成功时更新统计信息
	if (address) {
		struct mem_stats *stats = bpf_map_lookup_elem(&stats, &pid);
		if (stats) {
			__sync_fetch_and_add(&stats->total_allocated,
					     info->size);
			__sync_fetch_and_add(&stats->current_usage, info->size);
			__sync_fetch_and_add(&stats->allocation_count, 1);

			// 保存分配信息用于后续 free
			info->address = address;
			bpf_map_update_elem(&allocs, &address, info, BPF_ANY);
		}
	}

	// 清理临时数据
	bpf_map_delete_elem(&allocs, &pid_tgid);

	return 0;
}

// free probe
SEC("uprobe//opt/bwzhao/lib/libc.so.6:free")
int free_entry(struct pt_regs *ctx)
{
	u64 address;
	u32 pid = bpf_get_current_pid_tgid();

	// 读取要释放的地址
	bpf_probe_read_user(&address, sizeof(address),
			    (void *)&PT_REGS_PARM1(ctx));

	// 空指针检查
	if (!address)
		return 0;

	// 查找之前保存的分配信息
	struct alloc_info *info = bpf_map_lookup_elem(&allocs, &address);
	if (!info)
		return 0;

	// 更新统计信息
	struct mem_stats *stats = bpf_map_lookup_elem(&stats, &pid);
	if (stats) {
		__sync_fetch_and_add(&stats->total_freed, info->size);
		__sync_fetch_and_sub(&stats->current_usage, info->size);
		__sync_fetch_and_add(&stats->free_count, 1);
	}

	// 清理分配信息
	bpf_map_delete_elem(&allocs, &address);

	return 0;
}

// 进程退出时清理数据

struct mem_info {
	u64 current_brk; // 当前 brk 位置
	u64 total_allocated; // 总分配量
	u64 total_freed; // 总释放量
};
struct proc_info {
	u32 parent_pid; // 父进程 ID
	u64 create_time; // 创建时间
	u64 thread_count; // 线程数量（仅对进程有效）
    struct mem_info mem_info;
};


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

// 辅助函数：获取或创建 mem_info
static struct mem_info *get_or_create_mem_info(u32 pid)
{
	struct proc_info *proc_info = bpf_map_lookup_elem(&process_map, &pid);
    struct mem_info *info = &proc_info->mem_info;
	if (!info) {
		struct mem_info new_info = {};
		bpf_map_update_elem(&process_map, &pid, &new_info, BPF_ANY);
		info = bpf_map_lookup_elem(&process_map, &pid);
	}
	return info;
}

// 简单的自旋锁实现
static bool try_lock(u32 pid)
{
	u32 locked = 1;
	u32 *current = bpf_map_lookup_elem(&locks, &pid);
	if (!current) {
		bpf_map_update_elem(&locks, &pid, &locked, BPF_ANY);
		return true;
	}
	return false;
}

static void unlock(u32 pid)
{
	bpf_map_delete_elem(&locks, &pid);
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:brk")
int brk_entry(struct pt_regs *ctx)
{
	u64 addr;
	u32 pid = bpf_get_current_pid_tgid();

	// 尝试获取锁
	if (!try_lock(pid)) {
		return 0; // 如果获取锁失败，直接返回
	}

	bpf_probe_read(&addr, sizeof(addr), (void *)&PT_REGS_PARM1(ctx));
	struct mem_info *info = get_or_create_mem_info(pid);

	if (info) {
		// 保存请求地址供返回时使用
		info->current_brk = addr;
	}

	unlock(pid);
	return 0;
}

SEC("uretprobe//opt/bwzhao/lib/libc.so.6:brk")
int brk_return(struct pt_regs *ctx)
{
	u64 ret = PT_REGS_RC(ctx);
	u32 pid = bpf_get_current_pid_tgid();

	if (!try_lock(pid)) {
		return 0;
	}

	struct mem_info *info = get_or_create_mem_info(pid);
	if (info) {
		if (ret > info->current_brk) {
			// 内存增加
			u64 increase = ret - info->current_brk;
			info->total_allocated += increase;
		} else if (ret < info->current_brk) {
			// 内存释放
			u64 decrease = info->current_brk - ret;
			info->total_freed += decrease;
		}
		info->current_brk = ret;
	}

	unlock(pid);
	return 0;
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:sbrk")
int sbrk_entry(struct pt_regs *ctx)
{
	long increment;
	u32 pid = bpf_get_current_pid_tgid();

	if (!try_lock(pid)) {
		return 0;
	}

	bpf_probe_read(&increment, sizeof(increment),
		       (void *)&PT_REGS_PARM1(ctx));
	struct mem_info *info = get_or_create_mem_info(pid);

	if (info) {
		if (increment > 0) {
			info->total_allocated += increment;
		} else if (increment < 0) {
			info->total_freed += -increment;
		}
	}

	unlock(pid);
	return 0;
}


// 存储线程信息的 map
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 102400);
	__type(key, u32); // tid as key
	__type(value, struct proc_info);
} thread_map SEC(".maps");

// 辅助函数：获取当前时间戳
static u64 get_timestamp()
{
	return bpf_ktime_get_ns();
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:pthread_create")
int pthread_create_probe(struct pt_regs *ctx)
{
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 pid = pid_tgid >> 32;

	// 获取新线程的 ID（通过第一个参数）
	unsigned long *thread_ptr;
	bpf_probe_read(&thread_ptr, sizeof(thread_ptr),
		       (void *)&PT_REGS_PARM1(ctx));

	if (thread_ptr) {
		// 创建新的线程信息
		struct proc_info thread_info = {
			.parent_pid = pid,
			.create_time = get_timestamp(),
		};

		// 更新线程计数
		struct proc_info *parent_info =
			bpf_map_lookup_elem(&process_map, &pid);
		if (parent_info) {
			__sync_fetch_and_add(&parent_info->thread_count, 1);
		}

		// 注意：我们需要在 return probe 中获取实际的线程 ID
		// 这里先保存父进程信息
		bpf_map_update_elem(&thread_map, &thread_ptr, &thread_info,
				    BPF_ANY);
	}

	return 0;
}

SEC("uretprobe//opt/bwzhao/lib/libc.so.6:pthread_create")
int pthread_create_ret(struct pt_regs *ctx)
{
	int ret = PT_REGS_RC(ctx);

	// pthread_create 成功返回 0
	if (ret == 0) {
		// 这里可以添加额外的处理逻辑
		// 比如记录线程创建的结果
	}

	return 0;
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:pthread_exit")
int pthread_exit_probe(struct pt_regs *ctx)
{
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 tid = (u32)pid_tgid;
	u32 pid = pid_tgid >> 32;

	// 查找并删除线程信息
	struct proc_info *thread_info = bpf_map_lookup_elem(&thread_map, &tid);
	if (thread_info) {
		// 更新父进程的线程计数
		struct proc_info *parent_info =
			bpf_map_lookup_elem(&process_map, &pid);
		if (parent_info) {
			__sync_fetch_and_sub(&parent_info->thread_count, 1);
		}

		// 删除线程信息
		bpf_map_delete_elem(&thread_map, &tid);
	}

	return 0;
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:fork")
int fork_probe(struct pt_regs *ctx)
{
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 parent_pid = pid_tgid >> 32;

	// 父进程信息会在 fork 返回时记录
	// 这里可以添加额外的前置处理逻辑

	return 0;
}

SEC("uretprobe//opt/bwzhao/lib/libc.so.6:fork")
int fork_ret(struct pt_regs *ctx)
{
	u32 child_pid = PT_REGS_RC(ctx);
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 parent_pid = pid_tgid >> 32;

	if (child_pid > 0) {
		// 创建新的进程信息
		struct proc_info proc_info = {
			.parent_pid = parent_pid,
			.create_time = get_timestamp(),
			.thread_count = 1, // 初始只有主线程
		};

		// 更新进程 map
		bpf_map_update_elem(&process_map, &child_pid, &proc_info,
				    BPF_ANY);
	}

	return 0;
}

SEC("uprobe//opt/bwzhao/lib/libc.so.6:exit")
int exit_probe(struct pt_regs *ctx)
{
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 pid = pid_tgid >> 32;

	// 清理进程信息
	bpf_map_delete_elem(&process_map, &pid);

	return 0;
}
SEC("tracepoint/sched/sched_process_exit")
int process_exit(struct pt_regs *ctx)
{
	u32 pid = bpf_get_current_pid_tgid();

	// 清理统计信息
	bpf_map_delete_elem(&stats, &pid);

	// 进程退出时清理相关数据
	bpf_map_delete_elem(&process_map, &pid);
	bpf_map_delete_elem(&locks, &pid);

	bpf_map_delete_elem(&process_map, &pid);
	return 0;
}
char _license[] SEC("license") = "GPL";
