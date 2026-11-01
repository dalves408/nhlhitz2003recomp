/* Exercise the production scanner with serialized/bulk mock data ports. The
 * real append algorithm is shared; fences/control methods always stay scalar. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../overrides/nv2a_bulk_append.h"
static uint8_t *ram;
static struct {uint32_t indices[64],inl[64],ic,vc,prim,last[2048],draws;} state;
static int fast;
static unsigned bulk_calls;
ptrdiff_t xbox_GetMemoryOffset(void){return (ptrdiff_t)ram-(ptrdiff_t)0x80000000u;}
void xbox_Nv2aAckBusyBits(void){}
volatile uint32_t *xbox_Nv2aRegPtr(uint32_t offset){static uint32_t value;(void)offset;return &value;}
void nv2a_pb_exec_report(void){}
int nv2a_pb_exec_report_requested(void){return 1;}
void nv2a_pb_exec_walk_begin(void){}
void nv2a_gpu_flush_pending(void){}
void nv2a_pb_exec_method(uint32_t subch,uint32_t method,uint32_t value)
{
    if(subch)return;
    state.last[(method&0x1ffc)/4]=value;
    if(method==0x17fc){if(!value)state.draws++;state.prim=value;return;}
    if(!state.prim)return;
    if(method==0x1800 && state.ic+2<=64){state.indices[state.ic++]=(uint16_t)value;state.indices[state.ic++]=(uint16_t)(value>>16);}
    if(method==0x1808 && state.ic<64)state.indices[state.ic++]=value;
    if(method==0x1818 && state.vc<64)state.inl[state.vc++]=value;
}
uint32_t nv2a_pb_exec_bulk(uint32_t subch,uint32_t method,const uint32_t *words,uint32_t n)
{
    if(!fast||subch||n<2||(method!=0x1800 && method!=0x1808 && method!=0x1818))return 0;
    bulk_calls++;nv2a_pb_exec_method(subch,method,words[0]);
    hitz_bulk_append(method,state.prim,state.indices,&state.ic,64,state.inl,&state.vc,64,words+1,n-1);
    state.last[method/4]=words[n-1];return n;
}
#include "../overrides/nv2a_pb_scan.c"
static int failed;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failed++;}}while(0)
static unsigned packet(uint32_t *out,unsigned at,uint32_t method,unsigned n)
{
    unsigned i;out[at++]=0x40000000u|(n<<18)|method;
    for(i=0;i<n;i++)out[at++]=(0x9a00u+i)<<16|(0x1200u+i);
    return at;
}
int main(void)
{
    unsigned n=0,mode,scan,shorten;uint32_t *commands,words[2],jumps,unknown;
    unsigned char reference[sizeof state];
    ram=calloc(1,PB_RAM_BYTES);if(!ram)return 1;commands=(uint32_t*)(ram+0x100);
    n=packet(commands,n,0x1800,9);n=packet(commands,n,0x1808,12);n=packet(commands,n,0x1818,20);
    n=packet(commands,n,0x1d70,3);commands[n++]=0x000417fcu;commands[n++]=0;
    for(scan=0;scan<2;scan++)for(shorten=0;shorten<2;shorten++){
        for(mode=0;mode<2;mode++){
            memset(&state,0,sizeof state);state.prim=3;fast=mode;bulk_calls=0;
            s_exec_enabled=1;s_scan_enabled=scan;jumps=unknown=0;
            words[mode]=pb_walk_segment(0x80000100u,0x80000100u+4*(n-(shorten?4:0)),0,&jumps,&unknown);
            CHECK(!jumps && !unknown);if(scan)CHECK(!bulk_calls);
            if(!mode)memcpy(reference,&state,sizeof state);else CHECK(!memcmp(reference,&state,sizeof state));
        }
        CHECK(words[0]==words[1]);
    }
    /* A partially submitted DMA packet must retain its pending method and
     * stop exactly at PUT. A later PUT consumes only the remaining payload. */
    /* Truncated header boundary: the existing walker reads its header word,
     * but must not turn an end before the payload into an unsigned span. */
    for(mode=1;mode<4;mode++){
        memset(&state,0,sizeof state);state.prim=3;fast=1;s_scan_enabled=0;
        commands[0]=0x40000000u|(7u<<18)|0x1800u;jumps=unknown=0;
        CHECK(pb_walk_segment(0x80000100u,0x80000100u+mode,0,&jumps,&unknown)==1);
        CHECK(state.ic==0 && state.vc==0);
    }
    for(mode=0;mode<2;mode++){
        uint32_t count=0,i;memset(&state,0,sizeof state);state.prim=3;fast=mode;
        s_exec_enabled=1;s_scan_enabled=0;s_get=0x100;s_pend_left=7;s_pend_i=0;s_pend_noninc=1;s_pend_method=0x1800;s_pend_subch=0;
        for(i=0;i<7;i++)commands[i]=0x43210000u+i;
        pb_consume((uint8_t*)xbox_GetMemoryOffset(),0x108,&count);
        CHECK(s_get==0x108 && s_pend_left==5 && count==2 && state.ic==4);
        pb_consume((uint8_t*)xbox_GetMemoryOffset(),0x11c,&count);
        CHECK(s_get==0x11c && !s_pend_left && count==7 && state.ic==14);
        if(!mode)memcpy(reference,&state,sizeof state);else CHECK(!memcmp(reference,&state,sizeof state));
    }
    /* RAM boundary: do not read past the mapped parameter span. */
    fast=1;memset(&state,0,sizeof state);state.prim=3;s_get=PB_RAM_BYTES-8;s_pend_left=7;s_pend_i=0;
    s_pend_method=0x1818;((uint32_t*)(ram+PB_RAM_BYTES-8))[0]=123;((uint32_t*)(ram+PB_RAM_BYTES-8))[1]=456;
    {uint32_t count=0;pb_consume((uint8_t*)xbox_GetMemoryOffset(),0x100,&count);CHECK(count==2 && s_get==PB_RAM_BYTES && s_pend_left==5 && state.vc==2);}
    free(ram);
    if(!failed)puts("PASS: production scanner scalar/bulk equivalence, survey fallback, truncated segments, split PUT, pending payload and RAM boundary");
    return failed?1:0;
}
