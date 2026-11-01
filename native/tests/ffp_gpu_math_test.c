#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../src/d3d11_ffp_body.h"

/* Compile the production FFP body on the hardware GPU. A cancelling w row
 * distinguishes the CPU's left-associated arithmetic from dot reassociation. */
int main(int argc, char **argv)
{
    const char prefix[]="cbuffer T:register(b0){float4 cs[76];float4 attr[16];};"
        "float4 K(uint i){return cs[i];}float4 fetch(uint s,uint v){return attr[s];}"
        "RWStructuredBuffer<float4> result:register(u0);"
        "[numthreads(1,1,1)] void main(uint3 tid:SV_DispatchThreadID){uint vi=0;";
    const char suffix[]="result[0]=R[12];result[1]=O[3];}";
    char *src=malloc(sizeof prefix+sizeof k_hlsl_ffp_body+sizeof suffix);
    float constants[92][4]={0};
    float expected[4]={1,2,3,1};
    ID3D11Device *dev=NULL; ID3D11DeviceContext *ctx=NULL;
    ID3D11ComputeShader *shader=NULL;
    ID3DBlob *blob=NULL,*errors=NULL;
    ID3D11Buffer *cb=NULL,*out=NULL,*read=NULL;
    ID3D11UnorderedAccessView *uav=NULL,*nil=NULL;
    D3D11_BUFFER_DESC d={0}; D3D11_SUBRESOURCE_DATA init={0};
    D3D11_UNORDERED_ACCESS_VIEW_DESC u={0}; D3D11_MAPPED_SUBRESOURCE m;
    HRESULT hr; int failed=1; unsigned i;
    if (!src) return 1;
    strcpy(src,prefix);strcat(src,k_hlsl_ffp_body);strcat(src,suffix);
    hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&dev,NULL,&ctx);
    if (FAILED(hr)) goto done;
    hr=D3DCompile(src,strlen(src),NULL,NULL,NULL,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
    if (FAILED(hr)) {if(errors)puts(ID3D10Blob_GetBufferPointer(errors));goto done;}
    hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob),NULL,&shader);
    if (FAILED(hr)) goto done;
    constants[0][0]=1;constants[1][1]=2;constants[2][2]=3;
    constants[3][0]=1e8f;constants[3][1]=1;constants[3][2]=-1e8f;constants[3][3]=1;
    for(i=0;i<4;i++)constants[76][i]=1;
    if (argc > 1 && !strcmp(argv[1],"--depth")) {
        volatile float divisor=1.3f, quotient;
        constants[3][0]=constants[3][1]=constants[3][2]=0;
        constants[3][3]=divisor;
        constants[2][2]=0; constants[2][3]=-21810380.0f;
        constants[74][2]=16777215.0f;
        expected[0]=1/divisor;expected[1]=2/divisor;
        quotient=constants[2][3]/divisor;
        expected[2]=quotient+constants[74][2]; expected[3]=divisor;
    } else if (argc > 1) {
        FILE *f=fopen(argv[1],"rb");
        if (!f) goto done;
        if (fread(constants,sizeof constants,1,f)!=1 || fread(expected,sizeof expected,1,f)!=1) {
            fclose(f); goto done;
        }
        fclose(f);
    }
    d.ByteWidth=sizeof constants;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;init.pSysMem=constants;
    hr=ID3D11Device_CreateBuffer(dev,&d,&init,&cb);if(FAILED(hr))goto done;
    d.ByteWidth=32;d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;d.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;d.StructureByteStride=16;
    hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&out);if(FAILED(hr))goto done;
    u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;u.Buffer.NumElements=2;
    hr=ID3D11Device_CreateUnorderedAccessView(dev,(ID3D11Resource *)out,&u,&uav);if(FAILED(hr))goto done;
    ID3D11DeviceContext_CSSetShader(ctx,shader,NULL,0);
    ID3D11DeviceContext_CSSetConstantBuffers(ctx,0,1,&cb);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&uav,NULL);
    ID3D11DeviceContext_Dispatch(ctx,1,1,1);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&nil,NULL);
    d.BindFlags=0;d.MiscFlags=0;d.StructureByteStride=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&read);if(FAILED(hr))goto done;
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource *)read,(ID3D11Resource *)out);
    hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource *)read,0,D3D11_MAP_READ,0,&m);if(FAILED(hr))goto done;
    failed=0;
    for(i=0;i<4;i++) {
        float v=((const float *)m.pData)[i];
        printf("projection[%u] gpu=%g expected=%g\n",i,v,expected[i]);
        if(!isfinite(v)||fabsf(v-expected[i])>0.00001f)failed=1;
    }
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource *)read,0);
done:
    if(read)ID3D11Buffer_Release(read);if(out)ID3D11Buffer_Release(out);if(cb)ID3D11Buffer_Release(cb);
    if(uav)ID3D11UnorderedAccessView_Release(uav);if(shader)ID3D11ComputeShader_Release(shader);
    if(blob)ID3D10Blob_Release(blob);if(errors)ID3D10Blob_Release(errors);
    if(ctx)ID3D11DeviceContext_Release(ctx);if(dev)ID3D11Device_Release(dev);free(src);
    if(!failed)puts("PASS: production GPU FFP projection preserves CPU addition order");
    return failed;
}
