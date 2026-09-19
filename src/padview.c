#include "screens.h"
#include "ui.h"

#define COL_BG       RGB(8,8,14)
#define COL_PANEL    RGB(20,20,28)
#define COL_PANEL_HI RGB(30,30,42)
#define COL_BORDER   RGB(70,70,95)
#define COL_TEXT     RGB(230,230,240)
#define COL_TEXT_DIM RGB(120,120,140)
#define COL_ACCENT   RGB(0,180,255)
#define COL_TITLE    RGB(255,200,80)
#define COL_HDR      RGB(255,140,0)
#define COL_ON_BG    RGB(20,110,40)
#define COL_ON_BD    RGB(80,255,120)
#define COL_FLASH_BG RGB(110, 80, 20)
#define COL_FLASH_BD RGB(255,200, 60)

/* Draw a "button chip" showing live state. */
static void draw_chip(u32 *fb, int x, int y, int w, int h,
                      const char *name, u32 bit, u32 raw,
                      struct debug_state *dbg, u64 now) {
    int pressed  = (raw & bit) != 0;
    int flashing = 0;
    for (int i = 0; i < 16; i++) {
        if (dbg->flash[i].bit == bit && dbg->flash[i].until_ms > now) {
            flashing = 1; break;
        }
    }

    u32 bg, bd, fg;
    if (pressed) {
        bg = COL_ON_BG;    bd = COL_ON_BD;    fg = 0xFFFFFFFF;
    } else if (flashing) {
        bg = COL_FLASH_BG; bd = COL_FLASH_BD; fg = 0xFFFFF0A0;
    } else {
        bg = COL_PANEL;    bd = COL_BORDER;   fg = COL_TEXT_DIM;
    }

    ui_fill(fb, x, y, w, h, bg);
    ui_frame(fb, x, y, w, h, bd, 3);

    int dot_x = x + 20, dot_y = y + h / 2;
    u32 dot_col = pressed ? RGB(0,255,80)
                : (flashing ? RGB(255,200,60) : RGB(60,60,80));
    for (int dy = -8; dy <= 8; dy++) {
        for (int dx = -8; dx <= 8; dx++) {
            if (dx * dx + dy * dy > 64) continue;
            int px = dot_x + dx, py = dot_y + dy;
            if (px < 0 || px >= SCR_W || py < 0 || py >= SCR_H) continue;
            fb[py * SCR_W + px] = dot_col;
        }
    }

    ui_str(fb, x + 40, y + 10, name, fg, 3);
}

static const char *bit_name(u32 mask) {
    switch (mask) {
        case DS_SHARE:    return "Share";
        case DS_L3:       return "L3";
        case DS_R3:       return "R3";
        case DS_OPTIONS:  return "Options";
        case DS_UP:       return "Up";
        case DS_RIGHT:    return "Right";
        case DS_DOWN:     return "Down";
        case DS_LEFT:     return "Left";
        case DS_L2:       return "L2";
        case DS_R2:       return "R2";
        case DS_L1:       return "L1";
        case DS_R1:       return "R1";
        case DS_TRIANGLE: return "Triangle";
        case DS_CIRCLE:   return "Circle";
        case DS_CROSS:    return "Cross";
        case DS_SQUARE:   return "Square";
        case DS_TOUCHPAD: return "Touchpad";
    }
    return "?";
}

static void draw_mask_presses(u32 *fb, int x, int y, u32 mask) {
    if (!mask) { ui_str(fb, x, y, "(no bits)", COL_TEXT_DIM, 3); return; }
    int cx = x;
    for (int b = 0; b < 21; b++) {
        u32 bit = 1u << b;
        if (!(mask & bit)) continue;
        const char *n = bit_name(bit);
        char buf[40]; int p = 0;
        buf[p++] = '[';
        while (*n && p < 20) buf[p++] = *n++;
        buf[p++] = ']';
        buf[p++] = ' ';
        buf[p] = 0;
        ui_str(fb, cx, y, buf, COL_ACCENT, 3);
        cx += ui_str_w(buf, 3);
    }
}

