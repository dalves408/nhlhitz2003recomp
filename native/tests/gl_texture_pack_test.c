/* Real GL readback and cache ownership tests; no original game assets. */
#include "../src/gl_texture_pack.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static char root[]="/tmp/hitz-gl-pack-XXXXXX";
static void put(unsigned char *p,unsigned v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void image(unsigned hash,unsigned size,unsigned char red,int mips)
{
    char path[512];snprintf(path,sizeof(path),"%s/%016X.dds",root,hash);
    unsigned char h[128]={0};memcpy(h,"DDS ",4);put(h+4,124);put(h+12,size);put(h+16,size);
    put(h+28,mips?2:1);put(h+76,32);put(h+80,0x41);put(h+88,32);
    put(h+92,0xff);put(h+96,0xff00);put(h+100,0xff0000);put(h+104,0xff000000);
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(h,1,128,f)==128);
    for(unsigned level=0;level<(mips?2u:1u);level++){
        unsigned char px[4]={red,(unsigned char)(level*77),19,128};
        for(unsigned i=0;i<size*size;i++)assert(fwrite(px,1,4,f)==4);
        size=size>1?size/2:1;
    }
    fclose(f);
}
static GLuint fetch(int i,unsigned frame,unsigned *levels)
{
    GLuint id=0;for(int n=0;n<500&&!id;n++){id=hitz_gl_pack_get(i,frame,levels);hitz_gl_pack_pump(frame);if(!id)SDL_Delay(2);}return id;
}
static void pixel(GLuint id,unsigned level,unsigned size,unsigned char red,unsigned char green)
{
    glBindTexture(GL_TEXTURE_2D,id);GLint w;glGetTexLevelParameteriv(GL_TEXTURE_2D,level,GL_TEXTURE_WIDTH,&w);assert(w==(int)size);
    unsigned char *b=malloc((size_t)size*size*4);assert(b);glGetTexImage(GL_TEXTURE_2D,level,GL_RGBA,GL_UNSIGNED_BYTE,b);
    assert(b[0]==red&&b[1]==green&&b[2]==19&&b[3]==128);free(b);assert(glGetError()==GL_NO_ERROR);
}
int main(void)
{
    assert(mkdtemp(root));image(1,384,211,0);image(2,384,51,0);image(3,4,97,1);
    char path[512];snprintf(path,sizeof(path),"%s/%016X.png",root,4);
    png_image png={0};png.version=PNG_IMAGE_VERSION;png.width=png.height=1;png.format=PNG_FORMAT_RGBA;
    unsigned char rgba[]={155,0,19,128};assert(png_image_write_to_file(&png,path,0,rgba,0,NULL));
    snprintf(path,sizeof(path),"%s/%016X.dds",root,4);FILE *bad=fopen(path,"wb");assert(bad);fputs("bad DDS",bad);fclose(bad);
    setenv("RECOMP_TEXPACK",root,1);setenv("RECOMP_HIRES","1",1);setenv("RECOMP_PACK_CACHE_MB","1",1);setenv("RECOMP_PACK_PENDING_MB","1",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *w=SDL_CreateWindow("pack-test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);SDL_GLContext context=SDL_GL_CreateContext(w);assert(context);
    hitz_gl_pack_init(1,NULL);assert(!hitz_gl_pack_active(1));hitz_gl_pack_shutdown();
    for(int async=0;async<=1;async++){
        setenv("RECOMP_PACK_ASYNC",async?"1":"0",1);hitz_gl_pack_init(2,NULL);assert(hitz_gl_pack_active(2));unsigned levels;
        int a=hitz_gl_pack_find(1),b=hitz_gl_pack_find(2);assert(a>=0&&b>=0);
        GLuint first=fetch(a,1,&levels);assert(first&&levels==1);pixel(first,0,384,211,0);
        HitzGlPackStats before,after;hitz_gl_pack_stats(&before);
        assert(hitz_gl_pack_get(a,1,&levels)==first);hitz_gl_pack_stats(&after);assert(after.loads==before.loads&&after.hits>before.hits);
        GLuint second=fetch(b,2,&levels);assert(second);pixel(second,0,384,51,0);
        hitz_gl_pack_stats(&after);assert(after.resident_bytes<=1048576&&after.pending_bytes<=1048576&&after.evictions>=1);
        /* Resolve by index again after eviction; never reuse an old GL name. */
        assert(fetch(a,3,&levels));pixel(hitz_gl_pack_get(a,3,&levels),0,384,211,0);
        GLuint mip=fetch(hitz_gl_pack_find(3),4,&levels);assert(mip&&levels==2);pixel(mip,1,2,97,77);
        GLuint fallback=fetch(hitz_gl_pack_find(4),5,&levels);assert(fallback);pixel(fallback,0,1,155,0);
        hitz_gl_pack_shutdown();
    }
    setenv("RECOMP_HIRES","0",1);hitz_gl_pack_init(2,NULL);assert(!hitz_gl_pack_active(2));hitz_gl_pack_shutdown();
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();
    for(unsigned i=1;i<=4;i++){snprintf(path,sizeof(path),"%s/%016X.dds",root,i);unlink(path);}snprintf(path,sizeof(path),"%s/%016X.png",root,4);unlink(path);rmdir(root);
    puts("PASS: sync/async DDS readback, alpha, stored mips, PNG fallback, cache reuse and eviction, disabled/1x guards");return 0;
}
