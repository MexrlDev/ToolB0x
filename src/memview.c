#include "screens.h"
#include "ui.h"

#define COL_BG       RGB(8,8,14)
#define COL_PANEL    RGB(20,20,28)
#define COL_BORDER   RGB(70,70,95)
#define COL_TEXT     RGB(230,230,240)
#define COL_TEXT_DIM RGB(110,110,130)
#define COL_ACCENT   RGB(0,180,255)
#define COL_TITLE    RGB(255,200,80)
#define COL_HDR      RGB(255,140,0)
#define COL_HEX      RGB(200,220,255)
#define COL_HEX_DIM  RGB(140,150,170)
#define COL_ASCII    RGB(180,200,180)
#define COL_ASCII_NP RGB(90,100,110)
#define COL_EDIT_BG  RGB(140,40,40)
#define COL_EDIT_FG  RGB(255,255,180)
#define COL_CUR_BG   RGB(90,60,10)
#define COL_CUR_BD   RGB(255,200,80)
#define COL_COL_HDR  RGB(120,140,180)

#define MODE_VIEW   0
#define MODE_EDIT   1

/* Layout constants — ONE place to tune, no overlaps */
#define MV_ADDR_X      60
#define MV_ADDR_SCALE  3
#define MV_ADDR_CHARS  8           /* last 32 bits only */
#define MV_ADDR_END    (MV_ADDR_X + MV_ADDR_CHARS * 8 * MV_ADDR_SCALE)  /* 252 */

#define MV_BYTES_X     300
#define MV_BYTE_SCALE  3
#define MV_BYTE_W      (2 * 8 * MV_BYTE_SCALE)   /* 48 px */
#define MV_BYTE_GAP    8
#define MV_BYTE_STEP   (MV_BYTE_W + MV_BYTE_GAP) /* 56 px */
#define MV_BYTES_END   (MV_BYTES_X + 16 * MV_BYTE_STEP)  /* 300 + 896 = 1196 */

#define MV_ASCII_X     1280
#define MV_ASCII_SCALE 3
#define MV_ASCII_W     (8 * MV_ASCII_SCALE)      /* 24 px */
#define MV_ASCII_END   (MV_ASCII_X + 16 * MV_ASCII_W)   /* 1280 + 384 = 1664 */

#define MV_ROWS        13
#define MV_ROW_H       58
#define MV_Y0          220

static u64 g_addr        = 0;
static int g_cursor      = 0;
static int g_mode        = MODE_VIEW;
static u8  g_edit_val    = 0;

static char g_toast[80];
static u64  g_toast_until = 0;

static const char *HEX = "0123456789ABCDEF";

static void toast(const char *msg, u64 duration_ms) {
    int i = 0;
    while (msg[i] && i < 79) { g_toast[i] = msg[i]; i++; }
    g_toast[i] = 0;
    g_toast_until = get_uptime_ms(&G_CTX) + duration_ms;
}

static void ensure_addr(struct ctx *c) {
    if (g_addr == 0) g_addr = c->eboot_base;
}

static void fmt_hex64(char *out, u64 v) {
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; i++) out[2 + i] = HEX[(v >> ((15 - i) * 4)) & 0xF];
    out[18] = 0;
}

static void fmt_hex_full(char *out, u64 v, int nibbles) {
    for (int i = 0; i < nibbles; i++)
        out[i] = HEX[(v >> ((nibbles - 1 - i) * 4)) & 0xF];
    out[nibbles] = 0;
}

static int parse_hex_any(const char *s, u64 *out) {
    while (*s == ' ' || *s == '\t') s++;
    int negative = 0;
    if (*s == '-') { negative = 1; s++; }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    u64 v = 0; int any = 0;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        v = (v << 4) | (u64)d;
        any = 1;
        s++;
    }
    if (!any) return -1;
    *out = negative ? (u64)(-(s64)v) : v;
    return 0;
}

