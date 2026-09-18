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
    int        *value;
    int         min, max, step;
    ItemFn      on_confirm;
    ItemFn      on_left;
    ItemFn      on_right;
    ItemFn      on_change;
    Screen     *submenu;
    const char *(*get_info)(void);
    const char *info;
    u64         data;
};

struct Screen {
    const char *title;
    Item       *items;
    int         count;
    Screen     *parent;
    void       (*on_enter)(void);
    void       (*on_tick)(void);
    int         cursor;
    int         scroll;
    int         visible;
};

extern Screen *g_screen;
extern Screen scr_main, scr_controller, scr_lightbar, scr_lightbar_rgb;
extern Screen scr_vib, scr_trig, scr_pad;
extern Screen scr_system, scr_modules;
extern Screen scr_video;
extern Screen scr_audio, scr_tone;
extern Screen scr_network;
extern Screen scr_debug;

void   menu_init(void);
void   menu_enter(struct ctx *c);
void   menu_input(struct ctx *c, u32 raw, u32 pressed);
void   menu_draw(struct ctx *c, u32 *fb);
void   menu_tick(struct ctx *c);
void   menu_goto(Screen *s);

#endif
