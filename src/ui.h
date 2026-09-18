#ifndef UI_H
#define UI_H
#include "core.h"

void ui_clear(u32 *fb, u32 color);
void ui_fill(u32 *fb, int x, int y, int w, int h, u32 color);
void ui_frame(u32 *fb, int x, int y, int w, int h, u32 color, int t);
void ui_char(u32 *fb, int x, int y, char c, u32 color, int scale);
void ui_str(u32 *fb, int x, int y, const char *s, u32 color, int scale);
void ui_str_center(u32 *fb, int y, const char *s, u32 color, int scale);
void ui_str_right(u32 *fb, int xr, int y, const char *s, u32 color, int scale);
int  ui_str_w(const char *s, int scale);

#endif
