#include "screens.h"
#include "ui.h"

/* 3-page virtual keyboard with a movable insert cursor, an optional
 * UDP listener, and hold-to-repeat for most buttons.
 *
 * Controls:
 *   D-Pad     navigate (repeatable)
 *   Cross     select char / action (repeatable)
 *   Square    backspace (repeatable)
 *   Triangle  space (repeatable)
 *   L1        cursor left (repeatable)
 *   R1        cursor right (repeatable)
 *   Circle    cancel / back (single press only)
 *   R2        OK (single press only)
 *   Options   clear all (single press only)
 *
 * Hold any repeatable button for 300 ms and it will start spamming
 * every ~60 ms until released.
 *
 * UDP listener: binds port 9031 while the keyboard is open.
 */

#define VKB_MAXLEN 256
#define VKB_PAGES  3
#define VKB_UDP_PORT 9031

#define KB_COLS 10
#define KB_X    60
#define KB_Y    400
#define KB_W    176
#define KB_H    88
#define KB_GAP  8

#define ACT_COUNT 7

/* Auto-repeat tuning */
#define REP_DELAY_MS     300
#define REP_INTERVAL_MS   60

static char  g_buf[VKB_MAXLEN + 1];
static int   g_len    = 0;
static int   g_cursor = 0;
static int   g_maxlen = VKB_MAXLEN;
static int   g_row    = 1;
static int   g_col    = 0;
static int   g_shift  = 0;
static int   g_page   = 0;
static int   g_done   = 0;
static const char *g_title = 0;

static s32   g_udp_fd = -1;
static int   g_udp_ok = 0;

/* ---------- auto-repeat state ---------- */
struct rep_key {
    u32 bit;
    u64 press_ms;
    u64 fire_ms;
    int active;
};

static struct rep_key g_rep[] = {
    { DS_UP,       0, 0, 0 },
    { DS_DOWN,     0, 0, 0 },
    { DS_LEFT,     0, 0, 0 },
    { DS_RIGHT,    0, 0, 0 },
    { DS_CROSS,    0, 0, 0 },
    { DS_SQUARE,   0, 0, 0 },
    { DS_TRIANGLE, 0, 0, 0 },
    { DS_L1,       0, 0, 0 },
    { DS_R1,       0, 0, 0 },
};
#define REP_COUNT ((int)(sizeof(g_rep) / sizeof(g_rep[0])))

static void rep_reset(void) {
    for (int i = 0; i < REP_COUNT; i++) {
        g_rep[i].press_ms = 0;
        g_rep[i].fire_ms  = 0;
        g_rep[i].active   = 0;
    }
}

/* Merge real rising-edge presses with synthesized repeat presses. */
static u32 rep_apply(struct ctx *c, u32 raw, u32 pressed) {
    u64 now = get_uptime_ms(c);
    u32 eff = pressed;

    for (int i = 0; i < REP_COUNT; i++) {
        u32 bit = g_rep[i].bit;
        int held = (raw & bit) != 0;

        if (!held) {
            g_rep[i].active = 0;
            continue;
        }
        if (pressed & bit) {
            g_rep[i].press_ms = now;
            g_rep[i].fire_ms  = now;
            g_rep[i].active   = 1;
            continue;
        }
        if (g_rep[i].active
            && (now - g_rep[i].press_ms) >= REP_DELAY_MS
            && (now - g_rep[i].fire_ms)  >= REP_INTERVAL_MS) {
            g_rep[i].fire_ms = now;
            eff |= bit;
        }
    }
    return eff;
}

