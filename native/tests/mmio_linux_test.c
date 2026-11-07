/* The real decoder must update Linux ucontext registers, widths, flags and
 * instruction pointer exactly, and reject unknown opcodes without mutation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../overrides/mmio_x64_posix.h"
static volatile uint64_t value;static int last_size,writes;
static uint64_t rd(void *dev,uint32_t off,int size){assert(dev==&value&&off==0x80);last_size=size;return value;}
static void wr(void *dev,uint32_t off,uint64_t v,int size){assert(dev==&value&&off==0x80);value=v;last_size=size;writes++;}
static int run(ucontext_t *c,const unsigned char *code)
{c->uc_mcontext.gregs[REG_RIP]=(greg_t)code;return hitz_mmio_emulate(c,0x80,(void *)&value,rd,wr);}
static void fault(int sig,siginfo_t *info,void *ctx)
{
    (void)sig;(void)info;
    if(!hitz_mmio_emulate(ctx,0x80,(void *)&value,rd,wr))_exit(2);
}
int main(void)
{
    ucontext_t c={0};
    unsigned char load[]={0x8b,0x00};value=0x89abcdef;c.uc_mcontext.gregs[REG_RAX]=-1;
    assert(run(&c,load)&&last_size==4&&c.uc_mcontext.gregs[REG_RAX]==0x89abcdef);
    assert(c.uc_mcontext.gregs[REG_RIP]==(greg_t)(load+2));
    unsigned char store[]={0x44,0x89,0x08};c.uc_mcontext.gregs[REG_R9]=0x12345678;
    assert(run(&c,store)&&value==0x12345678&&writes==1);
    unsigned char wide[]={0x48,0x8b,0x08};value=0xfedcba9876543210ull;
    assert(run(&c,wide)&&last_size==8&&(uint64_t)c.uc_mcontext.gregs[REG_RCX]==value);
    unsigned char zext[]={0x0f,0xb6,0x10};value=0xfe;
    assert(run(&c,zext)&&last_size==1&&c.uc_mcontext.gregs[REG_RDX]==254);
    unsigned char test[]={0xf7,0x00,0x02,0,0,0};value=0;c.uc_mcontext.gregs[REG_EFL]=1;
    assert(run(&c,test)&&(c.uc_mcontext.gregs[REG_EFL]&0x40)&&!(c.uc_mcontext.gregs[REG_EFL]&1));
    unsigned char unknown[]={0x0f,0x0b};assert(!run(&c,unknown));
    assert(c.uc_mcontext.gregs[REG_RIP]==(greg_t)unknown);
    struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;
    sigemptyset(&sa.sa_mask);assert(!sigaction(SIGSEGV,&sa,NULL));
    size_t pagesize=(size_t)sysconf(_SC_PAGESIZE);
    void *page=mmap(NULL,pagesize,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(page!=MAP_FAILED);
    volatile uint32_t *reg=(volatile uint32_t *)((char *)page+0x80);
    value=0x12345678;assert(*reg==0x12345678);
    *reg=0xabcd1234;assert(value==0xabcd1234);
    assert(!munmap(page,pagesize));
    puts("PASS: x86 MMIO widths/flags/RIP, unsupported opcode and actual SIGSEGV read/write");return 0;
}
