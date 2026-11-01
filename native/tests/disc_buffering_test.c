#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../overrides/hitz_disc_buffering.h"
static int failed;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s (win32=%lu)\n",__LINE__,#x,GetLastError());failed=1;}}while(0)
static unsigned opts(const char *path,unsigned access,unsigned disp,unsigned options,int on)
{return hitz_disc_host_options(path,strlen(path),access,disp,options,on);}
int main(int argc,char **argv)
{
    const char *disc="d:\\sound\\fixture.dat";HANDLE file;DWORD got;
    unsigned char expected[8192],*data;unsigned i,mode;
    if(argc!=2)return 2;
    CHECK(opts(disc,GENERIC_READ,1,0x48,1)==0x40);
    CHECK(opts("\\Device\\CdRom0\\sound\\fixture.dat",0x120089,1,0x48,1)==0x40);
    CHECK(opts("D:/sound/fixture.dat",GENERIC_READ,1,0x48,1)==0x40);
    CHECK(opts(disc,GENERIC_READ,1,0x48,0)==0x48);
    CHECK(opts("z:\\cache.dat",GENERIC_READ,1,0x48,1)==0x48);
    CHECK(opts("u:\\save.dat",GENERIC_READ,1,0x48,1)==0x48);
    CHECK(opts("relative.dat",GENERIC_READ,1,0x48,1)==0x48);
    CHECK(opts(disc,GENERIC_READ,1,0x49,1)==0x49);
    CHECK(opts(disc,GENERIC_READ,3,0x48,1)==0x48);
    for(i=0;i<32;i++)if((1u<<i)&(0x40000000u|0x10000000u|0x02000000u|0xD0000u|0x100u|0x10u|4u|2u))
        CHECK(opts(disc,GENERIC_READ|(1u<<i),1,0x48,1)==0x48);
    for(i=0;i<sizeof expected;i++)expected[i]=(unsigned char)(i*71u+(i>>8));
    file=CreateFileA(argv[1],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(file!=INVALID_HANDLE_VALUE);if(file==INVALID_HANDLE_VALUE)return 1;
    CHECK(WriteFile(file,expected,sizeof expected,&got,NULL) && got==sizeof expected);CloseHandle(file);
    data=VirtualAlloc(NULL,16384,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(data!=NULL);if(!data)return 1;
    for(mode=0;mode<2;mode++){
        unsigned options=opts(disc,GENERIC_READ,1,0x48,mode);
        DWORD flags=options&8?FILE_FLAG_NO_BUFFERING:FILE_ATTRIBUTE_NORMAL;
        file=CreateFileA(argv[1],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,flags,NULL);
        CHECK(file!=INVALID_HANDLE_VALUE);if(file==INVALID_HANDLE_VALUE)continue;
        for(i=0;i<3;i++){
            OVERLAPPED ov={0};unsigned offset=i==0?2048:i==1?7680:8192;
            unsigned length=i==0?1024:i==1?1024:512;
            BOOL ok;ov.Offset=offset;memset(data,0xcc,16384);got=0;
            ok=ReadFile(file,data,length,&got,&ov);
            CHECK(ok || GetLastError()==ERROR_HANDLE_EOF);
            CHECK(got==(i==0?1024:i==1?512:0));
            CHECK(!memcmp(data,expected+offset,got));
        }
        CloseHandle(file);
    }
    VirtualFree(data,0,MEM_RELEASE);DeleteFileA(argv[1]);
    if(!failed)puts("PASS: read-only disc policy preserves writes/saves/directories; cached/uncached offset, partial and EOF reads match byte-for-byte");
    return failed;
}