/* ---------- page layouts ---------- */
static const char p0_low[4][10] = {
    {'1','2','3','4','5','6','7','8','9','0'},
    {'q','w','e','r','t','y','u','i','o','p'},
    {'a','s','d','f','g','h','j','k','l','-'},
    {'z','x','c','v','b','n','m','.',',','='},
};
static const char p0_up[4][10] = {
    {'!','@','#','$','%','^','&','*','(',')'},
    {'Q','W','E','R','T','Y','U','I','O','P'},
    {'A','S','D','F','G','H','J','K','L','_'},
    {'Z','X','C','V','B','N','M','<','>','+'},
};
static const char p1_syms[4][10] = {
    {'1','2','3','4','5','6','7','8','9','0'},
    {'-','/',':',';','(',')','$','&','@','"'},
    {'.',',','?','!','\'','_','+','=','<','>'},
    {'[',']','{','}','#','%','^','*','\\','|'},
};
static const char p2_syms[4][10] = {
    {'~','`','!','@','#','$','%','^','&','*'},
    {'(','[',']','{','}','<','>','|','/',')'},
    {'-','_','=','+','.',';',':','?','!','\''},
    {'\\','"','0','1','2','3','4','5','6','7'},
};

static char page_char(int page, int shift, int row, int col) {
    if (row < 0 || row >= 4 || col < 0 || col >= KB_COLS) return '?';
    if (page == 0) return shift ? p0_up[row][col] : p0_low[row][col];
    if (page == 1) return p1_syms[row][col];
    return p2_syms[row][col];
}

static const char *mode_label(int page) {
    if (page == 0) return "123";
    if (page == 1) return "EXT";
    return "ABC";
}

static const int act_start[ACT_COUNT] = {0, 2, 4, 5, 6, 7, 8};
static const int act_w    [ACT_COUNT] = {2, 2, 1, 1, 1, 1, 2};

static const char *action_label(int idx, int page) {
    switch (idx) {
        case 0: return "SHIFT";
        case 1: return "SPACE";
        case 2: return "DEL";
        case 3: return mode_label(page);
        case 4: return "CLEAR";
        case 5: return "CANCEL";
        case 6: return "OK";
    }
    return "?";
}

/* ---------- UDP ---------- */
static s32 udp_open(struct ctx *c) {
    if (!c->socket_fn || !c->bind_fn || !c->close_fn) return -1;
    s32 s = (s32)NC(c->G, c->socket_fn, 2, 2, 0, 0, 0, 0);
    if (s < 0) return -1;
    if (c->setsockopt_fn) {
        u32 one = 1;
        NC(c->G, c->setsockopt_fn, (u64)s, 0xFFFF, 0x0004, (u64)&one, 4, 0);
    }
    u8 sa[16]; m_set(sa, 0, 16);
    sa[0] = 16; sa[1] = 2;
    sa[2] = (u8)((VKB_UDP_PORT >> 8) & 0xFF);
    sa[3] = (u8)(VKB_UDP_PORT & 0xFF);
    if ((s32)NC(c->G, c->bind_fn, (u64)s, (u64)sa, 16, 0, 0, 0) != 0) {
        NC(c->G, c->close_fn, (u64)s, 0, 0, 0, 0, 0);
        return -1;
    }
    return s;
}

static void udp_close(struct ctx *c) {
    if (g_udp_fd >= 0 && c->close_fn)
        NC(c->G, c->close_fn, (u64)g_udp_fd, 0, 0, 0, 0, 0);
    g_udp_fd = -1;
    g_udp_ok = 0;
}

static int udp_poll(struct ctx *c) {
    if (g_udp_fd < 0 || !c->poll_fn || !c->recvfrom_fn) return 0;
    u8 pfd[8];
    *(s32*)(pfd + 0) = g_udp_fd;
    *(u16*)(pfd + 4) = 0x0001;
    *(u16*)(pfd + 6) = 0;
    s32 pr = (s32)NC(c->G, c->poll_fn, (u64)pfd, 1, 0, 0, 0, 0);
    if (pr <= 0) return 0;
    char buf[256];
    s32 n = (s32)NC(c->G, c->recvfrom_fn,
                    (u64)g_udp_fd, (u64)buf, 255, 0, 0, 0);
    if (n <= 0) return 0;
    buf[n] = 0;
    int injected = 0;
    for (int i = 0; i < n; i++) {
        char ch = buf[i];
        if (ch == '\r' || ch == '\n') continue;
        if (g_len >= g_maxlen) break;
        for (int j = g_len; j > g_cursor; j--) g_buf[j] = g_buf[j - 1];
        g_buf[g_cursor] = ch;
        g_cursor++;
        g_len++;
        injected++;
    }
    g_buf[g_len] = 0;
    return injected;
}

