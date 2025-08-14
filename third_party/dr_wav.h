#ifndef dr_wav_h
#define dr_wav_h

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t drwav_uint64;

typedef struct {
    void* pUserData;
    void* (* on_malloc)(size_t sz, void* pUserData);
    void* (* on_realloc)(void* p, size_t sz, void* pUserData);
    void (* on_free)(void* p, void* pUserData);
} drwav_allocation_callbacks;

int16_t* drwav_open_file_and_read_pcm_frames_s16(const char* filename, unsigned int* channels, unsigned int* sampleRate, drwav_uint64* totalFrameCount, const drwav_allocation_callbacks* pAllocationCallbacks);

void drwav_free(void* p, const drwav_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif

#endif // dr_wav_h
