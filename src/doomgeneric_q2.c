/* q2-doom: doomgeneric on the Shanling Q2 under Q2 Pod, as a card game.
 *
 * Q2 Pod (github.com/DiamondBond/q2-pod, docs/internals.md "Card games") forks this binary from
 * Library -> Games when /mnt/mmc/.doom/q2app holds game=1. While it runs, demo stops painting and
 * the game owns the screen and input:
 *
 *   screen  /dev/fb0: the portrait panel, 320 x 375, 32 bpp, red at bit 16, two pages (320 x 750
 *           virtual). The UI's view is landscape, 375 x 320, the panel turned anticlockwise, so a
 *           view pixel (X, Y) is the panel's row X, column 319 - Y. Frames go to the hidden page,
 *           then FBIOPAN_DISPLAY, as q2video and demo do.
 *   input   text datagrams on /tmp/q2-doom.input.sock: "k d CODE" / "k u CODE" (AWTK key codes:
 *           Return 170, Play/Pause 171, Centre 218, Previous 222, Next 223), "t d|m|u X Y" (a
 *           touch, in the view's 375 x 320 pixels) and "w N" (the wheel's raw travel, 200 a turn,
 *           rising anticlockwise). Return held 700 ms is Q2 Pod's: it ends the game.
 *   sound   with audio=1 the binary gets q2video's arguments: the ALSA device ("plughw:0,0",
 *           "plug:bluealsa" or "usb") and, off the jack, a volume ("NN", or "hNN" applied by the
 *           DAC). See i_q2sound.c.
 *
 * Controls (see README.md):
 *   wheel        turn (in menus: up and down)
 *   Centre       fire (in menus: select; on a yes/no prompt: yes)
 *   Return       menu (Escape); hold to leave Doom
 *   Play/Pause   use (doors, switches)
 *   Previous     automap
 *   Next         next weapon
 *   touchscreen  a floating stick: drag up/down to walk, left/right to strafe, further to run;
 *                a tap is use (in menus: select)
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "d_event.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_controls.h"
#include "q2doom.h"

#define VIEW_W 375 /* the UI's landscape view */
#define VIEW_H 320
#define FB_W 320 /* the portrait panel: the view turned */
#define FB_H 375
#define SRC_W DOOMGENERIC_RESX
#define SRC_H DOOMGENERIC_RESY

#define INPUT_SOCK "/tmp/q2-doom.input.sock"
#define APP_DIR "/mnt/mmc/.doom"
#define Q2DOOM_VERSION "0.3"
#define LOG_FILE "doom.log"
#define FULLSCREEN_FILE "fullscreen" /* present: fill the width, the glass's corners cut the HUD */
#define ARGS_FILE "args.txt"         /* extra command-line arguments, one a line (-file mods.wad) */

/* AWTK key codes as Q2 Pod sends them */
#define Q2K_RETURN 170
#define Q2K_PLAY 171
#define Q2K_CENTRE 218
#define Q2K_PREV 222
#define Q2K_NEXT 223

#define WHEEL_TICK 28        /* raw units a menu step, Q2 Pod's default wheel tick */
#define WHEEL_TURN 16        /* mouse units a raw unit: a wheel turn is about 140 degrees */
#define STICK_DEAD 10        /* px a drag must go before it moves you */
#define STICK_RUN 44         /* px past which you run */
#define STICK_MAX 64         /* px the origin trails behind, so reversing is quick */
#define TAP_MS 250           /* a touch shorter than this, that never moved, is a tap */
#define HOLD_MS 60           /* a tap's key is held this long, so a tic sees it */
#define IDLE_QUIT_MS 300000  /* 5 minutes untouched: Doom ends, as Q2 Pod's idle quit would */
#define ERROR_SHOW_MS 12000

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606

/* ---------------------------------------------------------------------------------------------
 * Clock */

uint32_t DG_GetTicksMs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)t.tv_sec * 1000u + (uint32_t)(t.tv_nsec / 1000000);
}