/* ---------- drawing ---------- */
static void draw_text_field(u32 *fb, u64 now) {
    int ty = 130;
    ui_fill(fb, 60, ty, SCR_W - 120, 100, RGB(20, 30, 50));
    ui_frame(fb, 60, ty, SCR_W - 120, 100, RGB(70, 100, 150), 3);

    const int VISIBLE = 40;
    int view_start = 0;
    if (g_cursor > VISIBLE - 5) view_start = g_cursor - (VISIBLE - 5);
    if (view_start < 0) view_start = 0;

    int x = 90;
    int blink = ((now / 400) % 2 == 0);
    for (int i = view_start; i < g_len; i++) {
        if (i == g_cursor && blink)
            ui_fill(fb, x - 2, ty + 26, 4, 56, RGB(0, 180, 255));
        char s[2] = { g_buf[i], 0 };
        ui_str(fb, x, ty + 30, s, RGB(255, 255, 255), 4);
        x += 8 * 4;
    }
    if (g_cursor == g_len && blink)
        ui_fill(fb, x - 2, ty + 26, 4, 56, RGB(0, 180, 255));

    char cnt[20]; int p = 0;
    p += s_itoa(cnt + p, g_len);
    cnt[p++] = '/';
    p += s_itoa(cnt + p, g_maxlen);
    cnt[p] = 0;
    ui_str_right(fb, SCR_W - 90, ty + 40, cnt, RGB(120, 120, 140), 3);
}

static void draw_vkb(struct ctx *c, u32 *fb) {
    u64 now = get_uptime_ms(c);
    ui_clear(fb, RGB(8, 8, 14));

    ui_fill(fb, 0, 0, SCR_W, 90, RGB(20,20,28));
    ui_fill(fb, 0, 90, SCR_W, 3, RGB(0,180,255));
    ui_str(fb, 60, 22, "KEYBOARD", RGB(0,180,255), 5);
    if (g_title) ui_str_right(fb, SCR_W - 60, 30, g_title,
                              RGB(120,120,140), 4);

    draw_text_field(fb, now);

    {
        char s[96]; int p = 0;
        const char *pre = "Page ";
        while (*pre && p < 40) s[p++] = *pre++;
        s[p++] = '1' + g_page;
        s[p++] = '/';
        s[p++] = '1' + (VKB_PAGES - 1);
        const char *cur = "   Cursor:";
        while (*cur && p < 60) s[p++] = *cur++;
        p += s_itoa(s + p, g_cursor);
        if (g_udp_ok) {
            const char *udp = "   UDP :9031 listening";
            while (*udp && p < 88) s[p++] = *udp++;
        }
        s[p] = 0;
        ui_str(fb, 60, 250, s,
               g_udp_ok ? RGB(0,220,120) : RGB(120,120,140), 3);
    }

    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < KB_COLS; col++) {
            int x = KB_X + col * (KB_W + KB_GAP);
            int y = KB_Y + r * (KB_H + KB_GAP);
            int sel = (g_row == r && g_col == col);
            char ch = page_char(g_page, g_shift, r, col);

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

    for (int i = 0; i < ACT_COUNT; i++) {
        int x = KB_X + act_start[i] * (KB_W + KB_GAP);
        int w = act_w[i] * KB_W + (act_w[i] - 1) * KB_GAP;
        int y = KB_Y + 4 * (KB_H + KB_GAP);
        int sel = (g_row == 4 && g_col == i);

        u32 bg = sel ? RGB(0,110,200) : RGB(35,35,48);
        ui_fill(fb, x, y, w, KB_H, bg);
        ui_frame(fb, x, y, w, KB_H,
                 sel ? RGB(255,200,80) : RGB(70,70,95), sel ? 4 : 2);

        const char *lbl = action_label(i, g_page);
        ui_str(fb, x + (w - ui_str_w(lbl, 3)) / 2,
               y + (KB_H - 8 * 3) / 2, lbl,
               sel ? 0xFFFFFFFF : RGB(230,230,240), 3);
    }

    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, RGB(20,20,28));
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, RGB(0,180,255));
    ui_str(fb, 60, SCR_H - 44,
           "D-Pad Move  X Select  [] Del  /\\ Space  L1/R1 Cursor  R2 OK  O Cancel",
           RGB(120,120,140), 3);
}

