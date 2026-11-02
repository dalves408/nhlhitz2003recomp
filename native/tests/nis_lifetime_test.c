/* Exercise the production guest ABI wrapper and queue links with a small guest
 * memory fixture. The loader stub deliberately writes its selected buffer. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
__declspec(thread) uint32_t g_eax, g_ecx, g_esp;
#else
__thread uint32_t g_eax, g_ecx, g_esp;
#endif
ptrdiff_t g_xbox_mem_offset;
extern void sub_00147AE0(void);
#define M(p) (*(uint32_t *)(g_xbox_mem_offset+(uint32_t)(p)))
static unsigned calls, failed;
static uint32_t expected_slot;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %u: %s\n", __LINE__, #c); failed++; } } while (0)
void sub_00147050(void)
{
    CHECK(M(g_esp+4) == 0x12);
    g_eax = 0x6000;
    g_esp += 4; /* cdecl */
}
void sub_00146F60(void)
{
    CHECK(g_ecx == expected_slot);
    CHECK(M(g_esp+4) == 0x6000);
    CHECK(M(g_esp+8) == 1);
    M(g_ecx+0x34) = 5;
    M(M(g_ecx+0x18)) = 0x12345678; /* actual destination reuse */
    g_eax = 0xABCDEF;
    g_esp += 12; /* thiscall ret 8 */
    calls++;
}
static void run(uint32_t active, uint32_t busy0, uint32_t busy1, uint32_t tail,
                uint32_t selected)
{
    uint32_t s0=0x1000, s1=0x2000;
    memset((void *)g_xbox_mem_offset, 0, 0x500000);
    M(0x491F3C)=s0; M(0x491F40)=s1; M(0x491F44)=active; M(0x491F54)=tail;
    M(s0+0x34)=busy0; M(s1+0x34)=busy1;
    M(s0+0x18)=0x7000; M(s1+0x18)=0x8000;
    M(0x7000)=M(0x8000)=0xCAFEBABE;
    M(0x4000)=0xDEADC0DE; M(0x4004)=0x12; M(0x4008)=1;
    g_esp=0x4000; expected_slot=selected;
    sub_00147AE0();
    CHECK(g_esp == 0x4004); /* caller still owns both arguments */
    CHECK(M(0x4000) == 0xDEADC0DE);
    CHECK(M(selected+0x54) == 0);
    CHECK(M(0x491F44) == active); /* teardown remains the game's responsibility */
    CHECK(M(0x491F54) == (tail ? tail : selected));
    if (tail) CHECK(M(tail+0x54) == selected);
    if (active < 2 && selected != M(0x491F3C+4*active))
        CHECK(M(M(M(0x491F3C+4*active)+0x18)) == 0xCAFEBABE);
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)calloc(1,0x500000);
    if (!g_xbox_mem_offset) return 1;
    run(0,0,0,0x1000,0x2000); /* reproduced premature idle/reuse, chained NIS */
    run(1,0,0,0x2000,0x1000); /* opposite active slot */
    run(0,10,0,0x1000,0x2000); /* normal preload while playback is busy */
    run(0xFFFFFFFF,0,0,0,0x1000); /* initial NIS, no current actors */
    run(0xFFFFFFFF,10,0,0x1000,0x2000); /* existing occupied-slot selection */
    CHECK(calls == 5);
    free((void *)g_xbox_mem_offset);
    if (!failed) puts("PASS: NIS preload ownership, guest stack ABI and request queue links");
    return failed ? 1 : 0;
}