void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }

void DG_SetWindowTitle(const char *title) { (void)title; }

/* ---------------------------------------------------------------------------------------------
 * Screen */

static struct {
    int fd;
    unsigned var[40], fix[17]; /* fb_var_screeninfo, fb_fix_screeninfo */
    unsigned char *mem;
    unsigned line;     /* bytes a panel row */
    unsigned y0, back; /* demo's page when we started, the page we draw next */
    int open;
} fb = { .fd = -1 };

/* Where Doom's 320 x 200 lands in the view: a 4:3 picture (each source pixel 1 wide and 1.2
 * high, as Doom was drawn for), 320 x 240, centred, which keeps every pixel clear of the glass's
 * 80 px rounded corners. FULLSCREEN_FILE fills the width instead, 375 x 281. */
static struct {
    int x0, y0, w, h;
    uint16_t col[VIEW_W]; /* view column -> source column */
    uint16_t row[VIEW_H]; /* view row -> source row */
} map;

void q2_layout(int fullscreen) {
    map.w = fullscreen ? VIEW_W : SRC_W;
    map.h = map.w * 3 / 4;
    map.x0 = (VIEW_W - map.w) / 2;
    map.y0 = (VIEW_H - map.h) / 2;
    for (int x = 0; x < map.w; ++x) map.col[x] = (uint16_t)(x * SRC_W / map.w);
    for (int y = 0; y < map.h; ++y) map.row[y] = (uint16_t)(y * SRC_H / map.h);
}

/* The Doom frame src (XRGB, red at bit 16) onto a portrait page (line bytes a row), the view
 * turned: view (X, Y) is panel row X, column 319 - Y. Only the picture's rectangle is written;
 * the borders were cleared once. */
void q2_blit(unsigned char *page, unsigned line, const uint32_t *src) {
    for (int x = 0; x < map.w; ++x) {
        uint32_t *out = (uint32_t *)(page + (unsigned)(map.x0 + x) * line);
        const uint32_t *in = src + map.col[x];
        int c = FB_W - 1 - map.y0; /* the panel column of the picture's top row */
        for (int y = 0; y < map.h; ++y) out[c - y] = in[map.row[y] * SRC_W] | 0xff000000u;
    }
}

static int fb_open(void) {
    fb.fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb.fd < 0 || ioctl(fb.fd, FBIOGET_VSCREENINFO, fb.var) ||
        ioctl(fb.fd, FBIOGET_FSCREENINFO, fb.fix))
        return 0;
    fb.line = fb.fix[11];
    /* the Q2's panel, or nothing: xres, yres, yres_virtual, bpp, red's offset */
    if (fb.var[0] != FB_W || fb.var[1] != FB_H || fb.var[3] < 2 * FB_H || fb.var[6] != 32 ||
        fb.var[8] != 16 || fb.line < FB_W * 4 || 2 * FB_H * fb.line > fb.fix[5])
        return 0;
    fb.mem = mmap(0, fb.fix[5], PROT_READ | PROT_WRITE, MAP_SHARED, fb.fd, 0);
    if (fb.mem == MAP_FAILED) return 0;
    fb.y0 = fb.var[5];
    fb.back = fb.y0 ? 0 : FB_H;
    memset(fb.mem, 0, 2 * FB_H * fb.line); /* both pages: the borders stay black */
    fb.open = 1;
    return 1;
}

static void fb_show(const uint32_t *frame) {
    if (!fb.open) return;
    q2_blit(fb.mem + fb.back * fb.line, fb.line, frame);
    fb.var[5] = fb.back;
    ioctl(fb.fd, FBIOPAN_DISPLAY, fb.var);
    fb.back = fb.back ? 0 : FB_H;
}

static void fb_close(void) {
    if (!fb.open) return;
    if (fb.var[5] != fb.y0) fb.var[5] = fb.y0, ioctl(fb.fd, FBIOPAN_DISPLAY, fb.var);
    fb.open = 0; /* demo repaints the whole screen once we have gone */
}

