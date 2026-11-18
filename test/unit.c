/* Host checks of q2-doom's own code: the picture's placement and turn onto the portrait panel,
 * the controls, and the sound mixer. Linked with the whole engine (main renamed). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "d_event.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "i_sound.h"
#include "m_controls.h"
#include "q2doom.h"

int DG_GetKey(int *pressed, unsigned char *key);
extern boolean messageToPrint;

static int fails;
#define CHECK(c) ((c) ? (void)0 : (void)(printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c), ++fails))

static uint32_t src[320 * 200];
static unsigned char page[320 * 4 * 375];
static uint32_t px(int X, int Y) { /* view (X, Y) on the portrait page */
    return ((uint32_t *)(page + X * 320 * 4))[319 - Y];
}

static void test_blit(void) {
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x) src[y * 320 + x] = (uint32_t)(x << 8 | y);
    memset(page, 0, sizeof page);
    q2_layout(0);
    q2_blit(page, 320 * 4, src);
    /* 320 x 240 at (27, 40): the source's corners land on the picture's corners */
    CHECK(px(27, 40) == (0xff000000u | 0));
    CHECK(px(27 + 319, 40) == (0xff000000u | 319u << 8));
    CHECK(px(27, 40 + 239) == (0xff000000u | 199));
    CHECK(px(27 + 319, 40 + 239) == (0xff000000u | 319u << 8 | 199));
    /* 1.2 rows a source row: rows 0..5 of the picture hold source rows 0,0,1,2,3,4 */
    CHECK((px(100, 40 + 6) & 0xff) == 5);
    /* borders untouched */
    CHECK(px(26, 100) == 0 && px(347, 100) == 0 && px(100, 39) == 0 && px(100, 280) == 0);
    /* every picture corner clear of the glass's 80 px rounded corners */
    int x0 = 27, y0 = 40;
    CHECK((80 - x0) * (80 - x0) + (80 - y0) * (80 - y0) <= 80 * 80);

    memset(page, 0, sizeof page);
    q2_layout(1); /* fullscreen: 375 x 281 */
    q2_blit(page, 320 * 4, src);
    CHECK(px(0, 19) == 0xff000000u && px(374, 19) == (0xff000000u | 319u << 8));
    CHECK((px(0, 19 + 280) & 0xff) == 199 && px(0, 18) == 0 && px(0, 300) == 0);
    q2_layout(0);
}

static void test_stick(void) {
    int f, s, r;
    q2_stick(0, -30, &f, &s, &r), CHECK(f == -1 && s == 0 && r == 0); /* up: forward */
    q2_stick(0, 30, &f, &s, &r), CHECK(f == 1 && s == 0 && r == 0);
    q2_stick(30, 0, &f, &s, &r), CHECK(f == 0 && s == 1 && r == 0); /* right: strafe right */
    q2_stick(-30, 0, &f, &s, &r), CHECK(f == 0 && s == -1);
    q2_stick(0, -50, &f, &s, &r), CHECK(f == -1 && r == 1); /* far: run */
    q2_stick(5, -5, &f, &s, &r), CHECK(f == 0 && s == 0 && r == 0); /* dead zone */
    q2_stick(30, -30, &f, &s, &r), CHECK(f == -1 && s == 1); /* diagonal */
    q2_stick(40, -12, &f, &s, &r), CHECK(f == 0 && s == 1); /* mostly sideways */
}

static void drain_events(void) {
    event_t *e;
    while ((e = D_PopEvent())) (void)e;
}

/* DG_GetKey's next key event, or -1 */
static int next_key(int *down) {
    unsigned char k;
    return DG_GetKey(down, &k) ? k : -1;
}

static void test_keys(void) {
    gamestate = GS_LEVEL, menuactive = false, demoplayback = false;
    CHECK(q2_doom_key(218, 0) == key_fire && q2_doom_key(218, 1) == KEY_ENTER);
    CHECK(q2_doom_key(170, 0) == KEY_ESCAPE && q2_doom_key(171, 0) == key_use);
    CHECK(q2_doom_key(222, 0) == key_map_toggle && q2_doom_key(223, 0) == key_nextweapon);

    int down;
    drain_events();
    q2_input("k d 218");
    CHECK(next_key(&down) == key_fire && down == 1);
    q2_input("k d 218"); /* a repeat: nothing */
    q2_input("k u 218"); /* a quick press: its release waits HOLD_MS */
    CHECK(next_key(&down) == -1);
    usleep(70000);
    CHECK(next_key(&down) == key_fire && down == 0);

    /* the release goes as the key the press sent, though a menu opened meanwhile */
    q2_input("k d 218");
    CHECK(next_key(&down) == key_fire);
    menuactive = true;
    usleep(70000);
    q2_input("k u 218");
    CHECK(next_key(&down) == key_fire && down == 0);
    menuactive = false;
}

