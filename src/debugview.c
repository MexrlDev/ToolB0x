#include "screens.h"
#include "ui.h"

#define COL_BG       RGB(8,8,14)
#define COL_PANEL    RGB(20,20,28)
#define COL_BORDER   RGB(70,70,95)
#define COL_TEXT     RGB(230,230,240)
#define COL_TEXT_DIM RGB(120,120,140)
#define COL_ACCENT   RGB(0,180,255)
#define COL_TITLE    RGB(255,200,80)
#define COL_HDR      RGB(255,140,0)
#define COL_GOOD     RGB(80,220,120)
#define COL_WARN     RGB(255,190,60)
#define COL_BAD      RGB(255,80,80)

#define BUILD_TAG    "v5-debug-mega"

static void row_kv(u32 *fb, int x, int y, int w,
                   const char *key, const char *val, u32 vcol) {
    ui_str(fb, x,        y, key, COL_TEXT_DIM, 3);
    ui_str_right(fb, x + w, y, val, vcol, 3);
}

static void fmt_u32(char *out, u32 v) { s_itoa(out, (int)v); }

static void fmt_hms(char *out, u64 ms) {
    u32 total = (u32)(ms / 1000);
    u32 h = total / 3600; total %= 3600;
    u32 m = total / 60;   total %= 60;
    u32 s = total;
    int p = 0;
    out[p++] = '0' + (h / 10); out[p++] = '0' + (h % 10); out[p++] = ':';
    out[p++] = '0' + (m / 10); out[p++] = '0' + (m % 10); out[p++] = ':';
    out[p++] = '0' + (s / 10); out[p++] = '0' + (s % 10);
    out[p] = 0;
}

