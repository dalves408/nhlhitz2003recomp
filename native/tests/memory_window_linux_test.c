#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "platform/win32_compat.h"
#include "../overrides/linux_guest_mapping.h"
int main(void)
{
    size_t bytes=65536;
    unsigned char *base=hitz_linux_reserve_guest((size_t)1<<32);assert(base);
    HANDLE ram=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,bytes,NULL);
    HANDLE pinned=CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,bytes,NULL);
    assert(ram&&pinned);
    unsigned char *low=hitz_linux_map_guest(ram,bytes,base);
    unsigned char *mirror=hitz_linux_map_guest(ram,bytes,base+0x04000000u);
    unsigned char *contig=hitz_linux_map_guest(pinned,bytes,base+0x80000000u);
    unsigned char *tiled=hitz_linux_map_guest(pinned,bytes,base+0xf0000000u);
    unsigned char *device=hitz_linux_commit_guest(base+0xfd000000u,bytes);
    assert(low&&mirror&&contig&&tiled&&device);
    low[123]=0x5a;assert(mirror[123]==0x5a&&contig[123]==0);
    contig[123]=0xa5;assert(tiled[123]==0xa5&&low[123]==0x5a);
    tiled[123]=0x3c;assert(contig[123]==0x3c);
    device[123]=0x17;assert(contig[123]==0x3c);
    unsigned char *foreign=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(foreign!=MAP_FAILED);foreign[0]=0xd7;
    assert(!hitz_linux_map_guest(ram,bytes,foreign));
    assert(!hitz_linux_commit_guest(foreign,bytes)&&foreign[0]==0xd7);
    assert(!munmap(foreign,bytes));
    assert(UnmapViewOfFile(low)&&UnmapViewOfFile(mirror));
    assert(UnmapViewOfFile(contig)&&UnmapViewOfFile(tiled));
    assert(VirtualFree(device,0,MEM_RELEASE));CloseHandle(ram);CloseHandle(pinned);
    hitz_linux_release_guest();
    base=hitz_linux_reserve_guest((size_t)1<<32);assert(base);hitz_linux_release_guest();
    puts("PASS: reserved guest window, RAM mirrors, separate pinned/tiled aliases, foreign mapping preserved");return 0;
}