void DG_Init(void) {}

void DG_DrawFrame(void) { fb_show(DG_ScreenBuffer); }

/* ---------------------------------------------------------------------------------------------
 * Messages: a 5 x 7 font for the screens shown before Doom draws (no WAD) or after it stops
 * (I_Error). Capitals, digits and a little punctuation; lower case is drawn as capitals. */

static const char glyph_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:;!?'-/()_\"=+%";
static const unsigned char glyphs[][7] = {
    { 14, 17, 17, 31, 17, 17, 17 }, { 30, 17, 17, 30, 17, 17, 30 }, { 14, 17, 16, 16, 16, 17, 14 },
    { 30, 17, 17, 17, 17, 17, 30 }, { 31, 16, 16, 30, 16, 16, 31 }, { 31, 16, 16, 30, 16, 16, 16 },
    { 14, 17, 16, 23, 17, 17, 15 }, { 17, 17, 17, 31, 17, 17, 17 }, { 14, 4, 4, 4, 4, 4, 14 },
    { 7, 2, 2, 2, 2, 18, 12 },      { 17, 18, 20, 24, 20, 18, 17 }, { 16, 16, 16, 16, 16, 16, 31 },
    { 17, 27, 21, 21, 17, 17, 17 }, { 17, 17, 25, 21, 19, 17, 17 }, { 14, 17, 17, 17, 17, 17, 14 },
    { 30, 17, 17, 30, 16, 16, 16 }, { 14, 17, 17, 17, 21, 18, 13 }, { 30, 17, 17, 30, 20, 18, 17 },
    { 15, 16, 16, 14, 1, 1, 30 },   { 31, 4, 4, 4, 4, 4, 4 },       { 17, 17, 17, 17, 17, 17, 14 },
    { 17, 17, 17, 17, 17, 10, 4 },  { 17, 17, 17, 21, 21, 21, 10 }, { 17, 17, 10, 4, 10, 17, 17 },
    { 17, 17, 10, 4, 4, 4, 4 },     { 31, 1, 2, 4, 8, 16, 31 },     { 14, 17, 19, 21, 25, 17, 14 },
    { 4, 12, 4, 4, 4, 4, 14 },      { 14, 17, 1, 2, 4, 8, 31 },     { 31, 2, 4, 2, 1, 17, 14 },
    { 2, 6, 10, 18, 31, 2, 2 },     { 31, 16, 30, 1, 1, 17, 14 },   { 6, 8, 16, 30, 17, 17, 14 },
    { 31, 1, 2, 4, 8, 8, 8 },       { 14, 17, 17, 14, 17, 17, 14 }, { 14, 17, 17, 15, 1, 2, 12 },
    { 0, 0, 0, 0, 0, 12, 12 },      { 0, 0, 0, 0, 12, 4, 8 },       { 0, 12, 12, 0, 12, 12, 0 },
    { 0, 12, 12, 0, 12, 4, 8 },     { 4, 4, 4, 4, 4, 0, 4 },        { 14, 17, 1, 2, 4, 0, 4 },
    { 4, 4, 8, 0, 0, 0, 0 },        { 0, 0, 0, 31, 0, 0, 0 },       { 1, 1, 2, 4, 8, 16, 16 },
    { 2, 4, 8, 8, 8, 4, 2 },        { 8, 4, 2, 2, 2, 4, 8 },        { 0, 0, 0, 0, 0, 0, 31 },
    { 10, 10, 0, 0, 0, 0, 0 },      { 0, 0, 31, 0, 31, 0, 0 },      { 0, 4, 4, 31, 4, 4, 0 },
    { 24, 25, 2, 4, 8, 19, 3 },
};

