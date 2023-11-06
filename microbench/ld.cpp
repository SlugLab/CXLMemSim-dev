#include "uarch.h"
#include <unistd.h>
int main() {
    int i;
    
    long seed = 0xdeadbeef1245678;
    long *seedptr = &seed;

    // should be based upon the size, no?
    int access_size = 4;
    long size  = 4096 * 64;

    uint64_t access_mask = size - 1;
    access_mask = access_mask - (access_mask % access_size);

    int trials = 100;
    int ret;

    long long base_rdtscp = 0, base_nanos = 0;    
    long long par_rdtscp = 0, par_nanos = 0;
    long long seq_rdtscp = 0, seq_nanos = 0;



    seed = 0xdeadbeef1245678;
    for (i = 0; i < trials; i++) {
      char *buf = static_cast<char *>(malloc(size));
      buf = buf + access_size - (((long)buf) % access_size);  

      BEFORE(buf, size, base);
      LOAD_NEW(buf, seedptr, access_mask);
      AFTER;

      base_rdtscp += diff;
      base_nanos += c_ntload_end - c_store_start;
    }
    

    
#ifdef OPERATION
#undef OPERATION
#define OPERATION				\
      "mov 0x0(%%r9), %%r13 \n"\
	"clflush (%%r9) \n"
#endif
    
    for (i = 0; i < trials; i++) {
      char *buf = static_cast<char *>(malloc(size));
      buf = buf + access_size - (((long)buf) % access_size);  

      BEFORE(buf, size, par);
      LOAD_NEW(buf, seedptr, access_mask);
      AFTER;

      par_rdtscp += diff;
      par_nanos += c_ntload_end - c_store_start;
    }

#ifdef OPERATION
#undef OPERATION
#define OPERATION				\
    "mov 0x0(%%r9), %%r13 \n"			\
      "clflush (%%r9) \n"			\
      "mfence\n"
#endif
    
    for (i = 0; i < trials; i++) {
      char *buf = static_cast<char *>(malloc(size));
      buf = buf + access_size - (((long)buf) % access_size);  

      BEFORE(buf, size, seq);
      LOAD_NEW(buf, seedptr, access_mask);
      AFTER;

      seq_rdtscp += diff;
      seq_nanos += c_ntload_end - c_store_start;
    }

    // subtract overhead and divide
    par_rdtscp -= base_rdtscp;
    par_rdtscp /= (trials * 16);
    par_nanos -= base_nanos;
    par_nanos /= (trials * 16);
    
    seq_rdtscp -= base_rdtscp;
    seq_rdtscp /= (trials * 16);
    seq_nanos -= base_nanos;
    seq_nanos /= (trials * 16);
    
    printf("%d trials, 16 par , rdtscp:%7lld, nanos:%7lld\n", trials, par_rdtscp, par_nanos);
    printf("%d trials, 16 seq , rdtscp:%7lld, nanos:%7lld\n", trials, seq_rdtscp, seq_nanos);
    return 0;
}
