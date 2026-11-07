#ifndef HITZ_GPU_UPLOAD_COPY_H
#define HITZ_GPU_UPLOAD_COPY_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(__x86_64__)
#include <emmintrin.h>
#endif
/* Mapped upload heaps may use write-combining memory. Avoid write-allocating
 * cached stores there. SSE2 is baseline on x86-64, including pre-AVX2 CPUs.
 * Never read beyond the source or write outside the requested destination. */
static void gpu_upload_copy(void *destination,const void *source,size_t bytes)
{
#if defined(__x86_64__)
    unsigned char *d=destination;const unsigned char *s=source;
    while(bytes&&((uintptr_t)d&15u)){*d++=*s++;bytes--;}
    while(bytes>=64){
        __m128i a=_mm_loadu_si128((const __m128i *)(s));
        __m128i b=_mm_loadu_si128((const __m128i *)(s+16));
        __m128i c=_mm_loadu_si128((const __m128i *)(s+32));
        __m128i e=_mm_loadu_si128((const __m128i *)(s+48));
        _mm_stream_si128((__m128i *)d,a);_mm_stream_si128((__m128i *)(d+16),b);
        _mm_stream_si128((__m128i *)(d+32),c);_mm_stream_si128((__m128i *)(d+48),e);
        d+=64;s+=64;bytes-=64;
    }
    while(bytes>=16){_mm_stream_si128((__m128i *)d,_mm_loadu_si128((const __m128i *)s));d+=16;s+=16;bytes-=16;}
    while(bytes--)*d++=*s++;
    /* Publish non-temporal writes before the following GPU draw command. */
    _mm_sfence();
#else
    memcpy(destination,source,bytes);
#endif
}
#endif
