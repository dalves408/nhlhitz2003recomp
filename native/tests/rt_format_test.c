#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/nv2a_surface_formats.h"

static int sample(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                  ID3D11ShaderResourceView *srv, const float want[4])
{
    const char code[] = "Texture2D<float4> t:register(t0); RWStructuredBuffer<float4> o:register(u0);"
        "[numthreads(4,4,1)] void main(uint3 p:SV_DispatchThreadID){o[p.y*4+p.x]=t.Load(int3(p.xy,0));}";
    ID3DBlob *blob=NULL, *errors=NULL;
    ID3D11ComputeShader *cs=NULL;
    ID3D11Buffer *out=NULL, *read=NULL;
    ID3D11UnorderedAccessView *uav=NULL, *nil_uav=NULL;
    ID3D11ShaderResourceView *nil_srv=NULL;
    D3D11_BUFFER_DESC d={0}; D3D11_UNORDERED_ACCESS_VIEW_DESC u={0};
    D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr; int failed=1;
    hr=D3DCompile(code,strlen(code),NULL,NULL,NULL,"main","cs_5_0",0,0,&blob,&errors);
    if (FAILED(hr)) goto done;
    hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob),NULL,&cs);
    if (FAILED(hr)) goto done;
    d.ByteWidth=16*4*sizeof(float); d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    d.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; d.StructureByteStride=4*sizeof(float);
    hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&out); if (FAILED(hr)) goto done;
    u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; u.Buffer.NumElements=16;
    hr=ID3D11Device_CreateUnorderedAccessView(dev,(ID3D11Resource *)out,&u,&uav); if (FAILED(hr)) goto done;
    ID3D11DeviceContext_CSSetShader(ctx,cs,NULL,0);
    ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&srv);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&uav,NULL);
    ID3D11DeviceContext_Dispatch(ctx,1,1,1);
    ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&nil_srv);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&nil_uav,NULL);
    d.BindFlags=0; d.Usage=D3D11_USAGE_STAGING; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    d.MiscFlags=0; d.StructureByteStride=0;
    hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&read); if (FAILED(hr)) goto done;
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)read,(ID3D11Resource *)out);
    hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)read,0,D3D11_MAP_READ,0,&m);
    if (SUCCEEDED(hr)) {
        unsigned i; failed=0;
        for (i=0;i<64;i++) {
            float diff=((const float *)m.pData)[i]-want[i%4];
            if (diff < -0.0001f || diff > 0.0001f) {
                if (i<4) printf("sample channel %u got %.8f expected %.8f\n",i,((const float *)m.pData)[i],want[i%4]);
                failed=1;
            }
        }
        ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)read,0);
    }
done:
    if (errors) ID3D10Blob_Release(errors);
    if (blob) ID3D10Blob_Release(blob);
    if (cs) ID3D11ComputeShader_Release(cs);
    if (uav) ID3D11UnorderedAccessView_Release(uav);
    if (out) ID3D11Buffer_Release(out);
    if (read) ID3D11Buffer_Release(read);
    return failed;
}

/* Native-format color-target round trip. Exact code points avoid depending on
 * unspecified halfway rounding in ClearRenderTargetView. */
static int check(ID3D11Device *dev, ID3D11DeviceContext *ctx, DXGI_FORMAT format,
                 const float color[4], uint16_t want, const float sampled[4])
{
    D3D11_TEXTURE2D_DESC d;
    D3D11_MAPPED_SUBRESOURCE m;
    ID3D11Texture2D *tex = NULL, *stage = NULL;
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11ShaderResourceView *srv = NULL;
    HRESULT hr; UINT support = 0; int failed = 1;
    ID3D11Device_CheckFormatSupport(dev, format, &support);
    printf("DXGI format %u support=%08X\n", format, support);
    memset(&d, 0, sizeof d);
    d.Width = d.Height = 4; d.MipLevels = d.ArraySize = d.SampleDesc.Count = 1;
    d.Format = format; d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &tex);
    if (FAILED(hr)) { printf("CreateTexture2D failed %08lX\n", (unsigned long)hr); goto done; }
    hr = ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)tex, NULL, &rtv);
    if (FAILED(hr)) { printf("CreateRTV failed %08lX\n", (unsigned long)hr); goto done; }
    hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &srv);
    if (FAILED(hr)) { printf("CreateSRV failed %08lX\n", (unsigned long)hr); goto done; }
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, color);
    d.BindFlags = 0; d.Usage = D3D11_USAGE_STAGING; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = ID3D11Device_CreateTexture2D(dev, &d, NULL, &stage);
    if (FAILED(hr)) goto done;
    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)stage, (ID3D11Resource *)tex);
    hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)stage, 0, D3D11_MAP_READ, 0, &m);
    if (SUCCEEDED(hr)) {
        unsigned x, y; failed = 0;
        for (y=0; y<4; ++y) for (x=0; x<4; ++x) {
            uint16_t got = ((const uint16_t *)((const uint8_t *)m.pData+y*m.RowPitch))[x];
            if (got != want) { printf("pixel %u,%u got %04X expected %04X\n", x,y,got,want); failed=1; }
        }
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)stage, 0);
    }
    if (!failed) failed=sample(dev,ctx,srv,sampled);
done:
    if (stage) ID3D11Texture2D_Release(stage);
    if (srv) ID3D11ShaderResourceView_Release(srv);
    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (tex) ID3D11Texture2D_Release(tex);
    return failed;
}
int main(void)
{
    ID3D11Device *dev = NULL; ID3D11DeviceContext *ctx = NULL;
    const float rgb565[4] = {8.0f/31, 16.0f/63, 24.0f/31, 1};
    const float argb1555[4] = {8.0f/31, 16.0f/31, 24.0f/31, 1};
    /* These native formats expand stored bits to eight-bit channels before
     * sampling. Check those code points, not an exact 5/6-bit division. */
    const float sample565[4] = {66.0f/255,65.0f/255,198.0f/255,1};
    const float sample1555[4] = {66.0f/255,132.0f/255,198.0f/255,1};
    int failed;
    HRESULT hr = D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&dev,NULL,&ctx);
    if (FAILED(hr)) { printf("D3D11CreateDevice failed %08lX\n", (unsigned long)hr); return 1; }
    failed = check(dev,ctx,hitz_rt_dxgi_format(3),rgb565,(uint16_t)((8<<11)|(16<<5)|24),sample565);
    failed |= check(dev,ctx,hitz_rt_dxgi_format(2),argb1555,(uint16_t)(0x8000|(8<<10)|(16<<5)|24),sample1555);
    if (hitz_rt_dxgi_format(1) != DXGI_FORMAT_B5G5R5A1_UNORM ||
        hitz_rt_bgra16((8<<11)|(16<<5)|24,DXGI_FORMAT_B5G6R5_UNORM) != 0xFF4241C6u ||
        hitz_rt_bgra16(0x8000|(8<<10)|(16<<5)|24,DXGI_FORMAT_B5G5R5A1_UNORM) != 0xFF4284C6u) failed=1;
    ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    if (!failed) puts("PASS: RGB565 and X1R5G5B5 render targets / raw readback / GPU texture sampling");
    return failed;
}
