/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t */
#include <windows.h>
#include <stdlib.h>

/* Guest cache flushes order coherent host memory; GPU completion is supplied
 * separately by the pushbuffer executor, never synthesized by this helper. */
void hitz_guest_wbinvd(void)
{
    MemoryBarrier();
}

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern __declspec(thread) uint32_t g_eax;
extern __declspec(thread) uint32_t g_esp;
extern __declspec(thread) uint32_t g_ebx;
extern __declspec(thread) uint32_t g_ecx;
extern __declspec(thread) uint32_t g_edx;
extern ptrdiff_t g_xbox_mem_offset;

extern recomp_func_t recomp_lookup_kernel(uint32_t xbox_va);

extern void sub_0003D650(void);
/* Protect the actual match-update entry from boundary repair around its
 * previously discovered interior alias at 0xA61EC. Keep its body generated. */
void sub_000A61B0_enter(void)
{
    static unsigned shown;
    if (shown++ < 3) fprintf(stderr, "[MATCH] update callback reached\n");
}

extern void sub_0003F680(void);
static int hitz_load_batch_depth, hitz_load_batch_pending;

static int hitz_nis_trace_enabled(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("RECOMP_NIS_TRACE"); on = e && e[0] == '1'; }
    return on;
}


static uint32_t hitz_trace_u32(uint32_t p)
{
    uint32_t v = 0; SIZE_T got;
    ReadProcessMemory(GetCurrentProcess(), (const void *)((uintptr_t)g_xbox_mem_offset+p), &v, sizeof v, &got);
    return v;
}

static void hitz_nis_player_trace(const char *where, uint32_t player)
{
    unsigned i;
    fprintf(stderr, "[NIS-PLAYER] %s player=%08X state=%X flags=%X restore=%u anims",
            where, player, hitz_trace_u32(player+0x3368), hitz_trace_u32(player+0x33A4),
            hitz_trace_u32(player+0x188)&255);
    for (i=0; i<4; ++i) {
        uint32_t a = hitz_trace_u32(player+0x70+4*i);
        fprintf(stderr, " %08X:%X/%08X", a, hitz_trace_u32(a)&255, hitz_trace_u32(a+8));
    }
    fputc('\n', stderr);
}

void hitz_nis_read_trace(uint32_t buffer, uint32_t bytes, int64_t offset)
{
    unsigned t, i, j;
    if (!hitz_nis_trace_enabled() || bytes != 503296u) return;
    fprintf(stderr, "[NIS-REUSE] buffer=%08X bytes=%u offset=%lld active=%08X\n",
            buffer, bytes, (long long)offset, hitz_trace_u32(0x491F44));
    for (t=0; t<2; ++t) {
        uint32_t team = hitz_trace_u32(0x441B20+4*t), roster = hitz_trace_u32(team);
        if (!team || !roster) continue;
        for (i=0; i<7; ++i) {
            uint32_t player = hitz_trace_u32(roster+4*i);
            if (!player) continue;
            for (j=0; j<4; ++j) {
                uint32_t a = hitz_trace_u32(player+0x70+4*j), p = hitz_trace_u32(a+8);
                if (p >= buffer && (uint64_t)p < (uint64_t)buffer+bytes) {
                    hitz_nis_player_trace("before read", player);
                    break;
                }
            }
        }
    }
}

extern void sub_0006CEB0(void);
static void hitz_nis_restore_trace(void)
{
    uint32_t player = g_ecx;
    hitz_nis_player_trace("restore enter", player);
    sub_0006CEB0();
    hitz_nis_player_trace("restore exit", player);
}

/* Observe the NIS double-buffer state machine without changing completion or
 * reference counts. The animation crash follows reuse of one of its buffers. */
extern void sub_00146FF0(void);
static void hitz_nis_pump_trace(void)
{
    static uint32_t last[2][5];
    uint32_t before[2][5];
    unsigned i, j;
    for (i = 0; i < 2; ++i) {
        uint32_t p = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x491F3Cu + 4*i);
        uint32_t *m = (uint32_t *)((uintptr_t)g_xbox_mem_offset + p);
        before[i][0] = p; before[i][1] = m[0x34/4]; before[i][2] = m[1];
        before[i][3] = m[3]; before[i][4] = m[0x24/4];
        for (j = 0; j < 5; ++j) if (last[i][j] != before[i][j]) break;
        if (j < 5) {
            fprintf(stderr, "[NIS] slot=%u obj=%08X state=%u refs=%u/%u filepos=%08X active=%08X\n",
                    i, p, before[i][1], before[i][2], before[i][3], before[i][4],
                    *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x491F44u));
            for (j = 0; j < 5; ++j) last[i][j] = before[i][j];
        }
    }
    sub_00146FF0();
}

/* Hitz 2003's group completion is instruction-equivalent to Pro's 0x28A70:
 * decrement 0x43F3A4 and post event 0x117 when it reaches zero. During the
 * request batch (0x3F680), synchronous reads must not post "all done" after
 * each individual request. Preserve the count and coalesce that event once.
 */
static void hitz_group_done(void)
{
    volatile uint32_t *count = (volatile uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x0043F3A4u);
    if (hitz_load_batch_depth) {
        *count = *count - 1;
        if (!*count) hitz_load_batch_pending = 1;
        g_esp += 4;
        return;
    }
    sub_0003D650();
}

