/* Order-preserving coalescing for adjacent CPU-vertex triangle-list draws.
 * No GPU-program/procedural draws are delayed. Queued vertex bytes already
 * live in owned D3D buffers; all state/resource mutations flush first. */
#ifndef HITZ_D3D11_DRAW_BATCH_H
#define HITZ_D3D11_DRAW_BATCH_H
static int s_batch_mode = -1;
static struct { ID3D11DeviceContext *ctx; UINT count, first; } s_pending_draw;
static uint64_t s_physical_draws, s_merged_draws, s_batch_flushes;
static int batch_enabled(void)
{
    if(s_batch_mode<0){const char *e=getenv("RECOMP_D3D11_BATCH");s_batch_mode=e && atoi(e)!=0;}
    return s_batch_mode;
}
static void batch_flush(void)
{
    if(!s_pending_draw.count)return;
    s_pending_draw.ctx->lpVtbl->Draw(s_pending_draw.ctx,s_pending_draw.count,s_pending_draw.first);
    s_physical_draws++;s_batch_flushes++;s_pending_draw.count=0;s_bind_before_change=NULL;
}
static void batch_draw(ID3D11DeviceContext *c, UINT count, UINT first, int cpu_triangles)
{
    if(!batch_enabled() || !cpu_triangles || count>65535){
        batch_flush();c->lpVtbl->Draw(c,count,first);s_physical_draws++;return;
    }
    if(s_pending_draw.count && s_pending_draw.ctx==c &&
       first==s_pending_draw.first+s_pending_draw.count && count<=65535-s_pending_draw.count){
        s_pending_draw.count+=count;s_merged_draws++;return;
    }
    batch_flush();s_bind_before_change=batch_flush;
    s_pending_draw.ctx=c;s_pending_draw.count=count;s_pending_draw.first=first;
}
static void batch_draw_immediate(ID3D11DeviceContext *c,UINT count,UINT first){batch_draw(c,count,first,0);}
static HRESULT batch_map(ID3D11DeviceContext *c,ID3D11Resource *resource,UINT sub,D3D11_MAP mode,UINT flags,D3D11_MAPPED_SUBRESOURCE *mapped)
{
    if(mode!=D3D11_MAP_WRITE_NO_OVERWRITE)batch_flush();
    return c->lpVtbl->Map(c,resource,sub,mode,flags,mapped);
}
static void batch_update(ID3D11DeviceContext *c,ID3D11Resource *resource,UINT sub,const D3D11_BOX *box,const void *data,UINT row,UINT depth)
{batch_flush();c->lpVtbl->UpdateSubresource(c,resource,sub,box,data,row,depth);}
static void batch_copy(ID3D11DeviceContext *c,ID3D11Resource *dst,ID3D11Resource *src)
{batch_flush();c->lpVtbl->CopyResource(c,dst,src);}
static void batch_resolve(ID3D11DeviceContext *c,ID3D11Resource *dst,UINT dsub,ID3D11Resource *src,UINT ssub,DXGI_FORMAT fmt)
{batch_flush();c->lpVtbl->ResolveSubresource(c,dst,dsub,src,ssub,fmt);}
static void batch_clear_color(ID3D11DeviceContext *c,ID3D11RenderTargetView *view,const FLOAT color[4])
{batch_flush();c->lpVtbl->ClearRenderTargetView(c,view,color);}
static void batch_clear_depth(ID3D11DeviceContext *c,ID3D11DepthStencilView *view,UINT flags,FLOAT depth,UINT8 stencil)
{batch_flush();c->lpVtbl->ClearDepthStencilView(c,view,flags,depth,stencil);}
static void batch_mips(ID3D11DeviceContext *c,ID3D11ShaderResourceView *view)
{batch_flush();c->lpVtbl->GenerateMips(c,view);}
#endif