static void draw_char(uint32_t *dst, int x, int y, char ch, uint32_t colour) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    const char *at = strchr(glyph_chars, ch);
    if (!at || !ch) return;
    const unsigned char *g = glyphs[at - glyph_chars];
    for (int r = 0; r < 7; ++r)
        for (int c = 0; c < 5; ++c)
            if (g[r] >> (4 - c) & 1 && x + c >= 0 && x + c < SRC_W && y + r >= 0 && y + r < SRC_H)
                dst[(y + r) * SRC_W + x + c] = colour;
}

/* title in red, then body word-wrapped in grey, onto a black Doom-sized frame */
void q2_message(uint32_t *dst, const char *title, const char *body) {
    enum { COLS = (SRC_W - 16) / 6, LINE = 10 };
    memset(dst, 0, SRC_W * SRC_H * 4);
    int y = 24;
    for (int i = 0; title[i] && i < COLS; ++i) draw_char(dst, 8 + 6 * i, y, title[i], 0xffcc2222u);
    y += 2 * LINE;
    const char *p = body;
    while (*p && y < SRC_H - LINE) {
        while (*p == ' ') ++p;
        int n = 0, cut = -1;
        while (p[n] && p[n] != '\n' && n < COLS) {
            if (p[n] == ' ') cut = n;
            ++n;
        }
        if (p[n] && p[n] != '\n' && cut > 0) n = cut; /* break at the last space that fits */
        for (int i = 0; i < n; ++i) draw_char(dst, 8 + 6 * i, y, p[i], 0xffc0c0c0u);
        p += n;
        if (*p == '\n') ++p;
        y += LINE;
    }
}

/* ---------------------------------------------------------------------------------------------
 * Input */

static int sock = -1;
static uint32_t last_input;
static volatile sig_atomic_t quit_asked;

static struct pending {
    unsigned char key, down;
    uint32_t due;
} keyq[64];
static unsigned kq_head, kq_tail;
static uint32_t kq_last_due; /* a tap's release: the next tap queues behind it */

static void push_key(int key, int down, uint32_t due) {
    unsigned next = (kq_head + 1) % (sizeof keyq / sizeof *keyq);
    if (next == kq_tail || !key) return;
    keyq[kq_head] = (struct pending){ (unsigned char)key, (unsigned char)down, due };
    kq_head = next;
    if (due > kq_last_due) kq_last_due = due;
}

/* down now (or after the last tap's release), up hold ms later */
static void tap_key(int key, uint32_t hold) {
    uint32_t now = DG_GetTicksMs(), at = kq_last_due > now ? kq_last_due : now;
    push_key(key, 1, at);
    push_key(key, 0, at + hold);
}

/* The wheel and Centre work the menus whenever Doom isn't playing a level */
int q2_menu_mode(void) {
    return menuactive || demoplayback || gamestate == GS_DEMOSCREEN;
}

static struct {
    int travel;        /* wheel units not yet turned into menu steps */
    int turn;          /* wheel units waiting for the next tic, as mouse motion */
    int sent[256];     /* Doom key sent for each Q2 key's down, for its up */
    uint32_t sent_at[256];
    int touching, moved, ox, oy, stick_fwd, stick_side, stick_run;
    uint32_t touch_at;
} in;

/* The touch stick from the finger's offset dx, dy (view px, y down) from its origin */
void q2_stick(int dx, int dy, int *fwd, int *side, int *run) {
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, far = ax > ay ? ax : ay;
    *fwd = ay > STICK_DEAD && ay * 2 > ax ? (dy < 0 ? -1 : 1) : 0; /* -1 forward, Doom's sign */
    *side = ax > STICK_DEAD && ax * 2 > ay ? (dx > 0 ? 1 : -1) : 0;
    *run = far > STICK_RUN;
}

static void post_stick(void) {
    event_t e = { ev_joystick, 0, 0, 0, 0 };
    if (in.stick_run && joybspeed < 20) e.data1 = 1 << joybspeed;
    e.data3 = in.stick_fwd;
    e.data4 = in.stick_side;
    D_PostEvent(&e);
}

