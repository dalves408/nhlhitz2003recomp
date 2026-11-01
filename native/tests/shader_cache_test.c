#define COBJMACROS
#include <stdlib.h>
#include "../src/shader_cache_format.h"
static int failed;
#define CHECK(c) do {if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);failed=1;}}while(0)
int main(int argc,char **argv)
{
    const char src[]="float4 main():SV_Target{return float4(.2,.4,.6,1);}";
    ID3DBlob *blob=NULL,*loaded=NULL,*err=NULL;uint64_t h1,h2,k1,k2;
    char path[MAX_PATH+64],source[MAX_PATH+64];FILE *f;ScSourceHdr sh={0};ScHdr bad={0};int v;
    if(argc!=2)return 2;
    CreateDirectoryA(argv[1],NULL);snprintf(path,sizeof path,"%s\\roundtrip.bin",argv[1]);
    CHECK(sizeof(ScHdr)==40 && sizeof(ScSourceHdr)==60);
    CHECK(SUCCEEDED(D3DCompile(src,sizeof src-1,NULL,NULL,NULL,"main","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&err)));
    if(!blob)return 1;
    sc_hash_key(src,sizeof src-1,"main","ps_4_0",&h1,&h2);
    sc_hash_key(src,sizeof src-1,"other","ps_4_0",&k1,&k2);CHECK(h1!=k1 && h2!=k2);
    CHECK(sc_write(path,h1,h2,sizeof src-1,blob));
    loaded=sc_load(path,h1,h2,sizeof src-1);CHECK(loaded!=NULL);
    if(loaded){CHECK(ID3D10Blob_GetBufferSize(loaded)==ID3D10Blob_GetBufferSize(blob));
        CHECK(!memcmp(ID3D10Blob_GetBufferPointer(loaded),ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob)));ID3D10Blob_Release(loaded);}
    CHECK(sc_load(path,k1,k2,sizeof src-1)==NULL);
    CHECK(sc_load(path,h1,h2,sizeof src)==NULL);
    f=fopen(path,"r+b");CHECK(f!=NULL);if(f){fseek(f,-1,SEEK_END);v=fgetc(f);fseek(f,-1,SEEK_END);fputc(v^1,f);fclose(f);}
    CHECK(sc_load(path,h1,h2,sizeof src-1)==NULL);
    CHECK(sc_write(path,h1,h2,sizeof src-1,blob));
    f=fopen(path,"ab");CHECK(f!=NULL);if(f){fputc(1,f);fclose(f);}CHECK(sc_load(path,h1,h2,sizeof src-1)==NULL);
    f=fopen(path,"wb");if(f){fwrite("short",1,5,f);fclose(f);}CHECK(sc_load(path,h1,h2,sizeof src-1)==NULL);
    bad.magic=SC_MAGIC;bad.format=SC_FORMAT;bad.h1=h1;bad.h2=h2;bad.srclen=sizeof src-1;bad.size=64u<<20;
    f=fopen(path,"wb");if(f){fwrite(&bad,sizeof bad,1,f);fclose(f);}CHECK(sc_load(path,h1,h2,sizeof src-1)==NULL);
    CHECK(sc_write(path,h1,h2,sizeof src-1,blob));
    sh.magic=SC_SOURCE_MAGIC;sh.format=1;sh.srclen=sizeof src-1;strcpy(sh.entry,"main");strcpy(sh.target,"ps_4_0");
    snprintf(source,sizeof source,"%s\\fixture.hsrc",argv[1]);f=fopen(source,"wb");CHECK(f!=NULL);
    if(f){CHECK(fwrite(&sh,sizeof sh,1,f)==1 && fwrite(src,1,sizeof src-1,f)==sizeof src-1);fclose(f);}
    ID3D10Blob_Release(blob);if(err)ID3D10Blob_Release(err);
    if(!failed)puts("PASS: shader cache roundtrip, identity, corruption, truncation, size bounds and captured source fixture");
    return failed;
}
