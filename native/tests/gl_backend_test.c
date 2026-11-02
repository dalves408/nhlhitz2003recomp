/* Exercise production GLSL and native 16-bit targets against the scalar
 * combiner, using constant-color triangles for independent pixel readback. */
#include "../src/backend_gl.c"
static const Nv2aBackend *registered;
static Nv2aBatchExt fixture;
static uint32_t color_format=8;
static unsigned char texture_guest[2*1024*1024];
void nv2a_backend_register(const Nv2aBackend *b){registered=b;}
void nv2a_blit_register(void (*fn)(uint32_t,uint32_t,uint32_t,uint32_t)){(void)fn;}
const Nv2aBatchExt *nv2a_pbx_current(void){return &fixture;}
uint32_t nv2a_pbx_surface_color_format(void){return color_format;}
int nv2a_backend_decode_texture(const Nv2aTexture *t,uint32_t *out)
{
    if(t->offset){memcpy(out,texture_guest+t->offset,(size_t)t->width*t->height*4);return 1;}
    for(uint32_t i=0;i<t->width*t->height;i++)out[i]=0xffffffff;return 1;
}
void xbox_InputSdlEvent(const SDL_Event *e){(void)e;}
void xbox_InputSetMainPump(int on){(void)on;}
void xbox_InputSdlInit(void){}
ptrdiff_t xbox_GetMemoryOffset(void){return (ptrdiff_t)texture_guest;}
int main(void)
{
    if(SDL_Init(SDL_INIT_VIDEO))return 1;
    if(!getenv("RECOMP_GL_TEXTURE_SNAPSHOT_MB"))setenv("RECOMP_GL_TEXTURE_SNAPSHOT_MB","1",1);
    backend_gl_register();
    Nv2aSurface s={.color_va=0x80000000,.width=16,.height=16,.bytes_per_pixel=4,.aa_sx=1,.aa_sy=1};
    Nv2aRenderState rs={.color_mask=0x01010101,.depth_func=GL_ALWAYS,.stencil_func=GL_ALWAYS,
        .stencil_fail=GL_KEEP,.stencil_zfail=GL_KEEP,.stencil_zpass=GL_KEEP};
    Nv2aVertex v[3]={{.x=0,.y=0,.rhw=1},{.x=32,.y=0,.rhw=1},{.x=0,.y=32,.rhw=1}};
    Nv2aVertexExt ext[3]={0};
    Nv2aCombiner rc={.control=1,.stage_program=1};
    Nv2aBatch b={.vertices=v,.count=3,.state=&rs};fixture.ext=ext;fixture.rc=&rc;
    float fog[4]={0,0,0,1},tex[4][4]={{1,1,1,1},{0,0,0,1},{0,0,0,1},{0,0,0,1}},d1[4]={0};
    unsigned failures=0;
    for(unsigned i=0;i<32;i++){
        uint32_t c=0xff000000u|((i*43u&255)<<16)|((i*79u&255)<<8)|(i*113u&255);
        for(int j=0;j<3;j++)v[j].diffuse=c;
        /* alternate vertex*constant, texture*vertex and signed mappings */
        rc.factor0[0]=0xff000000u|((i*37u&255)<<16)|((i*61u&255)<<8)|(i*97u&255);
        rc.color_icw[0]=(i%2)?0x04010000u:0x08040000u;
        if(i%3==0)rc.color_icw[0]|=0x40000000u;
        rc.alpha_icw[0]=0x18140000u;rc.color_ocw[0]=0xc00u;rc.alpha_ocw[0]=0xc00u;
        registered->clear(&s,&rs,0xf3,0,0xffffff00u);registered->draw(&s,&b);
        uint8_t pixel[4];glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        float d0[4],expected[4];nv2a_rc_unpack(c,d0);nv2a_rc_eval(&rc,d0,d1,fog,tex,expected);
        for(int k=0;k<4;k++)if(abs((int)pixel[k]-(int)lroundf(expected[k]*255))>1){
            fprintf(stderr,"FAIL combiner %u channel %d got=%u expected=%g\n",i,k,pixel[k],expected[k]*255);failures++;}
    }
    for(int f=1;f<=3;f++){
        color_format=f;s.color_va+=0x10000;s.bytes_per_pixel=2;
        registered->clear(&s,&rs,0xf0,0xffff0000,0);
        uint8_t pixel[4];glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        if(pixel[0]!=255||pixel[1]||pixel[2])failures++;
    }
    if(epoxy_gl_version()>=43){
        uint8_t block[4096]={0};float *constants=(float *)block;
        for(int i=0;i<4;i++)constants[i*4+i]=1;
        uint32_t slots[4]={3,9,1,0};memcpy(block+75*16,slots,16);
        uint32_t *desc=(uint32_t *)(block+GVS_OFF_ATTR);
        float *imm=(float *)(block+GVS_OFF_IMM);
        for(int i=0;i<16;i++){desc[i*4+3]=1;imm[i*4+3]=1;}
        imm[3*4]=1; /* immediate diffuse is opaque red */
        desc[0]=3648;desc[1]=16;desc[2]=2|(4<<8);desc[3]=0;
        uint32_t indices[3]={0,1,2};memcpy(block+GVS_OFF_IDX,indices,sizeof(indices));
        const float positions[12]={0,0,.5f,1,32,0,.5f,1,0,32,.5f,1};memcpy(block+3648,positions,sizeof(positions));
        float output[87]={0};uint32_t captured=0;
        Nv2aGpuVs gpu={.fixed_function=1,.block=block,.block_bytes=sizeof(block),.nidx=3,
            .aa_sx=1,.aa_sy=1,.verify=1,.verify_out=output,.verify_count=&captured};
        color_format=8;s.color_va=0x80040000;s.bytes_per_pixel=4;
        fixture.gpu=&gpu;b.vertices=NULL;rc.color_icw[0]=0x08040000u;
        registered->clear(&s,&rs,0xf3,0,0xffffff00u);
        if(!nv2a_gpuvs_probe(&gpu)){fprintf(stderr,"FAIL: fixed-function GPU shader unavailable\n");failures++;}
        else{
            registered->draw(&s,&b);uint8_t pixel[4];glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            if(pixel[0]!=255||pixel[1]||pixel[2]||captured!=3){fprintf(stderr,"FAIL: GPU FFP pixel/capture\n");failures++;}
            for(int i=0;i<3;i++)for(int k=0;k<4;k++)if(fabsf(output[i*29+k]-positions[i*4+k])>1e-6f){fprintf(stderr,"FAIL: FFP position %d/%d got=%g\n",i,k,output[i*29+k]);failures++;}
            /* Captured depth cancellation: division must round before the
             * viewport offset is added (fusing used to produce -0.684615). */
            memset(constants,0,16*sizeof(float));constants[0]=1;constants[5]=2;
            constants[2*4+3]=-21810380.f;constants[3*4+3]=1.3f;constants[74*4+2]=16777215.f;
            for(int i=0;i<12;i++)((float *)(block+3648))[i]=1;
            registered->draw(&s,&b);
            volatile float quotient=-21810380.f/1.3f;float depth=quotient+16777215.f;
            if(captured!=3||output[2]!=depth){fprintf(stderr,"FAIL: GPU depth cancellation got=%g expected=%g\n",output[2],depth);failures++;}
            /* A real decoded NV2A program: MOV position from v0 and diffuse
             * from c80. This uses the shared instruction generator. */
            memcpy(block+3648,positions,sizeof(positions));
            gpu.fixed_function=0;gpu.hash=0xabcdu;gpu.nslots=2;gpu.input_mask=1;
            gpu.prog[0][1]=(1u<<21)|0x1bu;gpu.prog[0][2]=2u<<26;gpu.prog[0][3]=(15u<<12)|(1u<<11);
            gpu.prog[1][1]=(1u<<21)|(80u<<13)|0x1bu;gpu.prog[1][2]=3u<<26;
            gpu.prog[1][3]=(15u<<12)|(1u<<11)|(3u<<3)|1u;
            constants[80*4]=1;constants[80*4+3]=1;
            if(!nv2a_gpuvs_probe(&gpu)){fprintf(stderr,"FAIL: decoded GPU program unavailable\n");failures++;}
            else {registered->draw(&s,&b);
                for(int i=0;i<3;i++)for(int k=0;k<4;k++)if(fabsf(output[i*29+k]-positions[i*4+k])>1e-6f){fprintf(stderr,"FAIL: programmable position\n");failures++;}}
        }
    }
    fixture.gpu=NULL;b.vertices=v;color_format=8;s.color_va=0x80050000;s.bytes_per_pixel=4;
    rc.color_icw[0]=0x08040000u;
    registered->clear(&s,&rs,0xf3,0xffff0000u,0xffffff00u);
    rs.blend_enable=1;rs.blend_src=GL_SRC_ALPHA;rs.blend_dst=GL_ONE_MINUS_SRC_ALPHA;rs.blend_eq=GL_FUNC_ADD;
    for(int j=0;j<3;j++)v[j].diffuse=0x800000ffu;
    uint8_t blended[4];
    for(int pass=0;pass<2;pass++){
        registered->draw(&s,&b);glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,blended);
        int red=pass?63:127,blue=pass?192:128;
        if(abs((int)blended[0]-red)>1||abs((int)blended[2]-blue)>1){fprintf(stderr,"FAIL: cached blend state\n");failures++;}
    }
    rs.blend_enable=0;rs.depth_test_enable=1;rs.depth_write=1;rs.depth_func=GL_LESS;rs.zeta_va=0x80100000;
    registered->clear(&s,&rs,0xf3,0xff000000u,0xffffff00u);
    for(int j=0;j<3;j++){v[j].diffuse=0xffff0000u;v[j].z=.2f;}registered->draw(&s,&b);
    for(int j=0;j<3;j++){v[j].diffuse=0xff0000ffu;v[j].z=.8f;}registered->draw(&s,&b);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,blended);
    if(blended[0]!=255||blended[2]){fprintf(stderr,"FAIL: cached depth state\n");failures++;}
    rs.depth_func=GL_GREATER;registered->draw(&s,&b);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,blended);
    if(blended[0]||blended[2]!=255){fprintf(stderr,"FAIL: depth function change\n");failures++;}
    s.color_va=0x80060000;rs.depth_func=GL_LESS;
    registered->clear(&s,&rs,0xf0,0xff000000u,0);
    for(int j=0;j<3;j++)v[j].z=.9f;registered->draw(&s,&b);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,blended);
    if(blended[0]||blended[1]||blended[2]){fprintf(stderr,"FAIL: shared depth target\n");failures++;}
    /* Guest memory can change between two draws in the same frame. Verify
     * actual GPU uploads for interior, word-boundary and final-byte writes,
     * including tiny spans and a non-power-of-two tail. */
    const unsigned widths[]={1,3,32},heights[]={1,5,32};
    for(unsigned shape=0;shape<3;shape++){
        Nv2aTexture t={.offset=128,.width=widths[shape],.height=heights[shape],
            .pitch=widths[shape]*4,.color=0x12};
        size_t span=(size_t)t.pitch*t.height;
        memset(texture_guest+t.offset,0xa5,span);
        GLuint id=texture(&t);uint64_t uploads=upload_count;
        if(texture(&t)!=id||upload_count!=uploads){fprintf(stderr,"FAIL: unchanged texture cache\n");failures++;}
        const size_t edits[]={0,3,8,16,28,29,31,32,span/2,span-1};
        for(unsigned edit=0;edit<sizeof(edits)/sizeof(edits[0]);edit++){
            if(edits[edit]>=span)continue;
            texture_guest[t.offset+edits[edit]]^=0x53;
            texture(&t);uint8_t readback[4096];
            glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_BYTE,readback);
            if(upload_count!=++uploads||memcmp(readback,texture_guest+t.offset,span)){
                fprintf(stderr,"FAIL: texture mutation shape=%u byte=%zu\n",shape,edits[edit]);failures++;}
        }
    }
    if(atoi(getenv("RECOMP_GL_TEXTURE_SNAPSHOT_MB"))==1){
        Nv2aTexture t={.offset=128,.width=256,.height=512,.pitch=1024,.color=0x12};
        memset(texture_guest,0x6d,sizeof(texture_guest));
        GLuint first=texture(&t);
        for(unsigned i=1;i<3;i++){Nv2aTexture other=t;other.offset+=i*524288;flips++;texture(&other);}
        Texture *entry=NULL;for(int i=0;i<1024;i++)if(textures[i].id==first)entry=&textures[i];
        if(!entry||entry->snapshot||snapshot_bytes>1024*1024){fprintf(stderr,"FAIL: bounded snapshot eviction\n");failures++;}
        texture_guest[t.offset+263171]^=0x7b;
        texture(&t);unsigned char *readback=malloc(524288);
        if(!readback)return 2;
        glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_BYTE,readback);
        if(memcmp(readback,texture_guest+t.offset,524288)){fprintf(stderr,"FAIL: evicted texture refresh\n");failures++;}
        free(readback);
    }
    registered->flip();
    registered->clear(&s,&rs,0xf3,0xff000000u,0xffffff00u);
    for(int j=0;j<3;j++){v[j].z=.2f;v[j].diffuse=0xff00ff00u;}
    registered->draw(&s,&b);
    glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,blended);
    if(blended[0]||blended[1]!=255||blended[2]){fprintf(stderr,"FAIL: render state after presentation\n");failures++;}
    GLenum error=glGetError();if(error){fprintf(stderr,"GL error %x\n",error);failures++;}
    if(!failures)puts("PASS: GPU combiners, 16-bit targets, vertex feedback, cached state and texture mutation/eviction");
    return failures?1:0;
}
