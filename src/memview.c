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
#define COL_SEL_BG   RGB(0,110,200)
#define COL_HEX      RGB(200,220,255)
#define COL_ASCII    RGB(180,200,180)
#define COL_EDIT_BG  RGB(140,40,40)
#define COL_EDIT_FG  RGB(255,255,180)

static u64 g_mem_addr = 0;
static int g_mem_cursor = 0;       /* 0..15 */
static int g_mem_editing = 0;
static u8  g_mem_edit_val = 0;
static u64 g_mem_start = 0;

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void fmt_hex64(char *out, u64 v) {
    const char *h = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; i++) out[2 + i] = h[(v >> ((15 - i) * 4)) & 0xF];
    out[18] = 0;
}

static void ensure_start(struct ctx *c) {
    if (g_mem_start == 0) {
        g_mem_start = c->eboot_base;
        g_mem_addr  = g_mem_start;
    }
}

void memview_draw(struct ctx *c, u32 *fb) {
    ensure_start(c);

    ui_clear(fb, COL_BG);

    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "MEMORY EDITOR", COL_ACCENT, 5);
    ui_str_right(fb, SCR_W - 60, 30,
                 g_mem_editing ? "EDIT MODE" : "VIEW MODE",
                 g_mem_editing ? RGB(255,80,80) : COL_TITLE, 4);

    /* address bar */
    {
        char b[24]; fmt_hex64(b, g_mem_addr);
        ui_fill(fb, 60, 110, SCR_W - 120, 60, COL_PANEL);
        ui_frame(fb, 60, 110, SCR_W - 120, 60, COL_BORDER, 2);
        ui_str(fb, 80, 122, "ADDR", COL_TEXT_DIM, 3);
        ui_str(fb, 200, 122, b, COL_TITLE, 4);
    }

    /* hex dump: 12 rows x 16 bytes */
    int y0 = 200;
    int row_h = 62;
    int rows = 12;

    for (int r = 0; r < rows; r++) {
        u64 a = g_mem_addr + (u64)r * 16;
        int y = y0 + r * row_h;
        int is_cur_row = (r == 0);

        if (is_cur_row) {
            ui_fill(fb, 60, y - 4, SCR_W - 120, row_h - 4, COL_SEL_BG);
        }

        /* row address */
        char ab[20];
        const char *h = "0123456789ABCDEF";
        ab[0] = '0'; ab[1] = 'x';
        for (int i = 0; i < 16; i++) ab[2 + i] = h[(a >> ((15 - i) * 4)) & 0xF];
        ab[18] = 0;
        ui_str(fb, 80, y + 6, ab + 8, is_cur_row ? 0xFFFFFFFF : COL_TEXT_DIM, 3);

        /* bytes */
        int bx = 240;
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, a + i);
            char b[4];
            b[0] = h[(v >> 4) & 0xF];
            b[1] = h[v & 0xF];
            b[2] = 0;

            int is_cursor = is_cur_row && (i == g_mem_cursor);

            if (is_cursor && g_mem_editing) {
                /* draw current (edited) value instead */
                char eb[4];
                eb[0] = h[(g_mem_edit_val >> 4) & 0xF];
                eb[1] = h[g_mem_edit_val & 0xF];
                eb[2] = 0;
                ui_fill(fb, bx - 4, y + 2, 32, 40, COL_EDIT_BG);
                ui_str(fb, bx, y + 6, eb, COL_EDIT_FG, 3);
            } else if (is_cursor) {
                ui_frame(fb, bx - 4, y + 2, 32, 40, COL_TITLE, 3);
                ui_str(fb, bx, y + 6, b, COL_TITLE, 3);
            } else {
                ui_str(fb, bx, y + 6, b, is_cur_row ? COL_HEX : COL_HEX, 3);
            }
            bx += 42;
        }

        /* ASCII column */
        int ax = 240 + 16 * 42 + 20;
        char ascii[17];
        for (int i = 0; i < 16; i++) {
            u8 v = mem_read8(c, a + i);
            ascii[i] = (v >= 0x20 && v < 0x7F) ? (char)v : '.';
        }
        ascii[16] = 0;
        ui_str(fb, ax, y + 6, ascii, COL_ASCII, 3);
    }

    /* footer */
    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);
    if (g_mem_editing) {
        ui_str(fb, 60, SCR_H - 44,
               "EDIT: Up/Down high nibble  L/R low nibble  X commit  O cancel",
               COL_EDIT_FG, 3);
    } else {
        ui_str(fb, 60, SCR_H - 44,
               "D-Pad: move  L1/R1: +-0x100  X: edit byte  O: back",
               COL_TEXT_DIM, 3);
    }
}

int memview_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)c; (void)raw;
    ensure_start(&G_CTX);

    if (g_mem_editing) {
        if (pressed & DS_UP)    g_mem_edit_val = (g_mem_edit_val + 0x10) & 0xFF;
        if (pressed & DS_DOWN)  g_mem_edit_val = (g_mem_edit_val - 0x10) & 0xFF;
        if (pressed & DS_LEFT)  g_mem_edit_val = (g_mem_edit_val - 1) & 0xFF;
        if (pressed & DS_RIGHT) g_mem_edit_val = (g_mem_edit_val + 1) & 0xFF;
        if (pressed & DS_CROSS) {
            mem_write8(c, g_mem_addr + g_mem_cursor, g_mem_edit_val);
            ulog(&G_CTX, "[toolbox] wrote byte\n");
            g_mem_editing = 0;
        }
        if (pressed & DS_CIRCLE) {
            g_mem_editing = 0;
        }
        return 1;
    }

    if (pressed & DS_UP)    g_mem_addr -= 16;
    if (pressed & DS_DOWN)  g_mem_addr += 16;
    if (pressed & DS_LEFT) {
        if (g_mem_cursor > 0) g_mem_cursor--;
        else { g_mem_addr -= 16; g_mem_cursor = 15; }
    }
    if (pressed & DS_RIGHT) {
        if (g_mem_cursor < 15) g_mem_cursor++;
        else { g_mem_addr += 16; g_mem_cursor = 0; }
    }
    if (pressed & DS_L1) g_mem_addr -= 0x100;
    if (pressed & DS_R1) g_mem_addr += 0x100;
    if (pressed & DS_L2) g_mem_addr -= 0x1000;
    if (pressed & DS_R2) g_mem_addr += 0x1000;

    if (pressed & DS_CROSS) {
        g_mem_editing = 1;
        g_mem_edit_val = mem_read8(c, g_mem_addr + g_mem_cursor);
    }
    if (pressed & DS_CIRCLE) {
        menu_goto(scr_memview.parent);
        return 1;
    }
    if (pressed & DS_OPTIONS) {
        /* jump to EBOOT start */
        g_mem_addr = G_CTX.eboot_base;
        g_mem_cursor = 0;
    }
    return 1;
}
