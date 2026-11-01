/* Exercise the executor's real lighting routine with captured net state and
 * independent directional-light fixtures. No GPU or guest assets required. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nv2a_ffp_material.h"

#define NV_LIGHTS 8
typedef struct { uint32_t size; } VertexAttr;
static struct { VertexAttr attr[16]; float imm_attr[16][4]; } s_gpu;
static uint32_t s_reg[0x2000/4];
static unsigned normal_reads, position_reads;
static float reg_f(unsigned address)
{ float v; memcpy(&v, &s_reg[address/4], sizeof v); return v; }
static void put_f(unsigned address, float v)
{ memcpy(&s_reg[address/4], &v, sizeof v); }
static const VertexAttr *color_attr(void) { return &s_gpu.attr[3]; }
static int ffp_flip_normals(void) { return 0; }
static int fetch_attr(const VertexAttr *a, uint32_t index, float out[4])
{
    (void)index;
    out[0] = out[1] = out[2] = 0; out[3] = 1;
    if (a == &s_gpu.attr[0]) { position_reads++; return 1; }
    if (a == &s_gpu.attr[2]) {
        normal_reads++;
        if (!a->size) return 0;
        out[2] = -1; return 1;
    }
    return 0;
}
#include "nv2a_ffp_lighting.inc"

static int expect(const char *name, int lit, const float rgb, const float alpha)
{
    float got[4]; unsigned k;
    int result = lit_color(0, got);
    if (result != lit) { fprintf(stderr, "%s: lighting returned %d, expected %d\n", name, result, lit); return 1; }
    if (!lit) return 0;
    for (k=0; k<4; ++k) {
        float want = k == 3 ? alpha : rgb;
        if (!(fabsf(got[k]-want) < 1e-6f)) {
            fprintf(stderr, "%s: channel %u got %g expected %g\n", name, k, got[k], want); return 1;
        }
    }
    return 0;
}

int main(void)
{
    unsigned k; int failed = 0;
    s_reg[0x0314/4] = 1;
    for (k=0; k<3; ++k) put_f(0x0A10+4*k, 43.0f/255.0f);
    put_f(0x03B4, .75f);
    /* Captured goal net: lighting enabled, no normal array, no active lights,
     * constant material, scene ambient 43/255. The old gate returned unlit white. */
    failed |= expect("net ambient without normals", 1, 43.0f/255.0f, .75f);
    if (normal_reads || position_reads) { fputs("ambient-only lighting fetched a normal/position\n", stderr); failed = 1; }
    s_reg[0x0314/4] = 0;
    failed |= expect("lighting disabled", 0, 0, 0);
    s_reg[0x0314/4] = 1;
    s_reg[0x03BC/4] = 1; /* one directional light */
    for (k=0; k<3; ++k) {
        put_f(0x0A10+4*k, .2f);
        put_f(0x0480+4*(4*k+k), 1); /* identity model-view */
        put_f(0x1000+4*k, .1f);
        put_f(0x100C+4*k, .5f);
    }
    put_f(0x103C, 1); /* direction +Z */
    s_gpu.imm_attr[2][2] = 1;
    failed |= expect("current normal without array", 1, .8f, .75f);
    s_gpu.attr[2].size = 3; /* array normal -Z, opposite the light */
    failed |= expect("array normal overrides current", 1, .3f, .75f);
    if (!failed) puts("PASS: net ambient without normals, lighting off, current normal, array normal");
    return failed;
}