static void touch(char what, int x, int y) {
    uint32_t now = DG_GetTicksMs();
    if (what == 'd') {
        in.touching = 1, in.moved = 0, in.ox = x, in.oy = y, in.touch_at = now;
        return;
    }
    if (!in.touching) return;
    int dx = x - in.ox, dy = y - in.oy;
    if (what == 'm') {
        if (dx * dx + dy * dy > STICK_DEAD * STICK_DEAD) in.moved = 1;
        /* past STICK_MAX the origin follows the finger, so a reversal acts at once */
        if (dx > STICK_MAX) in.ox = x - STICK_MAX, dx = STICK_MAX;
        if (dx < -STICK_MAX) in.ox = x + STICK_MAX, dx = -STICK_MAX;
        if (dy > STICK_MAX) in.oy = y - STICK_MAX, dy = STICK_MAX;
        if (dy < -STICK_MAX) in.oy = y + STICK_MAX, dy = -STICK_MAX;
        int f, s, r;
        q2_stick(dx, dy, &f, &s, &r);
        if (q2_menu_mode()) f = s = r = 0;
        if (f != in.stick_fwd || s != in.stick_side || r != in.stick_run)
            in.stick_fwd = f, in.stick_side = s, in.stick_run = r, post_stick();
        return;
    }
    /* up */
    in.touching = 0;
    if (in.stick_fwd || in.stick_side || in.stick_run)
        in.stick_fwd = in.stick_side = in.stick_run = 0, post_stick();
    if (!in.moved && now - in.touch_at < TAP_MS) tap_key(q2_menu_mode() ? KEY_ENTER : key_use, HOLD_MS);
}

/* The Doom key for Q2 key code at this moment */
int q2_doom_key(int code, int menu) {
    switch (code) {
    case Q2K_CENTRE: return menu ? KEY_ENTER : key_fire;
    case Q2K_RETURN: return KEY_ESCAPE;
    case Q2K_PLAY: return menu ? KEY_ENTER : key_use;
    case Q2K_PREV: return menu ? KEY_ESCAPE : key_map_toggle;
    case Q2K_NEXT: return menu ? KEY_ENTER : key_nextweapon;
    default: return 0;
    }
}

static void key(char what, int code) {
    if (code < 0 || code > 255) return;
    if (what == 'd') {
        if (in.sent[code]) return; /* a repeat */
        int k = q2_doom_key(code, q2_menu_mode());
        if (k) push_key(k, 1, 0), in.sent[code] = k, in.sent_at[code] = DG_GetTicksMs();
    } else if (in.sent[code]) {
        /* held at least HOLD_MS, so even a quick press reaches a tic's ticcmd */
        uint32_t due = in.sent_at[code] + HOLD_MS;
        push_key(in.sent[code], 0, kq_last_due > due ? kq_last_due : due);
        in.sent[code] = 0;
    }
}

static void wheel(int travel) {
    if (!q2_menu_mode() && gamestate == GS_LEVEL) {
        in.travel = 0;
        in.turn += travel;
        return;
    }
    in.turn = 0;
    if ((travel > 0) != (in.travel > 0)) in.travel = 0; /* a reversal steps at once */
    in.travel += travel;
    while (in.travel >= WHEEL_TICK) in.travel -= WHEEL_TICK, push_key(KEY_UPARROW, 1, 0), push_key(KEY_UPARROW, 0, 0);
    while (in.travel <= -WHEEL_TICK) in.travel += WHEEL_TICK, push_key(KEY_DOWNARROW, 1, 0), push_key(KEY_DOWNARROW, 0, 0);
}

/* one datagram from Q2 Pod */
void q2_input(const char *m) {
    char b = 0;
    int x = 0, y = 0;
    last_input = DG_GetTicksMs();
    if (sscanf(m, "k %c %d", &b, &x) == 2) key(b, x);
    else if (sscanf(m, "t %c %d %d", &b, &x, &y) == 3) touch(b, x, y);
    else if (sscanf(m, "w %d", &x) == 1) wheel(x);
}

