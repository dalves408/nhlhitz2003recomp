/* Immediate-context binding cache. All renderer setters use these wrappers;
 * invalidation is conservative at flip/reset/verification boundaries. Cached
 * pointers are weak: the D3D context retains its actual bound objects. */
#ifndef HITZ_D3D11_BIND_CACHE_H
#define HITZ_D3D11_BIND_CACHE_H
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <d3d11.h>

static int s_bind_mode = -1, s_upload_mode = -1;
static uint64_t s_bind_calls, s_bind_emitted, s_bind_skipped;
static uint64_t s_cb_uploads, s_cb_skipped, s_cb_bytes;
static void (*s_bind_before_change)(void);
static struct {
    ID3D11DeviceContext *ctx;
    unsigned valid;
    ID3D11RenderTargetView *rt;
    ID3D11DepthStencilView *ds;
    UINT rt_count, stencil, sample_mask;
    ID3D11BlendState *blend;
    float factor[4];
    ID3D11DepthStencilState *depth;
    ID3D11RasterizerState *raster;
    D3D11_VIEWPORT viewport;
    ID3D11InputLayout *layout;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11Buffer *vscb[16], *pscb[16], *vb[4];
    ID3D11ShaderResourceView *pssrv[8], *vssrv[8];
    ID3D11SamplerState *sampler[8];
    UINT vscb_mask, pscb_mask, pssrv_mask, vssrv_mask, sampler_mask, vb_mask;
    UINT stride[4], offset[4];
} s_bind;
enum { BC_RT=1, BC_BLEND=2, BC_DEPTH=4, BC_RASTER=8, BC_VIEW=16, BC_LAYOUT=32,
       BC_TOPOLOGY=64, BC_VS=128, BC_PS=256 };
static struct { ID3D11Buffer *buffer; unsigned bytes; unsigned char data[1024]; } s_cb_value[8];

