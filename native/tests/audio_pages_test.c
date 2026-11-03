#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MCPX_HW_MAX_VOICES 256
typedef uint64_t hwaddr;
static uint8_t *g_apu_ram_ptr;
static uint8_t named[16384];
static unsigned lookups;
static uint8_t *mcpx_apu_phys(uint64_t addr)
{
    uint32_t a=(uint32_t)addr&0x0FFFFFFFu;lookups++;
    return (uint8_t *)((uintptr_t)g_apu_ram_ptr+a+(a<(64u<<20)&&!named[a>>12]?0x80000000ull:0));
}
#include "../overrides/apu_voice_pages.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void map_page(uint32_t a)
{
    CHECK(VirtualAlloc(g_apu_ram_ptr+a,4096,MEM_COMMIT,PAGE_READWRITE));
    CHECK(VirtualAlloc((void *)((uintptr_t)g_apu_ram_ptr+0x80000000ull+a),4096,MEM_COMMIT,PAGE_READWRITE));
    memset(g_apu_ram_ptr+a,0x7f,4096);
}
int main(void)
{
    g_apu_ram_ptr=VirtualAlloc(NULL,0x84000000ull,MEM_RESERVE,PAGE_NOACCESS);CHECK(g_apu_ram_ptr);
    uint32_t a=0x200000,b=a+16*4096;map_page(a);map_page(a+4096);map_page(b);
    /* Persistent silent voices must not follow a later heap-name collision. */
    for(unsigned v=64;v<=67;v++)CHECK(vp_ldl(v,a)==0);
    named[a>>12]=1;
    for(unsigned v=64;v<=67;v++)CHECK(vp_ldl(v,a)==0);
    unsigned before=lookups;CHECK(vp_ldl(64,a)==0&&lookups==before);
    CHECK(vp_ldl(70,a)==0x7f7f7f7f); /* A fresh heap voice picks heap. */
    named[a>>12]=0;CHECK(vp_ldl(70,a)==0x7f7f7f7f); /* Keep that meaning too. */
    vp_pages_forget(70);CHECK(vp_ldl(70,a)==0); /* Reassignment picks afresh. */
    named[a>>12]=1;vp_pages_forget(64);CHECK(vp_ldl(64,a)==0x7f7f7f7f);
    CHECK(vp_ldl(65,a)==0); /* Reset affects only its own voice. */
    CHECK(vp_ldub(65,b)==0);CHECK(vp_ldl(65,a)==0); /* No 16-slot eviction. */
    /* Stream copies and multi-byte PCM reads resolve each crossed page. */
    named[(a+4096)>>12]=1;
    uint8_t bytes[8];vp_read(65,a+4092,bytes,sizeof bytes);
    for(unsigned i=0;i<8;i++)CHECK(bytes[i]==(i<4?0:0x7f));
    CHECK(vp_lduw(65,a+4095)==0x7f00);CHECK(vp_ldl(65,a+4094)==0x7f7f0000);
    CHECK(vp_phys(65,0x80000000ull+a)==vp_phys(65,a)); /* physical aliases */
    memset(s_vp_page_choice,0,sizeof s_vp_page_choice);
    CHECK(vp_ldl(65,a)==0x7f7f7f7f);
    before=lookups;CHECK(vp_phys(65,0x04000000)==mcpx_apu_phys(0x04000000));CHECK(lookups==before+2);
    CHECK(VirtualFree(g_apu_ram_ptr,0,MEM_RELEASE));
    puts("PASS: silent voice collisions, independent heap voices, restart/reset, full-page retention, boundary reads and unambiguous addresses");return 0;
}
