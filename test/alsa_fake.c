/* alsa-lib for the host build: a PCM that takes whatever is written at the real rate and keeps
 * the loudest sample, so a test can tell the mixer reached it. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int fake_pcm_opens, fake_pcm_peak;
long fake_pcm_frames;
static int handle;

int snd_pcm_open(void **pcm, const char *dev, int a, int b) {
    (void)a, (void)b;
    if (strcmp(dev, "plughw:0,0") && strncmp(dev, "plughw:", 7) && strcmp(dev, "plug:bluealsa"))
        return -2;
    *pcm = &handle;
    ++fake_pcm_opens;
    return 0;
}
int snd_pcm_close(void *p) { (void)p; return 0; }
int snd_pcm_drop(void *p) { (void)p; return 0; }
int snd_pcm_recover(void *p, int e, int s) { (void)p, (void)s; return e; }
int snd_pcm_set_params(void *p, int f, int a, unsigned c, unsigned r, int s, unsigned l) {
    (void)p, (void)f, (void)a, (void)c, (void)r, (void)s, (void)l;
    return 0;
}
long snd_pcm_writei(void *p, const void *buf, unsigned long frames) {
    (void)p;
    const short *s = buf;
    for (unsigned long i = 0; i < 2 * frames; ++i) {
        int v = s[i] < 0 ? -s[i] : s[i];
        if (v > fake_pcm_peak) fake_pcm_peak = v;
    }
    fake_pcm_frames += (long)frames;
    usleep((unsigned)(frames * 1000000 / 44100));
    return (long)frames;
}
const char *snd_strerror(int e) { (void)e; return "fake"; }
