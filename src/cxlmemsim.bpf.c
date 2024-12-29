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

// Define a BPF map to store event counts
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u32);    // PID as key
    __type(value, u64);  // count as value
    __uint(max_entries, 1024);
} counts_map SEC(".maps");

SEC("uprobe/libc.so.6:malloc")
int malloc_probe(struct pt_regs *ctx)
{
    // Example: read arguments as needed for your user function
    long arg1, arg2;
    // char fmt[] = "uprobe triggered: arg1=%ld arg2=%ld pid=%u\n";

    u32 pid = bpf_get_current_pid_tgid();
    bpf_probe_read_user(&arg1, sizeof(arg1), (void *)&PT_REGS_PARM1(ctx));
    bpf_probe_read_user(&arg2, sizeof(arg2), (void *)&PT_REGS_PARM2(ctx));
    // bpf_trace_printk(fmt, sizeof(fmt), arg1, arg2, pid);

    // increment counts_map for this PID
    u64 zero = 0, *valp;
    valp = bpf_map_lookup_elem(&counts_map, &pid);
    if (!valp) {
        bpf_map_update_elem(&counts_map, &pid, &zero, BPF_ANY);
        valp = bpf_map_lookup_elem(&counts_map, &pid);
    }
    if (valp) {
        __sync_fetch_and_add(valp, 1);
    }
    return 0;
}
SEC("uprobe/libc.so.6:brk")
int brk_init(struct pt_regs *ctx) {
    long address;
    char fmt[] = "brk %ld %u\n";
    u32 pid = bpf_get_current_pid_tgid();
    bpf_probe_read(&address, sizeof(address), (void *)&PT_REGS_PARM1(ctx));
    bpf_trace_printk(fmt, sizeof(fmt), address, pid);
    return 0;
}
SEC("uretprobe/libc.so.6:brk")
int brk_finish(struct pt_regs *ctx) {
    int size;
    char fmt[] = "brkret %d %u\n";
    u32 pid = bpf_get_current_pid_tgid();
    bpf_probe_read(&size, sizeof(size), (void *)&PT_REGS_PARM1(ctx));
    if (size > 0) {
        bpf_trace_printk(fmt, sizeof(fmt), size, pid);
    }
    return 0;
}
SEC("uprobe/libc.so.6:sbrk")
int sbrk_init(struct pt_regs *ctx) {
    int size;
    char fmt[] = "sbrkret %d %u\n";
    u32 pid = bpf_get_current_pid_tgid();
    bpf_probe_read(&size, sizeof(size), (void *)&PT_REGS_PARM1(ctx));
    if (size > 0) {
        bpf_trace_printk(fmt, sizeof(fmt), size, pid);
    }
    return 0;
}
SEC("uretprobe/libc.so.6:sbrk")
int sbrk_finish(struct pt_regs *ctx) {
    long address;
    char fmt[] = "sbrkret %ld %u\n";
    u32 pid = bpf_get_current_pid_tgid();
    bpf_probe_read(&address, sizeof(address), (void *)&PT_REGS_PARM1(ctx));
    bpf_trace_printk(fmt, sizeof(fmt), address, pid);

    return 0;
}
char _license[] SEC("license") = "GPL";