static int bind_enabled(void)
{
    if (s_bind_mode < 0) { const char *e = getenv("RECOMP_D3D11_STATECACHE"); s_bind_mode = e && atoi(e) != 0; }
    return s_bind_mode;
}
static int upload_enabled(void)
{
    if (s_upload_mode < 0) { const char *e = getenv("RECOMP_D3D11_CONSTCACHE"); s_upload_mode = e && atoi(e) != 0; }
    return s_upload_mode;
}
static void bind_invalidate(void) { memset(&s_bind, 0, sizeof s_bind); }
static void constants_invalidate(void) { memset(s_cb_value, 0, sizeof s_cb_value); }
static int bind_skip(ID3D11DeviceContext *ctx, int same)
{
    s_bind_calls++;
    if (s_bind.ctx != ctx) { bind_invalidate(); s_bind.ctx = ctx; same = 0; }
    if (bind_enabled() && same) { s_bind_skipped++; return 1; }
    if(s_bind_before_change)s_bind_before_change();
    s_bind_emitted++; return 0;
}
static int bind_range(void **old, UINT *valid, UINT limit, UINT first, UINT n, void *const *values)
{
    UINT i, mask = 0; int same = 1;
    if (first > limit || n > limit-first) { *valid = 0; return 0; }
    for (i=0;i<n;i++) {
        UINT bit = 1u << (first+i); mask |= bit;
        if (!(*valid & bit) || old[first+i] != values[i]) same = 0;
        old[first+i] = values[i];
    }
    *valid |= mask;
    return same;
}
static void bind_targets(ID3D11DeviceContext *c, UINT n, ID3D11RenderTargetView *const *rt, ID3D11DepthStencilView *ds)
{
    ID3D11RenderTargetView *r = n == 1 ? rt[0] : NULL;
    int same = n <= 1 && (s_bind.valid & BC_RT) && s_bind.rt_count == n && s_bind.rt == r && s_bind.ds == ds;
    if (bind_skip(c, same)) return;
    /* A target change can automatically unbind aliasing SRVs. Explicitly
     * unbind PS textures, and invalidate VS tracking to account for aliases. */
    { ID3D11ShaderResourceView *none[8] = {0};
      c->lpVtbl->PSSetShaderResources(c, 0, 8, none);
      memset(s_bind.pssrv, 0, sizeof s_bind.pssrv); s_bind.pssrv_mask = 255;
      s_bind.vssrv_mask = 0; }
    c->lpVtbl->OMSetRenderTargets(c, n, rt, ds);
    s_bind.rt = r; s_bind.ds = ds; s_bind.rt_count = n;
    if (n <= 1) s_bind.valid |= BC_RT; else s_bind.valid &= ~BC_RT;
}
static void bind_blend(ID3D11DeviceContext *c, ID3D11BlendState *state, const FLOAT factor[4], UINT mask)
{
    const float ones[4] = {1,1,1,1}; const float *f = factor ? factor : ones;
    int same = (s_bind.valid & BC_BLEND) && s_bind.blend == state && s_bind.sample_mask == mask && !memcmp(s_bind.factor, f, 16);
    if (bind_skip(c, same)) return;
    c->lpVtbl->OMSetBlendState(c, state, factor, mask);
    s_bind.blend=state; s_bind.sample_mask=mask; memcpy(s_bind.factor,f,16); s_bind.valid|=BC_BLEND;
}
static void bind_depth(ID3D11DeviceContext *c, ID3D11DepthStencilState *state, UINT reference)
{
    if (bind_skip(c,(s_bind.valid&BC_DEPTH) && s_bind.depth==state && s_bind.stencil==reference)) return;
    c->lpVtbl->OMSetDepthStencilState(c,state,reference); s_bind.depth=state;s_bind.stencil=reference;s_bind.valid|=BC_DEPTH;
}
static void bind_raster(ID3D11DeviceContext *c, ID3D11RasterizerState *state)
{
    if (bind_skip(c,(s_bind.valid&BC_RASTER) && s_bind.raster==state)) return;
    c->lpVtbl->RSSetState(c,state);s_bind.raster=state;s_bind.valid|=BC_RASTER;
}
static void bind_viewports(ID3D11DeviceContext *c, UINT n, const D3D11_VIEWPORT *v)
{
    if (bind_skip(c,n==1 && (s_bind.valid&BC_VIEW) && !memcmp(&s_bind.viewport,v,sizeof *v))) return;
    c->lpVtbl->RSSetViewports(c,n,v);
    if(n==1){s_bind.viewport=*v;s_bind.valid|=BC_VIEW;}else s_bind.valid&=~BC_VIEW;
}
static void bind_layout(ID3D11DeviceContext *c, ID3D11InputLayout *layout)
{
    if(bind_skip(c,(s_bind.valid&BC_LAYOUT) && s_bind.layout==layout))return;
    c->lpVtbl->IASetInputLayout(c,layout);s_bind.layout=layout;s_bind.valid|=BC_LAYOUT;
}
static void bind_topology(ID3D11DeviceContext *c, D3D11_PRIMITIVE_TOPOLOGY t)
{
    if(bind_skip(c,(s_bind.valid&BC_TOPOLOGY) && s_bind.topology==t))return;
    c->lpVtbl->IASetPrimitiveTopology(c,t);s_bind.topology=t;s_bind.valid|=BC_TOPOLOGY;
}
static void bind_vs(ID3D11DeviceContext *c, ID3D11VertexShader *shader, ID3D11ClassInstance *const *classes, UINT n)
{
    if(bind_skip(c,!n && (s_bind.valid&BC_VS) && s_bind.vs==shader))return;
    c->lpVtbl->VSSetShader(c,shader,classes,n);s_bind.vs=shader;
    if(!n)s_bind.valid|=BC_VS;else s_bind.valid&=~BC_VS;
}
static void bind_ps(ID3D11DeviceContext *c, ID3D11PixelShader *shader, ID3D11ClassInstance *const *classes, UINT n)
{
    if(bind_skip(c,!n && (s_bind.valid&BC_PS) && s_bind.ps==shader))return;
    c->lpVtbl->PSSetShader(c,shader,classes,n);s_bind.ps=shader;
    if(!n)s_bind.valid|=BC_PS;else s_bind.valid&=~BC_PS;
}
#define BIND_RANGE_FUNCTION(name, api, type, member, mask, limit) \
static void name(ID3D11DeviceContext *c, UINT first, UINT n, type *const *values) \
{ \
    int same; \
    if(s_bind.ctx!=c){bind_invalidate();s_bind.ctx=c;} \
    same=bind_range((void**)s_bind.member,&s_bind.mask,limit,first,n,(void*const*)values); \
    if(bind_skip(c,same))return; \
    c->lpVtbl->api(c,first,n,values); \
}
BIND_RANGE_FUNCTION(bind_vscb,VSSetConstantBuffers,ID3D11Buffer,vscb,vscb_mask,16)
BIND_RANGE_FUNCTION(bind_pscb,PSSetConstantBuffers,ID3D11Buffer,pscb,pscb_mask,16)
BIND_RANGE_FUNCTION(bind_pssrv,PSSetShaderResources,ID3D11ShaderResourceView,pssrv,pssrv_mask,8)
BIND_RANGE_FUNCTION(bind_vssrv,VSSetShaderResources,ID3D11ShaderResourceView,vssrv,vssrv_mask,8)
BIND_RANGE_FUNCTION(bind_samplers,PSSetSamplers,ID3D11SamplerState,sampler,sampler_mask,8)
#undef BIND_RANGE_FUNCTION
static void bind_vertices(ID3D11DeviceContext *c, UINT first, UINT n, ID3D11Buffer *const *buffers, const UINT *strides, const UINT *offsets)
{
    UINT i;int same=1;
    if(s_bind.ctx!=c){bind_invalidate();s_bind.ctx=c;}
    if(first>4 || n>4-first){same=0;s_bind.vb_mask=0;}
    else for(i=0;i<n;i++){
        UINT at=first+i,bit=1u<<at;
        if(!(s_bind.vb_mask&bit)||s_bind.vb[at]!=buffers[i]||s_bind.stride[at]!=strides[i]||s_bind.offset[at]!=offsets[i])same=0;
        s_bind.vb[at]=buffers[i];s_bind.stride[at]=strides[i];s_bind.offset[at]=offsets[i];s_bind.vb_mask|=bit;
    }
    if(bind_skip(c,same))return;
    c->lpVtbl->IASetVertexBuffers(c,first,n,buffers,strides,offsets);
}
static HRESULT upload_constants(ID3D11DeviceContext *c, ID3D11Buffer *buffer, const void *data, unsigned bytes)
{
    unsigned i, slot=0; D3D11_MAPPED_SUBRESOURCE m; HRESULT hr;
    for(i=0;i<8;i++)if(s_cb_value[i].buffer==buffer){slot=i;break;}else if(!s_cb_value[i].buffer)slot=i;
    if(upload_enabled() && bytes<=sizeof s_cb_value[slot].data && s_cb_value[slot].buffer==buffer &&
       s_cb_value[slot].bytes==bytes && !memcmp(s_cb_value[slot].data,data,bytes)){
        s_cb_skipped++;return S_OK;
    }
    if(s_bind_before_change)s_bind_before_change();
    hr=c->lpVtbl->Map(c,(ID3D11Resource*)buffer,0,D3D11_MAP_WRITE_DISCARD,0,&m);
    if(FAILED(hr)){s_cb_value[slot].buffer=NULL;return hr;}
    memcpy(m.pData,data,bytes);c->lpVtbl->Unmap(c,(ID3D11Resource*)buffer,0);
    s_cb_uploads++;s_cb_bytes+=bytes;
    if(bytes<=sizeof s_cb_value[slot].data){s_cb_value[slot].buffer=buffer;s_cb_value[slot].bytes=bytes;memcpy(s_cb_value[slot].data,data,bytes);}
    else s_cb_value[slot].buffer=NULL;
    return hr;
}
#endif
