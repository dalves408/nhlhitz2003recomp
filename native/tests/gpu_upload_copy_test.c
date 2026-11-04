#include "gpu_upload_copy.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    unsigned char *source=malloc(65700),*target=malloc(65700);
    if(!source||!target)return 2;
    for(unsigned i=0;i<65700;i++)source[i]=(unsigned char)(i*33u+(i>>8));
    const unsigned sizes[]={0,1,15,16,17,31,32,63,64,65,127,128,129,4095,4096,65535};
    for(unsigned da=0;da<32;da++)for(unsigned sa=0;sa<8;sa++)for(unsigned j=0;j<sizeof(sizes)/sizeof(sizes[0]);j++){
        size_t n=sizes[j];memset(target,0xa5,65700);gpu_upload_copy(target+da,source+sa,n);
        if(memcmp(target+da,source+sa,n)){fprintf(stderr,"FAIL: upload copy %u/%u/%zu\n",da,sa,n);return 1;}
        for(unsigned i=0;i<da;i++)if(target[i]!=0xa5)return 1;
        for(size_t i=da+n;i<65700;i++)if(target[i]!=0xa5)return 1;
    }
    free(source);free(target);puts("PASS: GPU upload copies, source/destination alignment, zero/short/vector tails and guard bytes");return 0;
}
