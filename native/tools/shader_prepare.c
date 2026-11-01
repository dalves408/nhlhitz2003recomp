/* Compile a captured shader set BEFORE launching the game. No guest executable,
 * game assets, graphics device or live match is required by this tool. */
#define COBJMACROS
#include <stdlib.h>
#include "../src/shader_cache_format.h"

static int prepare(const char *source,const char *outdir,int *built,int *hits)
{
    ScSourceHdr h;FILE *f=fopen(source,"rb");char *src=NULL,path[MAX_PATH+96];
    uint64_t h1,h2;ID3DBlob *blob=NULL,*err=NULL;HRESULT hr;int ok=0;
    if(!f)return 0;
    if(fread(&h,sizeof h,1,f)!=1 || h.magic!=SC_SOURCE_MAGIC || h.format!=1 || !h.srclen || h.srclen>(4u<<20)
       || !memchr(h.entry,0,sizeof h.entry) || !memchr(h.target,0,sizeof h.target))goto done;
    if(strcmp(h.target,"vs_4_0") && strcmp(h.target,"vs_5_0") && strcmp(h.target,"ps_4_0")
       && strcmp(h.target,"ps_5_0") && strcmp(h.target,"gs_5_0"))goto done;
    src=malloc((size_t)h.srclen+1);if(!src)goto done;
    if(fread(src,1,h.srclen,f)!=h.srclen || fgetc(f)!=EOF)goto done;
    src[h.srclen]=0;sc_hash_key(src,h.srclen,h.entry,h.target,&h1,&h2);
    snprintf(path,sizeof path,"%s\\%s_%016llx.bin",outdir,h.target,(unsigned long long)h1);
    blob=sc_load(path,h1,h2,h.srclen);
    if(blob){(*hits)++;ok=1;goto done;}
    hr=D3DCompile(src,h.srclen,source,NULL,NULL,h.entry,h.target,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&err);
    if(FAILED(hr)) {fprintf(stderr,"Compile failed %s: %s\n",source,err?(const char *)ID3D10Blob_GetBufferPointer(err):"unknown");goto done;}
    ok=sc_write(path,h1,h2,h.srclen,blob);if(ok)(*built)++;
done:
    if(err)ID3D10Blob_Release(err);if(blob)ID3D10Blob_Release(blob);free(src);fclose(f);return ok;
}
int main(int argc,char **argv)
{
    char pattern[MAX_PATH+32],path[MAX_PATH+96];WIN32_FIND_DATAA fd;HANDLE find;
    int built=0,hits=0,failed=0,total=0;
    if(argc!=3){fprintf(stderr,"Usage: hitz_shader_prepare <captured-source-directory> <output-cache-directory>\n");return 2;}
    if(!CreateDirectoryA(argv[2],NULL) && GetLastError()!=ERROR_ALREADY_EXISTS){fprintf(stderr,"Cannot create output directory\n");return 1;}
    snprintf(pattern,sizeof pattern,"%s\\*.hsrc",argv[1]);find=FindFirstFileA(pattern,&fd);
    if(find==INVALID_HANDLE_VALUE){fprintf(stderr,"No captured shaders in %s\n",argv[1]);return 1;}
    do {
        if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        snprintf(path,sizeof path,"%s\\%s",argv[1],fd.cFileName);total++;
        if(!prepare(path,argv[2],&built,&hits)){fprintf(stderr,"Rejected/failed: %s\n",path);failed++;}
    }while(FindNextFileA(find,&fd));FindClose(find);
    printf("Shader preparation: %d sources, %d compiled, %d already cached, %d failed\n",total,built,hits,failed);
    return failed?1:0;
}
