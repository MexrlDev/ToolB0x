#include "screens.h"
#include "ui.h"

/* Virtual keyboard — 10x4 char grid + 6 action buttons. */

#define VKB_MAXLEN 96

static char  g_buf[VKB_MAXLEN + 1];
static int   g_len = 0;
static int   g_maxlen = VKB_MAXLEN;
static int   g_row = 1;
static int   g_col = 0;
static int   g_shift = 0;    /* 0=off, 1=once, 2=lock */
static int   g_done = 0;     /* 0 = running, 1 = OK, -1 = cancel */
static const char *g_title = 0;

#define KB_COLS 10
#define KB_X    60
#define KB_Y    400
#define KB_W    176
#define KB_H    88
#define KB_GAP  8

static const char ch_low[4][11] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl-",
    "zxcvbnm.,=",
};
static const char ch_up[4][11] = {
    "!@#$%^&*()",
    "QWERTYUIOP",
    "ASDFGHJKL_",
    "ZXCVBNM<>+",
};

static const int act_start[6] = {0, 2, 5, 6, 7, 8};
static const int act_w    [6] = {2, 3, 1, 1, 1, 2};
static const char *act_label[6] = {
    "SHIFT", "SPACE", "BKSP", "CLEAR", "CANCEL", "OK"
};

static void draw_vkb(struct ctx *c, u32 *fb) {
    ui_clear(fb, RGB(8, 8, 14));

    ui_fill(fb, 0, 0, SCR_W, 90, RGB(20,20,28));
    ui_fill(fb, 0, 90, SCR_W, 3, RGB(0,180,255));
    ui_str(fb, 60, 22, "KEYBOARD", RGB(0,180,255), 5);
    if (g_title) ui_str_right(fb, SCR_W - 60, 30, g_title,
                              RGB(120,120,140), 4);

    /* Text field */
    {
        int ty = 130;
        ui_fill(fb, 60, ty, SCR_W - 120, 100, RGB(20, 30, 50));
        ui_frame(fb, 60, ty, SCR_W - 120, 100, RGB(70, 100, 150), 3);
        ui_str(fb, 90, ty + 30, g_buf, RGB(255,255,255), 4);

        u64 now = get_uptime_ms(c);
        if ((now / 400) % 2 == 0) {
            int cx = 90 + ui_str_w(g_buf, 4);
            ui_fill(fb, cx, ty + 26, 8, 56, RGB(0,180,255));
        }

        char cnt[16]; int p = 0;
        p += s_itoa(cnt + p, g_len);
        cnt[p++] = '/';
        p += s_itoa(cnt + p, g_maxlen);
        cnt[p] = 0;
        ui_str_right(fb, SCR_W - 90, ty + 40, cnt, RGB(120,120,140), 3);
    }

    /* Char grid */
    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < KB_COLS; col++) {
            int x = KB_X + col * (KB_W + KB_GAP);
            int y = KB_Y + r * (KB_H + KB_GAP);
            int sel = (g_row == r && g_col == col);
            char ch = (g_shift ? ch_up : ch_low)[r][col];

            u32 bg = sel ? RGB(0,110,200) : RGB(20,20,28);
            ui_fill(fb, x, y, KB_W, KB_H, bg);
            if (sel) ui_frame(fb, x, y, KB_W, KB_H, RGB(255,200,80), 4);
            else     ui_frame(fb, x, y, KB_W, KB_H, RGB(70,70,95), 2);

            char s[2] = { ch, 0 };
            ui_str(fb, x + (KB_W - 8 * 4) / 2,
                   y + (KB_H - 8 * 4) / 2, s,
                   sel ? 0xFFFFFFFF : RGB(230,230,240), 4);
        }
    }

    /* Action row */
    for (int i = 0; i < 6; i++) {
        int x = KB_X + act_start[i] * (KB_W + KB_GAP);
        int w = act_w[i] * KB_W + (act_w[i] - 1) * KB_GAP;
        int y = KB_Y + 4 * (KB_H + KB_GAP);
        int sel = (g_row == 4 && g_col == i);

        u32 bg = sel ? RGB(0,110,200) : RGB(35,35,48);
        ui_fill(fb, x, y, w, KB_H, bg);
        ui_frame(fb, x, y, w, KB_H,
                 sel ? RGB(255,200,80) : RGB(70,70,95), sel ? 4 : 2);

        ui_str(fb, x + (w - ui_str_w(act_label[i], 3)) / 2,
               y + (KB_H - 8 * 3) / 2, act_label[i],
               sel ? 0xFFFFFFFF : RGB(230,230,240), 3);
    }

    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, RGB(20,20,28));
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, RGB(0,180,255));
    ui_str(fb, 60, SCR_H - 44,
           "D-Pad Move   X Select   O Backspace   SQ Space   TRI Shift   OPT Clear",
           RGB(120,120,140), 3);
    ui_str_right(fb, SCR_W - 60, SCR_H - 44,
                 "R1 Cancel", RGB(255,200,80), 3);
}

