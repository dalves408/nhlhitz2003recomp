#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include "../src/d3d11_bind_cache.h"
static int failed;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failed++;}}while(0)
int main(void)
{
    ID3D11Device *dev=NULL;ID3D11DeviceContext *ctx=NULL;D3D_FEATURE_LEVEL level;
    ID3D11Texture2D *tex[2]={0},*stage=NULL;ID3D11RenderTargetView *rt[2]={0};ID3D11ShaderResourceView *srv[2]={0},*got=NULL;
    ID3D11Buffer *cb=NULL,*cb2=NULL,*gotcb=NULL;ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;
    D3D11_TEXTURE2D_DESC td={0};D3D11_BUFFER_DESC bd={0};D3D11_MAPPED_SUBRESOURCE map;
    ID3DBlob *v=NULL,*p=NULL,*err=NULL;D3D11_VIEWPORT vp={0,0,2,2,0,1};
    float color[4]={1,0,0,1};unsigned i;uint64_t skipped;
    const char *source="cbuffer C:register(b0){float4 c;}float4 vs(uint id:SV_VertexID):SV_Position {return float4(id==2?3:-1,id==1?3:-1,0,1);}float4 ps():SV_Target{return c;}";
    s_bind_mode=1;s_upload_mode=1;
    CHECK(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&dev,&level,&ctx)));
    if(!dev)return 1;
    td.Width=td.Height=2;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    for(i=0;i<2;i++){
        CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&tex[i])));
        CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)tex[i],NULL,&rt[i])));
        CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)tex[i],NULL,&srv[i])));
    }
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&stage)));
    bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(dev,&bd,NULL,&cb)));
    CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(dev,&bd,NULL,&cb2)));
    CHECK(SUCCEEDED(D3DCompile(source,strlen(source),NULL,NULL,NULL,"vs","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&err)));
    CHECK(SUCCEEDED(D3DCompile(source,strlen(source),NULL,NULL,NULL,"ps","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&err)));
    if(!v||!p||!cb||!cb2||!stage||!rt[0]||!rt[1])return 1;
    CHECK(SUCCEEDED(ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(v),ID3D10Blob_GetBufferSize(v),NULL,&vs)));
    CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(p),ID3D10Blob_GetBufferSize(p),NULL,&ps)));

    bind_targets(ctx,0,NULL,NULL);bind_pssrv(ctx,0,1,&srv[0]);
    bind_targets(ctx,1,&rt[0],NULL);
    ID3D11DeviceContext_PSGetShaderResources(ctx,0,1,&got);CHECK(got==NULL);if(got)ID3D11ShaderResourceView_Release(got);
    bind_pssrv(ctx,0,1,&srv[1]);skipped=s_bind_skipped;
    bind_targets(ctx,1,&rt[0],NULL);CHECK(s_bind_skipped==skipped+1);
    ID3D11DeviceContext_PSGetShaderResources(ctx,0,1,&got);CHECK(got==srv[1]);if(got)ID3D11ShaderResourceView_Release(got);
    bind_targets(ctx,1,&rt[1],NULL);
    ID3D11DeviceContext_PSGetShaderResources(ctx,0,1,&got);CHECK(got==NULL);if(got)ID3D11ShaderResourceView_Release(got);

    bind_vscb(ctx,0,1,&cb);bind_vscb(ctx,2,1,&cb2);skipped=s_bind_skipped;
    bind_vscb(ctx,0,1,&cb);CHECK(s_bind_skipped==skipped+1);
    ID3D11DeviceContext_VSGetConstantBuffers(ctx,2,1,&gotcb);CHECK(gotcb==cb2);if(gotcb)ID3D11Buffer_Release(gotcb);
    bind_invalidate();skipped=s_bind_skipped;bind_vscb(ctx,0,1,&cb);CHECK(s_bind_skipped==skipped);

    bind_layout(ctx,NULL);bind_topology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    bind_vs(ctx,vs,NULL,0);bind_ps(ctx,ps,NULL,0);bind_pscb(ctx,0,1,&cb);
    bind_viewports(ctx,1,&vp);bind_blend(ctx,NULL,NULL,~0u);bind_depth(ctx,NULL,0);bind_raster(ctx,NULL);
    for(i=0;i<3;i++){
        color[0]=i==1?0:1;color[1]=i==1?1:0;
        bind_targets(ctx,1,&rt[i%2],NULL);
        CHECK(SUCCEEDED(upload_constants(ctx,cb,color,sizeof color)));
        skipped=s_cb_skipped;CHECK(SUCCEEDED(upload_constants(ctx,cb,color,sizeof color)));CHECK(s_cb_skipped==skipped+1);
        ID3D11DeviceContext_Draw(ctx,3,0);
        ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)stage,(ID3D11Resource*)tex[i%2]);
        CHECK(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stage,0,D3D11_MAP_READ,0,&map)));
        if(map.pData){unsigned char *pixel=map.pData;CHECK(pixel[0]==(i==1?0:255));CHECK(pixel[1]==(i==1?255:0));CHECK(pixel[3]==255);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stage,0);}
    }
    constants_invalidate();skipped=s_cb_skipped;CHECK(SUCCEEDED(upload_constants(ctx,cb,color,sizeof color)));CHECK(s_cb_skipped==skipped);
    s_bind_mode=0;skipped=s_bind_skipped;bind_viewports(ctx,1,&vp);bind_viewports(ctx,1,&vp);CHECK(s_bind_skipped==skipped);
    ID3D11DeviceContext_ClearState(ctx);ID3D11DeviceContext_Flush(ctx);
    ID3D11VertexShader_Release(vs);ID3D11PixelShader_Release(ps);ID3D10Blob_Release(v);ID3D10Blob_Release(p);if(err)ID3D10Blob_Release(err);
    for(i=0;i<2;i++){ID3D11ShaderResourceView_Release(srv[i]);ID3D11RenderTargetView_Release(rt[i]);ID3D11Texture2D_Release(tex[i]);}
    ID3D11Texture2D_Release(stage);ID3D11Buffer_Release(cb);ID3D11Buffer_Release(cb2);ID3D11DeviceContext_Release(ctx);ID3D11Device_Release(dev);
    if(!failed)puts("PASS: actual D3D11 SRV/RT hazards, retained bindings, partial CB slots, invalidation, cached uploads and rendered readback");
    return failed?1:0;
}
