#include <stdio.h>
#include "../src/texture_content_hash.h"
static int failed;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);failed=1;}}while(0)
int main(void)
{
    uint32_t pixels[]={0x12345678,0xabcdef01,0x99887766,0x01020304};
    uint64_t hash;int enabled,installed,scale;
    for(enabled=0;enabled<2;enabled++)for(installed=0;installed<2;installed++)for(scale=1;scale<=5;scale++){
        if(!enabled || !installed || scale<2){
            /* NULL ensures disabled/unavailable packs do not read pixel data. */
            hash=~0ull;CHECK(!px_hash_for_use(enabled,scale,installed,0,2,2,NULL,&hash));CHECK(hash==0);
        }else{
            CHECK(px_hash_for_use(enabled,scale,installed,0,2,2,pixels,&hash));CHECK(hash==0xcb70d97235ce0589ull);
        }
        /* Explicit source capture preserves stable filenames with any setting. */
        CHECK(px_hash_for_use(enabled,scale,installed,1,2,2,pixels,&hash));CHECK(hash==0xcb70d97235ce0589ull);
    }
    if(!failed)puts("PASS: disabled/missing/1x packs never read pixels; active pack and explicit capture retain CRC32/Adler32 identity");
    return failed;
}
