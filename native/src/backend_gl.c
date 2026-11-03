/* Native OpenGL backend for the project's NV2A executor. Guest completion is
 * published by the executor only after these synchronous callbacks return. */
#include <SDL2/SDL.h>
#include <epoxy/gl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nv2a_pbx.h"
#include "input/keyboard_sdl.h"
#include "texture_content_hash.h"
#include "gl_texture_pack.h"
#include "linux_performance.h"

typedef struct { uint32_t va,w,h,format; GLuint fbo,color,depth,attached_depth; unsigned uses; } Target;
typedef struct { Nv2aTexture key; uint64_t hash; GLuint id; uint64_t used;
    unsigned char *snapshot;size_t snapshot_size;int valid,pack_index; } Texture;
typedef struct { float p[4],color[4],d1[4],tc[4][4],fog; } Vertex;
static SDL_Window *window;
static SDL_GLContext context;
static GLuint program,vao,vbo,white;
static const char *const uniform_names[]={"surf","zscale","atest","afunc","aref","use_rc","hdr","hdr2","fogc","stg","uvs","tx0","tx1","tx2","tx3","gaa","gfogmode","gfpx","gfpy"};
typedef struct {GLuint id,vs;int generic;uint32_t key[36];GLint locations[19];
    uint8_t uniform_valid[19];uint32_t uniform_words[19][64];} ShaderProgram;
static ShaderProgram programs[512],*active_program;
static unsigned program_count;
static unsigned programs_at_last_flip;
static GLuint vertex_shader;
static char *fragment_source;
static Target targets[32];
static Texture textures[1024];
static size_t snapshot_bytes;
static uint64_t flips,draws,last_report,last_present;
static uint64_t hash_ticks,decode_ticks,draw_ticks,hashed_bytes,decode_count,upload_count,cpu_vertices,gpu_vertices;
static Vertex *vertices;
static size_t vertex_cap;
static uint32_t *pixels;
static size_t pixel_cap;
static FILE *frame_csv;
static unsigned scale=1;
static unsigned sampled_mips=1;
static float anisotropy=1;
static HitzGlPackStats last_pack_stats;
static uint64_t title_flips,title_draws;
static double present_hz=60;
static uint64_t next_present_ns;
static Nv2aRenderState previous_state;
static int state_valid;
static unsigned active_texture_unit;
static GLuint bound_textures[4],bound_samplers[4],bound_framebuffer;
static unsigned viewport_w,viewport_h;
extern void xbox_InputSetMainPump(int);
static char *read_shader_file(const char *name);

static void pack_invalidate_bindings(GLuint id)
{
    if(!id){active_texture_unit=0;for(unsigned i=0;i<4;i++)bound_textures[i]=~0u;}
    else for(unsigned i=0;i<4;i++)if(bound_textures[i]==id)bound_textures[i]=~0u;
}

static void texture_unit(unsigned unit)
{
    if(active_texture_unit!=unit){glActiveTexture(GL_TEXTURE0+unit);active_texture_unit=unit;}
}
static void texture_bind(GLuint id)
{
    if(bound_textures[active_texture_unit]!=id){glBindTexture(GL_TEXTURE_2D,id);bound_textures[active_texture_unit]=id;}
}
static void texture_bind_unit(unsigned unit,GLuint id)
{
    if(bound_textures[unit]!=id){texture_unit(unit);texture_bind(id);}
}
static void framebuffer_bind(GLuint id)
{
    if(bound_framebuffer!=id){glBindFramebuffer(GL_FRAMEBUFFER,id);bound_framebuffer=id;}
}
static void fatal(const char *what)
{ fprintf(stderr,"[GL-FATAL] %s: %s\n",what,SDL_GetError()); exit(1); }
enum { U_SURF,U_ZSCALE,U_ATEST,U_AFUNC,U_AREF,U_USE_RC,U_HDR,U_HDR2,U_FOGC,U_STG,U_UVS,U_TX0,U_TX1,U_TX2,U_TX3,U_GAA,U_GFOGMODE,U_GFPX,U_GFPY };
static GLint uniform_change(unsigned i,const void *value,unsigned words)
{
    GLint location=active_program->locations[i];if(location<0)return -1;
    if(active_program->uniform_valid[i]&&!memcmp(active_program->uniform_words[i],value,words*4))return -1;
    memcpy(active_program->uniform_words[i],value,words*4);active_program->uniform_valid[i]=1;
    return location;
}
static void uniform1ui(unsigned n,uint32_t v){GLint l=uniform_change(n,&v,1);if(l>=0)glUniform1ui(l,v);}
static void uniform1f(unsigned n,float v){GLint l=uniform_change(n,&v,1);if(l>=0)glUniform1f(l,v);}
static void uniform2f(unsigned n,float a,float b){float v[]={a,b};GLint l=uniform_change(n,v,2);if(l>=0)glUniform2fv(l,1,v);}
static void uniform4ui(unsigned n,uint32_t a,uint32_t b,uint32_t c,uint32_t d){uint32_t v[]={a,b,c,d};GLint l=uniform_change(n,v,4);if(l>=0)glUniform4uiv(l,1,v);}
static void uniform4fv(unsigned n,unsigned count,const float *v){GLint l=uniform_change(n,v,count*4);if(l>=0)glUniform4fv(l,count,v);}
static void uniform4uiv(unsigned n,unsigned count,const uint32_t *v){GLint l=uniform_change(n,v,count*4);if(l>=0)glUniform4uiv(l,count,v);}

