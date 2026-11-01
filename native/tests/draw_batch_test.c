#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include "../src/d3d11_bind_cache.h"
#include "../src/d3d11_draw_batch.h"
static int failed;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);failed++;}}while(0)
int main(void)
{
    ID3D11Device *dev=NULL;ID3D11DeviceContext *ctx=NULL;D3D_FEATURE_LEVEL fl;
    ID3D11Texture2D *tex=NULL,*stage=NULL;ID3D11RenderTargetView *rt=NULL;
    ID3D11Buffer *vb=NULL,*cb=NULL;ID3D11InputLayout *layout=NULL;ID3D11VertexShader *vs=NULL;ID3D11PixelShader *ps=NULL;
    ID3D11BlendState *blend=NULL;ID3DBlob *v=NULL,*p=NULL,*error=NULL;
    D3D11_TEXTURE2D_DESC td={0};D3D11_BUFFER_DESC bd={0};D3D11_SUBRESOURCE_DATA init={0};D3D11_MAPPED_SUBRESOURCE mapped;
    D3D11_INPUT_ELEMENT_DESC input={"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
    D3D11_VIEWPORT vp={0,0,4,4,0,1};D3D11_BLEND_DESC blend_desc={0};
    float vertices[12][2]={{-1,-1},{-1,1},{1,-1},{1,-1},{-1,1},{1,1}, {-1,-1},{-1,1},{1,-1},{1,-1},{-1,1},{1,1}};
    float red[4]={1,0,0,.5f},green[4]={0,1,0,.5f},black[4]={0,0,0,0};
    unsigned mode,scenario,row;unsigned char reference[64],pixels[64];UINT stride=8,offset=0;
    const char *src="cbuffer C:register(b0){float4 c;}float4 vs(float2 p:POSITION):SV_Position{return float4(p,0,1);}float4 ps():SV_Target{return c;}";
    s_bind_mode=s_upload_mode=1;
    CHECK(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&dev,&fl,&ctx)));if(!dev)return 1;
    td.Width=td.Height=4;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&tex)));CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)tex,NULL,&rt)));
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&stage)));
    bd.ByteWidth=sizeof vertices;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;init.pSysMem=vertices;CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(dev,&bd,&init,&vb)));
    bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(dev,&bd,NULL,&cb)));
    CHECK(SUCCEEDED(D3DCompile(src,strlen(src),NULL,NULL,NULL,"vs","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&error)));
    CHECK(SUCCEEDED(D3DCompile(src,strlen(src),NULL,NULL,NULL,"ps","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&error)));
    if(!v||!p||!vb||!cb||!rt||!stage)return 1;
    CHECK(SUCCEEDED(ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(v),ID3D10Blob_GetBufferSize(v),NULL,&vs)));
    CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(p),ID3D10Blob_GetBufferSize(p),NULL,&ps)));
    CHECK(SUCCEEDED(ID3D11Device_CreateInputLayout(dev,&input,1,ID3D10Blob_GetBufferPointer(v),ID3D10Blob_GetBufferSize(v),&layout)));
    blend_desc.RenderTarget[0].BlendEnable=TRUE;blend_desc.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;blend_desc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;blend_desc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;blend_desc.RenderTarget[0].RenderTargetWriteMask=15;
    CHECK(SUCCEEDED(ID3D11Device_CreateBlendState(dev,&blend_desc,&blend)));
    bind_targets(ctx,1,&rt,NULL);bind_layout(ctx,layout);bind_topology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    bind_vs(ctx,vs,NULL,0);bind_ps(ctx,ps,NULL,0);bind_vertices(ctx,0,1,&vb,&stride,&offset);bind_pscb(ctx,0,1,&cb);
    bind_depth(ctx,NULL,0);bind_raster(ctx,NULL);bind_blend(ctx,blend,NULL,~0u);
    for(scenario=0;scenario<4;scenario++)for(mode=0;mode<2;mode++){
        uint64_t merged=s_merged_draws,physical=s_physical_draws;
        s_batch_mode=mode;batch_clear_color(ctx,rt,black);bind_viewports(ctx,1,&vp);
        CHECK(SUCCEEDED(upload_constants(ctx,cb,red,16)));
        batch_draw(ctx,6,0,1);
        if(scenario==1)CHECK(SUCCEEDED(upload_constants(ctx,cb,green,16)));
        if(scenario==2){D3D11_VIEWPORT half=vp;half.Width=2;bind_viewports(ctx,1,&half);}
        if(scenario==3)batch_update(ctx,(ID3D11Resource*)vb,0,NULL,vertices,0,0);
        batch_draw(ctx,6,6,1);
        batch_copy(ctx,(ID3D11Resource*)stage,(ID3D11Resource*)tex);
        CHECK(SUCCEEDED(batch_map(ctx,(ID3D11Resource*)stage,0,D3D11_MAP_READ,0,&mapped)));
        for(row=0;row<4;row++)memcpy(pixels+row*16,(unsigned char*)mapped.pData+row*mapped.RowPitch,16);
        ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stage,0);
        if(!mode)memcpy(reference,pixels,64);else CHECK(!memcmp(reference,pixels,64));
        if(mode && !scenario){CHECK(s_merged_draws==merged+1);CHECK(s_physical_draws==physical+1);}
        else {CHECK(s_merged_draws==merged);CHECK(s_physical_draws==physical+2);}
        CHECK(pixels[3]>0);
    }
    batch_flush();ID3D11DeviceContext_ClearState(ctx);ID3D11DeviceContext_Flush(ctx);
    ID3D11BlendState_Release(blend);ID3D11InputLayout_Release(layout);ID3D11VertexShader_Release(vs);ID3D11PixelShader_Release(ps);
    ID3D10Blob_Release(v);ID3D10Blob_Release(p);if(error)ID3D10Blob_Release(error);
    ID3D11Buffer_Release(vb);ID3D11Buffer_Release(cb);ID3D11RenderTargetView_Release(rt);ID3D11Texture2D_Release(tex);ID3D11Texture2D_Release(stage);
    ID3D11DeviceContext_Release(ctx);ID3D11Device_Release(dev);
    if(!failed)puts("PASS: batched/scalar hardware pixels identical; alpha ordering, constant/view changes and resource updates flush correctly");
    return failed?1:0;
}
