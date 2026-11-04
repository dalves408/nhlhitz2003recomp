#include "linux_gpu_wait.h"
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
ptrdiff_t g_xbox_mem_offset;
ptrdiff_t xbox_GetMemoryOffset(void){return g_xbox_mem_offset;}
static uint32_t status=0x100a5u;
static void *executor(void *unused)
{
    (void)unused;const struct timespec delay={0,25000000};nanosleep(&delay,NULL);
    __atomic_fetch_and(&status,~0x10000u,__ATOMIC_RELEASE);
    hitz_linux_gpu_wait_notify(&status);return NULL;
}
static double seconds(clockid_t id)
{struct timespec t;clock_gettime(id,&t);return t.tv_sec+t.tv_nsec/1e9;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);return 1;}}while(0)
int main(void)
{
    setenv("RECOMP_LINUX_GPU_WAIT","cooperative",1);
    pthread_t thread;CHECK(!pthread_create(&thread,NULL,executor,NULL));
    double wall=seconds(CLOCK_MONOTONIC),cpu=seconds(CLOCK_THREAD_CPUTIME_ID);
    while(__atomic_load_n(&status,__ATOMIC_ACQUIRE)&0x10000u)hitz_linux_gpu_wait_hint(&status);
    cpu=seconds(CLOCK_THREAD_CPUTIME_ID)-cpu;wall=seconds(CLOCK_MONOTONIC)-wall;
    CHECK(!pthread_join(thread,NULL));CHECK(status==0xa5u);CHECK(wall>=.020);CHECK(cpu<wall*.5);
    /* An already acknowledged request must return without sleeping. */
    for(unsigned i=0;i<1000;i++)hitz_linux_gpu_wait_hint(&status);
    CHECK(status==0xa5u);
    printf("PASS: executor alone acknowledges, unrelated bits retained, waiting thread CPU %.3f ms / %.3f ms elapsed\n",cpu*1000,wall*1000);
    return 0;
}