static void pump(void) {
    char m[64];
    ssize_t n;
    while (sock >= 0 && (n = recv(sock, m, sizeof m - 1, MSG_DONTWAIT)) > 0) {
        m[n] = 0;
        q2_input(m);
    }
    if (!key_nextweapon) key_nextweapon = ']'; /* unbound in Doom's defaults */
}

/* The wheel's turn as one mouse event a tic: G_Responder sets, not adds, the motion, so a second
 * event in the same tic would replace the first. In a menu it would move the sliders: dropped. */
static void flush_turn(void) {
    if (!in.turn) return;
    if (q2_menu_mode() || gamestate != GS_LEVEL) {
        in.turn = 0;
        return;
    }
    event_t e = { ev_mouse, 0, -in.turn * WHEEL_TURN, 0, 0 }; /* anticlockwise turns left */
    in.turn = 0;
    D_PostEvent(&e);
}

/* I_GetEvent calls this until it answers 0, once each tic (it also stops after a release) */
int DG_GetKey(int *pressed, unsigned char *doomKey) {
    pump();
    struct pending *p = &keyq[kq_tail];
    if (kq_head == kq_tail || (p->due && (int32_t)(DG_GetTicksMs() - p->due) < 0)) {
        flush_turn(); /* the end of this tic's events */
        return 0;
    }
    *pressed = p->down;
    *doomKey = p->key;
    kq_tail = (kq_tail + 1) % (sizeof keyq / sizeof *keyq);
    return 1;
}

static int input_open(void) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, INPUT_SOCK);
    sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    unlink(INPUT_SOCK);
    return sock >= 0 && !bind(sock, &addr, sizeof addr);
}

/* ---------------------------------------------------------------------------------------------
 * Start and end */

static void on_term(int sig) {
    (void)sig;
    quit_asked = 1;
}

static void cleanup(void) {
    fb_close();
    if (sock >= 0) close(sock), sock = -1, unlink(INPUT_SOCK);
}

/* text on screen until a key or ms */
static void show_message(const char *title, const char *body, uint32_t ms) {
    static uint32_t frame[SRC_W * SRC_H];
    q2_message(frame, title, body);
    fb_show(frame);
    fb_show(frame); /* both pages */
    char m[64];
    uint32_t until = DG_GetTicksMs() + ms;
    while (!quit_asked && (int32_t)(DG_GetTicksMs() - until) < 0) {
        ssize_t n = sock >= 0 ? recv(sock, m, sizeof m - 1, MSG_DONTWAIT) : -1;
        if (n > 0 && m[0] == 'k' && m[2] == 'd') break;
        usleep(20000);
    }
}

void DG_ShowError(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    show_message("DOOM STOPPED", msg, ERROR_SHOW_MS);
    cleanup();
}

static int exists(const char *path) { return access(path, R_OK) == 0; }

/* The name in folder dir that matches name in any case (DOOM1.WAD, Doom1.wad), into out. The
 * card is exFAT or FAT, which usually ignore case, but the engine's own lookups and -iwad are
 * then given the name as it is on the card. */
static int find_any_case(const char *dir, const char *name, char *out, unsigned size) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    while (d && !found && (e = readdir(d)))
        if (!strcasecmp(e->d_name, name)) {
            snprintf(out, size, "%s/%s", dir, e->d_name);
            found = exists(out);
        }
    if (d) closedir(d);
    return found;
}

/* The IWAD: in the app folder, else in /mnt/mmc/Doom (a visible folder), any case. Its path
 * goes to -iwad. 0: none anywhere. */
static const char *find_iwad(void) {
    static const char *const names[] = { "doom2.wad", "plutonia.wad", "tnt.wad", "doom.wad",
                                         "doom1.wad", "freedoom2.wad", "freedoom1.wad",
                                         "freedm.wad", "chex.wad" };
    static const char *const dirs[] = { ".", "/mnt/mmc/Doom", "/mnt/mmc/doom" };
    static char path[160];
    for (unsigned k = 0; k < sizeof dirs / sizeof *dirs; ++k)
        for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
            if (find_any_case(dirs[k], names[i], path, sizeof path)) return path;
    return 0;
}

