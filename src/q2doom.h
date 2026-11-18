/* q2-doom's own interfaces between the backend (doomgeneric_q2.c), the sound module
 * (i_q2sound.c) and the host tests (test/). */
#ifndef Q2DOOM_H
#define Q2DOOM_H
#include <stdint.h>

/* doomgeneric_q2.c */
void q2_layout(int fullscreen);
void q2_blit(unsigned char *page, unsigned line, const uint32_t *src);
void q2_message(uint32_t *dst, const char *title, const char *body);
void q2_stick(int dx, int dy, int *fwd, int *side, int *run);
int q2_doom_key(int code, int menu);
int q2_menu_mode(void);
void q2_input(const char *datagram);

/* i_q2sound.c: Q2 Pod's arguments for a game with audio=1 (the ALSA device, then a soft
 * output's volume), before Doom starts */
void q2_sound_args(const char *device, const char *volume);
int q2_gain(const char *volume);
void q2_mix(int16_t *out, unsigned frames);
#endif
