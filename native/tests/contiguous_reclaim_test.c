#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t va) { (void)va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
void hitz_nis_read_trace(uint32_t buffer, uint32_t bytes, int64_t offset)
{ (void)buffer; (void)bytes; (void)offset; }
int nv2a_gpuvs_probe(const void *state) { (void)state; return 0; }
void nv2a_gpu_flush_pending(void) {}
void xrdbg_note_exit(const char *reason) { (void)reason; }
extern uint32_t xbox_ContiguousInUseBytes(void);
extern uint32_t xbox_ContiguousAlloc(uint32_t size, uint32_t alignment);
extern uint32_t xbox_ContiguousAllocEx(uint32_t size, uint32_t alignment, uint32_t low, uint32_t high);
extern int xbox_ContiguousFree(uint32_t va);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)

int main(int argc, char **argv)
{
    int immediate = argc > 1 && !strcmp(argv[1], "immediate");
    setenv("RECOMP_CONTIG_CAP_KB", "64", 1);
    if (immediate) setenv("RECOMP_HEAP_RECLAIM", "1", 1);
    else unsetenv("RECOMP_HEAP_RECLAIM");
    /* Exercise the actual runtime allocator without booting a guest. Its
     * default offset is zero until layout init; only map its backing window. */
    void *memory=VirtualAlloc((void *)(uintptr_t)XBOX_CONTIG_BASE,XBOX_CONTIG_SIZE,
                             MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(memory==(void *)(uintptr_t)XBOX_CONTIG_BASE);
    uint32_t a=xbox_ContiguousAlloc(8192,4096),b=xbox_ContiguousAlloc(8192,4096),c=xbox_ContiguousAlloc(8192,4096);
    CHECK(a&&b&&c&&a!=b&&b!=c);memset((void *)(uintptr_t)a,0xA5,8192);memset((void *)(uintptr_t)b,0xB6,8192);
    CHECK(xbox_ContiguousFree(a));
    if(immediate){
        CHECK(xbox_ContiguousInUseBytes()==16384);
        CHECK(xbox_ContiguousAlloc(8192,4096)==a);
    }else{
        CHECK(xbox_ContiguousFree(a)); /* Duplicate free must not count twice. */
        CHECK(xbox_ContiguousInUseBytes()==24576);
        uint32_t d=xbox_ContiguousAlloc(32768,4096);CHECK(d&&d!=a);
        CHECK(*(unsigned char *)(uintptr_t)a==0xA5); /* Not reused early. */
        CHECK(xbox_ContiguousAlloc(8192,4096)==a); /* Pressure retries from the original lower bound. */
        CHECK(*(unsigned char *)(uintptr_t)a==0);CHECK(*(unsigned char *)(uintptr_t)b==0xB6);
        CHECK(xbox_ContiguousInUseBytes()==57344);
        CHECK(!xbox_ContiguousAlloc(8192,4096)); /* Live memory is never reclaimed. */
        CHECK(xbox_ContiguousFree(d));CHECK(xbox_ContiguousFree(a));
        for(unsigned i=0;i<200;i++){
            uint32_t v=xbox_ContiguousAlloc(8192,8192);CHECK(v&&!(v&8191));
            CHECK(v!=b&&v!=c);CHECK(xbox_ContiguousFree(v));
            CHECK(*(unsigned char *)(uintptr_t)b==0xB6);
        }
        /* A deferred block must not be used outside an explicit physical range. */
        CHECK(!xbox_ContiguousAllocEx(8192,4096,0x20000,0x21fff));
    }
    CHECK(VirtualFree(memory,0,MEM_RELEASE));
    puts(immediate?"PASS: explicit immediate contiguous reclaim":"PASS: deferred contiguous reclaim, pressure retry, duplicate free, live blocks, alignment/range and repeated reuse");
    return 0;
}
