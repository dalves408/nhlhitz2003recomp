#ifndef HITZ_PLAYER_ENVIRONMENT_H
#define HITZ_PLAYER_ENVIRONMENT_H
#include <stdlib.h>
#include <string.h>

/* The public player accepts only the runtime settings initialized by main.c.
 * Diagnostic/experimental switches are unavailable, including in toolkit code.
 * Inherited RECOMP_* values are cleared before these internal values are set.
 * Constant names let the optimizer discard disabled diagnostic branches. */
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) /* Read-only CRT getenv API, used behind this filter. */
#define HITZ_ENV_INLINE static __forceinline
#else
#define HITZ_ENV_INLINE static inline
#endif
HITZ_ENV_INLINE char *hitz_player_getenv(const char *name)
{
    if (!strncmp(name, "XRDBG_", 6) || !strcmp(name, "XBOX_LOG_LEVEL")) return NULL;
    if (!strncmp(name, "RECOMP_", 7)) {
#define HITZ_RUNTIME(key) if (!strcmp(name, "RECOMP_" key)) return getenv(name)
        HITZ_RUNTIME("D3D11_SHADERCACHE_DIR");
        HITZ_RUNTIME("PB_EXEC");
        HITZ_RUNTIME("PB_WALK");
        HITZ_RUNTIME("AC97_READY");
        HITZ_RUNTIME("USB");
        HITZ_RUNTIME("TITLE_KEVENTS");
        HITZ_RUNTIME("KEYBOARD");
        HITZ_RUNTIME("DISPATCH_GATE");
        HITZ_RUNTIME("DISC_BUFFERED");
        HITZ_RUNTIME("VBLANK");
        HITZ_RUNTIME("D3D11");
        HITZ_RUNTIME("GFX_THREAD");
        HITZ_RUNTIME("FLIP_PACING");
        HITZ_RUNTIME("D3D11_RC");
        HITZ_RUNTIME("D3D11_GPUVS");
        HITZ_RUNTIME("GPU_FFP");
        HITZ_RUNTIME("D3D11_GPUVS_ASYNC");
        HITZ_RUNTIME("TEXDEC_FAST");
        HITZ_RUNTIME("D3D11_SHADERCACHE");
        HITZ_RUNTIME("SHADER_PREWARM");
        HITZ_RUNTIME("D3D11_STATECACHE");
        HITZ_RUNTIME("D3D11_CONSTCACHE");
        HITZ_RUNTIME("PB_BULK");
        HITZ_RUNTIME("D3D11_SHAREZ");
        HITZ_RUNTIME("NV_BLIT");
        HITZ_RUNTIME("INLINE_SHORTFIX");
        HITZ_RUNTIME("INLINE_FFP");
#undef HITZ_RUNTIME
        return NULL;
    }
    return getenv(name);
}
#define getenv hitz_player_getenv
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif
