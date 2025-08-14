/*
  Minimal dr_wav-compatible shim for 16-bit PCM WAV I/O
  -----------------------------------------------------
  This header implements a **very small subset** of the dr_wav API that is
  sufficient for this project:
    - drwav_data_format, drwav, drwav_container, DR_WAVE_FORMAT_PCM
    - drwav_init_file_write, drwav_write_pcm_frames, drwav_uninit
    - drwav_open_file_and_read_pcm_frames_s16, drwav_free

  It handles mono or multi-channel 16-bit PCM RIFF/WAVE files.
  It is *not* the full upstream dr_wav from mackron/dr_libs.

  License: MIT (this shim only)
*/

#ifndef MINI_DR_WAV_H_INCLUDED
#define MINI_DR_WAV_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------- Types compatible with dr_wav -------- */

typedef uint64_t drwav_uint64;
typedef uint32_t drwav_uint32;
typedef uint16_t drwav_uint16;
typedef int16_t  drwav_int16;

typedef enum
{
    drwav_container_unknown = 0,
    drwav_container_riff    = 1
} drwav_container;

#define DR_WAVE_FORMAT_PCM 1

typedef struct
{
    drwav_container container;
    drwav_uint32    format;
    drwav_uint32    channels;
    drwav_uint32    sampleRate;
    drwav_uint32    bitsPerSample;
} drwav_data_format;

typedef struct
{
    FILE*            fp;
    drwav_data_format fmt;
    drwav_uint64     framesWritten;
    long             dataChunkPos;   /* file offset where "data" chunk size is stored */
} drwav;

/* -------- Public API (subset) -------- */

int  drwav_init_file_write(drwav* pWav, const char* filename, const drwav_data_format* format, const void* pAllocationCallbacks);
drwav_uint64 drwav_write_pcm_frames(drwav* pWav, drwav_uint64 framesToWrite, const void* pData);
void drwav_uninit(drwav* pWav);

drwav_int16* drwav_open_file_and_read_pcm_frames_s16(const char* filename,
                                                     unsigned int* channels,
                                                     unsigned int* sampleRate,
                                                     drwav_uint64* totalPCMFrameCount,
                                                     const void* pAllocationCallbacks);

void drwav_free(void* p, const void* pAllocationCallbacks);

/* -------- Implementation -------- */
#ifdef DR_WAV_IMPLEMENTATION

/* write little-endian helpers */
static void drwav__write_u32le(FILE* f, drwav_uint32 v)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v      & 0xFF);
    b[1] = (unsigned char)((v>>8) & 0xFF);
    b[2] = (unsigned char)((v>>16)& 0xFF);
    b[3] = (unsigned char)((v>>24)& 0xFF);
    fwrite(b, 1, 4, f);
}
static void drwav__write_u16le(FILE* f, drwav_uint16 v)
{
    unsigned char b[2];
    b[0] = (unsigned char)(v      & 0xFF);
    b[1] = (unsigned char)((v>>8) & 0xFF);
    fwrite(b, 1, 2, f);
}

/* read little-endian helpers */
static drwav_uint32 drwav__read_u32le(FILE* f)
{
    unsigned char b[4];
    if (fread(b,1,4,f)!=4) return 0;
    return (drwav_uint32)b[0] | ((drwav_uint32)b[1] << 8) | ((drwav_uint32)b[2] << 16) | ((drwav_uint32)b[3] << 24);
}
static drwav_uint16 drwav__read_u16le(FILE* f)
{
    unsigned char b[2];
    if (fread(b,1,2,f)!=2) return 0;
    return (drwav_uint16)b[0] | ((drwav_uint16)b[1] << 8);
}

/* ---- Writing ---- */
int drwav_init_file_write(drwav* pWav, const char* filename, const drwav_data_format* format, const void* /*pAllocationCallbacks*/)
{
    if (!pWav || !filename || !format) return 0;
    if (format->format != DR_WAVE_FORMAT_PCM) return 0;
    if (format->bitsPerSample != 16) return 0;
    if (format->channels == 0) return 0;

    pWav->fp = fopen(filename, "wb");
    if (!pWav->fp) return 0;

    pWav->fmt = *format;
    pWav->framesWritten = 0;
    pWav->dataChunkPos = 0;

    /* RIFF Header */
    fwrite("RIFF", 1, 4, pWav->fp);
    drwav__write_u32le(pWav->fp, 0); /* placeholder for RIFF chunk size */
    fwrite("WAVE", 1, 4, pWav->fp);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, pWav->fp);
    drwav__write_u32le(pWav->fp, 16); /* PCM fmt chunk size */
    drwav__write_u16le(pWav->fp, (drwav_uint16)format->format);
    drwav__write_u16le(pWav->fp, (drwav_uint16)format->channels);
    drwav__write_u32le(pWav->fp, (drwav_uint32)format->sampleRate);
    drwav_uint32 byteRate = (drwav_uint32)(format->sampleRate * format->channels * (format->bitsPerSample/8));
    drwav__write_u32le(pWav->fp, byteRate);
    drwav__write_u16le(pWav->fp, (drwav_uint16)(format->channels * (format->bitsPerSample/8))); /* blockAlign */
    drwav__write_u16le(pWav->fp, (drwav_uint16)format->bitsPerSample);

    /* data chunk header */
    fwrite("data", 1, 4, pWav->fp);
    pWav->dataChunkPos = ftell(pWav->fp);
    drwav__write_u32le(pWav->fp, 0); /* placeholder for data size */

    return 1;
}

drwav_uint64 drwav_write_pcm_frames(drwav* pWav, drwav_uint64 framesToWrite, const void* pData)
{
    if (!pWav || !pWav->fp || !pData) return 0;
    if (pWav->fmt.bitsPerSample != 16) return 0;
    drwav_uint64 samples = framesToWrite * pWav->fmt.channels;
    size_t written = fwrite(pData, sizeof(int16_t), (size_t)samples, pWav->fp);
    drwav_uint64 frames = written / pWav->fmt.channels;
    pWav->framesWritten += frames;
    return frames;
}

void drwav_uninit(drwav* pWav)
{
    if (!pWav || !pWav->fp) return;

    long fileEnd = ftell(pWav->fp);
    /* finalize data chunk size */
    if (pWav->dataChunkPos > 0) {
        long cur = ftell(pWav->fp);
        drwav_uint32 dataSize = (drwav_uint32)(fileEnd - (pWav->dataChunkPos + 4));
        fseek(pWav->fp, pWav->dataChunkPos, SEEK_SET);
        drwav__write_u32le(pWav->fp, dataSize);
        fseek(pWav->fp, cur, SEEK_SET);
    }

    /* finalize RIFF chunk size (file size - 8) */
    {
        long cur = ftell(pWav->fp);
        drwav_uint32 riffSize = (drwav_uint32)(fileEnd - 8);
        fseek(pWav->fp, 4, SEEK_SET);
        drwav__write_u32le(pWav->fp, riffSize);
        fseek(pWav->fp, cur, SEEK_SET);
    }

    fclose(pWav->fp);
    pWav->fp = NULL;
}

/* ---- Reading ---- */
static int drwav__read_chunk_header(FILE* f, char id[4], drwav_uint32* pSize)
{
    if (fread(id,1,4,f)!=4) return 0;
    *pSize = drwav__read_u32le(f);
    return 1;
}

drwav_int16* drwav_open_file_and_read_pcm_frames_s16(const char* filename,
                                                     unsigned int* channels,
                                                     unsigned int* sampleRate,
                                                     drwav_uint64* totalPCMFrameCount,
                                                     const void* /*pAllocationCallbacks*/)
{
    if (channels) *channels = 0;
    if (sampleRate) *sampleRate = 0;
    if (totalPCMFrameCount) *totalPCMFrameCount = 0;

    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    char riff[4]; char wave[4];
    if (fread(riff,1,4,f)!=4 || memcmp(riff,"RIFF",4)!=0) { fclose(f); return NULL; }
    (void)drwav__read_u32le(f); /* riff size */
    if (fread(wave,1,4,f)!=4 || memcmp(wave,"WAVE",4)!=0) { fclose(f); return NULL; }

    /* parse chunks */
    drwav_uint16 fmtFormat=0, fmtChannels=0, fmtBits=0;
    drwav_uint32 fmtSampleRate=0;
    drwav_uint32 dataSize=0;
    long dataPos = 0;

    while (!feof(f)) {
        char id[4]; drwav_uint32 sz=0;
        if (!drwav__read_chunk_header(f, id, &sz)) break;
        long next = ftell(f) + sz;

        if (memcmp(id,"fmt ",4)==0) {
            fmtFormat     = drwav__read_u16le(f);
            fmtChannels   = drwav__read_u16le(f);
            fmtSampleRate = drwav__read_u32le(f);
            (void)drwav__read_u32le(f); /* byteRate */
            (void)drwav__read_u16le(f); /* blockAlign */
            fmtBits       = drwav__read_u16le(f);
            /* skip any remaining fmt bytes */
            fseek(f, next, SEEK_SET);
        } else if (memcmp(id,"data",4)==0) {
            dataPos  = ftell(f);
            dataSize = sz;
            fseek(f, next, SEEK_SET);
        } else {
            /* skip unknown chunk */
            fseek(f, next, SEEK_SET);
        }
        if (dataPos && fmtFormat) break; /* we have enough */
    }

    if (fmtFormat != DR_WAVE_FORMAT_PCM || fmtBits != 16 || dataPos == 0 || fmtChannels == 0) {
        fclose(f);
        return NULL;
    }

    /* allocate and read */
    drwav_uint64 totalSamples = dataSize / 2; /* 16-bit */
    drwav_int16* pcm = (drwav_int16*)malloc((size_t)(totalSamples * sizeof(drwav_int16)));
    if (!pcm) { fclose(f); return NULL; }

    fseek(f, dataPos, SEEK_SET);
    size_t rd = fread(pcm, sizeof(drwav_int16), (size_t)totalSamples, f);
    (void)rd;
    fclose(f);

    if (channels) *channels = fmtChannels;
    if (sampleRate) *sampleRate = fmtSampleRate;
    if (totalPCMFrameCount) *totalPCMFrameCount = (fmtChannels > 0) ? (totalSamples / fmtChannels) : 0;
    return pcm;
}

void drwav_free(void* p, const void* /*pAllocationCallbacks*/)
{
    free(p);
}

#endif /* DR_WAV_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MINI_DR_WAV_H_INCLUDED */
