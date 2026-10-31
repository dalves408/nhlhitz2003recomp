#include <math.h>
#include <stdio.h>
#include "nv2a_ffp_material.h"

static int check(unsigned mode, const float scene[3], const float emission[3],
                 const float diffuse[4], const float specular[4], const float want[4])
{
    float got[4]; unsigned k;
    hitz_ffp_material_base(got, mode, scene, emission, diffuse, specular, .75f);
    for (k = 0; k < 4; k++) {
        if (!(fabsf(got[k] - want[k]) < 1e-6f)) {
            fprintf(stderr, "mode %u component %u: got %g expected %g\n", mode, k, got[k], want[k]);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    const float dark[3] = {0,0,0}, ambient[3] = {.1f,.2f,.3f};
    const float material[3] = {.2f,.4f,.6f};
    const float diffuse[4] = {.5f,.25f,1,.8f}, specular[4] = {.1f,.2f,.3f,.4f};
    const float constant[4] = {.1f,.2f,.3f,.75f};
    /* Captured rink state: ambient from vertex color (0x4), no lights and
     * zero scene ambient. Ignoring the material multiplier made it black. */
    const float rink[4] = {.1f,.1f,.6f,.75f};
    const float both[4] = {.6f,.35f,1.6f,.8f};
    const float spec_emission[4] = {.12f,.28f,.48f,.4f};
    int failed = 0;
    failed |= check(0, ambient, material, diffuse, specular, constant);
    failed |= check(4, dark, material, diffuse, specular, rink);
    failed |= check(0x15, dark, material, diffuse, specular, both);
    failed |= check(0x22, ambient, material, diffuse, specular, spec_emission);
    if (!failed) puts("PASS: constant, rink vertex ambient, dual vertex sources, specular source/alpha");
    return failed;
}
