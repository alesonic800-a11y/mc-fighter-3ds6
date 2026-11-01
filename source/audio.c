#include "audio.h"
#ifdef __3DS__
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_CHANNELS 12
#define MUSIC_CHANNEL 0
#define SFX_FIRST 1
#define SFX_COUNT 8
#define VOICE_FIRST 9
#define VOICE_COUNT 3

typedef struct {
    ndspWaveBuf wave;
    void *pcm;
    char name[48];
    int active;
} AudioSlot;

static AudioSlot slots[AUDIO_CHANNELS];
static int audio_ready, next_sfx = 0, next_voice = 0;
static char current_music[48];

static unsigned rd16(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static unsigned rd32(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }

static void stop_slot(int ch) {
    if (ch < 0 || ch >= AUDIO_CHANNELS) return;
    ndspChnReset(ch);
    ndspChnWaveBufClear(ch);
    if (slots[ch].pcm) linearFree(slots[ch].pcm);
    memset(&slots[ch], 0, sizeof slots[ch]);
}

static int play_file(int ch, const char *name, int loop, float volume) {
    char path[96];
    unsigned char hdr[128];
    FILE *f;
    unsigned rate = 22050, data_size = 0, data_offset = 0;
    int channels = 2, bits = 16;
    void *pcm;
    size_t got;
    if (!audio_ready || ch < 0 || ch >= AUDIO_CHANNELS || !name || !*name) return 0;
    snprintf(path, sizeof path, "romfs:/audio/%s.wav", name);
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) { fclose(f); return 0; }

    /* Recorre chunks RIFF: admite WAV con chunks adicionales antes de fmt/data. */
    for (;;) {
        unsigned char chunk[8];
        if (fread(chunk, 1, 8, f) != 8) { fclose(f); return 0; }
        unsigned n = rd32(chunk + 4);
        if (!memcmp(chunk, "fmt ", 4)) {
            unsigned char fmt[40];
            if (n < 16 || n > sizeof fmt || fread(fmt, 1, n, f) != n) { fclose(f); return 0; }
            channels = (int)rd16(fmt + 2); rate = rd32(fmt + 4); bits = (int)rd16(fmt + 14);
        } else if (!memcmp(chunk, "data", 4)) {
            data_offset = (unsigned)ftell(f); data_size = n; break;
        } else {
            if (fseek(f, (long)n + (n & 1), SEEK_CUR)) { fclose(f); return 0; }
        }
    }
    if (!data_size || channels != 2 || bits != 16 || !rate) { fclose(f); return 0; }
    pcm = linearAlloc(data_size);
    if (!pcm) { fclose(f); return 0; }
    if (fseek(f, data_offset, SEEK_SET)) { linearFree(pcm); fclose(f); return 0; }
    got = fread(pcm, 1, data_size, f); fclose(f);
    if (got != data_size) { linearFree(pcm); return 0; }

    stop_slot(ch);
    slots[ch].pcm = pcm;
    slots[ch].active = 1;
    strncpy(slots[ch].name, name, sizeof slots[ch].name - 1);
    memset(&slots[ch].wave, 0, sizeof slots[ch].wave);
    slots[ch].wave.data_vaddr = pcm;
    slots[ch].wave.nsamples = data_size / 4;
    slots[ch].wave.looping = loop ? true : false;
    DSP_FlushDataCache(pcm, data_size);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, (float)rate);
    ndspChnSetFormat(ch, NDSP_FORMAT_STEREO_PCM16);
    {
        float mix[12] = {0};
        mix[0] = volume; mix[1] = volume;
        ndspChnSetMix(ch, mix);
    }
    ndspChnWaveBufAdd(ch, &slots[ch].wave);
    return 1;
}

void audio_init(void) {
    if (R_SUCCEEDED(ndspInit())) {
        audio_ready = 1;
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspSetMasterVol(1.0f);
    }
}
void audio_stop_music(void) {
    if (!audio_ready) return;
    stop_slot(MUSIC_CHANNEL);
    current_music[0] = 0;
}
void audio_music(const char *name) {
    if (!audio_ready || !name) return;
    if (!strcmp(current_music, name) && slots[MUSIC_CHANNEL].active) return;
    audio_stop_music();
    if (play_file(MUSIC_CHANNEL, name, 1, 0.36f)) {
        strncpy(current_music, name, sizeof current_music - 1);
    }
}
void audio_play(const char *name) {
    if (!audio_ready || !name) return;
    int ch = SFX_FIRST + (next_sfx++ % SFX_COUNT);
    play_file(ch, name, 0, 0.72f);
}
void audio_voice(const char *name) {
    if (!audio_ready || !name) return;
    int ch = VOICE_FIRST + (next_voice++ % VOICE_COUNT);
    play_file(ch, name, 0, 0.88f);
}
void audio_quit(void) {
    if (!audio_ready) return;
    for (int i = 0; i < AUDIO_CHANNELS; ++i) stop_slot(i);
    ndspExit();
    audio_ready = 0;
}
#else
void audio_init(void) {}
void audio_quit(void) {}
void audio_music(const char *name) { (void)name; }
void audio_stop_music(void) {}
void audio_play(const char *name) { (void)name; }
void audio_voice(const char *name) { (void)name; }
#endif
