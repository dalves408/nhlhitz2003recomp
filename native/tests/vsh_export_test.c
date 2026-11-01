/* Production exporter, including its real upload cursors and decoder. */
#include <windows.h>
#include "../overrides/nv2a_vsh_interp.c"
static unsigned rng=0x312408u;
static unsigned random_word(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static int failed;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failed=1;}}while(0)
static void compare_export(void)
{
    Nv2aGpuVs scalar,cached,again;int a,b,c;
    memset(&scalar,0xa5,sizeof scalar);cached=again=scalar;
    s_export_cache_mode=0;a=nv2a_vsh_export(&scalar);
    s_export_cache_mode=1;b=nv2a_vsh_export(&cached);c=nv2a_vsh_export(&again);
    CHECK(a==b && a==c);
    CHECK(!memcmp(&scalar,&cached,sizeof scalar));
    CHECK(!memcmp(&scalar,&again,sizeof scalar));
}
int main(int argc,char **argv)
{
    unsigned trial,i,start,n,words[4];uint64_t hits,misses;
    Nv2aGpuVs e;LARGE_INTEGER f,before,after;volatile unsigned hash=0;
    if(argc>1 && !strcmp(argv[1],"--diag")){
        _putenv_s("RECOMP_VSH_DUMP","1");memset(&e,0,sizeof e);
        CHECK(!nv2a_vsh_export(&e));
        if(!failed)puts("PASS: explicit VSH diagnostics retain exporter fallback");
        return failed;
    }
    _putenv_s("RECOMP_VSH_DUMP","");_putenv_s("RECOMP_VSH_PROFILE","");_putenv_s("RECOMP_VSH_CHECK","");
    for(trial=0;trial<3000;trial++){
        start=random_word()%120;n=1+random_word()%12;
        nv2a_vsh_set_start_slot(start);nv2a_vsh_set_cxt_write(trial%3==0);
        for(i=0;i<n;i++){
            words[0]=random_word();words[1]=random_word();words[2]=random_word();words[3]=(random_word()&~1u)|(i==n-1);
            nv2a_vsh_set_instruction(start+i,words);
        }
        compare_export();
        /* Serialized uploads must advance their cursor even when unchanged. */
        nv2a_vsh_set_load_slot(start);
        for(i=0;i<4;i++)nv2a_vsh_program_word(s_program[start][i]);
        compare_export();
        nv2a_vsh_set_load_slot(start);nv2a_vsh_program_word(random_word());compare_export();
        nv2a_vsh_set_cxt_write(!s_cxt_write);compare_export();
        nv2a_vsh_set_constant_load(5);nv2a_vsh_constant_word(random_word());compare_export();
    }
    /* A terminating non-writing program permits a stable repeated-export benchmark. */
    memset(words,0,sizeof words);words[3]=1;
    nv2a_vsh_set_instruction(0,words);nv2a_vsh_set_start_slot(0);nv2a_vsh_set_cxt_write(0);
    memset(&e,0,sizeof e);nv2a_vsh_export(&e);nv2a_vsh_export_stats(&hits,&misses);
    nv2a_vsh_set_instruction(0,words);nv2a_vsh_set_load_slot(0);
    for(i=0;i<4;i++)nv2a_vsh_program_word(words[i]);
    nv2a_vsh_export(&e);CHECK(s_export_hits==hits+1 && s_export_misses==misses);
    for(i=0;i<24;i++){words[3]=i==23;nv2a_vsh_set_instruction(i,words);}
    QueryPerformanceFrequency(&f);
    for(i=0;i<2;i++){
        unsigned j;s_export_cache_mode=i;QueryPerformanceCounter(&before);
        for(j=0;j<200000;j++){nv2a_vsh_export(&e);hash^=e.hash;}
        QueryPerformanceCounter(&after);
        printf("export cache=%u 200000 calls %.3f ms\n",i,1000.0*(double)(after.QuadPart-before.QuadPart)/f.QuadPart);
    }
    (void)hash;
    if(!failed)puts("PASS: 15000 production scalar/cached export comparisons; mutation, start/context, partial/repeated upload and constant changes");
    return failed;
}
