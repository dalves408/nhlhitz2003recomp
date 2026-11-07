#include "../src/pack_image.h"
#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void put(unsigned char *p,uint32_t v)
{p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void header(unsigned char *b,unsigned w,unsigned h,unsigned mips,uint32_t four)
{
    memset(b,0,512);memcpy(b,"DDS ",4);put(b+4,124);put(b+12,h);put(b+16,w);
    put(b+28,mips);put(b+76,32);put(b+80,4);put(b+84,four);
}
int main(void)
{
    unsigned char b[512];HitzPackImage im;
    /* A valid legacy 1x1 DXT1 file is only 136 bytes. */
    header(b,1,1,1,0x31545844);assert(hitz_pack_parse_dds(b,136,&im));
    assert(im.format==HITZ_PACK_BC1&&im.level[0].bytes==8&&im.level[0].data==b+128);
    assert(!hitz_pack_parse_dds(b,135,&im));
    header(b,4,4,3,0x30315844);put(b+128,98);put(b+132,3);put(b+140,1);
    assert(hitz_pack_parse_dds(b,196,&im));
    assert(im.format==HITZ_PACK_BC7&&im.mips==3&&im.payload_bytes==48);
    assert(im.level[1].width==2&&im.level[2].width==1&&im.level[2].data==b+180);
    assert(!hitz_pack_parse_dds(b,195,&im));
    put(b+140,0);assert(!hitz_pack_parse_dds(b,196,&im));
    put(b+140,2);assert(!hitz_pack_parse_dds(b,196,&im));put(b+140,1);
    put(b+136,4);assert(!hitz_pack_parse_dds(b,196,&im));put(b+136,0);
    put(b+132,4);assert(!hitz_pack_parse_dds(b,196,&im));put(b+132,3);
    put(b+128,99);assert(!hitz_pack_parse_dds(b,196,&im));put(b+128,98);
    put(b+28,4);assert(!hitz_pack_parse_dds(b,212,&im));put(b+28,3);
    put(b+16,0);assert(!hitz_pack_parse_dds(b,196,&im));put(b+16,4);
    put(b+112,0x200);assert(!hitz_pack_parse_dds(b,196,&im));put(b+112,0);
    put(b+24,2);assert(!hitz_pack_parse_dds(b,196,&im));put(b+24,0);
    put(b+76,31);assert(!hitz_pack_parse_dds(b,196,&im));
    header(b,4,4,0,0x33545844);assert(hitz_pack_parse_dds(b,144,&im)&&im.format==HITZ_PACK_BC2);
    header(b,4,4,1,0x35545844);assert(hitz_pack_parse_dds(b,144,&im)&&im.format==HITZ_PACK_BC3);
    header(b,1,1,1,0);put(b+80,0x40);put(b+88,32);put(b+92,0xff0000);put(b+96,0xff00);put(b+100,0xff);
    assert(hitz_pack_parse_dds(b,132,&im)&&im.format==HITZ_PACK_BGRA8&&im.opaque);
    put(b+80,0x41);put(b+104,0xff000000u);assert(hitz_pack_parse_dds(b,132,&im)&&!im.opaque);
    put(b+8,8);put(b+20,8);assert(!hitz_pack_parse_dds(b,132,&im));
    put(b+20,4);assert(hitz_pack_parse_dds(b,132,&im));
    put(b+96,0xff);assert(!hitz_pack_parse_dds(b,132,&im));
    assert(!hitz_pack_parse_dds(NULL,0,&im));

    char dir[]="/tmp/hitz-pack-image-XXXXXX";assert(mkdtemp(dir));
    char path[512];snprintf(path,sizeof(path),"%s/pixels.PnG",dir);
    unsigned char pixels[16]={1,2,3,4,5,6,7,128,9,10,11,255,13,14,15,0};
    png_image png={0};png.version=PNG_IMAGE_VERSION;png.width=2;png.height=2;png.format=PNG_FORMAT_BGRA;
    assert(png_image_write_to_file(&png,path,0,pixels,0,NULL));
    assert(hitz_pack_image_estimate(path)==16);
    assert(!hitz_pack_image_read(path,15,&im));
    assert(hitz_pack_image_read(path,16,&im));
    assert(im.generate_mips&&im.format==HITZ_PACK_BGRA8&&im.mips==1&&im.width==2&&im.height==2);
    assert(!memcmp(im.level[0].data,pixels,16));hitz_pack_image_free(&im);
    assert(!im.allocation&&!im.mips);unlink(path);
    snprintf(path,sizeof(path),"%s/tiny.DDS",dir);
    header(b,1,1,1,0x31545844);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(b,1,136,f)==136);fclose(f);
    assert(hitz_pack_image_read(path,136,&im)&&im.allocation&&!im.generate_mips);
    hitz_pack_image_free(&im);unlink(path);rmdir(dir);
    puts("PASS: DDS bounds/formats/mip tails/legacy tiny files and bounded PNG BGRA+alpha loading");
    return 0;
}
