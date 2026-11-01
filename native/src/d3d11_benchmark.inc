/* Opt-in scale crossover in ONE match. Applied only after a presented frame;
 * normal RT-reset logic rebuilds surfaces before the next clear/draw. No
 * saved settings or guest clocks are changed. Scene evolution is not frozen. */
static void benchmark_plan_tick(void)
{
    static int loaded, count, next;
    static struct { double seconds; int scale, bindings, constants, progress, bulk, batch, reports, ack_yield; } steps[64];
    static ULONGLONG process_boot_ms;
    FILETIME c, e, k, u, now;
    if (!loaded) {
        const char *path = getenv("RECOMP_BENCH_PLAN");
        FILE *f;
        char line[256];
        loaded = 1;
        if (!path || !*path) return;
        if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return;
        GetSystemTimeAsFileTime(&now);
        { ULARGE_INTEGER a, b; a.LowPart = now.dwLowDateTime; a.HighPart = now.dwHighDateTime;
          b.LowPart = c.dwLowDateTime; b.HighPart = c.dwHighDateTime;
          process_boot_ms = GetTickCount64()-(a.QuadPart-b.QuadPart)/10000; }
        f = fopen(path, "r");
        if (!f) { logf_("benchmark plan not found: %s\n", path); return; }
        while (count < 64 && fgets(line, sizeof line, f)) {
            double sec; int scale, bindings=-1, constants=-1, progress=-1, bulk=-1, batch=-1, reports=-1, ack_yield=-1;
            if (sscanf(line, "%lf %d %d %d %d %d %d %d %d", &sec, &scale, &bindings, &constants, &progress, &bulk, &batch, &reports,&ack_yield) < 2) continue;
            if (sec < 0 || scale < 1 || scale > 5 || (count && sec <= steps[count-1].seconds)) {
                logf_("invalid benchmark scale plan; disabled\n"); count = 0; break;
            }
            steps[count].seconds = sec; steps[count].scale = scale;
            steps[count].bindings=bindings;steps[count].constants=constants;steps[count].progress=progress;steps[count].bulk=bulk;steps[count].batch=batch;steps[count].reports=reports;count++;
            steps[count-1].ack_yield=ack_yield;
        }
        fclose(f);
    }
    if (next < count && (double)(GetTickCount64()-process_boot_ms)/1000 >= steps[next].seconds) {
        s_scale = steps[next].scale;
        if(steps[next].bindings>=0){s_bind_mode=steps[next].bindings!=0;bind_invalidate();}
        if(steps[next].constants>=0){s_upload_mode=steps[next].constants!=0;constants_invalidate();}
        if(steps[next].progress>=0)nv2a_pbx_progress_logs(steps[next].progress);
        if(steps[next].bulk>=0)nv2a_pbx_bulk_mode(steps[next].bulk);
        if(steps[next].batch>=0){batch_flush();s_batch_mode=steps[next].batch!=0;}
        if(steps[next].reports>=0)nv2a_pbx_report_mode(steps[next].reports);
        if(steps[next].ack_yield>=0)nv2a_pbx_ack_yield_mode(steps[next].ack_yield);
        logf_("BENCH phase=%d tick_ms=%llu scale=%d msaa=%d aniso=%d bindings=%d constants=%d progress=%d bulk=%d batch=%d reports=%d ack_yield=%d\n", next,
              (unsigned long long)GetTickCount64(), s_scale, s_msaa, s_aniso, bind_enabled(), upload_enabled(), steps[next].progress, steps[next].bulk, steps[next].batch, steps[next].reports,steps[next].ack_yield);
        next++;
    }
}