static GLuint shader(GLenum kind,const char *src)
{
    GLuint id=glCreateShader(kind); GLint ok;
    glShaderSource(id,1,&src,NULL); glCompileShader(id); glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if (!ok) { char log[8192]; glGetShaderInfoLog(id,sizeof(log),NULL,log); fprintf(stderr,"[GL-SHADER] %s\n",log); exit(1); }
    return id;
}
static const char *vs_source=
    "#version 330 core\n"
    "layout(location=0) in vec4 p; layout(location=1) in vec4 color;\n"
    "layout(location=2) in vec4 d1; layout(location=3) in vec4 tc0;\n"
    "layout(location=4) in vec4 tc1; layout(location=5) in vec4 tc2;\n"
    "layout(location=6) in vec4 tc3; layout(location=7) in float fog;\n"
    "uniform vec2 surf; uniform float zscale;\n"
    "out vec4 v_color,v_d1; out vec4 v_tc[4]; out float v_fog;\n"
    "void main(){ float w=p.w>1e-12?1.0/p.w:1.0;\n"
    "gl_Position=vec4((p.xy/surf*2.0-1.0)*w,(clamp(p.z*zscale,0.0,1.0)*2.0-1.0)*w,w);\n"
    "v_color=color;v_d1=d1;v_tc[0]=tc0;v_tc[1]=tc1;v_tc[2]=tc2;v_tc[3]=tc3;v_fog=fog;}\n";

static ShaderProgram *make_program(const char *source,GLuint vs)
{
    ShaderProgram *p=&programs[program_count++];
    GLuint fs=shader(GL_FRAGMENT_SHADER,source);p->id=glCreateProgram();p->vs=vs;p->generic=1;
    glAttachShader(p->id,vs);glAttachShader(p->id,fs);
    if(vs!=vertex_shader){const char *varyings[]={"raw_position","v_color","v_d1","v_tc","v_fog"};glTransformFeedbackVaryings(p->id,5,varyings,GL_INTERLEAVED_ATTRIBS);}
    glLinkProgram(p->id);GLint ok;glGetProgramiv(p->id,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];glGetProgramInfoLog(p->id,sizeof(log),NULL,log);fprintf(stderr,"[GL-LINK] %s\n",log);exit(1);}
    glDeleteShader(fs);glUseProgram(p->id);program=p->id;
    for(unsigned i=0;i<19;i++)p->locations[i]=glGetUniformLocation(p->id,uniform_names[i]);
    for(int i=0;i<4;i++)glUniform1i(p->locations[11+i],i);
    return p;
}
static void select_program(const Nv2aCombiner *rc,GLuint vs)
{
    ShaderProgram *p=&programs[0];
    for(unsigned i=0;i<program_count;i++)if(programs[i].generic&&programs[i].vs==vs){p=&programs[i];break;}
    ShaderProgram *fallback=p;
    if(rc){uint32_t key[36]={rc->control,rc->stage_program,rc->final0,rc->final1};
        for(int i=0;i<8;i++){key[4+4*i]=rc->color_icw[i];key[5+4*i]=rc->alpha_icw[i];key[6+4*i]=rc->color_ocw[i];key[7+4*i]=rc->alpha_ocw[i];}
        for(unsigned i=1;i<program_count;i++)if(!programs[i].generic&&programs[i].vs==vs&&!memcmp(key,programs[i].key,sizeof(key))){p=&programs[i];break;}
        if(p==fallback&&program_count<512){
            char prefix[4096];int n=snprintf(prefix,sizeof(prefix),"#version 330 core\n#define RC_SPECIALIZED\n#define RC_H0 %uu\n#define RC_H1 %uu\n#define RC_H2 %uu\n#define RC_H3 %uu\n#define RC_WORDS ",key[0],key[1],key[2],key[3]);
            for(int i=0;i<8;i++)n+=snprintf(prefix+n,sizeof(prefix)-(size_t)n,"%suvec4(%uu,%uu,%uu,%uu)",i?",":"",key[4+4*i],key[5+4*i],key[6+4*i],key[7+4*i]);
            prefix[n++]='\n';prefix[n]=0;
            const char *body=strchr(fragment_source,'\n')+1;size_t len=strlen(body)+n+1;char *src=malloc(len);
            if(!src)fatal("shader allocation");strcpy(src,prefix);strcat(src,body);p=make_program(src,vs);p->generic=0;memcpy(p->key,key,sizeof(key));free(src);
            fprintf(stderr,"[GL-SPECIALIZE] shader=%u stages=%u\n",program_count-1,rc->control&255);
        }
    }
    active_program=p;if(program!=p->id){program=p->id;glUseProgram(program);}
}

