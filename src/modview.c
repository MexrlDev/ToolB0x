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
#define COL_BAD      RGB(255,80,80)

#define MAX_ROW 16

struct mod_row {
    char label[24];
    u64  addr;
    u64  base;
    char name[32];
    int  valid;
};

static struct mod_row g_rows[MAX_ROW];
static int g_nrows = 0;
static int g_entered = 0;

static void add_row(const char *label, u64 addr) {
    if (g_nrows >= MAX_ROW) return;
    struct mod_row *r = &g_rows[g_nrows];
    int p = 0;
    while (label[p] && p < 23) { r->label[p] = label[p]; p++; }
    r->label[p] = 0;
    r->addr  = addr;
    r->base  = 0;
    r->name[0] = 0;
    r->valid = 0;
    g_nrows++;
}

static void modview_refresh(void) {
    ulog(&G_CTX, "[toolbox] modview refresh start\n");
    g_nrows = 0;
    add_row("EBOOT",         G_CTX.eboot_base);
    add_row("libScePad",     (u64)G_CTX.pad_read);
    add_row("libSceVideo",   (u64)G_CTX.vid_flip);
    add_row("libSceAudio",   (u64)G_CTX.aud_out);
    add_row("kernel:usleep", (u64)G_CTX.usleep);
    add_row("kernel:alloc",  (u64)G_CTX.alloc_dm);
    add_row("kernel:mmap",   (u64)G_CTX.mmap_fn);
    add_row("kernel:socket", (u64)G_CTX.socket_fn);
    add_row("kernel:modinfo",(u64)G_CTX.module_info_from_addr);
    add_row("kernel:swver",  (u64)G_CTX.sys_sw_version);

    for (int i = 0; i < g_nrows; i++) {
        struct mod_row *r = &g_rows[i];
        ulog_num(&G_CTX, "[toolbox] modview row addr=", r->addr);
        if (!r->addr) continue;
        if (!G_CTX.module_info_from_addr) continue;

        struct module_info_simple mi;
        s32 rc = get_module_info_of_addr(&G_CTX, r->addr, &mi);
        ulog_num(&G_CTX, "[toolbox]   rc=", (u64)(s64)rc);
        if (rc == 0 && mi.valid) {
            r->base = mi.base;
            for (int j = 0; j < 31; j++) r->name[j] = mi.name[j];
            r->name[31] = 0;
            r->valid = 1;
            ulog_num(&G_CTX, "[toolbox]   base=", r->base);
        }
    }
    ulog(&G_CTX, "[toolbox] modview refresh done\n");
}

void modview_draw(struct ctx *c, u32 *fb) {
    if (!g_entered) { modview_refresh(); g_entered = 1; }

    ui_clear(fb, COL_BG);

    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "MODULES & KERNEL", COL_ACCENT, 5);
    ui_str_right(fb, SCR_W - 60, 30, "O: back  X: refresh", COL_TEXT_DIM, 3);

    int y = 130;
    int row_h = 62;
    char b[64];

    u32 fw = get_fw_version_int(c);
    ui_str(fb, 60, y, ">> Firmware", COL_HDR, 3); y += 40;

    ui_str(fb, 60, y, "Sw version:", COL_TEXT_DIM, 3);
    if (fw) {
        u32 major = (fw >> 24) & 0xFF;
        u32 minor = (fw >> 16) & 0xFF;
        u32 build = fw & 0xFFFF;
        int p = 0;
        p += s_itoa(b + p, (int)major); b[p++] = '.';
        if (minor < 10) b[p++] = '0';
        p += s_itoa(b + p, (int)minor);
        b[p++] = '.'; b[p++] = ' ';
        if (build < 100) b[p++] = '0';
        if (build < 10)  b[p++] = '0';
        p += s_itoa(b + p, (int)build);
        b[p] = 0;
    } else {
        b[0]='('; b[1]='n'; b[2]='o'; b[3]=' '; b[4]='d';
        b[5]='a'; b[6]='t'; b[7]='a'; b[8]=')'; b[9]=0;
    }
    ui_str_right(fb, SCR_W - 60, y, b, COL_TITLE, 4); y += row_h;

    s_hex64(b, (u64)fw);
    ui_str(fb, 60, y, "Raw value:", COL_TEXT_DIM, 3);
    ui_str_right(fb, SCR_W - 60, y, b, COL_TEXT, 3); y += row_h;

    y += 20;
    ui_str(fb, 60, y, ">> Modules (via sceKernelGetModuleInfoFromAddr)", COL_HDR, 3);
    y += 40;

    for (int i = 0; i < g_nrows; i++) {
        struct mod_row *r = &g_rows[i];
        ui_str(fb, 60, y, r->label, COL_TEXT_DIM, 3);
        if (r->valid) {
            char nameb[40]; int p = 0;
            for (int j = 0; j < 31 && r->name[j]; j++) nameb[p++] = r->name[j];
            nameb[p] = 0;
            ui_str(fb, 300, y, nameb, COL_ACCENT, 3);
            s_hex64(b, r->base);
            ui_str_right(fb, SCR_W - 60, y, b, COL_TITLE, 3);
        } else {
            ui_str(fb, 300, y, "(not resolved)", COL_BAD, 3);
        }
        y += 40;
    }

    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, SCR_H - 44,
           "X: Refresh    O: Back to main menu",
           COL_TEXT_DIM, 3);
}

int modview_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)c; (void)raw;
    if (pressed & DS_CIRCLE) {
        menu_goto(scr_modview.parent);
        return 1;
    }
    if (pressed & DS_CROSS) {
        g_entered = 0;
        return 1;
    }
    return 1;
}
