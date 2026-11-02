/* Native GPU vertex execution. The executor owns and packs each input block;
 * no guest memory is retained after draw(). Transform feedback returns the
 * same 29-float record the independent CPU verifier already checks. */
typedef struct {GLuint shader;uint32_t hash,fixed,nslots,prog[NV2A_VSH_SLOTS_MAX][4];} GlVertexProgram;
static GlVertexProgram vertex_programs[64];
static unsigned vertex_program_count;
static GLuint raw_buffer,feedback_buffer;

static char *read_shader_file(const char *name)
{
    char path[4096];snprintf(path,sizeof(path),"%s/%s",HITZ_SHADER_DIR,name);
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);
    if(n<0||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    char *p=malloc((size_t)n+1);
    if(p&&fread(p,1,(size_t)n,f)==(size_t)n)p[n]=0;
    else {free(p);p=NULL;}fclose(f);return p;
}
static GLuint gpu_vertex_shader(const Nv2aGpuVs *e)
{
    init();if(epoxy_gl_version()<43||e->nslots>NV2A_VSH_SLOTS_MAX)return 0;
    for(unsigned i=0;i<vertex_program_count;i++){
        GlVertexProgram *v=&vertex_programs[i];
        if(v->fixed==e->fixed_function&&(v->fixed||(v->hash==e->hash&&v->nslots==e->nslots&&!memcmp(v->prog,e->prog,e->nslots*16))))return v->shader;
    }
    if(vertex_program_count>=64||program_count>=512)return 0;
    GlVertexProgram *v=&vertex_programs[vertex_program_count++];
    v->hash=e->hash;v->fixed=e->fixed_function;v->nslots=e->nslots;memcpy(v->prog,e->prog,e->nslots*16);
    char *wrapper=read_shader_file("gpu.vert"),*body=NULL;
    if(e->fixed_function)body=read_shader_file("ffp.glsl");
    else {body=malloc(65536);if(body&&!nv2a_vsh_gen_glsl(e,body,65536)){free(body);body=NULL;}}
    if(!wrapper||!body){free(wrapper);free(body);return 0;}
    char *marker=strstr(wrapper,"//@@BODY@@");
    if(!marker){free(wrapper);free(body);return 0;}
    *marker=0;const char *tail=marker+strlen("//@@BODY@@");
    size_t len=strlen(wrapper)+strlen(body)+strlen(tail)+1;char *src=malloc(len);
    if(!src){free(wrapper);free(body);return 0;}
    snprintf(src,len,"%s%s%s",wrapper,body,tail);free(wrapper);free(body);
    GLuint vs=glCreateShader(GL_VERTEX_SHADER);glShaderSource(vs,1,(const char *const *)&src,NULL);glCompileShader(vs);free(src);
    GLint ok;glGetShaderiv(vs,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[8192];glGetShaderInfoLog(vs,sizeof(log),NULL,log);fprintf(stderr,"[GL-GPUVS] CPU fallback: %s\n",log);glDeleteShader(vs);return 0;}
    v->shader=vs;
    /* Reserve a correct generic fragment fallback before specialization cache fills. */
    make_program(fragment_source,vs);
    fprintf(stderr,"[GL-GPUVS] shader=%u fixed=%u slots=%u\n",vertex_program_count,e->fixed_function,e->nslots);
    return vs;
}
int nv2a_gpuvs_probe(const Nv2aGpuVs *e){return gpu_vertex_shader(e)!=0;}

static void gpu_vertex_draw(const Nv2aGpuVs *e,uint32_t count)
{
    if(!raw_buffer)glGenBuffers(1,&raw_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,raw_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,(e->block_bytes+3u)&~3u,NULL,GL_STREAM_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,e->block_bytes,e->block);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,raw_buffer);
    uniform2f(U_GAA,e->aa_sx,e->aa_sy);uniform1ui(U_GFOGMODE,e->fog_mode);
    uniform1f(U_GFPX,e->fog_px);uniform1f(U_GFPY,e->fog_py);
    int verify=e->verify&&e->verify_out;
    if(e->verify_count)*e->verify_count=0;
    if(verify){if(!feedback_buffer)glGenBuffers(1,&feedback_buffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback_buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,(size_t)count*29*4,NULL,GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback_buffer);glBeginTransformFeedback(GL_TRIANGLES);}
    glDrawArrays(GL_TRIANGLES,0,count);
    if(verify){glEndTransformFeedback();uint32_t n=count<4096?count:4096;
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback_buffer);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,(size_t)n*29*4,e->verify_out);
        if(e->verify_count)*e->verify_count=n;glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);}
}
