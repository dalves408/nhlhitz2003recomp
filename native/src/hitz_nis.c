/* Project-owned cutscene preload lifetime fix. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
extern __declspec(thread) uint32_t g_eax, g_ecx, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
static int hitz_nis_trace_enabled(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("RECOMP_NIS_TRACE"); on = e && e[0] == '1'; }
    return on;
}
extern void sub_00147050(void);
extern void sub_00146F60(void);
#define NIS_MEM32(p) (*(uint32_t *)((uintptr_t)g_xbox_mem_offset+(uint32_t)(p)))
static void hitz_nis_push(uint32_t v) { g_esp -= 4; NIS_MEM32(g_esp) = v; }

/* The streaming state becoming idle does not release the current actors.
 * sub_001466B0 restores them when the NEXT NIS starts. Prefer the other idle
 * slot until then, so preloading cannot overwrite animations still sampled by
 * those actors. All requests, callbacks and player restoration remain native. */
void sub_00147AE0(void)
{
    uint32_t entry = g_esp, slot, other, active, tail, request;
    hitz_nis_push(NIS_MEM32(entry+4));
    hitz_nis_push(0x00147AEAu);
    sub_00147050();
    g_esp += 4;
    request = g_eax;
    slot = NIS_MEM32(0x491F3C);
    if (NIS_MEM32(slot+0x34)) slot = NIS_MEM32(0x491F40);
    active = NIS_MEM32(0x491F44);
    if (active < 2 && slot == NIS_MEM32(0x491F3C+4*active)) {
        other = NIS_MEM32(0x491F3C+4*(active^1));
        if (!NIS_MEM32(other+0x34)) {
            if (hitz_nis_trace_enabled())
                fprintf(stderr, "[NIS-OWNERSHIP] preload slot %u instead of current slot %u\n", active^1, active);
            slot = other;
        }
    }
    tail = NIS_MEM32(0x491F54);
    if (tail) NIS_MEM32(tail+0x54) = slot;
    else NIS_MEM32(0x491F54) = slot;
    NIS_MEM32(slot+0x54) = 0;
    g_ecx = slot; g_eax = request;
    hitz_nis_push(NIS_MEM32(entry+8));
    hitz_nis_push(request);
    hitz_nis_push(tail ? 0x00147B1Fu : 0x00147B38u);
    sub_00146F60();
    g_esp += 4;
}
