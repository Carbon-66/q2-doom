/* q2-doom's sound: Doom's sound effects mixed here and played through ALSA, the way Q2 Pod's own
 * q2video plays (patch/video.c sound_open, docs/librespot.md's contract):
 *
 *   - Q2 Pod passes the output as argv[1]: "plughw:0,0" (the headphone DAC), "plug:bluealsa" or
 *     "usb" (the USB-Audio card in /proc/asound/cards), and off the jack argv[2], the volume:
 *     "NN" (0-100, applied here as hciplayer's Bluetooth curve) or "hNN" (the DAC applies it).
 *   - hciplayer lets go of the PCM a moment after Q2 Pod stops it, so the open is retried (2 s on
 *     the jack, 5 s elsewhere, and set_params again while bluealsa answers EBUSY).
 *   - On the jack, the DAC's PCM mode and mute are cleared through /dev/shanling_dac after the
 *     open, and the mute set again on close, as hciplayer leaves a stopped DAC.
 *   - Mono Audio and Swap L/R follow /tmp/q2mono and /tmp/q2swap.
 *
 * The device is opened on the mixer thread, so Doom starts drawing while it waits. Music isn't
 * played (Doom's MUS needs an OPL synth): DG_music_module is silent. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "doomtype.h"
#include "i_sound.h"
#include "m_misc.h"
#include "q2doom.h"
#include "w_wad.h"
#include "z_zone.h"

#define RATE 44100      /* 4 x 11025, the rate of almost every Doom sound */
#define CHUNK 256       /* frames a write, about 6 ms */
#define CHANNELS 16     /* Doom asks for snd_channels, 8 by default */
#define LATENCY_JACK 60000
#define LATENCY_OTHER 200000 /* Bluetooth needs the slack, as q2video gives it */
#define DAC "/dev/shanling_dac"
#define DAC_PCM 0xc0044d1bu
#define DAC_MUTE 0xc0044d1fu
#define MONO_FILE "/tmp/q2mono"
#define SWAP_FILE "/tmp/q2swap"

/* alsa-lib, as q2video declares it */
int snd_pcm_open(void **, const char *, int, int), snd_pcm_close(void *), snd_pcm_drop(void *);
int snd_pcm_set_params(void *, int, int, unsigned, unsigned, int, unsigned);
int snd_pcm_recover(void *, int, int);
long snd_pcm_writei(void *, const void *, unsigned long);
const char *snd_strerror(int);
int pthread_timedjoin_np(pthread_t, void **, const struct timespec *);

typedef struct {
    const unsigned char *data; /* 8-bit unsigned samples */
    unsigned len, step;        /* samples; source samples an output frame, 16.16 */
} sound_t;

static struct {
    const unsigned char *data;
    unsigned len, pos, step; /* pos and step 16.16 */
    int left, right;         /* 0-254 */
} chan[CHANNELS];

static struct {
    char device[32];
    int gain; /* 1/65536; 65536 where the DAC applies the volume */
    int ok;   /* a device was given */
    int use_prefix;
    void *pcm;
    int dac; /* /dev/shanling_dac on the jack, -1 */
    pthread_t thread;
    int running;
    volatile int stop, stuck;
    volatile int *mono_on, *swap_on;
} snd = { .dac = -1 };

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* hciplayer's Bluetooth soft volume for Q2 Pod's v (0-100), as q2video's bt_gain:
 * 0.002 v under 50, then 0.1 + 0.018 (v - 50), 1 at 100. "hNN", or none, is unity. */
int q2_gain(const char *volume) {
    if (!volume || volume[0] == 'h') return 65536;
    int v = atoi(volume);
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    return 65536 * (v < 50 ? 2 * v : 18 * v - 800) / 1000;
}

void q2_sound_args(const char *device, const char *volume) {
    snd.ok = device && device[0] && strcmp(device, "-");
    if (!snd.ok) return;
    M_StringCopy(snd.device, device, sizeof snd.device);
    snd.gain = q2_gain(volume);
}