static void init(void)
{
    if(window)return;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    window=SDL_CreateWindow("NHL Hitz 2003 — Linux",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!window || !(context=SDL_GL_CreateContext(window)))fatal("OpenGL window");
    xbox_InputSetMainPump(1);xbox_InputSdlInit();
    const char *vsync=getenv("RECOMP_GL_VSYNC");SDL_GL_SetSwapInterval(vsync?atoi(vsync):0);
    const char *hz=getenv("RECOMP_PRESENT_HZ");if(hz)present_hz=atof(hz);if(present_hz<0||present_hz>1000)present_hz=60;
    fprintf(stderr,"[GL] %s / %s / %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    char *src=read_shader_file("nv2a.frag");if(!src)fatal("read fragment shader");
    fragment_source=src;vertex_shader=shader(GL_VERTEX_SHADER,vs_source);
    active_program=make_program(src,vertex_shader);program=active_program->id;
    glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    for(int i=0;i<8;i++){
        static const size_t offsets[]={offsetof(Vertex,p),offsetof(Vertex,color),offsetof(Vertex,d1),offsetof(Vertex,tc[0]),offsetof(Vertex,tc[1]),offsetof(Vertex,tc[2]),offsetof(Vertex,tc[3]),offsetof(Vertex,fog)};
        glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==7?1:4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsets[i]);
    }
    glGenTextures(1,&white);texture_bind(white);uint32_t w=0xffffffff;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_BGRA,GL_UNSIGNED_BYTE,&w);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const char *csv=getenv("RECOMP_FRAME_CSV");if(csv){frame_csv=fopen(csv,"w");if(frame_csv){setvbuf(frame_csv,NULL,_IOFBF,65536);fprintf(frame_csv,"frame,seconds,interval_ms,draws,draw_cpu_ms,hash_ms,decode_ms,hash_bytes,decodes,uploads,cpu_vertices,gpu_vertices,capture,shader_compiles,pack_loads,pack_load_ms,pack_create_ms,pack_cache_hits,pack_cache_mb,pack_pending_mb,pack_queue_jobs,pack_evictions,pack_failures\n");}}
    const char *sc=getenv("RECOMP_RENDER_SCALE");if(sc)scale=(unsigned)atoi(sc);if(scale<1||scale>5)scale=1;
    if(getenv("RECOMP_ANISOTROPY"))anisotropy=(float)atof(getenv("RECOMP_ANISOTROPY"));
    if(!(anisotropy>=1&&anisotropy<=16))anisotropy=1;
    if(epoxy_gl_version()>=46||epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic")){
        GLfloat max_aniso;glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&max_aniso);if(anisotropy>max_aniso)anisotropy=max_aniso;
    }else anisotropy=1;
    hitz_gl_pack_init(scale,pack_invalidate_bindings);
    atexit(hitz_gl_pack_shutdown);
    hitz_linux_performance_init();atexit(hitz_linux_performance_shutdown);
    last_report=last_present=SDL_GetPerformanceCounter();
}

