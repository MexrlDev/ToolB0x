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

/* Field order chosen so designated initializers are compact and readable.
 * ALWAYS use designated init: { .label="X", .kind=ITEM_ACTION, ... } */
struct Item {
    const char *label;
    int         kind;
    ItemFn      on_confirm;   /* press Cross               */
    ItemFn      on_left;      /* press D-Pad Left          */
    ItemFn      on_right;     /* press D-Pad Right         */
    ItemFn      on_change;    /* called by tick if slider  */
    Screen     *submenu;      /* ITEM_SUBMENU target       */
    const char *info;         /* ITEM_INFO static text     */
    const char *(*get_info)(void);  /* ITEM_INFO/ITEM_SLIDER dynamic */
    int        *value;        /* ITEM_SLIDER/ITEM_TOGGLE   */
    int         min, max, step;
    u64         data;         /* arbitrary payload         */
};

struct Screen {
    const char *title;
    Item       *items;
    int         count;
    int         visible;
    Screen     *parent;
    int         cursor;
    int         scroll;
};

extern Screen *g_screen;

extern Screen scr_main, scr_controller, scr_lightbar, scr_lightbar_rgb;
extern Screen scr_vib, scr_trig, scr_pad;
extern Screen scr_system, scr_video;
extern Screen scr_audio, scr_network, scr_debug;

void menu_init(void);
void menu_input(struct ctx *c, u32 raw, u32 pressed);
void menu_draw (struct ctx *c, u32 *fb);
void menu_tick (struct ctx *c);
void menu_goto (Screen *s);
void menu_request_exit(void);

#endif