static void hitz_load_batch(void)
{
    hitz_load_batch_depth++;
    sub_0003F680();
    hitz_load_batch_depth--;
    if (!hitz_load_batch_depth && hitz_load_batch_pending) {
        volatile uint32_t *count = (volatile uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x0043F3A4u);
        uint32_t eax = g_eax, ecx = g_ecx, edx = g_edx;
        hitz_load_batch_pending = 0;
        if (!*count) {
            *count = 1;
            g_esp -= 4;
            *(uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp) = 0;
            sub_0003D650();
            g_eax = eax; g_ecx = ecx; g_edx = edx;
        }
    }
}

/* Diagnostic wrapper only: the event semantics live in kernel_bridge.c.
 * XPP at 0x002B397A / 0x002B44DE writes a stack KEVENT header directly,
 * then waits via this title's import at 0x002B9260. */
static void hitz_wait_trace(void)
{
    uint32_t *sp = (uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
    uint32_t ret = sp[0], obj = sp[1];
    uint32_t target = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x002B9260u);
    recomp_func_t original = recomp_lookup_kernel(target);
    /* D3D BlockOnTime queued its work before entering this interrupt wait.
     * The executor retires the real semaphore releases, but does not deliver
     * D3D's software-method-5 interrupt. Wait on the completion word instead.
     * At this call EBX is the fence record; its first dword is the target.
     * Verified in this XBE at 0x0021B5B8..0x0021B65B. */
    if (ret == 0x0021B65Bu) {
        uint32_t device = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x002359A0u);
        uint32_t word = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + device + 0x34u);
        uint32_t fence = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + g_ebx);
        volatile LONG *completed = (volatile LONG *)((uintptr_t)g_xbox_mem_offset + word);
        ULONGLONG began = GetTickCount64();
        static unsigned gpu_shown;
        if (word < 0x80000000u || word >= 0x84000000u || (word & 3u)) {
            fprintf(stderr, "[GPUWAIT] invalid completion word %08X\n", word);
            _Exit(87);
        }
        if (gpu_shown++ < 4)
            fprintf(stderr, "[GPUWAIT] record=%08X word=%08X target=%08X completed=%08X\n",
                    g_ebx, word, fence, (uint32_t)*completed);
        while ((int32_t)((uint32_t)InterlockedCompareExchange(completed, 0, 0) - fence) < 0) {
            if (GetTickCount64() - began > 10000) {
                fprintf(stderr, "[GPUWAIT] stalled: word=%08X completed=%08X target=%08X\n",
                        word, (uint32_t)*completed, fence);
                _Exit(87);
            }
            Sleep(1);
        }
        g_eax = 0;
        g_esp += 24; /* KeWaitForSingleObject: five arguments plus return */
        return;
    }
    static unsigned shown;
    int trace = ((ret >= 0x002B14A0u && ret < 0x002B9260u) ||
                 (ret >= 0x0020D500u && ret < 0x00238840u)) && shown++ < 64;
    ULONGLONG start = GetTickCount64();
    if (trace)
        fprintf(stderr, "[XPPWAIT] enter ret=%08X obj=%08X\n", ret, obj);
    if (original) original();
    if (trace)
        fprintf(stderr, "[XPPWAIT] return ret=%08X status=%08X after=%llu ms\n",
                ret, g_eax, (unsigned long long)(GetTickCount64() - start));
}

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    if (xbox_va == 0x0006CEB0u && hitz_nis_trace_enabled()) return hitz_nis_restore_trace;
    if (xbox_va == 0x00146FF0u) {
        static int trace = -1;
        if (trace < 0) { const char *e = getenv("RECOMP_NIS_TRACE"); trace = e && e[0] == '1'; }
        if (trace) return hitz_nis_pump_trace;
    }
    if (xbox_va == 0x0003D650u) return hitz_group_done;
    if (xbox_va == 0x0003F680u) return hitz_load_batch;
    if (xbox_va >= 0xFE000000u && g_xbox_mem_offset) {
        static int trace = -1;
        if (trace < 0) { const char *e = getenv("RECOMP_XPP_WAIT_LOG"); trace = e && e[0] == '1'; }
        if (xbox_va == *(uint32_t *)((uintptr_t)g_xbox_mem_offset + 0x002B9260u)) {
            uint32_t ret = *(uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
            if (trace || ret == 0x0021B65Bu) return hitz_wait_trace;
        }
    }
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    static uint64_t null_hits;
    if (!va && ++null_hits > 4 && null_hits % 1000 != 0) return;
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);
    if (g_xbox_mem_offset && g_esp >= 0x10000u && g_esp < 0x03FFF000u) {
        const uint32_t *sp = (const uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
        fprintf(stderr, "  caller return=%08X esp=%08X eax=%08X ecx=%08X ebx=%08X args=%08X,%08X,%08X,%08X\n",
                sp[0], g_esp, g_eax, g_ecx, g_ebx, sp[1], sp[2], sp[3], sp[4]);
    }

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Untranslated instructions ───────────────────────────────────────────
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, in place of the bare comment it used to leave. The
 * instruction is still a no-op; this only stops the omission being silent.
 * RECOMP_UNIMPL_TRAP=1 aborts at the first hit, at the guest address of the
 * cause rather than wherever the damage surfaces. */
#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}