static void test_wheel(void) {
    int down;
    gamestate = GS_LEVEL, menuactive = false, demoplayback = false;
    drain_events();
    q2_input("w 40");
    q2_input("w 60"); /* one tic's travel: one mouse event, anticlockwise is left */
    CHECK(next_key(&down) == -1);
    event_t *e = D_PopEvent();
    CHECK(e && e->type == ev_mouse && e->data2 == -100 * 16 && e->data3 == 0);
    CHECK(D_PopEvent() == 0);

    /* menus: steps of 28 units become arrows */
    menuactive = true;
    q2_input("w -60");
    CHECK(next_key(&down) == KEY_DOWNARROW && down == 1);
    CHECK(next_key(&down) == KEY_DOWNARROW && down == 0);
    CHECK(next_key(&down) == KEY_DOWNARROW && down == 1);
    CHECK(next_key(&down) == KEY_DOWNARROW && down == 0);
    CHECK(next_key(&down) == -1);
    CHECK(D_PopEvent() == 0); /* no mouse motion reaches a menu */
    q2_input("w 30");
    CHECK(next_key(&down) == KEY_UPARROW && down == 1);
    next_key(&down);
    menuactive = false;
    drain_events();
}

static void test_touch(void) {
    int down;
    gamestate = GS_LEVEL, menuactive = false, demoplayback = false;
    drain_events();
    q2_input("t d 200 200");
    q2_input("t m 200 170"); /* 30 up: walk forward */
    event_t *e = D_PopEvent();
    CHECK(e && e->type == ev_joystick && e->data3 == -1 && e->data4 == 0 && e->data1 == 0);
    q2_input("t m 200 120"); /* 80 up: the origin trails to 64 behind, and you run */
    e = D_PopEvent();
    CHECK(e && e->type == ev_joystick && e->data3 == -1 && e->data1 == 1 << joybspeed);
    q2_input("t m 200 150"); /* back 30 from the trailed origin (184): still forward, walking */
    e = D_PopEvent();
    CHECK(e && e->data3 == -1 && e->data1 == 0);
    q2_input("t m 200 200"); /* 16 below the trailed origin: backwards at once */
    e = D_PopEvent();
    CHECK(e && e->data3 == 1);
    q2_input("t u 200 200");
    e = D_PopEvent();
    CHECK(e && e->type == ev_joystick && e->data3 == 0 && e->data4 == 0 && e->data1 == 0);
    CHECK(next_key(&down) == -1); /* a drag isn't a tap */

    q2_input("t d 50 50");
    q2_input("t u 52 51"); /* a tap: use, held for a tic */
    CHECK(next_key(&down) == key_use && down == 1);
    CHECK(next_key(&down) == -1);
    usleep(70000);
    CHECK(next_key(&down) == key_use && down == 0);

    menuactive = true; /* in a menu a tap selects */
    q2_input("t d 50 50");
    q2_input("t u 50 50");
    CHECK(next_key(&down) == KEY_ENTER && down == 1);
    usleep(70000);
    next_key(&down);
    menuactive = false;
    drain_events();
}

typedef struct {
    const unsigned char *data;
    unsigned len, step;
} sound_t; /* i_q2sound.c's */

static void test_sound(void) {
    CHECK(q2_gain(0) == 65536 && q2_gain("h40") == 65536 && q2_gain("100") == 65536);
    CHECK(q2_gain("50") == 6553 && q2_gain("0") == 0 && q2_gain("25") == 65536 * 50 / 1000);

    static unsigned char samples[100];
    memset(samples, 255, sizeof samples);
    sound_t s = { samples, 100, 1 << 14 }; /* 11025 Hz into 44100: a quarter step */
    sfxinfo_t sfx;
    memset(&sfx, 0, sizeof sfx);
    sfx.driver_data = &s;
    int16_t out[2 * 512];
    CHECK(DG_sound_module.StartSound(&sfx, 0, 127, 128) == 0);
    CHECK(DG_sound_module.SoundIsPlaying(0));
    q2_mix(out, 512);
    /* 255 is +127; centre: left (254 - 128) * 127 / 127 = 126, right 128 */
    CHECK(out[0] == 127 * 126 && out[1] == 127 * 128);
    CHECK(out[2 * 399] == 127 * 126 && out[2 * 400] == 0); /* 100 samples x 4 frames, then quiet */
    CHECK(!DG_sound_module.SoundIsPlaying(0));
    /* full left */
    DG_sound_module.StartSound(&sfx, 1, 127, 0);
    q2_mix(out, 4);
    CHECK(out[0] == 127 * 254 && out[1] == 0);
    DG_sound_module.StopSound(1);
    q2_mix(out, 4);
    CHECK(out[0] == 0);
    /* many loud channels clip, never wrap */
    for (int c = 0; c < 8; ++c) DG_sound_module.StartSound(&sfx, c, 127, 0);
    q2_mix(out, 1);
    CHECK(out[0] == 32767);
    for (int c = 0; c < 8; ++c) DG_sound_module.StopSound(c);
}

static void test_message(void) {
    static uint32_t frame[320 * 200];
    q2_message(frame, "TITLE", "a b c");
    int lit = 0;
    for (int i = 0; i < 320 * 200; ++i) lit += frame[i] != 0;
    CHECK(lit > 50);
}

int main(void) {
    test_blit();
    test_stick();
    test_keys();
    test_wheel();
    test_touch();
    test_sound();
    test_message();
    if (fails) return printf("%d failed\n", fails), 1;
    printf("unit: all passed\n");
    return 0;
}