static void add_char(char ch) {
    if (g_len >= g_maxlen) return;
    g_buf[g_len++] = ch;
    g_buf[g_len] = 0;
    if (g_shift == 1) g_shift = 0;
}

static void do_backspace(void) {
    if (g_len > 0) { g_len--; g_buf[g_len] = 0; }
}

static void do_action(int idx) {
    switch (idx) {
        case 0: /* SHIFT */
            g_shift = (g_shift + 1) % 3;
            break;
        case 1: add_char(' '); break;
        case 2: do_backspace(); break;
        case 3: g_len = 0; g_buf[0] = 0; break;
        case 4: g_done = -1; break;
        case 5: g_done = 1; break;
    }
}

int vkb_prompt(struct ctx *c, const char *title,
               const char *initial, char *out, int out_len, int max_len) {
    if (max_len > VKB_MAXLEN) max_len = VKB_MAXLEN;
    if (out_len < 1) return -1;

    g_len = 0;
    if (initial) {
        while (initial[g_len] && g_len < max_len) {
            g_buf[g_len] = initial[g_len];
            g_len++;
        }
    }
    g_buf[g_len] = 0;
    g_maxlen = max_len;
    g_row = 1; g_col = 0;
    g_shift = 0;
    g_done = 0;
    g_title = title;

    u32 saved_prev = c->pad_prev;
    c->pad_prev = pad_raw(c);

    while (!g_done) {
        u32 raw     = pad_raw(c);
        u32 pressed = raw & ~c->pad_prev;
        c->pad_prev = raw;

        if (pressed & DS_UP) {
            if (g_row > 0) {
                g_row--;
                if (g_row == 4 && g_col > 5) g_col = 5;
            }
        }
        if (pressed & DS_DOWN) {
            if (g_row < 4) {
                g_row++;
                if (g_row == 4 && g_col > 5) g_col = 5;
            }
        }
        if (pressed & DS_LEFT) {
            if (g_col > 0) g_col--;
        }
        if (pressed & DS_RIGHT) {
            int maxc = (g_row == 4) ? 5 : 9;
            if (g_col < maxc) g_col++;
        }

        if (pressed & DS_CROSS) {
            if (g_row == 4) do_action(g_col);
            else add_char((g_shift ? ch_up : ch_low)[g_row][g_col]);
        }
        if (pressed & DS_CIRCLE)   do_backspace();
        if (pressed & DS_SQUARE)   add_char(' ');
        if (pressed & DS_TRIANGLE) g_shift = (g_shift + 1) % 3;
        if (pressed & DS_OPTIONS)  { g_len = 0; g_buf[0] = 0; }
        if (pressed & DS_R1)       g_done = -1;

        draw_vkb(c, c->fbs[c->active]);
        video_flip(c, 1);
    }

    c->pad_prev = saved_prev;

    /* Wait for release to avoid phantom press in caller */
    for (int i = 0; i < 30; i++) {
        if (pad_raw(c) == 0) break;
        if (c->usleep) NC(c->G, c->usleep, 33000, 0,0,0,0,0);
    }
    c->pad_prev = pad_raw(c);

    if (g_done != 1) return -1;

    int n = 0;
    while (g_buf[n] && n < out_len - 1) {
        out[n] = g_buf[n];
        n++;
    }
    out[n] = 0;
    return 0;
}