static Target *target_format(const Nv2aSurface *s,uint32_t format)
{
    if(!s->width||!s->height||s->width>4096||s->height>4096)return NULL;
    unsigned color=format&15u;
    if(color<1||color>8){static unsigned reported;
        if(!(reported&(1u<<color))){reported|=1u<<color;fprintf(stderr,"[GL-UNSUPPORTED] surface color format=%u at %08X\n",color,s->color_va);}
        return NULL;
    }
    Target *r=NULL;
    for(int i=0;i<32;i++)if(targets[i].va==s->color_va){r=&targets[i];break;}
    if(!r)for(int i=0;i<32;i++)if(!targets[i].color){r=&targets[i];break;}
    if(!r){fprintf(stderr,"[GL-FATAL] render target capacity\n");exit(1);}
    if(r->color && r->w==s->width && r->h==s->height && r->format==format)return r;
    if(r->color){
        for(unsigned i=0;i<4;i++)if(bound_textures[i]==r->color)bound_textures[i]=~0u;
        if(bound_framebuffer==r->fbo)bound_framebuffer=~0u;
        glDeleteTextures(1,&r->color);glDeleteRenderbuffers(1,&r->depth);glDeleteFramebuffers(1,&r->fbo);}
    memset(r,0,sizeof(*r));r->va=s->color_va;r->w=s->width;r->h=s->height;r->format=format;
    GLenum internal=s->bytes_per_pixel==2?((format&15)==3?GL_RGB565:GL_RGB5_A1):GL_RGBA8;
    glGenTextures(1,&r->color);texture_bind(r->color);
    glTexImage2D(GL_TEXTURE_2D,0,internal,r->w*scale,r->h*scale,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glGenRenderbuffers(1,&r->depth);glBindRenderbuffer(GL_RENDERBUFFER,r->depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,r->w*scale,r->h*scale);
    glGenFramebuffers(1,&r->fbo);framebuffer_bind(r->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r->color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,r->depth);
    r->attached_depth=r->depth;
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)fatal("framebuffer incomplete");
    fprintf(stderr,"[GL-RT] %08X %ux%u format=%u\n",r->va,r->w,r->h,format);
    return r;
}
static Target *target(const Nv2aSurface *s){return target_format(s,nv2a_pbx_surface_color_format());}
static struct {uint32_t va,w,h;GLuint depth;} depths[32];
static void bind_target(Target *r,uint32_t zeta)
{
    GLuint depth=r->depth;
    if(zeta){int slot=-1;
        for(int i=0;i<32;i++)if(depths[i].va==zeta&&depths[i].w==r->w&&depths[i].h==r->h){slot=i;break;}
        if(slot<0)for(int i=0;i<32;i++)if(!depths[i].depth){slot=i;depths[i].va=zeta;depths[i].w=r->w;depths[i].h=r->h;
            glGenRenderbuffers(1,&depths[i].depth);glBindRenderbuffer(GL_RENDERBUFFER,depths[i].depth);
            glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,r->w*scale,r->h*scale);break;}
        if(slot<0)fatal("depth target capacity");depth=depths[slot].depth;
    }
    framebuffer_bind(r->fbo);
    if(r->attached_depth!=depth){
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
        r->attached_depth=depth;
    }
}

static uint64_t hash_pixels(const uint32_t *p,size_t n)
{uint64_t h=1469598103934665603ull;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ull;}return h;}
extern ptrdiff_t xbox_GetMemoryOffset(void);
static size_t texture_span(const Nv2aTexture *t)
{
    size_t n=t->pitch?(size_t)t->pitch*t->height:(size_t)t->width*t->height*4;
    if(!t->pitch&&t->color==0x0c)n=((t->width+3)/4)*((t->height+3)/4)*8;
    if(!t->pitch&&(t->color==0x0e||t->color==0x0f))n=((t->width+3)/4)*((t->height+3)/4)*16;
    uint64_t off=t->offset;
    if(off>=0x80000000ull)off-=0x80000000ull;
    return off+n<=64ull*1024*1024?n:0;
}
#include "texture_memory_hash.h"
static void snapshot_release(Texture *e)
{
    snapshot_bytes-=e->snapshot_size;free(e->snapshot);
    e->snapshot=NULL;e->snapshot_size=0;e->valid=0;
}
static int snapshot_allocate(Texture *e,size_t span)
{
    static size_t budget;static int configured;
    if(!configured){const char *v=getenv("RECOMP_GL_TEXTURE_SNAPSHOT_MB");
        long mb=v?strtol(v,NULL,10):32;if(mb<0)mb=0;if(mb>128)mb=128;
        budget=(size_t)mb*1024*1024;configured=1;}
    if(e->snapshot)return 0;
    if(span>budget)return 0;
    while(snapshot_bytes+span>budget){
        Texture *old=NULL;
        /* Keep this frame's working set resident instead of repeatedly
         * copying/evicting it when the scene exceeds the memory budget. */
        for(int i=0;i<1024;i++)if(&textures[i]!=e&&textures[i].snapshot&&textures[i].used<flips&&(!old||textures[i].used<old->used))old=&textures[i];
        if(!old)return 0;
        uint64_t hash=texture_memory_hash(old->snapshot,old->snapshot_size);int valid=old->valid;
        snapshot_release(old);old->hash=hash;old->valid=valid;
    }
    e->snapshot=malloc(span);if(!e->snapshot)return 0;
    e->snapshot_size=span;snapshot_bytes+=span;return 1;
}
static int decode_texture(const Nv2aTexture *t,uint32_t *out)
{
    uint64_t begin=frame_csv?SDL_GetPerformanceCounter():0;
    int ok=nv2a_backend_decode_texture(t,out);
    if(frame_csv)decode_ticks+=SDL_GetPerformanceCounter()-begin;
    decode_count++;return ok;
}
static GLuint texture(const Nv2aTexture *t)
{
    sampled_mips=1;
    if(!t||!t->width||!t->height||t->width>4096||t->height>4096)return white;
    for(int i=0;i<32;i++)if(targets[i].color&&targets[i].va==t->offset&&targets[i].w==t->width&&targets[i].h==t->height)return targets[i].color;
    size_t count=(size_t)t->width*t->height;
    if(pixel_cap<count){uint32_t *p=realloc(pixels,count*4);if(!p)fatal("texture allocation");pixels=p;pixel_cap=count;}
    ptrdiff_t base=xbox_GetMemoryOffset();size_t span=texture_span(t);
    Texture *e=NULL,*old=&textures[0];
    for(int i=0;i<1024;i++){
        if(textures[i].id&&!memcmp(&textures[i].key,t,sizeof(*t))){e=&textures[i];break;}
        if(!textures[i].id){old=&textures[i];}
        else if(old->id&&textures[i].used<old->used)old=&textures[i];
    }
    if(!e){e=old;snapshot_release(e);e->key=*t;e->pack_index=-1;if(!e->id)glGenTextures(1,&e->id);}
    const unsigned char *raw=base&&span?(const unsigned char *)((uintptr_t)base+t->offset):NULL;
    uint64_t hash_start=frame_csv?SDL_GetPerformanceCounter():0,hash=0;
    int changed=!e->valid;
    if(raw){
        /* Exact byte comparisons avoid serial hashing for resident textures.
         * The bounded snapshot cache retains every-use validation and catches
         * writes anywhere in a texture, even between draws in one frame. */
        if(e->snapshot)changed|=memcmp(e->snapshot,raw,span)!=0;
        else {hash=texture_memory_hash(raw,span);changed|=e->hash!=hash;}
        hashed_bytes+=span;
    }
    if(frame_csv)hash_ticks+=SDL_GetPerformanceCounter()-hash_start;
    texture_bind(e->id);
    if(!raw){if(!decode_texture(t,pixels))return white;hash=hash_pixels(pixels,count);changed|=e->hash!=hash;}
    if(changed){
        if(raw&&!decode_texture(t,pixels))return white;
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,t->width,t->height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);upload_count++;
        e->pack_index=-1;
        if(hitz_gl_pack_active(scale)){
            uint64_t key=0;
            if(px_hash_for_use(1,(int)scale,1,0,t->width,t->height,pixels,&key))e->pack_index=hitz_gl_pack_find(key);
        }
    }
    if(raw){int allocated=snapshot_allocate(e,span);if(e->snapshot&&(changed||allocated))memcpy(e->snapshot,raw,span);}
    e->hash=hash;e->valid=1;
    e->used=flips;
    if(hitz_gl_pack_active(scale)&&e->pack_index>=0){
        GLuint replacement=hitz_gl_pack_get(e->pack_index,flips,&sampled_mips);
        if(replacement)return replacement;
    }
    return e->id;
}