void debugview_draw(struct ctx *c, u32 *fb) {
    ui_clear(fb, COL_BG);

    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "DEBUG PANEL", COL_ACCENT, 5);
    {
        char tag[64]; int p = 0;
        const char *pre = "BUILD "; while (*pre) tag[p++] = *pre++;
        const char *bt = BUILD_TAG; while (*bt && p < 50) tag[p++] = *bt++;
        tag[p] = 0;
        ui_str_right(fb, SCR_W - 60, 30, tag, COL_TITLE, 3);
    }

    u64 now  = get_uptime_ms(c);
    u32 uptime = (u32)((now - c->dbg.start_ms) / 1000);

    /* two columns */
    int xL = 60;
    int xR = SCR_W / 2 + 20;
    int colW = SCR_W / 2 - 100;
    int y = 130;
    int rowH = 44;
    char b[48];

    /* ================= LEFT COLUMN ================= */
    ui_fill(fb, xL - 20, y - 20, colW + 40, 20, COL_BG);
    ui_str(fb, xL, y - 40, ">> Timing", COL_HDR, 3);
    y += 0;

    fmt_hms(b, (u64)uptime * 1000);
    row_kv(fb, xL, y, colW, "Uptime:", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.fps);
    u32 fps_col = (c->dbg.fps >= 55) ? COL_GOOD :
                  (c->dbg.fps >= 30) ? COL_WARN : COL_BAD;
    row_kv(fb, xL, y, colW, "FPS:", b, fps_col); y += rowH;

    fmt_u32(b, c->dbg.frame_time_ms);
    row_kv(fb, xL, y, colW, "Frame ms (cur):", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.frame_time_min == 0xFFFFFFFF ? 0 : c->dbg.frame_time_min);
    row_kv(fb, xL, y, colW, "Frame ms (min):", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.frame_time_max);
    row_kv(fb, xL, y, colW, "Frame ms (max):", b, COL_TEXT); y += rowH;

    u32 avg = c->dbg.frame_time_n
        ? (u32)(c->dbg.frame_time_sum / c->dbg.frame_time_n) : 0;
    fmt_u32(b, avg);
    row_kv(fb, xL, y, colW, "Frame ms (avg):", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.draw_time_ms);
    row_kv(fb, xL, y, colW, "Draw ms:", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.flip_time_ms);
    row_kv(fb, xL, y, colW, "Flip ms:", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->total_frames);
    row_kv(fb, xL, y, colW, "Frames flipped:", b, COL_TEXT); y += rowH;

    y += 20;
    ui_str(fb, xL, y, ">> Video", COL_HDR, 3); y += 40;

    fmt_u32(b, (u32)c->video_h);
    row_kv(fb, xL, y, colW, "Video handle:", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.flips);
    row_kv(fb, xL, y, colW, "Total flips:", b, COL_TEXT); y += rowH;

    s_hex64(b, (u64)c->fbs[0]);
    row_kv(fb, xL, y, colW, "FB0 addr:", b, COL_TEXT); y += rowH;

    s_hex64(b, (u64)c->fbs[1]);
    row_kv(fb, xL, y, colW, "FB1 addr:", b, COL_TEXT); y += rowH;

    y += 20;
    ui_str(fb, xL, y, ">> Audio", COL_HDR, 3); y += 40;

    s_itoa(b, c->audio_h);
    row_kv(fb, xL, y, colW, "Audio handle:", b, COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.audio_submits);
    row_kv(fb, xL, y, colW, "Tones played:", b, COL_TEXT); y += rowH;

    /* ================= RIGHT COLUMN ================= */
    y = 130;

    ui_str(fb, xR, y - 40, ">> Pad", COL_HDR, 3);

    s_itoa(b, c->user_id);
    row_kv(fb, xR, y, colW, "User ID:", b, COL_TEXT); y += rowH;

    s_itoa(b, c->pad_h);
    row_kv(fb, xR, y, colW, "Pad handle:", b,
           c->pad_h >= 0 ? COL_GOOD : COL_BAD); y += rowH;

    fmt_u32(b, c->dbg.pad_ok);
    row_kv(fb, xR, y, colW, "Reads OK:", b, COL_GOOD); y += rowH;

    fmt_u32(b, c->dbg.pad_err);
    row_kv(fb, xR, y, colW, "Reads failed:", b,
           c->dbg.pad_err ? COL_BAD : COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.pad_disc);
    row_kv(fb, xR, y, colW, "Reads disconn:", b,
           c->dbg.pad_disc ? COL_WARN : COL_TEXT); y += rowH;

    fmt_u32(b, c->dbg.pad_presses);
    row_kv(fb, xR, y, colW, "Press events:", b, COL_TEXT); y += rowH;

    {
        char s[24] = "0x00000000";
        const char *h = "0123456789ABCDEF";
        u32 v = c->dbg.last_press_mask;
        for (int i = 0; i < 8; i++) s[2+i] = h[(v >> ((7-i)*4)) & 0xF];
        row_kv(fb, xR, y, colW, "Last press:", s, COL_TITLE); y += rowH;
    }

    {
        u32 ago = c->dbg.pad_presses ? c->dbg.last_press_ago_ms : 0;
        char buf[24]; int p = 0;
        p += s_itoa(buf + p, (int)ago);
        buf[p++] = ' '; buf[p++] = 'm'; buf[p++] = 's'; buf[p] = 0;
        row_kv(fb, xR, y, colW, "Ago:", buf, COL_TEXT); y += rowH;
    }

    s_hex64(b, get_dm_size(c));
    row_kv(fb, xR, y, colW, "DMEM size:", b, COL_TEXT); y += rowH;

    s_hex64(b, c->eboot_base);
    row_kv(fb, xR, y, colW, "EBOOT base:", b, COL_TEXT); y += rowH;

    s_hex64(b, (u64)c->G);
    row_kv(fb, xR, y, colW, "Gadget:", b, COL_TEXT); y += rowH;

    /* ---- pad bytes ---- */
    y += 20;
    ui_str(fb, xR, y, ">> Raw scePadRead (first 32 bytes)", COL_HDR, 3); y += 40;

    {
        const char *hx = "0123456789ABCDEF";
        u32 nb = c->raw_buf_n; if (nb > 32) nb = 32;
        for (u32 off = 0; off < nb; off += 8) {
            char line[64]; int p = 0;
            line[p++] = hx[(off >> 4) & 0xF];
            line[p++] = hx[off & 0xF];
            line[p++] = ':'; line[p++] = ' ';
            for (u32 i = off; i < off + 8 && i < nb; i++) {
                u8 v = c->raw_buf[i];
                line[p++] = hx[(v >> 4) & 0xF];
                line[p++] = hx[v & 0xF];
                line[p++] = ' ';
            }
            line[p] = 0;
            ui_str(fb, xR, y, line, COL_TEXT, 2);
            y += 22;
        }
        if (nb == 0) ui_str(fb, xR, y, "(no bytes yet)", COL_TEXT_DIM, 2);
    }

    /* ---- history ---- */
    y += 20;
    ui_str(fb, xR, y, ">> Press history", COL_HDR, 3); y += 40;

    {
        int n = c->dbg.press_hist_count;
        if (n > 8) n = 8;
        int head = c->dbg.press_hist_head;
        for (int i = 0; i < n; i++) {
            int idx = (head - 1 - i + 16) & 15;
            u32 m  = c->dbg.press_hist[idx].mask;
            u32 ms = c->dbg.press_hist[idx].ms;
            char line[80]; int p = 0;
            int ss = ms / 1000, mi = ss / 60; ss %= 60;
            line[p++]='0'+(mi/10); line[p++]='0'+(mi%10); line[p++]=':';
            line[p++]='0'+(ss/10); line[p++]='0'+(ss%10); line[p++]='.';
            u32 frac = ms % 1000;
            line[p++]='0'+(frac/100); line[p++]='0'+((frac/10)%10); line[p++]='0'+(frac%10);
            line[p++]=' ';
            char hx[12] = "0x00000000";
            const char *h = "0123456789ABCDEF";
            for (int k = 0; k < 8; k++) hx[2+k] = h[(m >> ((7-k)*4)) & 0xF];
            for (int k = 0; k < 10; k++) line[p++] = hx[k];
            line[p]=0;
            ui_str(fb, xR, y, line, (i == 0) ? COL_TITLE : COL_TEXT, 3);
            y += 26;
        }
        if (n == 0) ui_str(fb, xR, y, "(no presses yet)", COL_TEXT_DIM, 3);
    }

    /* footer */
    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, SCR_H - 44,
           "X: Reset counters    O: Back",
           COL_TEXT_DIM, 3);
}

int debugview_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)raw;
    if (pressed & DS_CIRCLE) {
        menu_goto(scr_debug.parent);
        return 1;
    }
    if (pressed & DS_CROSS) {
        dbg_reset(c);
        ulog(c, "[toolbox] debug counters reset\n");
        return 1;
    }
    return 1;
}
