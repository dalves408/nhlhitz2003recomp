#include <stdio.h>
#include "../src/player_environment.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    static const char *disabled[] = {
        "RECOMP_WATCHDOG_SECS", "RECOMP_HANG_DUMP", "RECOMP_CRASH_GUEST_DUMP",
        "RECOMP_D3D11_DEBUG", "RECOMP_KEYLOG", "RECOMP_FRAME_CAPTURE_REQUEST",
        "RECOMP_GPUVS_VERIFY", "RECOMP_BENCH_PLAN", "RECOMP_PAD_LIVE",
        "XRDBG_SELFTEST", "XBOX_LOG_LEVEL"
    };
    for (unsigned i = 0; i < sizeof disabled / sizeof disabled[0]; ++i) {
        CHECK(_putenv_s(disabled[i], "1") == 0);
        CHECK(getenv(disabled[i]) == NULL);
    }
    CHECK(_putenv_s("RECOMP_D3D11_SHADERCACHE_DIR", "test-cache") == 0);
    CHECK(!strcmp(getenv("RECOMP_D3D11_SHADERCACHE_DIR"), "test-cache"));
    CHECK(_putenv_s("RECOMP_GFX_THREAD", "0") == 0);
    CHECK(!strcmp(getenv("RECOMP_GFX_THREAD"), "0"));
    CHECK(_putenv_s("HITZ_TEST_VALUE", "normal environment") == 0);
    CHECK(!strcmp(getenv("HITZ_TEST_VALUE"), "normal environment"));
    puts("PASS: public player rejects debug switches and retains internal runtime settings");
    return 0;
}
