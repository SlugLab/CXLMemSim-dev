/*
* CXLMemSim lbr
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2025 Regents of the University of California
 *  UC Santa Cruz Sluglab.
 */

#include "lbr.h"
// PERF_RECORD_READ			= 8,

/*
* struct {
*	struct perf_event_header	header;
*
*	#
*	# Note that PERF_SAMPLE_IDENTIFIER duplicates PERF_SAMPLE_ID.
*	# The advantage of PERF_SAMPLE_IDENTIFIER is that its position
*	# is fixed relative to header.
*	#
*
*	{ u64			id;	  } && PERF_SAMPLE_IDENTIFIER
*	{ u64			ip;	  } && PERF_SAMPLE_IP
*	{ u32			pid, tid; } && PERF_SAMPLE_TID
*	{ u64			time;     } && PERF_SAMPLE_TIME
*	{ u64			addr;     } && PERF_SAMPLE_ADDR
*	{ u64			id;	  } && PERF_SAMPLE_ID
*	{ u64			stream_id;} && PERF_SAMPLE_STREAM_ID
*	{ u32			cpu, res; } && PERF_SAMPLE_CPU
*	{ u64			period;   } && PERF_SAMPLE_PERIOD
*
*	{ struct read_format	values;	  } && PERF_SAMPLE_READ
*
*	{ u64			nr,
*	  u64			ips[nr];  } && PERF_SAMPLE_CALLCHAIN
*
*	#
*	# The RAW record below is opaque data wrt the ABI
*	#
*	# That is, the ABI doesn't make any promises wrt to
*	# the stability of its content, it may vary depending
*	# on event, hardware, kernel version and phase of
*	# the moon.
*	#
*	# In other words, PERF_SAMPLE_RAW contents are not an ABI.
*	#
*
*	{ u32			size;
*	  char                  data[size];}&& PERF_SAMPLE_RAW
*
*	{ u64                   nr;
*	  { u64	hw_idx; } && PERF_SAMPLE_BRANCH_HW_INDEX
*        { u64 from, to, flags } lbr[nr];
*        #
*        # The format of the counters is decided by the
*        # "branch_counter_nr" and "branch_counter_width",
*        # which are defined in the ABI.
*        #
*        { u64 counters; } cntr[nr] && PERF_SAMPLE_BRANCH_COUNTERS
*   } && PERF_SAMPLE_BRANCH_STACK */
struct lbr {
    uint64_t from;
    uint64_t to;
    uint64_t flags;
};
struct cntr {
    uint64_t counters;
};
struct lbr_sample {
    perf_event_header header;
    uint32_t pid;
    uint32_t tid;
    uint64_t nr;
    uint64_t ips[4];
    uint32_t cpu;
    uint64_t timestamp;
    uint64_t nr2;
    uint64_t hw_idx;
    lbr lbrs[4];
    cntr counters[4];
};

LBR::LBR(pid_t pid) : pid(pid){
    // Configure perf_event_attr struct
    perf_event_attr pe = {
        .type = PERF_TYPE_RAW,
        .size = sizeof(perf_event_attr),
        .config = 0x7835, // mem_load_retired.l3_miss
        .sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_CPU | PERF_SAMPLE_TIME | PERF_SAMPLE_BRANCH_STACK,
        .read_format = PERF_FORMAT_TOTAL_TIME_ENABLED,
        .disabled = 1, // Event is initially disabled
        .exclude_kernel = 1,
        .exclude_hv = 1,
        .precise_ip = 1,
        .config1 = 3,
    }; 

    int cpu = -1; // measure on any cpu
    int group_fd = -1;
    unsigned long flags = 0;

    this->fd = perf_event_open(&pe, pid, cpu, group_fd, flags);
    if (this->fd == -1) {
        perror("perf_event_open");
        exit(EXIT_FAILURE);
    }

    this->start();
}

int LBR::read(CXLController *controller, LBRElem *elem) {
     if (this->fd < 0) {
        return 0;
    }

    if (mp == MAP_FAILED)
        return -1;

    int r = 0;
    perf_event_header *header;
    lbr_sample *data;
    uint64_t last_head;
    char *dp = ((char *)mp) + PAGE_SIZE;

    do {
        this->seq = mp->lock; // explicit copy
        barrier();
        last_head = mp->data_head;
        while ((uint64_t)this->rdlen < last_head) {
            header = reinterpret_cast<perf_event_header *>(dp + this->rdlen % DATA_SIZE);

            switch (header->type) {
            case PERF_RECORD_LOST:
                SPDLOG_DEBUG("received PERF_RECORD_LOST\n");
                break;
            case PERF_RECORD_SAMPLE:
                data = reinterpret_cast<lbr_sample *>(dp + this->rdlen % DATA_SIZE);

                if (header->size < sizeof(*data)) {
                    SPDLOG_DEBUG("size too small. size:{}\n", header->size);
                    r = -1;
                    continue;
                }
                if (this->pid == data->pid) {
                    SPDLOG_ERROR("pid:{} tid:{} time:{} addr:{} phys_addr:{} llc_miss:{} timestamp={}\n", data->pid,
                                 data->tid, data->nr, data->nr2, data->ips[0], data->cpu,
                                 data->timestamp, data->nr2,data->hw_idx, data->lbrs[0],data->counters[0]);
                    controller->insert(data->timestamp, data->ips, data->lbrs,  data->counters);
                    // elem->total++;
                    // elem->llcmiss = data->value; // this is the number of llc miss
                }
                break;
            case PERF_RECORD_THROTTLE:
                SPDLOG_DEBUG("received PERF_RECORD_THROTTLE\n");
                break;
            case PERF_RECORD_UNTHROTTLE:
                SPDLOG_DEBUG("received PERF_RECORD_UNTHROTTLE\n");
                break;
            case PERF_RECORD_LOST_SAMPLES:
                SPDLOG_DEBUG("received PERF_RECORD_LOST_SAMPLES\n");
                break;
            default:
                SPDLOG_DEBUG("other data received. type:{}\n", header->type);
                break;
            }

            this->rdlen += header->size;
        }

        mp->data_tail = last_head;
        barrier();
    } while (mp->lock != this->seq);

    return r;
}
int LBR::start() {
        if (this->fd < 0) {
        return 0;
    }
    if (ioctl(this->fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        perror("ioctl");
        return -1;
    }

    return 0;
}

int LBR::stop() {
    if (this->fd < 0) {
        return 0;
    }
    if (ioctl(this->fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
        perror("ioctl");
        return -1;
    }
}

LBR::~LBR() {
    this->stop();

    if (this->fd < 0) {
        return;
    }

    if (this->mp != MAP_FAILED) {
        munmap(this->mp, this->mplen);
        this->mp = static_cast<perf_event_mmap_page *>(MAP_FAILED);
        this->mplen = 0;
    }

    if (this->fd != -1) {
        close(this->fd);
        this->fd = -1;
    }

    this->pid = -1;
}