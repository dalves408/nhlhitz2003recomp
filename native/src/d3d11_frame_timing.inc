/* Optional delayed GPU timestamps. Never wait/flush for query results. The
 * elapsed GPU timeline includes starvation between CPU submissions; it is not
 * a measurement of active GPU busy time. Match flip IDs with the frame CSV. */
#define GPU_TIME_SLOTS 32
typedef struct {
    ID3D11Query *disjoint, *first, *last;
    uint32_t flip;
    int pending;
} GpuTimeSlot;
static GpuTimeSlot s_gt[GPU_TIME_SLOTS];
static FILE *s_gt_out;
static int s_gt_checked, s_gt_active = -1;
static unsigned s_gt_cursor, s_gt_dropped;
static uint32_t s_gt_drop_flip = 0xFFFFFFFFu;

static void gpu_time_poll(void)
{
    unsigned i;
    for (i = 0; i < GPU_TIME_SLOTS; i++) {
        GpuTimeSlot *q = &s_gt[i];
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d;
        UINT64 a, b;
        if (!q->pending) continue;
        if (ID3D11DeviceContext_GetData(g.ctx, (ID3D11Asynchronous *)q->disjoint, &d, sizeof d, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;
        if (ID3D11DeviceContext_GetData(g.ctx, (ID3D11Asynchronous *)q->first, &a, sizeof a, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            ID3D11DeviceContext_GetData(g.ctx, (ID3D11Asynchronous *)q->last, &b, sizeof b, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;
        if (!d.Disjoint && d.Frequency && b >= a)
            fprintf(s_gt_out, "%u,%.6f,%u\n", q->flip, 1000.0*(double)(b-a)/(double)d.Frequency, s_gt_dropped);
        q->pending = 0;
    }
}

static void gpu_time_begin(void)
{
    unsigned i;
    if (!s_gt_checked) {
        const char *path = getenv("RECOMP_GPU_TIME_CSV");
        s_gt_checked = 1;
        if (!path || !*path) return;
        s_gt_out = fopen(path, "w");
        if (!s_gt_out) { logf_("cannot open GPU timing CSV: %s\n", path); return; }
        setvbuf(s_gt_out, NULL, _IOFBF, 16384);
        fputs("flip,gpu_timeline_ms,dropped_query_frames\n", s_gt_out);
        for (i = 0; i < GPU_TIME_SLOTS; i++) {
            D3D11_QUERY_DESC d = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
            if (FAILED(ID3D11Device_CreateQuery(g.dev, &d, &s_gt[i].disjoint))) break;
            d.Query = D3D11_QUERY_TIMESTAMP;
            if (FAILED(ID3D11Device_CreateQuery(g.dev, &d, &s_gt[i].first)) ||
                FAILED(ID3D11Device_CreateQuery(g.dev, &d, &s_gt[i].last))) break;
        }
        if (i != GPU_TIME_SLOTS) {
            logf_("GPU timestamps unavailable; timing disabled\n");
            for (i = 0; i < GPU_TIME_SLOTS; i++) {
                if (s_gt[i].disjoint) ID3D11Query_Release(s_gt[i].disjoint);
                if (s_gt[i].first) ID3D11Query_Release(s_gt[i].first);
                if (s_gt[i].last) ID3D11Query_Release(s_gt[i].last);
            }
            fclose(s_gt_out); s_gt_out = NULL;
            return;
        }
    }
    if (!s_gt_out || s_gt_active >= 0) return;
    gpu_time_poll();
    for (i = 0; i < GPU_TIME_SLOTS; i++) {
        unsigned slot = (s_gt_cursor+i)%GPU_TIME_SLOTS;
        if (!s_gt[slot].pending) {
            s_gt_active = (int)slot;
            s_gt[slot].flip = g.flips;
            ID3D11DeviceContext_Begin(g.ctx, (ID3D11Asynchronous *)s_gt[slot].disjoint);
            ID3D11DeviceContext_End(g.ctx, (ID3D11Asynchronous *)s_gt[slot].first);
            s_gt_cursor = (slot+1)%GPU_TIME_SLOTS;
            return;
        }
    }
    if(s_gt_drop_flip!=g.flips){s_gt_drop_flip=g.flips;s_gt_dropped++;}
}

static void gpu_time_end(void)
{
    if (s_gt_active < 0) return;
    GpuTimeSlot *q = &s_gt[s_gt_active];
    ID3D11DeviceContext_End(g.ctx, (ID3D11Asynchronous *)q->last);
    ID3D11DeviceContext_End(g.ctx, (ID3D11Asynchronous *)q->disjoint);
    q->pending = 1;
    s_gt_active = -1;
    if (!(g.flips % 64)) fflush(s_gt_out);
}
