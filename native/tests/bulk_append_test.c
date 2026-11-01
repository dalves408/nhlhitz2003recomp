#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../overrides/nv2a_bulk_append.h"
static uint32_t seed=0x54a826;
static uint32_t next(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void)
{
    unsigned trial;
    for(trial=0;trial<100000;trial++){
        uint32_t a[64],b[64],x[64],y[64],w[64],ac,bc,xc,yc,cap,n,i,method,prim;
        const uint32_t methods[]={0x1800,0x1808,0x1818,0x1d70};
        for(i=0;i<64;i++){a[i]=b[i]=next();x[i]=y[i]=next();w[i]=next();}
        cap=next()%65;ac=bc=next()%(cap+1);xc=yc=next()%(cap+1);n=next()%65;
        method=methods[trial%4];prim=(trial/4)%2;
        for(i=0;i<n;i++)if(prim){
            if(method==0x1818 && yc<cap)y[yc++]=w[i];
            else if(method==0x1808 && bc<cap)b[bc++]=w[i];
            else if(method==0x1800 && bc+2<=cap){b[bc++]=(uint16_t)w[i];b[bc++]=(uint16_t)(w[i]>>16);}
        }
        if(hitz_bulk_append(method,prim,a,&ac,cap,x,&xc,cap,w,n)!=(method==0x1d70?0:n)||
            ac!=bc||xc!=yc||memcmp(a,b,sizeof a)||memcmp(x,y,sizeof x)){
            fprintf(stderr,"FAIL trial=%u method=%x cap=%u n=%u\n",trial,method,cap,n);return 1;
        }
    }
    puts("PASS: 100000 serialized/bulk payload comparisons, 16/32-bit indices, inline data, odd capacities, overflow and inactive batches");
    return 0;
}