void padview_draw(struct ctx *c, u32 *fb) {
    u64 now = get_uptime_ms(c);

    ui_clear(fb, COL_BG);

    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "PAD STATE", COL_ACCENT, 5);
    ui_str_right(fb, SCR_W - 60, 30,
                 "LIVE  -  press O to go back", COL_TEXT_DIM, 3);

    int col_x = 60;
    int col_w = (SCR_W - 180) / 2;

    /* ---------- LEFT COLUMN: BUTTON CHIPS ---------- */
    int x0 = col_x, y0 = 130;
    int chip_w = (col_w - 40) / 2;
    int chip_h = 66;
    int gap = 10;

    int row = 0;
    #define ROW(label_a, bit_a, label_b, bit_b) do { \
        draw_chip(fb, x0, y0 + row * (chip_h + gap), chip_w, chip_h, \
                  label_a, bit_a, c->pad_prev, &c->dbg, now); \
        draw_chip(fb, x0 + chip_w + 20, y0 + row * (chip_h + gap), chip_w, chip_h, \
                  label_b, bit_b, c->pad_prev, &c->dbg, now); \
        row++; \
    } while (0)

    ROW("Cross",    DS_CROSS,    "Circle",    DS_CIRCLE);
    ROW("Triangle", DS_TRIANGLE, "Square",    DS_SQUARE);
    ROW("Up",       DS_UP,       "Right",     DS_RIGHT);
    ROW("Down",     DS_DOWN,     "Left",      DS_LEFT);
    ROW("L1",       DS_L1,       "R1",        DS_R1);
    ROW("L2",       DS_L2,       "R2",        DS_R2);
    ROW("L3",       DS_L3,       "R3",        DS_R3);
    ROW("Options",  DS_OPTIONS,  "Share",     DS_SHARE);
    ROW("Touchpad", DS_TOUCHPAD, "",          0);
    #undef ROW

    /* ---------- RIGHT COLUMN ---------- */
    int rx = col_x + col_w + 40;
    int rw = col_w;

    /* Big bitmask */
    ui_fill(fb, rx, y0, rw, 80, COL_PANEL);
    ui_frame(fb, rx, y0, rw, 80, COL_BORDER, 2);
    ui_str(fb, rx + 16, y0 + 8, "Bitmask:", COL_TEXT_DIM, 3);
    {
        char b[20] = "0x00000000";
        const char *h = "0123456789ABCDEF";
        u32 v = c->pad_prev;
        for (int i = 0; i < 8; i++) b[2 + i] = h[(v >> ((7 - i) * 4)) & 0xF];
        ui_str(fb, rx + 16, y0 + 40, b, COL_TITLE, 4);
    }

    /* Currently pressed named list */
    int ny = y0 + 100;
    ui_str(fb, rx, ny, "Pressed:", COL_TEXT_DIM, 3);
    ny += 32;
    draw_mask_presses(fb, rx, ny, c->pad_prev);

    /* Raw bytes dump */
    int by = ny + 50;
    ui_str(fb, rx, by, "Raw scePadRead bytes:", COL_TEXT_DIM, 3);
    by += 30;
    {
        char line[80];
        const char *hx = "0123456789ABCDEF";
        u32 nb = c->raw_buf_n;
        if (nb > 48) nb = 48;
        for (u32 off = 0; off < nb; off += 16) {
            int p = 0;
            line[p++] = hx[(off >> 4) & 0xF];
            line[p++] = hx[off & 0xF];
            line[p++] = ':';
            line[p++] = ' ';
            u32 lim = off + 16;
            if (lim > nb) lim = nb;
            for (u32 i = off; i < lim; i++) {
                u8 v = c->raw_buf[i];
                line[p++] = hx[(v >> 4) & 0xF];
                line[p++] = hx[v & 0xF];
                line[p++] = ' ';
            }
            line[p] = 0;
            ui_str(fb, rx, by, line, COL_TEXT, 3);
            by += 28;
        }
        if (nb == 0) {
            ui_str(fb, rx, by, "(no bytes)", COL_TEXT_DIM, 3);
            by += 28;
        }
    }

    /* Press history */
    by += 10;
    ui_str(fb, rx, by, "History (last 8):", COL_TEXT_DIM, 3);
    by += 30;
    {
        int n = c->dbg.press_hist_count;
        if (n > 8) n = 8;
        int head = c->dbg.press_hist_head;
        for (int i = 0; i < n; i++) {
            int idx = (head - 1 - i + 16) & 15;
            u32 m  = c->dbg.press_hist[idx].mask;
            u32 ms = c->dbg.press_hist[idx].ms;
            char line[80];
            int p = 0;
            line[p++] = '[';
            int ss = ms / 1000;
            int mi = ss / 60;
            ss %= 60;
            line[p++] = '0' + (mi / 10);
            line[p++] = '0' + (mi % 10);
            line[p++] = ':';
            line[p++] = '0' + (ss / 10);
            line[p++] = '0' + (ss % 10);
            line[p++] = '.';
            u32 frac = ms % 1000;
            line[p++] = '0' + (frac / 100);
            line[p++] = '0' + ((frac / 10) % 10);
            line[p++] = '0' + (frac % 10);
            line[p++] = ']';
            line[p++] = ' ';
            const char *nm = bit_name(m);
            while (*nm && p < 60) line[p++] = *nm++;
            line[p] = 0;
            u32 col = (i == 0) ? COL_TITLE : COL_TEXT;
            ui_str(fb, rx, by, line, col, 3);
            by += 26;
        }
        if (n == 0)
            ui_str(fb, rx, by, "(nothing yet)", COL_TEXT_DIM, 3);
    }

    /* footer */
    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);

    char info[160]; int ip = 0;
    const char *pre = "scePadRead ret = ";
    while (*pre) info[ip++] = *pre++;
    ip += s_hex64(info + ip, (u64)(s64)c->dbg.last_read_ret);
    const char *pre2 = "   bytes = ";
    while (*pre2) info[ip++] = *pre2++;
    char nb[8]; int nl = s_itoa(nb, (int)c->raw_buf_n);
    for (int i = 0; i < nl; i++) info[ip++] = nb[i];
    info[ip] = 0;
    ui_str(fb, 60, SCR_H - 44, info, COL_TEXT, 3);
}

int padview_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)c; (void)raw;
    if (pressed & DS_CIRCLE) {
        menu_goto(scr_pad.parent);
        return 1;
    }
    return 1;
}
