#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "platform/host_time.h"
extern void xbox_host_pause_begin(void),xbox_host_pause_end(void);
int main(void)
{
    struct timespec delay={0,20000000};
    uint64_t before=xbox_HostNowNs();nanosleep(&delay,NULL);
    assert(xbox_HostNowNs()>before);
    xbox_host_pause_begin();uint64_t paused=xbox_HostNowNs();nanosleep(&delay,NULL);
    assert(xbox_HostNowNs()==paused);xbox_host_pause_end();nanosleep(&delay,NULL);
    assert(xbox_HostNowNs()>paused);
    puts("PASS: Linux monotonic guest clock stops during host pause and resumes");return 0;
}