/* The mix of every playing channel, n stereo frames. Called with the lock held. */
void q2_mix(int16_t *out, unsigned frames) {
    for (unsigned f = 0; f < frames; ++f) {
        int l = 0, r = 0;
        for (int c = 0; c < CHANNELS; ++c) {
            if (!chan[c].data) continue;
            int s = (int)chan[c].data[chan[c].pos >> 16] - 128;
            l += s * chan[c].left;
            r += s * chan[c].right;
            chan[c].pos += chan[c].step;
            if (chan[c].pos >> 16 >= chan[c].len) chan[c].data = 0;
        }
        out[2 * f] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        out[2 * f + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
}

/* /tmp/q2mono and /tmp/q2swap: Q2 Pod's ints, mapped once they exist */
static volatile int *map_flag(volatile int *have, const char *path) {
    if (have) return have;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    void *p = mmap(0, sizeof(int), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    return p == MAP_FAILED ? 0 : p;
}

/* "usb": the card /proc/asound/cards lists as USB-Audio, as plughw:N,0 */
static void usb_device(char *out, unsigned size) {
    char b[1024];
    int fd = open("/proc/asound/cards", O_RDONLY | O_CLOEXEC), n = fd >= 0 ? (int)read(fd, b, sizeof b - 1) : 0;
    if (fd >= 0) close(fd);
    b[n > 0 ? n : 0] = 0;
    char *u = strstr(b, "USB-Audio");
    while (u && u > b && u[-1] != '\n') --u; /* the line's start: " N [id ]: USB-Audio" */
    snprintf(out, size, "plughw:%d,0", u ? atoi(u) : 1);
}

static int sound_open(void) {
    char dev[32];
    int err = 0, off = 0;
    if (!strcmp(snd.device, "usb")) usb_device(dev, sizeof dev);
    else M_StringCopy(dev, snd.device, sizeof dev);
    int jack = !strcmp(dev, "plughw:0,0");
    for (int i = 0; i < (jack ? 20 : 50) && !snd.stop && (err = snd_pcm_open(&snd.pcm, dev, 0, 0)); ++i)
        snd.pcm = 0, usleep(100000);
    if (!snd.pcm) {
        fprintf(stderr, "Sound: can't open %s: %s\n", dev, snd_strerror(err));
        return 0;
    }
    /* S16_LE, RW_INTERLEAVED, stereo, resampled as needed; bluealsa claims its PCM here */
    for (int i = 0; (err = snd_pcm_set_params(snd.pcm, 2, 3, 2, RATE, 1,
                                              jack ? LATENCY_JACK : LATENCY_OTHER)) == -EBUSY &&
                    !jack && i < 50 && !snd.stop;
         ++i)
        usleep(100000);
    if (err) {
        fprintf(stderr, "Sound: %s: %s\n", dev, snd_strerror(err));
        snd_pcm_close(snd.pcm);
        snd.pcm = 0;
        return 0;
    }
    if (jack && (snd.dac = open(DAC, O_RDWR | O_CLOEXEC)) >= 0)
        ioctl(snd.dac, DAC_PCM, &off), ioctl(snd.dac, DAC_MUTE, &off);
    printf("Sound: %s at %d Hz\n", dev, RATE);
    return 1;
}

static void *mixer(void *unused) {
    (void)unused;
    int16_t buf[2 * CHUNK];
    if (!sound_open()) return 0;
    for (unsigned n = 0; !snd.stop; ++n) {
        if (!(n % 200)) snd.mono_on = map_flag(snd.mono_on, MONO_FILE), snd.swap_on = map_flag(snd.swap_on, SWAP_FILE);
        pthread_mutex_lock(&lock);
        q2_mix(buf, CHUNK);
        pthread_mutex_unlock(&lock);
        int gain = snd.gain, mono = snd.mono_on && *snd.mono_on, swap = snd.swap_on && *snd.swap_on;
        for (unsigned i = 0; i < 2 * CHUNK; i += 2) {
            int l = buf[i] * gain >> 16, r = buf[i + 1] * gain >> 16;
            if (mono) l = r = (l + r) >> 1;
            buf[i] = (int16_t)(swap ? r : l), buf[i + 1] = (int16_t)(swap ? l : r);
        }
        long w = snd_pcm_writei(snd.pcm, buf, CHUNK);
        if (w < 0 && snd_pcm_recover(snd.pcm, (int)w, 1) < 0) usleep(20000);
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * Doom's sound module */

static snddevice_t devices[] = { SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER,
                                 SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32 };

static boolean Init(boolean use_sfx_prefix) {
    snd.use_prefix = use_sfx_prefix;
    if (!snd.ok) return false; /* started without audio=1's arguments: silent */
    snd.stop = 0;
    snd.running = !pthread_create(&snd.thread, 0, mixer, 0);
    return snd.running;
}

static void Shutdown(void) {
    if (!snd.running) return;
    snd.stop = 1;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += 1;
    /* an output that takes no sound can hold the writer in ALSA: leave it, the process ends */
    if (pthread_timedjoin_np(snd.thread, 0, &t)) return;
    snd.running = 0;
    if (snd.pcm) snd_pcm_drop(snd.pcm), snd_pcm_close(snd.pcm), snd.pcm = 0;
    int on = 1;
    if (snd.dac >= 0) ioctl(snd.dac, DAC_MUTE, &on), close(snd.dac), snd.dac = -1;
}

static int GetSfxLumpNum(sfxinfo_t *sfx) {
    char name[9];
    if (sfx->link) sfx = sfx->link;
    if (snd.use_prefix) M_snprintf(name, sizeof name, "ds%s", sfx->name);
    else M_StringCopy(name, sfx->name, sizeof name);
    return W_CheckNumForName(name);
}

/* A DMX sound lump: format 3, the rate, the length, then 16 bytes of padding either side of
 * 8-bit unsigned samples (which DMX skips, as Chocolate Doom does) */
static sound_t *load(sfxinfo_t *sfx) {
    if (sfx->driver_data) return sfx->driver_data;
    sound_t *s = Z_Malloc(sizeof *s, PU_STATIC, 0);
    memset(s, 0, sizeof *s);
    sfx->driver_data = s;
    int lump = GetSfxLumpNum(sfx);
    if (lump < 0) return s;
    unsigned size = W_LumpLength(lump);
    const unsigned char *d = W_CacheLumpNum(lump, PU_STATIC);
    if (size < 8 || d[0] != 3 || d[1] != 0) return s;
    unsigned rate = d[2] | d[3] << 8, len = d[4] | d[5] << 8 | d[6] << 16 | (unsigned)d[7] << 24;
    if (len > size - 8 || len <= 48 || !rate) return s;
    s->data = d + 8 + 16;
    s->len = len - 32;
    s->step = (unsigned)(((uint64_t)rate << 16) / RATE);
    return s;
}

static void params(int c, int vol, int sep) {
    vol = vol < 0 ? 0 : vol > 127 ? 127 : vol;
    sep = sep < 0 ? 0 : sep > 254 ? 254 : sep;
    chan[c].left = (254 - sep) * vol / 127;
    chan[c].right = sep * vol / 127;
}

static void UpdateSoundParams(int c, int vol, int sep) {
    if (c < 0 || c >= CHANNELS) return;
    pthread_mutex_lock(&lock);
    params(c, vol, sep);
    pthread_mutex_unlock(&lock);
}

static int StartSound(sfxinfo_t *sfx, int c, int vol, int sep) {
    if (c < 0 || c >= CHANNELS) return -1;
    sound_t *s = load(sfx);
    pthread_mutex_lock(&lock);
    chan[c].data = s->data;
    chan[c].len = s->len;
    chan[c].pos = 0;
    chan[c].step = s->step;
    params(c, vol, sep);
    pthread_mutex_unlock(&lock);
    return s->data ? c : -1;
}

static void StopSound(int c) {
    if (c < 0 || c >= CHANNELS) return;
    pthread_mutex_lock(&lock);
    chan[c].data = 0;
    pthread_mutex_unlock(&lock);
}

static boolean SoundIsPlaying(int c) { return c >= 0 && c < CHANNELS && chan[c].data; }

static void Update(void) {}

static void CacheSounds(sfxinfo_t *sounds, int n) {
    (void)sounds, (void)n; /* each loads at its first play */
}

sound_module_t DG_sound_module = {
    devices, sizeof devices / sizeof *devices, Init, Shutdown, GetSfxLumpNum, Update,
    UpdateSoundParams, StartSound, StopSound, SoundIsPlaying, CacheSounds,
};

/* Music: none yet (Doom's MUS songs need an OPL synthesiser) */
static boolean MusInit(void) { return true; }
static void MusNone(void) {}
static void MusVolume(int v) { (void)v; }
static void *MusRegister(void *data, int len) {
    (void)data, (void)len;
    return (void *)1;
}
static void MusUnregister(void *h) { (void)h; }
static void MusPlay(void *h, boolean loop) { (void)h, (void)loop; }
static boolean MusPlaying(void) { return false; }

music_module_t DG_music_module = {
    devices, sizeof devices / sizeof *devices, MusInit, MusNone, MusVolume, MusNone, MusNone,
    MusRegister, MusUnregister, MusPlay, MusNone, MusPlaying, NULL,
};

/* i_sound.c binds these config variables; they belong to the SDL module this one replaces */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;