/* ---------- editing ---------- */
static void insert_char(char ch) {
    if (g_len >= g_maxlen) return;
    for (int j = g_len; j > g_cursor; j--) g_buf[j] = g_buf[j - 1];
    g_buf[g_cursor] = ch;
    g_cursor++;
    g_len++;
    g_buf[g_len] = 0;
    if (g_shift == 1 && g_page == 0) g_shift = 0;
}

static void delete_before_cursor(void) {
    if (g_cursor <= 0) return;
    for (int j = g_cursor - 1; j < g_len - 1; j++) g_buf[j] = g_buf[j + 1];
    g_cursor--;
    g_len--;
    g_buf[g_len] = 0;
}

static void do_action(int idx) {
    switch (idx) {
        case 0: if (g_page == 0) g_shift = (g_shift + 1) % 3; break;
        case 1: insert_char(' '); break;
        case 2: delete_before_cursor(); break;
        case 3: g_page = (g_page + 1) % VKB_PAGES;
                if (g_page != 0) g_shift = 0; break;
        case 4: g_len = 0; g_cursor = 0; g_buf[0] = 0; break;
        case 5: g_done = -1; break;
        case 6: g_done = 1;  break;
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
    g_cursor = g_len;
    g_maxlen = max_len;
    g_row = 1; g_col = 0;
    g_shift = 0;
    g_page = 0;
    g_done = 0;
    g_title = title;

    rep_reset();

    g_udp_fd = udp_open(c);
    g_udp_ok = (g_udp_fd >= 0);
    if (g_udp_ok) ulog(c, "[toolbox] vkb: UDP listening on :9031\n");
    else          ulog(c, "[toolbox] vkb: UDP bind failed\n");

    u32 saved_prev = c->pad_prev;
    c->pad_prev = pad_raw(c);

    while (!g_done) {
        udp_poll(c);

        u32 raw     = pad_raw(c);
        u32 pressed = raw & ~c->pad_prev;
        c->pad_prev = raw;

        /* Merge real edges with auto-repeat synthesized presses */
        u32 eff = rep_apply(c, raw, pressed);

        /* ---- Movement ---- */
        if (eff & DS_UP) {
            if (g_row > 0) {
                g_row--;
                if (g_row == 4 && g_col >= ACT_COUNT) g_col = ACT_COUNT - 1;
            }
        }
        if (eff & DS_DOWN) {
            if (g_row < 4) {
                g_row++;
                if (g_row == 4 && g_col >= ACT_COUNT) g_col = ACT_COUNT - 1;
            }
        }
        if (eff & DS_LEFT)  { if (g_col > 0) g_col--; }
        if (eff & DS_RIGHT) {
            int maxc = (g_row == 4) ? (ACT_COUNT - 1) : (KB_COLS - 1);
            if (g_col < maxc) g_col++;
        }

        /* ---- Char / action ---- */
        if (eff & DS_CROSS) {
            if (g_row == 4) do_action(g_col);
            else insert_char(page_char(g_page, g_shift, g_row, g_col));
        }

        /* ---- Shortcuts ---- */
        if (eff & DS_SQUARE)   delete_before_cursor();
        if (eff & DS_TRIANGLE) insert_char(' ');

        if (eff & DS_L1) { if (g_cursor > 0) g_cursor--; }
        if (eff & DS_R1) { if (g_cursor < g_len) g_cursor++; }

        /* ---- Terminal actions: only on real edges ---- */
        if (pressed & DS_CIRCLE)  g_done = -1;
        if (pressed & DS_OPTIONS) { g_len = 0; g_cursor = 0; g_buf[0] = 0; }
        if (pressed & DS_R2)      g_done = 1;

        draw_vkb(c, c->fbs[c->active]);
        video_flip(c, 1);
    }

    udp_close(c);
    rep_reset();

    c->pad_prev = saved_prev;

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
