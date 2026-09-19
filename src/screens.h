#ifndef SCREENS_H
#define SCREENS_H
#include "app.h"

#define ITEM_ACTION  0
#define ITEM_SUBMENU 1
#define ITEM_SLIDER  2
#define ITEM_TOGGLE  3
#define ITEM_INFO    4
#define ITEM_BACK    5
#define ITEM_HEADER  6

typedef struct Screen Screen;
typedef struct Item Item;
typedef void (*ItemFn)(Item *it);

struct Item {
    const char *label;
    int         kind;
    ItemFn      on_confirm;
    ItemFn      on_left;
    ItemFn      on_right;
    ItemFn      on_change;
    Screen     *submenu;
    const char *info;
    const char *(*get_info)(void);
    int        *value;
    int         min, max, step;
    u64         data;
};

struct Screen {
    const char *title;
    Item       *items;
    int         count;
    int         visible;
    Screen     *parent;
    int         cursor;
    int         scroll;
    void      (*custom_draw )(struct ctx *c, u32 *fb);
    int       (*custom_input)(struct ctx *c, u32 raw, u32 pressed);
};

extern Screen *g_screen;

extern Screen scr_main, scr_controller, scr_lightbar, scr_lightbar_rgb;
extern Screen scr_vib, scr_trig, scr_pad;
extern Screen scr_system, scr_video;
extern Screen scr_audio, scr_network, scr_debug;
extern Screen scr_memview, scr_modview, scr_notify;

void menu_init   (void);
void menu_input  (struct ctx *c, u32 raw, u32 pressed);
void menu_draw   (struct ctx *c, u32 *fb);
void menu_tick   (struct ctx *c);
void menu_goto   (Screen *s);
void menu_request_exit(void);

void padview_draw   (struct ctx *c, u32 *fb);
int  padview_input  (struct ctx *c, u32 raw, u32 pressed);
void debugview_draw (struct ctx *c, u32 *fb);
int  debugview_input(struct ctx *c, u32 raw, u32 pressed);
void memview_draw   (struct ctx *c, u32 *fb);
int  memview_input  (struct ctx *c, u32 raw, u32 pressed);
void modview_draw   (struct ctx *c, u32 *fb);
int  modview_input  (struct ctx *c, u32 raw, u32 pressed);

#endif
