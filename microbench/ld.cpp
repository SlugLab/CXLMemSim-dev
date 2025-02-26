/*
 * Microbench testies for MLP and memory latency in CXLMS
 *
 *  By: Andrew Quinn
 *      Yiwei Yang
 *
 *  Copyright 2023 Regents of the Univeristy of California
 *  UC Santa Cruz Sluglab.
 */


#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cpuid.h>
#include <pthread.h>
#include <stdlib.h>

#include <sys/mman.h>


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MOVE_SIZE 128
#define MAP_SIZE  (long)(10241024)
#define CACHELINE_SIZE  64

#ifndef FENCE_COUNT
#define FENCE_COUNT 8
#endif

#define FENCE_BOUND (FENCE_COUNT * MOVE_SIZE)

// we need to jump in MOVE_SIZE increments otherwise segfault!

#define BODY(start)						\
  "xor %%r8, %%r8 \n"						\
  "LOOP_START%=: \n"						\
  "lea (%[" #start "], %%r8), %%r9 \n"				\
  "movdqa  (%%r9), %%xmm0 \n"					\
  "add $" STR(MOVE_SIZE) ", %%r8 \n"				\
  "cmp $" STR(FENCE_BOUND) ",%%r8\n"				\
  "jl LOOP_START%= \n"						\
  "mfence \n"
// 另一种方式是使用 lock 指令前缀来实现内存屏障
// 这个宏在特定场景下可以替换上面的宏
#define BODY_ALT(start)						\
  "xor %%r8, %%r8 \n"						\
  "LOOP_START%=: \n"						\
  "lea (%[" #start "], %%r8), %%r9 \n"				\
  "movdqa  (%%r9), %%xmm0 \n"					\
  "add $" STR(MOVE_SIZE) ", %%r8 \n"				\
  "cmp $" STR(FENCE_BOUND) ",%%r8\n"				\
  "jl LOOP_START%= \n"						\
  "lock addl $0, 0(%%rsp) \n"                                    \
  /* lock 指令前缀会强制实现完整的内存屏障 */
int main(int argc, char **argv) {

  // in principle, you would want to clear out cache lines (and the
  // pipeline) before doing any of the inline assembly stuff.  But,
  // that's hard.  And, its probably noise when you execute over
  // enough things.


  // allocate some meomery
  char *base =(char *) mmap(nullptr,
		    MAP_SIZE,
		    PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_PRIVATE,
		    -1,
		    0);

  if (base == MAP_FAILED) {
    fprintf(stderr, "oops, you suck %d\n", errno);
    return -1;
  }
  char *addr = NULL;

  intptr_t *iaddr = (intptr_t*) base;
  intptr_t hash = 0;
  struct timespec tstart = {0,0}, tend = {0,0};

  // Necessary so that we don't include allocation costs in our benchmark
  while (iaddr < (intptr_t *)(base + MAP_SIZE)) {
    hash = hash ^ (intptr_t) iaddr;
    *iaddr = hash;
    iaddr++;
  }

  // should flush everything from the cache. But, how big is the cache?
  addr = base;
  size_t large_buffer_size = 32 * 1024 * 1024; // 足够大以覆盖大多数缓存
  char* large_buffer = (char*)malloc(large_buffer_size);
  if (large_buffer) {
    for (size_t i = 0; i < large_buffer_size; i += CACHELINE_SIZE) {
      // 读取和写入大缓冲区，挤出目标内存的缓存行
      large_buffer[i] = (char)(i & 0xFF);
    }
    free(large_buffer);
  }

  clock_gettime(CLOCK_MONOTONIC, &tstart);
  for (int i=0; i<1e3; i++) {
    addr = base;
    while (addr < (base + MAP_SIZE)) {
      asm volatile(
		 BODY_ALT(addr)
		 :
		 : [addr] "r" (addr)
		 : "r8", "r9", "xmm0");

      addr += (FENCE_COUNT * MOVE_SIZE);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &tend);
    uint64_t nanos = (1000000000  * tend.tv_sec + tend.tv_nsec);
    nanos -= (1000000000 * tstart.tv_sec + tstart.tv_nsec);


  printf("%lu\n", nanos);
}
  return 0;
}
