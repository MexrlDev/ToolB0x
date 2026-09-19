#include "ui.h"
#include "font.h"

void ui_clear(u32 *fb, u32 color) {
    for (int i = 0; i < SCR_W * SCR_H; i++) fb[i] = color;
}

void ui_fill(u32 *fb, int x, int y, int w, int h, u32 color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    for (int j = 0; j < h; j++) {
        u32 *row = fb + (y + j) * SCR_W;
        for (int i = 0; i < w; i++) row[x + i] = color;
    }
}

void ui_frame(u32 *fb, int x, int y, int w, int h, u32 color, int t) {
    ui_fill(fb, x, y, w, t, color);
    ui_fill(fb, x, y + h - t, w, t, color);
    ui_fill(fb, x, y, t, h, color);
    ui_fill(fb, x + w - t, y, t, h, color);
}

void ui_char(u32 *fb, int x, int y, char c, u32 color, int scale) {
    u8 ch = (u8)c;
    if (ch < 0x20 || ch > 0x7F) ch = '?';
    const u8 *g = font8x8[ch - 0x20];

    for (int row = 0; row < 8; row++) {
        u8 bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (!(bits & (0x80 >> col))) continue;
            int bx = x + col * scale;
            int by = y + row * scale;
            for (int dy = 0; dy < scale; dy++) {
                int py = by + dy;
                if (py < 0 || py >= SCR_H) continue;
                u32 *rp = fb + py * SCR_W;
                for (int dx = 0; dx < scale; dx++) {
                    int px = bx + dx;
                    if (px < 0 || px >= SCR_W) continue;
                    rp[px] = color;
                }
            }
        }
    }
}

void ui_str(u32 *fb, int x, int y, const char *s, u32 color, int scale) {
    while (*s) { ui_char(fb, x, y, *s, color, scale); x += 8 * scale; s++; }
}

int ui_str_w(const char *s, int scale) { return s_len(s) * 8 * scale; }

void ui_str_center(u32 *fb, int y, const char *s, u32 color, int scale) {
    ui_str(fb, (SCR_W - ui_str_w(s, scale)) / 2, y, s, color, scale);
}

void ui_str_right(u32 *fb, int xr, int y, const char *s, u32 color, int scale) {
    ui_str(fb, xr - ui_str_w(s, scale), y, s, color, scale);
}