static void dump_to_log(struct ctx *c, u64 addr, int bytes) {
    char line[160];
    for (int off = 0; off < bytes; off += 16) {
        int p = 0;
        line[p++] = '[';
        for (int i = 8; i < 16; i++)
            line[p++] = HEX[(addr >> ((15 - i) * 4)) & 0xF];
        line[p++] = ']'; line[p++] = ' ';
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, addr + off + i);
            line[p++] = HEX[(v >> 4) & 0xF];
            line[p++] = HEX[v & 0xF];
            line[p++] = ' ';
        }
        line[p++] = '|'; line[p++] = ' ';
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, addr + off + i);
            line[p++] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
        }
        line[p++] = '\n'; line[p] = 0;
        ulog(c, line);
    }
}

static void dump_to_file(struct ctx *c, u64 addr, int bytes) {
    const char *path = "/savedata0/memdump.bin";
    s32 fd = (s32)NC(c->G, c->kopen, (u64)path, 0x0601, 0x1FF, 0,0,0);
    if (fd < 0) { toast("Cannot open memdump.bin", 2000); return; }
    for (int i = 0; i < bytes; i++) {
        u8 v = mem_read8(c, addr + i);
        NC(c->G, c->kwrite, (u64)fd, (u64)&v, 1, 0,0,0);
    }
    NC(c->G, c->kclose, (u64)fd, 0,0,0,0,0);
    toast("Saved memdump.bin (256 B)", 2000);
}

/* ---------------------------------------------------------
 * OSK wrappers
 * --------------------------------------------------------- */
static void osk_jump_address(struct ctx *c) {
    char initial[24];
    fmt_hex64(initial, g_addr);
    char result[64];
    int r = osk_prompt(c, "Jump to address", initial, result, 64, 20);
    if (r != 0) { toast("OSK failed or cancelled", 1500); return; }
    u64 new_addr = 0;
    if (parse_hex_any(result, &new_addr) == 0) {
        g_addr = new_addr;
        g_cursor = 0;
        toast("Jumped", 1000);
    } else {
        toast("Bad hex address", 1500);
    }
}

static void osk_write_value(struct ctx *c) {
    u64 cur_addr = g_addr + g_cursor;
    u8  cur_val  = mem_read8(c, cur_addr);
    char initial[8];
    fmt_hex_full(initial, cur_val, 2);
    char result[64];
    int r = osk_prompt(c, "Write value (hex)", initial, result, 64, 16);
    if (r != 0) { toast("OSK failed or cancelled", 1500); return; }
    u64 val = 0;
    if (parse_hex_any(result, &val) != 0) { toast("Bad hex value", 1500); return; }

    int digits = 0;
    for (const char *s = result; *s; s++) {
        if ((*s >= '0' && *s <= '9') ||
            (*s >= 'a' && *s <= 'f') ||
            (*s >= 'A' && *s <= 'F')) digits++;
    }
    if      (digits <= 2) mem_write8 (c, cur_addr, (u8) val);
    else if (digits <= 4) mem_write16(c, cur_addr, (u16)val);
    else if (digits <= 8) mem_write32(c, cur_addr, (u32)val);
    else                  mem_write64(c, cur_addr, val);
    toast("Value written", 1000);
}

/* ---------------------------------------------------------
 * Drawing — clean column layout, no overlaps
 * --------------------------------------------------------- */

static void draw_col_header(u32 *fb, int y) {
    ui_str(fb, MV_ADDR_X, y, "ADDR", COL_COL_HDR, 3);
    for (int i = 0; i < 16; i++) {
        char b[3]; b[0] = HEX[i]; b[1] = 0;
        ui_str(fb, MV_BYTES_X + i * MV_BYTE_STEP, y, b, COL_COL_HDR, 3);
    }
    ui_str(fb, MV_ASCII_X, y, "ASCII", COL_COL_HDR, 3);
}

