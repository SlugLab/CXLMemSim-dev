/*
 * CXLMemSim bpftime runtime
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "bpf_attach_ctx.hpp"
#include "bpftime_helper_group.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm_internal.hpp"
#include "handler/prog_handler.hpp"
#include <bpftime_shm.hpp>
#include <bpf/bpf.h>

extern "C" {
uint64_t bpftime_redirect_map_runtime(uint64_t map, __u64 key, __u64 flags);
uint64_t bpftime_xdp_adjust_head_runtime(struct xdp_md_userspace* xdp, int offset);
long bpftime_xdp_load_bytes_runtime(struct xdp_md_userspace *xdp_md, __u32 offset, void *buf, __u32 len);
}
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>
#include <time.h>
#include <fstream>

typedef signed char __s8;
typedef unsigned char __u8;
typedef short int __s16;
typedef short unsigned int __u16;
typedef int __s32;
typedef unsigned int __u32;
typedef long long int __s64;
typedef long long unsigned int __u64;
typedef __s8 s8;
typedef __u8 u8;
typedef __s16 s16;
typedef __u16 u16;
typedef __s32 s32;
typedef __u32 u32;
typedef __s64 s64;
typedef __u64 u64;
using namespace bpftime;
using namespace std;

static int selected_handler_id = -1;

static bpftime_prog *entry_prog = nullptr;

bpftime::bpftime_helper_info bpf_redirect_map = {
	.index = 51,
	.name = "bpf_redirect_map",
	.fn = (void *)bpftime_redirect_map_runtime
};
bpftime::bpftime_helper_info xdp_adjust_head = {
    .index = 52,
    .name = "xdp_adjust_head",
    .fn = (void *)bpftime_xdp_adjust_head_runtime
};
bpftime::bpftime_helper_info bpf_xdp_load_bytes = {
    .index = 52,
    .name = "bpf_xdp_load_bytes",
    .fn = (void *)bpftime_xdp_load_bytes_runtime
};

static bool if_enable_jit()
{
	char *env = getenv("DISABLE_JIT");
	if (env != nullptr) {
		printf("disable JIT\n");
	}
	return env == nullptr;
}

int get_aot_object(std::vector<uint8_t> &buf)
{
	if (getenv("AOT_OBJECT_NAME") == nullptr) {
		fprintf(stderr, "No AOT object found\n");
		return -1;
	}
	const char *name = getenv("AOT_OBJECT_NAME");
	FILE *fp = fopen(name, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open file %s\n", name);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf.resize(size);
	size_t read_size = fread(buf.data(), 1, size, fp);
	if (read_size != size) {
		fprintf(stderr, "Failed to read file %s\n", name);
		return -1;
	}
	fclose(fp);
	return 0;
}

int read_file(const std::string &file, std::vector<uint8_t> &data)
{
	std::ifstream in(file, std::ios::binary);
	if (!in) {
		cerr << "Failed to open file " << file << endl;
		return 1;
	}
	in.seekg(0, std::ios::end);
	size_t size = in.tellg();
	in.seekg(0, std::ios::beg);
	data.resize(size);
	in.read((char *)data.data(), size);
	in.close();
	return 0;
}

static int load_ebpf_programs()
{
	const handler_manager *manager =
		shm_holder.global_shared_memory.get_manager();
	size_t handler_size = manager->size();

	bool using_aot;
	std::vector<uint8_t> aot_buf;
	if (get_aot_object(aot_buf) == 0) {
		using_aot = true;
	} else {
		using_aot = false;
	}
	for (size_t i = 0; i < manager->size(); i++) {
		if (std::holds_alternative<bpf_prog_handler>(
			    manager->get_handler(i))) {
			const auto &prog = std::get<bpf_prog_handler>(
				manager->get_handler(i));
			// temp work around: we need to create new attach points
			// in the runtime
			auto new_prog = new bpftime_prog(prog.insns.data(),
							 prog.insns.size(),
							 prog.name.c_str());
			printf("find eBPF program %s   %ld\n", prog.name.c_str(),
			       prog.insns.size());
			bpftime_helper_group::get_kernel_utils_helper_group()
				.add_helper_group_to_prog(new_prog);
			bpftime_helper_group::get_shm_maps_helper_group()
				.add_helper_group_to_prog(new_prog);
			// new_prog->bpftime_prog_register_raw_helper(csum_diff);
			// new_prog->bpftime_prog_register_raw_helper(
				// xdp_adjust_tail);
			new_prog->bpftime_prog_register_raw_helper(
				bpf_redirect_map);
			new_prog->bpftime_prog_register_raw_helper(
				xdp_adjust_head);
			new_prog->bpftime_prog_register_raw_helper(
				bpf_xdp_load_bytes);
			if (using_aot) {
				// load aot object from share memory
				int res = new_prog->load_aot_object(aot_buf);
				if (res < 0) {
					fprintf(stderr,
						"Failed to load aot object %s\n",
						prog.name.c_str());
					return -1;
				}
				printf("load aot object %s\n",
				       prog.name.c_str());

			} else {
				int res = new_prog->bpftime_prog_load(
					if_enable_jit());
				if (res < 0) {
					fprintf(stderr,
						"Failed to load eBPF program %s\n",
						prog.name.c_str());
					return -1;
				}
			}

			printf("load eBPF program %s\n", prog.name.c_str());
			if (prog.name == "xdp_pass" || prog.name == "balancer_ingres") {
				entry_prog = new_prog;
				printf("set entry program %s\n",
				       prog.name.c_str());
			}
			return 0;
		}
	}
	return 0;
}
static std::map<int, int> dev_hash_map = {};

static void *elem_lookup_static(int id, const void *key, bool from_syscall)
{
    auto k = *(int *)key;
    auto it = dev_hash_map.find(k);
    if (it == dev_hash_map.end())
        return nullptr;
    return &it->second;
}

static long elem_update_static(int id, const void *key, const void *value,
                   uint64_t flags, bool from_syscall)
{
    auto k = *(int *)key;
    auto v = *(int *)value;
    dev_hash_map[k] = v;
    return 0;
}

static long elem_delete_static(int id, const void *key, bool from_syscall)
{
    auto k = *(int *)key;
    auto it = dev_hash_map.find(k);
    if (it == dev_hash_map.end())
        return -1;
    dev_hash_map.erase(it);
    return 0;
}

static int elem_get_next_key_static(int id, const void *key, void *next_key, bool from_syscall)
{
    auto k = *(int *)key;
    auto it = dev_hash_map.upper_bound(k);
    if (it == dev_hash_map.end())
        return -1;
    *(int *)next_key = it->first;
    return 0;
}

bpftime::bpftime_map_ops dev_map_ops{
    .elem_lookup = elem_lookup_static,
    .elem_update = elem_update_static,
    .elem_delete = elem_delete_static,
    .map_get_next_key = elem_get_next_key_static,
};


static int register_maps()
{
	bpftime_register_map_ops(
		(int)bpftime::bpf_map_type::BPF_MAP_TYPE_DEVMAP, &dev_map_ops);
	return 0;
}

extern "C" int ebpf_module_init()
{
	bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	load_ebpf_programs();
	register_maps();

	// Example: after registering maps, read the counts_map once
	// read_counts_map();
	return -1;
}

extern "C" int ebpf_module_run_at_handler(void *mem, uint64_t mem_size,
					  uint64_t *ret)
{
	if (!entry_prog) {
		fprintf(stderr, "No prog found\n");
		return 0;
	}
	return entry_prog->bpftime_prog_exec(mem, mem_size, ret);
}

int run_xdp_and_measure(bpftime_prog &prog, std::vector<uint8_t> &data_in,
			int repeat_N)
{
	// Tes the program
	uint64_t return_val;
	void *memory = data_in.data();
	size_t memory_size = data_in.size();

	// start timer
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// run the program
	// for (int i = 0; i < repeat_N; i++) {
	// 	prog.bpftime_prog_exec(&xdp, sizeof(xdp), &return_val);
	// }
	// end timer
	clock_gettime(CLOCK_MONOTONIC, &end);
	// get avg time in ns
	uint64_t time_ns = (end.tv_sec - start.tv_sec) * 1000000000 +
			   (end.tv_nsec - start.tv_nsec);
	time_ns /= repeat_N;
	cout << "Time taken: " << time_ns << " ns" << endl;
	cout << "Return value: " << return_val << endl;
	return 0;
}

int bpftime_run_xdp_program(int id, const std::string &data_in_file,
			    int repeat_N, const std::string &run_type)
{
	cout << "Running eBPF program with id " << id << " and data in file "
	     << data_in_file << endl;
	cout << "Repeat N: " << repeat_N << " with run type " << run_type
	     << endl;
	std::vector<uint8_t> data_in;
	if (read_file(data_in_file, data_in) != 0) {
		cerr << "Failed to read data in file" << endl;
		return 1;
	}
	bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	const handler_manager *manager =
		shm_holder.global_shared_memory.get_manager();
	size_t handler_size = manager->size();
	if (id >= handler_size || id < 0) {
		cerr << "Invalid id " << id << " not exist" << endl;
		return 1;
	}
	if (std::holds_alternative<bpf_prog_handler>(
		    manager->get_handler(id))) {
		const auto &prog =
			std::get<bpf_prog_handler>(manager->get_handler(id));
		auto new_prog =
			bpftime_prog(prog.insns.data(), prog.insns.size(),
				     prog.name.c_str());
		bpftime::bpftime_helper_group::get_kernel_utils_helper_group()
			.add_helper_group_to_prog(&new_prog);
		bpftime::bpftime_helper_group::get_shm_maps_helper_group()
			.add_helper_group_to_prog(&new_prog);
		new_prog.bpftime_prog_register_raw_helper(bpf_redirect_map);
		new_prog.bpftime_prog_register_raw_helper(xdp_adjust_head);
		if (run_type == "JIT") {
			new_prog.bpftime_prog_load(true);
		} else if (run_type == "AOT") {
			if (prog.aot_insns.size() == 0) {
				cerr << "AOT instructions not found" << endl;
				return 1;
			}
			new_prog.load_aot_object(std::vector<uint8_t>(
				prog.aot_insns.begin(), prog.aot_insns.end()));
		} else if (run_type == "INTERPRET") {
			new_prog.bpftime_prog_load(false);
		}
		return run_xdp_and_measure(new_prog, data_in, repeat_N);
	} else {
		cerr << "Invalid id " << id << " not a bpf program" << endl;
		return 1;
	}
	return 0;
}


struct xdp_buff {
	void *data;
	void *data_end;
	void *data_meta;
	void *data_hard_start;
	struct xdp_rxq_info *rxq;
	struct xdp_txq_info *txq;
	__u32 frame_sz;
	__u32 flags;
};

enum xdp_buff_flags {
	XDP_FLAGS_HAS_FRAGS = 1,
	XDP_FLAGS_FRAGS_PF_MEMALLOC = 2,
};

typedef s64 ktime_t;

struct skb_shared_hwtstamps {
	union {
		ktime_t hwtstamp;
		void *netdev_data;
	};
};

typedef struct {
	int counter;
} atomic_t;

typedef struct bio_vec skb_frag_t;

struct skb_shared_info {
	__u8 flags;
	__u8 meta_len;
	__u8 nr_frags;
	__u8 tx_flags;
	short unsigned int gso_size;
	short unsigned int gso_segs;
	struct sk_buff *frag_list;
	struct skb_shared_hwtstamps hwtstamps;
	unsigned int gso_type;
	__u32 tskey;
	atomic_t dataref;
	unsigned int xdp_frags_size;
	void *destructor_arg;
	// skb_frag_t frags[17];
};
typedef void (*redirect_call_back_func)(void *data, int ifindex);

redirect_call_back_func redirect_map_callback = NULL;

void register_redirect_map_callback(int map_id, redirect_call_back_func cb)
{
	redirect_map_callback = cb;
}

uint64_t bpftime_redirect_map_runtime(uint64_t map, __u64 key, __u64 flags)
{
	if (redirect_map_callback) {
		// redirect_map_callback(map, key);
		return XDP_TX;
	} else {
		printf("redirect_map_callback is NULL\n");
		return XDP_DROP;
	}
}

uint64_t bpftime_xdp_adjust_head_runtime(xdp_md_userspace* xdp, int offset) {
	// Do nothing because we don't use xdp meta data
	printf("bpftime_xdp_adjust_head_runtime is not supported\n");
	void *data = malloc(100);
	// xdp->data = data;
	return 0;
}

long bpftime_xdp_load_bytes_runtime(xdp_md_userspace *xdp_md, __u32 offset, void *buf, __u32 len) {
	void *data = malloc(100);
	memcpy(buf, data, len);
	return 0;
}