static void clear(const Nv2aSurface *s,const Nv2aRenderState *rs,uint32_t flags,uint32_t argb,uint32_t zs)
{
    init();Target *r=target(s);if(!r)return;
    bind_target(r,rs->zeta_va);glDisable(GL_SCISSOR_TEST);
    GLbitfield bits=0;
    if(flags&0xf0){glColorMask((flags&0x10)!=0,(flags&0x20)!=0,(flags&0x40)!=0,(flags&0x80)!=0);
        glClearColor(((argb>>16)&255)/255.f,((argb>>8)&255)/255.f,(argb&255)/255.f,(argb>>24)/255.f);bits|=GL_COLOR_BUFFER_BIT;}
    if(flags&1){glDepthMask(GL_TRUE);glClearDepth((zs>>8)/16777215.0);bits|=GL_DEPTH_BUFFER_BIT;}
    if(flags&2){glStencilMask(255);glClearStencil(zs&255);bits|=GL_STENCIL_BUFFER_BIT;}
    glClear(bits);r->uses++;state_valid=0;
}
static void toggle(GLenum cap,int enabled){if(enabled)glEnable(cap);else glDisable(cap);}
static void render_state(const Nv2aRenderState *s)
{
    const Nv2aRenderState *o=&previous_state;int invalid=!state_valid;
    if(invalid||s->blend_enable!=o->blend_enable)toggle(GL_BLEND,s->blend_enable);
    if(invalid||s->blend_src!=o->blend_src||s->blend_dst!=o->blend_dst)glBlendFunc(s->blend_src,s->blend_dst);
    if(invalid||s->blend_eq!=o->blend_eq)glBlendEquation(s->blend_eq?s->blend_eq:GL_FUNC_ADD);
    if(invalid||s->blend_color!=o->blend_color){float c[4];nv2a_rc_unpack(s->blend_color,c);glBlendColor(c[0],c[1],c[2],c[3]);}
    if(invalid||(s->depth_test_enable&&s->zeta_va)!=(o->depth_test_enable&&o->zeta_va))toggle(GL_DEPTH_TEST,s->depth_test_enable&&s->zeta_va);
    if(invalid||s->depth_func!=o->depth_func)glDepthFunc(s->depth_func?s->depth_func:GL_LESS);
    if(invalid||s->depth_write!=o->depth_write)glDepthMask(s->depth_write!=0);
    if(invalid||s->stencil_enable!=o->stencil_enable)toggle(GL_STENCIL_TEST,s->stencil_enable);
    if(s->stencil_enable){
        int changed=invalid||!o->stencil_enable;
        if(changed||s->stencil_func!=o->stencil_func||s->stencil_ref!=o->stencil_ref||s->stencil_func_mask!=o->stencil_func_mask)glStencilFunc(s->stencil_func,s->stencil_ref,s->stencil_func_mask);
        if(changed||s->stencil_write_mask!=o->stencil_write_mask)glStencilMask(s->stencil_write_mask);
        if(changed||s->stencil_fail!=o->stencil_fail||s->stencil_zfail!=o->stencil_zfail||s->stencil_zpass!=o->stencil_zpass)glStencilOp(s->stencil_fail,s->stencil_zfail,s->stencil_zpass);
    }
    /* Winding is opt-in, matching the established Windows backend. */
    static int cull=-1;if(cull<0){const char *v=getenv("RECOMP_GL_CULL");cull=v&&atoi(v);}
    if(invalid||s->cull_enable!=o->cull_enable)toggle(GL_CULL_FACE,cull&&s->cull_enable);
    if(cull&&s->cull_enable){
        if(invalid||s->cull_face!=o->cull_face)glCullFace(s->cull_face?s->cull_face:GL_BACK);
        if(invalid||s->front_face!=o->front_face)glFrontFace(s->front_face?s->front_face:GL_CCW);
    }
    if(invalid||s->color_mask!=o->color_mask)glColorMask((s->color_mask&0x10000)!=0,(s->color_mask&0x100)!=0,(s->color_mask&1)!=0,(s->color_mask&0x1000000)!=0);
    previous_state=*s;state_valid=1;
}
static GLuint sampler(const Nv2aTexture *t,unsigned levels)
{
    static GLuint cache[4][4][2][2];
    unsigned u=t?t->addr_u:3,v=t?t->addr_v:3;
    u=u==1?0:u==2?1:u==4?2:3;v=v==1?0:v==2?1:v==4?2:3;
    unsigned point=t&&((t->filter>>16)&255)==1&&((t->filter>>24)&15)==1;
    unsigned mipped=levels>1;
    GLuint *id=&cache[u][v][point][mipped];
    if(!*id){static const GLenum modes[]={GL_REPEAT,GL_MIRRORED_REPEAT,GL_CLAMP_TO_BORDER,GL_CLAMP_TO_EDGE};
        glGenSamplers(1,id);glSamplerParameteri(*id,GL_TEXTURE_WRAP_S,modes[u]);glSamplerParameteri(*id,GL_TEXTURE_WRAP_T,modes[v]);
        glSamplerParameteri(*id,GL_TEXTURE_MIN_FILTER,mipped?(point?GL_NEAREST_MIPMAP_NEAREST:GL_LINEAR_MIPMAP_LINEAR):(point?GL_NEAREST:GL_LINEAR));glSamplerParameteri(*id,GL_TEXTURE_MAG_FILTER,point?GL_NEAREST:GL_LINEAR);
        if(mipped&&!point&&anisotropy>1)glSamplerParameterf(*id,GL_TEXTURE_MAX_ANISOTROPY_EXT,anisotropy);}
    return *id;
}
#include "gl_gpu_vertex.inc"
static void draw(const Nv2aSurface *s,const Nv2aBatch *b)
{
    if(!b||!b->count)return;
    init();Target *r=target(s);if(!r)return;
    const Nv2aRenderState *rs=b->state;const Nv2aBatchExt *x=nv2a_pbx_current();
    uint64_t draw_start=frame_csv?SDL_GetPerformanceCounter():0;
    const Nv2aGpuVs *gpu=x?x->gpu:NULL;
    GLuint vs=gpu?gpu_vertex_shader(gpu):vertex_shader;
    if(!vs||(!gpu&&!b->vertices))fatal("unavailable vertex program");
    if(vertex_cap<b->count){Vertex *v=realloc(vertices,b->count*sizeof(*v));if(!v)fatal("vertices");vertices=v;vertex_cap=b->count;}
    for(uint32_t i=0;!gpu&&i<b->count;i++){
        const Nv2aVertex *a=&b->vertices[i];Vertex *v=&vertices[i];
        v->p[0]=a->x;v->p[1]=a->y;v->p[2]=a->z;v->p[3]=a->rhw;nv2a_rc_unpack(a->diffuse,v->color);
        memset(v->d1,0,sizeof(v->d1));memset(v->tc,0,sizeof(v->tc));v->fog=1;
        for(int k=0;k<4;k++)v->tc[k][3]=1;
        v->tc[0][0]=a->u;v->tc[0][1]=a->v;
        if(x&&x->ext){memcpy(v->d1,x->ext[i].d1,16);memcpy(v->tc,x->ext[i].tc,sizeof(v->tc));v->fog=x->ext[i].fog;}
    }
    select_program(x?x->rc:NULL,vs);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    if(!gpu)glBufferData(GL_ARRAY_BUFFER,b->count*sizeof(Vertex),vertices,GL_STREAM_DRAW);
    uniform2f(U_SURF,r->w,r->h);uniform1f(U_ZSCALE,rs->depth_max>1?1.f/rs->depth_max:1.f);
    uniform1ui(U_ATEST,rs->alpha_test_enable);uniform1ui(U_AFUNC,rs->alpha_func>=0x200?rs->alpha_func-0x200:rs->alpha_func);uniform1f(U_AREF,rs->alpha_ref);
    uniform1ui(U_USE_RC,x&&x->rc?1:0);
    if(x&&x->rc){const Nv2aCombiner *c=x->rc;uint32_t stg[16][4]={{0}};
        uniform4ui(U_HDR,c->control,c->stage_program,c->final0,c->final1);
        uniform4ui(U_HDR2,c->final_c0,c->final_c1,x->clip_plane_mode,1);
        uniform4fv(U_FOGC,1,x->fog_color);
        for(int k=0;k<8;k++){stg[2*k][0]=c->color_icw[k];stg[2*k][1]=c->alpha_icw[k];stg[2*k][2]=c->color_ocw[k];stg[2*k][3]=c->alpha_ocw[k];stg[2*k+1][0]=c->factor0[k];stg[2*k+1][1]=c->factor1[k];}
        uniform4uiv(U_STG,16,&stg[0][0]);
    }
    float uvs[4][4]={{0}};
    for(int k=0;k<4;k++){
        unsigned mode=x&&x->rc?(x->rc->stage_program>>(k*5))&31u:1u;
        int needed=mode==1||mode==2;
        const Nv2aTexture *t=!needed?NULL:x&&x->tex_valid[k]?&x->tex[k]:(k==0?b->texture:NULL);
        if(t)texture_unit(k);
        GLuint id=texture(t);texture_bind_unit(k,id);
        GLuint samp=sampler(t,sampled_mips);if(bound_samplers[k]!=samp){glBindSampler(k,samp);bound_samplers[k]=samp;}
        uvs[k][0]=t&&!(x&&x->tex_sized[k])?1.f/t->width:1;uvs[k][1]=t&&!(x&&x->tex_sized[k])?1.f/t->height:1;uvs[k][2]=t?1:0;
    }
    uniform4fv(U_UVS,4,&uvs[0][0]);
    bind_target(r,rs->zeta_va);
    if(viewport_w!=r->w*scale||viewport_h!=r->h*scale){viewport_w=r->w*scale;viewport_h=r->h*scale;glViewport(0,0,viewport_w,viewport_h);}
    render_state(rs);
    if(gpu)gpu_vertex_draw(gpu,b->count);else glDrawArrays(GL_TRIANGLES,0,b->count);
    if(frame_csv)draw_ticks+=SDL_GetPerformanceCounter()-draw_start;
    if(gpu)gpu_vertices+=b->count;else cpu_vertices+=b->count;
    r->uses++;draws++;
}
static void events(void)
{
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT)exit(0);
        xbox_InputSdlEvent(&e);
        if(e.type==SDL_KEYDOWN&&!e.key.repeat){
            if(e.key.keysym.sym==SDLK_ESCAPE)exit(0);
            if(e.key.keysym.sym==SDLK_F11)SDL_SetWindowFullscreen(window,(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)?0:SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
    }
}
static void flip(void)
{
    init();events();Target *r=NULL;unsigned captured=0;
    for(int i=0;i<32;i++)if(targets[i].uses&&(!r||targets[i].w*targets[i].h>r->w*r->h))r=&targets[i];
    if(r){int w,h;SDL_GL_GetDrawableSize(window,&w,&h);int vw=w,vh=w*3/4;if(vh>h){vh=h;vw=h*4/3;}
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);bound_framebuffer=~0u;glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);state_valid=0;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,r->fbo);glBlitFramebuffer(0,0,r->w*scale,r->h*scale,(w-vw)/2,(h+vh)/2,(w+vw)/2,(h-vh)/2,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        const char *dump=getenv("RECOMP_GL_CAPTURE_PREFIX"),*every=getenv("RECOMP_GL_CAPTURE_EVERY");
        if(!dump)dump=getenv("RECOMP_FB_DUMP");if(!every)every=getenv("RECOMP_FB_WINDOW_DUMP_EVERY");
        if(dump&&every&&atoi(every)>0&&flips%(unsigned)atoi(every)==0){
            glBindFramebuffer(GL_READ_FRAMEBUFFER,r->fbo);SDL_Surface *img=SDL_CreateRGBSurfaceWithFormat(0,r->w*scale,r->h*scale,32,SDL_PIXELFORMAT_ARGB8888);
            if(img){glReadPixels(0,0,img->w,img->h,GL_BGRA,GL_UNSIGNED_BYTE,img->pixels);char path[4096];snprintf(path,sizeof(path),"%sgl_%06llu.bmp",dump,(unsigned long long)flips);SDL_SaveBMP(img,path);SDL_FreeSurface(img);captured=1;}
        }
        if(present_hz>0){
            struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);
            uint64_t now=(uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec;
            uint64_t period=(uint64_t)(1e9/present_hz);
            if(!next_present_ns)next_present_ns=now;
            next_present_ns+=period;
            if(now>next_present_ns+period)next_present_ns=now;
            ts.tv_sec=(time_t)(next_present_ns/1000000000ull);ts.tv_nsec=(long)(next_present_ns%1000000000ull);
            clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&ts,NULL);
        }
        SDL_GL_SwapWindow(window);
    }
    hitz_gl_pack_pump(flips);
    uint64_t now=SDL_GetPerformanceCounter();double freq=(double)SDL_GetPerformanceFrequency();
    HitzGlPackStats pack;hitz_gl_pack_stats(&pack);
    if(frame_csv){
        fprintf(frame_csv,"%llu,%.6f,%.6f,%llu,%.6f,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%u,%u",(unsigned long long)flips,now/freq,(now-last_present)*1000/freq,(unsigned long long)draws,draw_ticks*1000/freq,hash_ticks*1000/freq,decode_ticks*1000/freq,(unsigned long long)hashed_bytes,(unsigned long long)decode_count,(unsigned long long)upload_count,(unsigned long long)cpu_vertices,(unsigned long long)gpu_vertices,captured,program_count-programs_at_last_flip);
        fprintf(frame_csv,",%llu,%.6f,%.6f,%llu,%.6f,%.6f,%u,%llu,%llu\n",(unsigned long long)(pack.loads-last_pack_stats.loads),(pack.load_us-last_pack_stats.load_us)/1000.0,(pack.create_us-last_pack_stats.create_us)/1000.0,(unsigned long long)(pack.hits-last_pack_stats.hits),pack.resident_bytes/1048576.0,pack.pending_bytes/1048576.0,pack.queued,(unsigned long long)(pack.evictions-last_pack_stats.evictions),(unsigned long long)(pack.failures-last_pack_stats.failures));
    }
    last_pack_stats=pack;
    programs_at_last_flip=program_count;
    title_draws+=draws;
    if(now-last_report>=(uint64_t)freq){
        double secs=(now-last_report)/freq,fps=(flips+1-title_flips)/secs;
        int cpu,gpu;hitz_linux_performance_sample(&cpu,&gpu);
        char title[192],cpu_text[24],gpu_text[24];
        if(cpu>=0)snprintf(cpu_text,sizeof(cpu_text),"%d%%",cpu);else snprintf(cpu_text,sizeof(cpu_text),"N/A");
        if(gpu>=0)snprintf(gpu_text,sizeof(gpu_text),"%d%%",gpu);else snprintf(gpu_text,sizeof(gpu_text),"N/A");
        double per_frame=(flips+1-title_flips)?(double)title_draws/(flips+1-title_flips):0;
        snprintf(title,sizeof(title),"NHL Hitz 2003 - OpenGL | FPS: %.1f | CPU: %s | GPU: %s | draws/frame: %.0f",fps,cpu_text,gpu_text,per_frame);
        SDL_SetWindowTitle(window,title);
        if(getenv("RECOMP_PERF_LOG"))fprintf(stderr,"[PERF] fps=%.3f process_cpu_pct=%d gpu_pct=%d logical_cpu_normalized=1 draws_per_frame=%.1f\n",fps,cpu,gpu,per_frame);
        title_flips=flips+1;title_draws=0;last_report=now;if(frame_csv)fflush(frame_csv);
    }
    for(int i=0;i<32;i++)targets[i].uses=0;flips++;draws=0;last_present=now;
    draw_ticks=hash_ticks=decode_ticks=hashed_bytes=decode_count=upload_count=cpu_vertices=gpu_vertices=0;
}
/* Every draw is submitted immediately; there is no deferred CPU batch to drain. */
void nv2a_gpu_flush_pending(void){}
static const Nv2aBackend backend={clear,draw,flip};
extern void nv2a_blit_register(void (*fn)(uint32_t,uint32_t,uint32_t,uint32_t));
static void blit(uint32_t src,uint32_t dst,uint32_t w,uint32_t h)
{
    Target *a=NULL;for(int i=0;i<32;i++)if(targets[i].va==src&&targets[i].w==w&&targets[i].h==h){a=&targets[i];break;}
    if(!a)return;
    Nv2aSurface s={.color_va=dst,.width=w,.height=h,.bytes_per_pixel=(a->format&15)<=3?2:4,.aa_sx=1,.aa_sy=1};
    Target *b=target_format(&s,a->format);if(!b)return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,a->fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,b->fbo);bound_framebuffer=~0u;
    glDisable(GL_SCISSOR_TEST);glBlitFramebuffer(0,0,w*scale,h*scale,0,0,w*scale,h*scale,GL_COLOR_BUFFER_BIT,GL_NEAREST);b->uses++;
}
void backend_gl_register(void){nv2a_backend_register(&backend);nv2a_blit_register(blit);}