void memview_draw(struct ctx *c, u32 *fb) {
    ensure_addr(c);

    ui_clear(fb, COL_BG);

    /* Header */
    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "MEMORY EDITOR", COL_ACCENT, 5);
    {
        const char *mode = (g_mode == MODE_EDIT) ? "EDIT BYTE" : "VIEW";
        u32 mc = (g_mode == MODE_EDIT) ? COL_EDIT_FG : COL_TITLE;
        ui_str_right(fb, SCR_W - 60, 30, mode, mc, 4);
    }

    /* Address bar */
    {
        char b[24]; fmt_hex64(b, g_addr);
        ui_fill(fb, 60, 110, SCR_W - 120, 60, COL_PANEL);
        ui_frame(fb, 60, 110, SCR_W - 120, 60, COL_BORDER, 2);
        ui_str(fb, 80, 122, "ADDR", COL_TEXT_DIM, 3);
        ui_str(fb, 200, 122, b, COL_TITLE, 4);

        char cb[24]; int p = 0;
        cb[p++]='+'; cb[p++]='0'; cb[p++]='x';
        cb[p++]=HEX[(g_cursor >> 4) & 0xF];
        cb[p++]=HEX[g_cursor & 0xF];
        cb[p]=0;
        ui_str_right(fb, SCR_W - 100, 132, cb, COL_TEXT_DIM, 3);
    }

    /* Column header */
    draw_col_header(fb, MV_Y0 - 32);

    /* Rows */
    for (int r = 0; r < MV_ROWS; r++) {
        u64 a = g_addr + (u64)r * 16;
        int y = MV_Y0 + r * MV_ROW_H;
        int is_cur_row = (r == 0);

        if (is_cur_row) {
            ui_fill(fb, 40, y - 4, SCR_W - 80, MV_ROW_H - 4, RGB(20, 40, 70));
        }

        /* Address: last 8 hex chars */
        char ab[24];
        for (int i = 0; i < 16; i++) ab[i] = HEX[(a >> ((15 - i) * 4)) & 0xF];
        ab[16] = 0;
        ui_str(fb, MV_ADDR_X, y + 6, ab + 8,
               is_cur_row ? 0xFFFFFFFF : COL_TEXT_DIM, MV_ADDR_SCALE);

        /* Bytes */
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, a + i);
            char b[4];
            b[0] = HEX[(v >> 4) & 0xF];
            b[1] = HEX[v & 0xF];
            b[2] = 0;
            int bx = MV_BYTES_X + i * MV_BYTE_STEP;
            int is_cursor = is_cur_row && (i == g_cursor);

            if (is_cursor && g_mode == MODE_EDIT) {
                char eb[4];
                eb[0] = HEX[(g_edit_val >> 4) & 0xF];
                eb[1] = HEX[g_edit_val & 0xF];
                eb[2] = 0;
                ui_fill(fb, bx - 6, y + 2, MV_BYTE_W + 12, 40, COL_EDIT_BG);
                ui_str(fb, bx, y + 6, eb, COL_EDIT_FG, MV_BYTE_SCALE);
            } else if (is_cursor) {
                ui_fill(fb, bx - 6, y + 2, MV_BYTE_W + 12, 40, COL_CUR_BG);
                ui_frame(fb, bx - 6, y + 2, MV_BYTE_W + 12, 40, COL_CUR_BD, 3);
                ui_str(fb, bx, y + 6, b, COL_TITLE, MV_BYTE_SCALE);
            } else {
                ui_str(fb, bx, y + 6, b,
                       is_cur_row ? COL_HEX : COL_HEX_DIM, MV_BYTE_SCALE);
            }
        }

        /* ASCII */
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, a + i);
            char cbuf[2];
            cbuf[0] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
            cbuf[1] = 0;
            int cx = MV_ASCII_X + i * MV_ASCII_W;
            int is_cursor = is_cur_row && (i == g_cursor);
            u32 col = (v >= 0x20 && v < 0x7F) ? COL_ASCII : COL_ASCII_NP;
            if (is_cursor) col = COL_TITLE;
            ui_str(fb, cx, y + 6, cbuf, col, MV_ASCII_SCALE);
        }
    }

    /* Toast */
    u64 now = get_uptime_ms(c);
    if (g_toast_until > now && g_toast[0]) {
        int tw = ui_str_w(g_toast, 4) + 80;
        ui_fill(fb, SCR_W / 2 - tw / 2, SCR_H / 2 - 60, tw, 120, RGB(30,30,45));
        ui_frame(fb, SCR_W / 2 - tw / 2, SCR_H / 2 - 60, tw, 120, COL_TITLE, 3);
        ui_str_center(fb, SCR_H / 2 - 20, g_toast, COL_TITLE, 4);
    }

    /* Footer */
    ui_fill(fb, 0, SCR_H - 90, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 93, SCR_W, 3, COL_ACCENT);

    if (g_mode == MODE_EDIT) {
        ui_str(fb, 60, SCR_H - 70,
               "EDIT:  U/D high nibble  L/R low nibble  X commit  O cancel",
               COL_EDIT_FG, 3);
    } else {
        ui_str(fb, 60, SCR_H - 68,
               "D-Pad move  L1/R1 +/-100  L2/R2 +/-1000  X edit",
               COL_TEXT_DIM, 3);
        ui_str(fb, 60, SCR_H - 32,
               "SQ OSK:jump  TR OSK:write  OPT EBOOT  TOUCH dump-log  START save-file  O back",
               COL_TEXT_DIM, 3);
    }
}