/* argv for Doom: ours, then ARGS_FILE's words, and -iwad with the WAD found unless ARGS_FILE
 * names one */
static char **doom_args(int *argc, const char *iwad) {
    static char *v[64];
    static char text[1024];
    int n = 0, named = 0;
    v[n++] = "doom";
    FILE *f = fopen(ARGS_FILE, "r");
    if (f) {
        size_t len = fread(text, 1, sizeof text - 1, f);
        fclose(f);
        text[len] = 0;
        for (char *p = text; *p && n < 61;) {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') *p++ = 0;
            if (!*p) break;
            if (*p == '#') { /* a comment line */
                while (*p && *p != '\n') *p++ = 0;
                continue;
            }
            v[n++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
            if (*p) *p++ = 0;
            named |= !strcasecmp(v[n - 1], "-iwad");
        }
    }
    if (!named) v[n++] = "-iwad", v[n++] = (char *)iwad;
    v[n] = 0;
    *argc = n;
    return v;
}

int main(int argc, char **argv) {
    q2_sound_args(argc > 1 ? argv[1] : 0, argc > 2 ? argv[2] : 0);
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGHUP, on_term);
    signal(SIGPIPE, SIG_IGN);

    /* the app's folder: the WAD, the config, the saves and the log */
    char dir[256];
    snprintf(dir, sizeof dir, "%s", argv[0]);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = 0;
    else snprintf(dir, sizeof dir, "%s", APP_DIR);
    chdir(dir);
    if (freopen(LOG_FILE, "w", stdout)) setvbuf(stdout, 0, _IONBF, 0);
    if (freopen(LOG_FILE, "a", stderr)) setvbuf(stderr, 0, _IONBF, 0);
    printf("q2-doom %s: started in %s, sound %s%s%s\n", Q2DOOM_VERSION, dir,
           argc > 1 ? argv[1] : "off", argc > 2 ? ", volume " : "", argc > 2 ? argv[2] : "");

    if (!fb_open()) {
        fprintf(stderr, "No Q2 framebuffer: fd %d, %u x %u, virtual %u, %u bpp, red at %u, line %u, "
                        "%u bytes (errno %d)\n", fb.fd, fb.var[0], fb.var[1], fb.var[3], fb.var[6],
                fb.var[8], fb.fix[11], fb.fix[5], errno);
        return 1;
    }
    printf("Screen: page %u shown, drawing at %s\n", fb.y0, fb.back ? "page 1" : "page 0");
    I_AtExit(cleanup, true); /* Quit Game, I_Error and a TERM all end through I_Quit or I_Error */
    input_open();
    q2_layout(exists(FULLSCREEN_FILE));
    last_input = DG_GetTicksMs();

    const char *iwad = find_iwad();
    printf("WAD: %s\n", iwad ? iwad : "none");
    if (!iwad) {
        show_message("NO DOOM WAD FOUND",
                     "Copy a Doom game file onto the card, into the .doom folder (or a Doom folder "
                     "at the card's root):\n\n"
                     "DOOM1.WAD - the free shareware episode\n"
                     "DOOM.WAD or DOOM2.WAD - from your copy of Doom\n"
                     "FREEDOOM1.WAD or FREEDOOM2.WAD - free, from freedoom.github.io\n\n"
                     "Press any key to go back.",
                     60000);
        cleanup();
        return 0;
    }

    int dargc;
    char **dargv = doom_args(&dargc, iwad);
    doomgeneric_Create(dargc, dargv);
    for (;;) {
        if (quit_asked) I_Quit(); /* saves the config, then exits */
        if (DG_GetTicksMs() - last_input > IDLE_QUIT_MS) {
            printf("No input for %d minutes: quitting\n", IDLE_QUIT_MS / 60000);
            I_Quit();
        }
        doomgeneric_Tick();
    }
}
