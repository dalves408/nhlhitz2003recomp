#include "../src/linux_performance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void parse(const char *text,HitzGpuCounters *v)
{FILE *f=fmemopen((void*)text,strlen(text),"r");assert(f);hitz_linux_gpu_parse(f,v);fclose(f);}
int main(void)
{
    assert(hitz_linux_cpu_percent(0,1000000000,1000000000,16)==6);
    assert(hitz_linux_cpu_percent(0,16000000000ull,1000000000,16)==100);
    assert(hitz_linux_cpu_percent(0,99999999999ull,1000000000,16)==100);
    assert(hitz_linux_cpu_percent(1,0,100,1)==-1);
    assert(hitz_linux_cpu_percent(0,0,0,1)==-1);
    HitzGpuCounters old={0},cur={0};
    const char *a="drm-client-id: 7\ndrm-pdev: 0000:01:00.0\ndrm-engine-gfx: 100000000 ns\ndrm-engine-capacity-gfx: 1\ndrm-cycles-gfx: 100000\n";
    const char *b="drm-client-id: 7\ndrm-pdev: 0000:01:00.0\ndrm-engine-gfx: 600000000 ns\n";
    parse(a,&old);parse(a,&old);assert(old.count==1);parse(b,&cur);
    assert(hitz_linux_gpu_percent(&old,&cur,1000000000)==50);
    assert(hitz_linux_gpu_percent(&cur,&cur,1000000000)==0);
    assert(hitz_linux_gpu_percent(&cur,&old,1000000000)==-1);
    parse("drm-client-id: 8\ndrm-pdev: 0000:01:00.0\ndrm-engine-gfx: 0 ns\n",&old);
    parse("drm-client-id: 8\ndrm-pdev: 0000:01:00.0\ndrm-engine-gfx: 250000000 ns\n",&cur);
    assert(hitz_linux_gpu_percent(&old,&cur,1000000000)==75);
    parse("drm-client-id: 9\ndrm-pdev: 0000:01:00.0\ndrm-engine-copy: 900000000 ns\n",&old);
    parse("drm-client-id: 9\ndrm-pdev: 0000:01:00.0\ndrm-engine-copy: 1800000000 ns\n",&cur);
    assert(hitz_linux_gpu_percent(&old,&cur,1000000000)==90);
    HitzGpuCounters unavailable={0};parse("pos: 0\nflags: 0100002\n",&unavailable);
    assert(hitz_linux_gpu_percent(&unavailable,&unavailable,1000000000)==-1);
    puts("PASS: Windows-compatible total CPU normalization; process GPU counters, deduplication, resets and unavailable telemetry");
    return 0;
}
