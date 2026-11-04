/* Uses the production streaming resampler with a deterministic source provider.
 * Synthetic spectra verify reconstruction and anti-aliasing independently of
 * game samples, ADPCM decoding, output devices and subjective listening. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#define NUM_SAMPLES_PER_FRAME 32
#define MCPX_HW_MAX_VOICES 4
#define NV_PAVS_VOICE_PAR_STATE 0
#define NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE 1
typedef struct { int rs_have; } MCPXAPUVoiceFilter;
typedef struct {
    struct { MCPXAPUVoiceFilter filters[MCPX_HW_MAX_VOICES]; } vp;
    struct { unsigned pos,remaining,max_fetch; double hz,source_hz; int constant,active; } feed[MCPX_HW_MAX_VOICES];
} MCPXAPUState;
static int voice_get_samples(MCPXAPUState *d,uint16_t v,float out[][2],int n)
{
    if((unsigned)n>d->feed[v].max_fetch)n=(int)d->feed[v].max_fetch;
    if((unsigned)n>d->feed[v].remaining)n=(int)d->feed[v].remaining;
    for(int i=0;i<n;i++){
        float x=d->feed[v].constant?0.75f:(float)sin(6.283185307179586*d->feed[v].hz*d->feed[v].pos/d->feed[v].source_hz);
        out[i][0]=x;out[i][1]=-0.5f*x;d->feed[v].pos++;
    }
    d->feed[v].remaining-=n;return n;
}
static int voice_get_mask(MCPXAPUState *d,uint16_t v,int reg,int mask)
{ (void)reg;(void)mask;return d->feed[v].active; }
#include "../overrides/apu_voice_resample.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(MCPXAPUState *d,unsigned v,double hz,double sr)
{
    d->vp.filters[v].rs_have=0;memset(&d->feed[v],0,sizeof(d->feed[v]));
    d->feed[v].remaining=1000000;d->feed[v].max_fetch=130;d->feed[v].active=1;
    d->feed[v].hz=hz;d->feed[v].source_hz=sr;
}
static void render(MCPXAPUState *d,unsigned v,float *out,unsigned n,unsigned chunk,float rate)
{
    unsigned made=0;int empty=0;while(made<n){float b[32][2];unsigned want=n-made<chunk?n-made:chunk;
        int got=voice_resample(d,(uint16_t)v,b,(int)want,rate);CHECK(got>=0&&got<=(int)want);
        CHECK(got||++empty<32);
        for(int i=0;i<got;i++){CHECK(isfinite(b[i][0])&&fabsf(b[i][1]+0.5f*b[i][0])<1e-6f);out[made++]=b[i][0];}
    }
}
static double amplitude(const float *s,unsigned start,unsigned n,double hz,double sr)
{
    double a=0,b=0;for(unsigned i=0;i<n;i++){double phase=6.283185307179586*hz*(start+i)/sr;
        a+=s[start+i]*cos(phase);b+=s[start+i]*sin(phase);}
    return 2*sqrt(a*a+b*b)/n;
}
int main(void)
{
    MCPXAPUState d={0};unsigned n=48512;float *a=calloc(n,sizeof(float)),*b=calloc(n,sizeof(float));CHECK(a&&b);
    const float rate=48000.f/22050.f;
    setup(&d,0,5000,22050);render(&d,0,a,n,32,rate);
    setup(&d,0,5000,22050);render(&d,0,b,n,1,rate);
    double error=0;for(unsigned i=0;i<n;i++)if(fabs(a[i]-b[i])>error)error=fabs(a[i]-b[i]);
    printf("Chunk-size maximum difference: %.9f\n",error);CHECK(error<0.00001);
    /* An isolated voice restart cannot inherit the previous sound's history. */
    setup(&d,1,5000,22050);render(&d,1,b,n,32,rate);
    for(unsigned i=0;i<n;i++)CHECK(fabs(a[i]-b[i])<1e-6);
    double wanted=amplitude(a,512,48000,5000,48000),image=amplitude(a,512,48000,17050,48000);
    for(unsigned i=0;i<n;i++){
        double t=i/(double)rate,k=floor(t),f=t-k;
        b[i]=(float)((1-f)*sin(6.283185307179586*5000*k/22050)+f*sin(6.283185307179586*5000*(k+1)/22050));
    }
    double old_image=amplitude(b,512,48000,17050,48000);
    printf("22.05 -> 48 kHz, 5 kHz tone: image old %.2f dB, sinc %.2f dB, reduction %.2f dB; chunk error %.6f\n",
        20*log10(old_image),20*log10(image),20*log10(old_image/image),error);
    CHECK(wanted>0.95&&wanted<1.05&&image<old_image*0.25);
    setup(&d,0,18000,48000);render(&d,0,a,n,32,0.5f);
    double alias=amplitude(a,512,24000,6000,24000);printf("48 -> 24 kHz: 18 kHz rejection %.2f dB\n",20*log10(alias));CHECK(alias<0.05);
    setup(&d,0,0,22050);d.feed[0].constant=1;render(&d,0,a,1024,32,rate);
    for(unsigned i=32;i<1024;i++)CHECK(fabs(a[i]-0.75)<1e-6);
    setup(&d,0,0,22050);d.feed[0].constant=1;d.feed[0].max_fetch=1;render(&d,0,a,1024,32,rate);
    for(unsigned i=32;i<1024;i++)CHECK(fabs(a[i]-0.75)<1e-6);
    setup(&d,0,5000,22050);d.feed[0].remaining=0;float out[32][2];CHECK(voice_resample(&d,0,out,32,rate)==0);
    d.feed[0].active=0;CHECK(voice_resample(&d,0,out,32,rate)==-1);
    setup(&d,0,0,22050);d.feed[0].constant=1;render(&d,0,a,1024,32,NAN);
    for(unsigned i=32;i<1024;i++)CHECK(fabs(a[i]-0.75)<1e-6);
    for(unsigned i=0;i<3;i++){
        const float pitches[]={0.125f,1.f,8.f};
        setup(&d,0,0,22050);d.feed[0].constant=1;render(&d,0,a,1024,32,pitches[i]);
        for(unsigned j=128;j<1024;j++)CHECK(fabs(a[j]-0.75)<1e-6);
    }
    clock_t began=clock();
    for(unsigned i=0;i<53;i++){
        setup(&d,0,0,22050);d.feed[0].constant=1;render(&d,0,a,48000,32,rate);
    }
    printf("Synthetic 53-voice-second workload: %.1f ms on this host (includes test-source overhead)\n",
        1000.0*(clock()-began)/CLOCKS_PER_SEC);
    free(a);free(b);puts("PASS: reconstruction/anti-aliasing, pitch continuity, stereo, DC, fragmented streams, underrun/end and voice reset");return 0;
}
