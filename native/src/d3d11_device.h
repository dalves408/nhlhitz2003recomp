#ifndef HITZ_D3D11_DEVICE_H
#define HITZ_D3D11_DEVICE_H
#include <d3d11.h>
#include <dxgi1_6.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <stdio.h>
#include <string.h>

#define HITZ_GPU_LIMIT 32
typedef struct HitzGpuCandidate {
    IDXGIAdapter1 *adapter;
    DXGI_ADAPTER_DESC1 description;
    int priority;
} HitzGpuCandidate;

/* Driver classification wins over vendor/memory heuristics. */
static int hitz_gpu_priority(UINT vendor, SIZE_T memory, int discrete, int integrated)
{
    if (integrated) return 0;
    if (discrete) return 3;
    if (vendor == 0x10de || memory >= (SIZE_T)1024 * 1024 * 1024) return 2;
    return 1;
}
static void hitz_sort_gpus(HitzGpuCandidate *candidates, UINT count)
{
    UINT i, j;
    for (i = 1; i < count; ++i) {
        HitzGpuCandidate item = candidates[i];
        for (j = i; j && candidates[j-1].priority < item.priority; --j)
            candidates[j] = candidates[j-1];
        candidates[j] = item; /* Stable DXGI preference order within a class. */
    }
}
static int hitz_hybrid_type(LUID luid, D3DKMT_ADAPTERTYPE *type)
{
    HMODULE gdi = GetModuleHandleA("gdi32.dll");
    PFND3DKMT_OPENADAPTERFROMLUID open_adapter;
    PFND3DKMT_QUERYADAPTERINFO query_adapter;
    PFND3DKMT_CLOSEADAPTER close_adapter;
    D3DKMT_OPENADAPTERFROMLUID opened = {0};
    D3DKMT_QUERYADAPTERINFO query = {0};
    D3DKMT_CLOSEADAPTER closed = {0};
    LONG status;
    memset(type, 0, sizeof *type);
    if (!gdi) return 0;
    open_adapter = (PFND3DKMT_OPENADAPTERFROMLUID)GetProcAddress(gdi,"D3DKMTOpenAdapterFromLuid");
    query_adapter = (PFND3DKMT_QUERYADAPTERINFO)GetProcAddress(gdi,"D3DKMTQueryAdapterInfo");
    close_adapter = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(gdi,"D3DKMTCloseAdapter");
    if (!open_adapter || !query_adapter || !close_adapter) return 0;
    opened.AdapterLuid = luid;
    if (open_adapter(&opened) < 0) return 0;
    query.hAdapter = opened.hAdapter;
    query.Type = KMTQAITYPE_ADAPTERTYPE;
    query.pPrivateDriverData = type;
    query.PrivateDriverDataSize = sizeof *type;
    status = query_adapter(&query);
    closed.hAdapter = opened.hAdapter;
    close_adapter(&closed);
    return status >= 0;
}
static void hitz_gpu_name(const WCHAR *name, char out[512])
{
    if (!WideCharToMultiByte(CP_UTF8,0,name,-1,out,512,NULL,NULL)) strcpy(out,"(unknown)");
}
/* Takes ownership of adapter, including duplicates/rejected software devices. */
static void hitz_add_gpu(HitzGpuCandidate *candidates, UINT *count, IDXGIAdapter1 *adapter, FILE *report)
{
    DXGI_ADAPTER_DESC1 description;
    D3DKMT_ADAPTERTYPE type;
    UINT i;
    int known, priority;
    char name[512];
    if (FAILED(IDXGIAdapter1_GetDesc1(adapter,&description))) goto reject;
    for (i = 0; i < *count; ++i)
        if (description.AdapterLuid.HighPart == candidates[i].description.AdapterLuid.HighPart &&
            description.AdapterLuid.LowPart == candidates[i].description.AdapterLuid.LowPart) goto reject;
    hitz_gpu_name(description.Description,name);
    if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        if (report) fprintf(report,"Skipped software adapter: %s\n",name);
        goto reject;
    }
    known = hitz_hybrid_type(description.AdapterLuid,&type);
    priority = hitz_gpu_priority(description.VendorId,description.DedicatedVideoMemory,
        known && type.HybridDiscrete,known && type.HybridIntegrated);
    if (report) fprintf(report,"Detected: %s; vendor=%04X device=%04X VRAM_MB=%llu hybrid=%s priority=%d\n",
        name,description.VendorId,description.DeviceId,(unsigned long long)(description.DedicatedVideoMemory/1048576),
        known && type.HybridDiscrete ? "discrete" : known && type.HybridIntegrated ? "integrated" : "unknown",priority);
    if (*count == HITZ_GPU_LIMIT) goto reject;
    candidates[*count].adapter = adapter;
    candidates[*count].description = description;
    candidates[*count].priority = priority;
    ++*count;
    return;
