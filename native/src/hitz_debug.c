/**
 * NHL Hitz 2003 - xrdbg debug kit glue (HITZ_DEBUGKIT builds only)
 *
 * Everything game-specific about the kit lives here; tools/xrdbg is vendored
 * unchanged (see tools/xrdbg/CUSTOMIZING.md). Files go to
 * %LOCALAPPDATA%\NHLHitz2003\debug, next to the renderer's settings.ini.
 *
 * Hook points elsewhere (all under #ifdef XRDBG):
 *   src/main.c            hitz_debug_init() first in WinMain, hitz_crash_context()
 *   src/backend_d3d11.c   hitz_flip_count(), DEL in quit_key(), screenshots in
 *                         be_flip(), exit notes on every quit path
 *   overrides/kernel_hal.c  exit notes on KeBugCheck(Ex) / HalReturnToFirmware
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xrdbg.h"

#ifndef HITZ_DEBUG_TAG
#define HITZ_DEBUG_TAG "debug"
#endif

extern ptrdiff_t g_xbox_mem_offset;          /* host = guest + offset (xbox_memory_layout.c) */
extern uint64_t hitz_flip_count(void);       /* backend_d3d11.c: frames presented */
extern void hitz_crash_context(PEXCEPTION_POINTERS ep);   /* main.c: guest regs + call chain */

/* The RECOMP_* switches change rendering, pacing and audio enough that a log
 * is unreadable without them. At the header only the user's own are set
 * (main.c fills in its defaults later); a mark prints the effective set. */
static void print_recomp_env(const char *tag)
{
    LPCH env = GetEnvironmentStringsA(), p;
    if (!env) return;
    for (p = env; *p; p += strlen(p) + 1)
        if (!strncmp(p, "RECOMP_", 7))
            fprintf(stderr, "%s %s\n", tag, p);
    FreeEnvironmentStringsA(env);
}

static void header_extra(void)
{
    fprintf(stderr, "xbe         : NHL Hitz 2003 (USA) title 0x4D57000B, entry 0x001C5F4E, XDK 4531\n");
    fprintf(stderr, "renderer    : D3D11 (NV2A push-buffer executor)\n");
    print_recomp_env("env (user)  :");
}

static void on_mark(int n)
{
    fprintf(stderr, "[MARK-HITZ] #%d frames=%llu, effective switches:\n", n,
            (unsigned long long)hitz_flip_count());
    print_recomp_env("[MARK-HITZ]  ");
}

void hitz_debug_init(void)
{
    XrDbgConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.game_name = "NHL Hitz 2003";
    cfg.data_folder = "NHLHitz2003";
    cfg.build_tag = HITZ_DEBUG_TAG;
    cfg.window_class = "NhlD3D11Backend";
    cfg.settings_file = "settings.ini";
    cfg.frame_count = hitz_flip_count;
    cfg.print_crash_context = hitz_crash_context;
    cfg.header_extra = header_extra;
    cfg.on_mark = on_mark;
    /* Guest RAM (64 MB from VA 0) and the contiguous window the GPU and
     * DSOUND use (64 MB at 0x80000000). g_xbox_mem_offset is still 0 here;
     * the kit reads it through the pointer when a dump is written. */
    cfg.guest_offset = &g_xbox_mem_offset;
    cfg.n_dump_ranges = 2;
    cfg.dump_ranges[0].guest_addr = 0x00000000u;
    cfg.dump_ranges[0].size = 64u << 20;
    cfg.dump_ranges[1].guest_addr = 0x80000000u;
    cfg.dump_ranges[1].size = 64u << 20;
    XRDBG_INIT(&cfg);
}