/* ---------------------------------------------------------
 * Input
 * --------------------------------------------------------- */
int memview_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)raw;
    ensure_addr(c);

    if (g_mode == MODE_EDIT) {
        if (pressed & DS_UP)    g_edit_val = (g_edit_val + 0x10) & 0xFF;
        if (pressed & DS_DOWN)  g_edit_val = (g_edit_val - 0x10) & 0xFF;
        if (pressed & DS_LEFT)  g_edit_val = (g_edit_val - 1) & 0xFF;
        if (pressed & DS_RIGHT) g_edit_val = (g_edit_val + 1) & 0xFF;
        if (pressed & DS_CROSS) {
            mem_write8(c, g_addr + g_cursor, g_edit_val);
            g_mode = MODE_VIEW;
            toast("Byte written", 800);
        }
        if (pressed & DS_CIRCLE) {
            g_mode = MODE_VIEW;
        }
        return 1;
    }

    if (pressed & DS_UP)    g_addr -= 16;
    if (pressed & DS_DOWN)  g_addr += 16;
    if (pressed & DS_LEFT) {
        if (g_cursor > 0) g_cursor--;
        else { g_addr -= 16; g_cursor = 15; }
    }
    if (pressed & DS_RIGHT) {
        if (g_cursor < 15) g_cursor++;
        else { g_addr += 16; g_cursor = 0; }
    }
    if (pressed & DS_L1) g_addr -= 0x100;
    if (pressed & DS_R1) g_addr += 0x100;
    if (pressed & DS_L2) g_addr -= 0x1000;
    if (pressed & DS_R2) g_addr += 0x1000;

    if (pressed & DS_CROSS) {
        g_mode = MODE_EDIT;
        g_edit_val = mem_read8(c, g_addr + g_cursor);
    }
    if (pressed & DS_SQUARE)   osk_jump_address(c);
    if (pressed & DS_TRIANGLE) osk_write_value(c);
    if (pressed & DS_OPTIONS) {
        g_addr = c->eboot_base;
        g_cursor = 0;
        toast("Jumped to EBOOT", 800);
    }
    if (pressed & DS_TOUCHPAD) {
        dump_to_log(c, g_addr, 256);
        toast("Dumped 256 bytes to log", 1500);
    }
    if (pressed & DS_SHARE) {
        dump_to_file(c, g_addr, 256);
    }
    if (pressed & DS_CIRCLE) {
        menu_goto(scr_memview.parent);
        return 1;
    }
    return 1;
}