reject:
    IDXGIAdapter1_Release(adapter);
}
static void hitz_report_actual_gpu(ID3D11Device *device, FILE *report)
{
    IDXGIDevice *dxgi = NULL;
    IDXGIAdapter *adapter = NULL;
    DXGI_ADAPTER_DESC description;
    char name[512];
    HRESULT hr;
    if (!report || !device) return;
    hr = ID3D11Device_QueryInterface(device,&IID_IDXGIDevice,(void **)&dxgi);
    if (SUCCEEDED(hr)) {
        hr = IDXGIDevice_GetAdapter(dxgi,&adapter);
        IDXGIDevice_Release(dxgi);
        if (SUCCEEDED(hr)) {
            hr = IDXGIAdapter_GetDesc(adapter,&description);
            IDXGIAdapter_Release(adapter);
            if (SUCCEEDED(hr)) {
                hitz_gpu_name(description.Description,name);
                fprintf(report,"ACTUAL RENDER GPU: %s; vendor=%04X device=%04X\n",
                    name,description.VendorId,description.DeviceId);
                return;
            }
        }
    }
    fprintf(report,"Actual adapter query failed: 0x%08lX\n",(unsigned long)hr);
}
static HRESULT hitz_create_graphics_device(UINT flags, const D3D_FEATURE_LEVEL *levels,
    UINT count, ID3D11Device **device, D3D_FEATURE_LEVEL *level, ID3D11DeviceContext **context, FILE *report)
{
    IDXGIFactory1 *factory = NULL;
    IDXGIFactory6 *preferred = NULL;
    HitzGpuCandidate candidates[HITZ_GPU_LIMIT];
    UINT found = 0, index;
    HRESULT hr;
    if (report) fprintf(report,"NHL Hitz 2003 GPU selection: dedicated-first-v2 (%s %s)\n",__DATE__,__TIME__);
    hr = CreateDXGIFactory1(&IID_IDXGIFactory1,(void **)&factory);
    if (SUCCEEDED(hr)) {
        hr = IDXGIFactory1_QueryInterface(factory,&IID_IDXGIFactory6,(void **)&preferred);
        if (report) fprintf(report,"Preferred-adapter interface: 0x%08lX\n",(unsigned long)hr);
        if (SUCCEEDED(hr)) {
            for (index = 0; index < HITZ_GPU_LIMIT; ++index) {
                IDXGIAdapter1 *adapter = NULL;
                hr = IDXGIFactory6_EnumAdapterByGpuPreference(preferred,index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,&IID_IDXGIAdapter1,(void **)&adapter);
                if (FAILED(hr)) break;
                hitz_add_gpu(candidates,&found,adapter,report);
            }
            if (report) fprintf(report,"Preferred enumeration ended: 0x%08lX\n",(unsigned long)hr);
            IDXGIFactory6_Release(preferred);
        }
        /* Always scan the legacy list. Older Optimus drivers may not support
         * or fully populate the newer GPU-preference enumeration. */
        for (index = 0; index < HITZ_GPU_LIMIT; ++index) {
            IDXGIAdapter1 *adapter = NULL;
            hr = IDXGIFactory1_EnumAdapters1(factory,index,&adapter);
            if (FAILED(hr)) break;
            hitz_add_gpu(candidates,&found,adapter,report);
        }
        if (report) fprintf(report,"Legacy enumeration ended: 0x%08lX\n",(unsigned long)hr);
        IDXGIFactory1_Release(factory);
    } else if (report) fprintf(report,"DXGI factory failed: 0x%08lX\n",(unsigned long)hr);
    hitz_sort_gpus(candidates,found);
    hr = E_FAIL;
    for (index = 0; index < found; ++index) {
        char name[512];
        hitz_gpu_name(candidates[index].description.Description,name);
        if (report) { fprintf(report,"Trying: %s\n",name); fflush(report); }
        hr = D3D11CreateDevice((IDXGIAdapter *)candidates[index].adapter,D3D_DRIVER_TYPE_UNKNOWN,
            NULL,flags,levels,count,D3D11_SDK_VERSION,device,level,context);
        if (report) fprintf(report,"Device creation: 0x%08lX\n",(unsigned long)hr);
        if (SUCCEEDED(hr)) break;
    }
    for (index = 0; index < found; ++index) IDXGIAdapter1_Release(candidates[index].adapter);
    if (FAILED(hr)) {
        if (report) { fprintf(report,"Trying Windows default hardware fallback\n"); fflush(report); }
        hr = D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,flags,levels,
            count,D3D11_SDK_VERSION,device,level,context);
        if (report) fprintf(report,"Fallback device creation: 0x%08lX\n",(unsigned long)hr);
    }
    if (SUCCEEDED(hr)) hitz_report_actual_gpu(*device,report);
    return hr;
}
#endif
