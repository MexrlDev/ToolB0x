#include "screens.h"
#include "ui.h"

/* ---- colors ---- */
#define COL_BG       RGB(8,8,14)
#define COL_PANEL    RGB(20,20,28)
#define COL_PANEL_2  RGB(30,30,42)
#define COL_BORDER   RGB(70,70,95)
#define COL_TEXT     RGB(230,230,240)
#define COL_TEXT_DIM RGB(120,120,140)
#define COL_ACCENT   RGB(0,180,255)
#define COL_ACCENT_D RGB(0,90,130)
#define COL_SEL_BG   RGB(0,110,200)
#define COL_TITLE    RGB(255,200,80)
#define COL_GOOD     RGB(80,220,120)
#define COL_WARN     RGB(255,190,60)
#define COL_BAD      RGB(255,80,80)
#define COL_HDR      RGB(255,140,0)

Screen *g_screen;

/* ============ forward decls ============ */
extern Screen scr_main, scr_controller, scr_lightbar, scr_lightbar_rgb;
extern Screen scr_vib, scr_trig, scr_pad;
extern Screen scr_system, scr_modules;
extern Screen scr_video;
extern Screen scr_audio, scr_tone;
extern Screen scr_network;
extern Screen scr_debug;

/* ============ shared state ============ */
static int  g_rgb[3] = {255, 100, 0};
static int  g_vib_l = 128, g_vib_s = 128;
static int  g_lb_mode = 0;   /* 0=static 1=rainbow */
static int  g_lb_hue = 0;
static int  g_vib_pulse = 0;
static int  g_vib_phase = 0;
static u32  g_vib_last = 0;

/* ============ utility ============ */
static void set_lb_from_rgb(void) {
    pad_set_lightbar(&G_CTX, (u8)g_rgb[0], (u8)g_rgb[1], (u8)g_rgb[2]);
}

static void clamp(int *v, int lo, int hi) {
    if (*v < lo) *v = lo;
    if (*v > hi) *v = hi;
}

/* ============ LIGHTBAR ============ */
static void act_lb_preset(Item *it) {
    u32 rgb = (u32)it->data;
    g_lb_mode = 0;
    g_rgb[0] = (rgb >> 16) & 0xFF;
    g_rgb[1] = (rgb >> 8)  & 0xFF;
    g_rgb[2] = rgb & 0xFF;
    set_lb_from_rgb();
}

static void act_lb_off(Item *it)  { (void)it; g_lb_mode = 0; g_rgb[0]=g_rgb[1]=g_rgb[2]=0; set_lb_from_rgb(); }
static void act_lb_rb(Item *it)   { (void)it; g_lb_mode = 1; }
static void act_lb_stop(Item *it) { (void)it; g_lb_mode = 0; set_lb_from_rgb(); }

static Item scr_lightbar_items[] = {
    { "Static Colors", ITEM_HEADER },
    { "Off",         ITEM_ACTION, 0,0,0, act_lb_off,0,0,0,0,0,0,0 },
    { "Red",         ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0xFF0000 },
    { "Green",       ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0x00FF00 },
    { "Blue",        ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0x0000FF },
    { "White",       ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0xFFFFFF },
    { "Orange",      ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0xFF8000 },
    { "Purple",      ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0xA020FF },
    { "Cyan",        ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0x00FFFF },
    { "Pink",        ITEM_ACTION, 0,0,0, act_lb_preset,0,0,0,0,0,0, 0xFF40A0 },
    { "Animated", ITEM_HEADER },
    { "Rainbow Pulse", ITEM_ACTION, 0,0,0, act_lb_rb,0,0,0,0,0,0,0 },
    { "Stop Animation",ITEM_ACTION, 0,0,0, act_lb_stop,0,0,0,0,0,0,0 },
    { "Custom RGB...",  ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_lightbar_rgb,0,0,0 },
    { "Back",           ITEM_BACK },
};

static const char *get_rgb_r(void) { static char b[8]; s_itoa(b, g_rgb[0]); return b; }
static const char *get_rgb_g(void) { static char b[8]; s_itoa(b, g_rgb[1]); return b; }
static const char *get_rgb_b(void) { static char b[8]; s_itoa(b, g_rgb[2]); return b; }

static void rgb_inc_r(Item *it) { (void)it; g_rgb[0] += 16; clamp(&g_rgb[0],0,255); set_lb_from_rgb(); }
static void rgb_dec_r(Item *it) { (void)it; g_rgb[0] -= 16; clamp(&g_rgb[0],0,255); set_lb_from_rgb(); }
static void rgb_inc_g(Item *it) { (void)it; g_rgb[1] += 16; clamp(&g_rgb[1],0,255); set_lb_from_rgb(); }
static void rgb_dec_g(Item *it) { (void)it; g_rgb[1] -= 16; clamp(&g_rgb[1],0,255); set_lb_from_rgb(); }
static void rgb_inc_b(Item *it) { (void)it; g_rgb[2] += 16; clamp(&g_rgb[2],0,255); set_lb_from_rgb(); }
static void rgb_dec_b(Item *it) { (void)it; g_rgb[2] -= 16; clamp(&g_rgb[2],0,255); set_lb_from_rgb(); }

static Item scr_lightbar_rgb_items[] = {
    { "Left/Right to adjust RGB", ITEM_HEADER },
    { "Red",   ITEM_SLIDER, 0, 0, 255, 16, 0, rgb_dec_r, rgb_inc_r, 0, 0, get_rgb_r, 0, 0 },
    { "Green", ITEM_SLIDER, 0, 0, 255, 16, 0, rgb_dec_g, rgb_inc_g, 0, 0, get_rgb_g, 0, 0 },
    { "Blue",  ITEM_SLIDER, 0, 0, 255, 16, 0, rgb_dec_b, rgb_inc_b, 0, 0, get_rgb_b, 0, 0 },
    { "Back",  ITEM_BACK },
};

/* ============ VIBRATION ============ */
static void vib_preset(Item *it) {
    u8 l = (it->data >> 8) & 0xFF;
    u8 s = it->data & 0xFF;
    g_vib_pulse = 0;
    g_vib_l = l; g_vib_s = s;
    pad_set_vibration(&G_CTX, l, s);
}
static void vib_start_pulse(Item *it) { (void)it; g_vib_pulse = 1; g_vib_phase = 0; }
static void vib_stop(Item *it)        { (void)it; g_vib_pulse = 0; pad_set_vibration(&G_CTX, 0, 0); }

static Item scr_vib_items[] = {
    { "Presets", ITEM_HEADER },
    { "Off",          ITEM_ACTION,0,0,0, vib_stop,0,0,0,0,0,0,0 },
    { "Weak",         ITEM_ACTION,0,0,0, vib_preset,0,0,0,0,0,0, 0x5050 },
    { "Medium",       ITEM_ACTION,0,0,0, vib_preset,0,0,0,0,0,0, 0x8080 },
    { "Strong",       ITEM_ACTION,0,0,0, vib_preset,0,0,0,0,0,0, 0xFFFF },
    { "Left Motor",   ITEM_ACTION,0,0,0, vib_preset,0,0,0,0,0,0, 0xFF00 },
    { "Right Motor",  ITEM_ACTION,0,0,0, vib_preset,0,0,0,0,0,0, 0x00FF },
    { "Animated", ITEM_HEADER },
    { "Pulse On",     ITEM_ACTION,0,0,0, vib_start_pulse,0,0,0,0,0,0,0 },
    { "Stop",         ITEM_ACTION,0,0,0, vib_stop,0,0,0,0,0,0,0 },
    { "Back",         ITEM_BACK },
};

/* ============ TRIGGERS ============ */
static void trig_preset(Item *it) {
    int mode = (int)((it->data >> 24) & 0xFF);
    int p1   = (int)((it->data >> 16) & 0xFF);
    int p2   = (int)((it->data >> 8)  & 0xFF);
    int p3   = (int)( it->data        & 0xFF);
    pad_set_trigger(&G_CTX, mode, 2, p1, p2, p3);  /* 2 = both triggers */
}
static void trig_off(Item *it) { (void)it; pad_set_trigger(&G_CTX, 0, 2, 0, 0, 0); }

static Item scr_trig_items[] = {
    { "Trigger Effects", ITEM_HEADER },
    { "Off",              ITEM_ACTION,0,0,0, trig_off,0,0,0,0,0,0,0 },
    { "Feedback Weak",    ITEM_ACTION,0,0,0, trig_preset,0,0,0,0,0,0, 0x010201 },
    { "Feedback Medium",  ITEM_ACTION,0,0,0, trig_preset,0,0,0,0,0,0, 0x010405 },
    { "Feedback Strong",  ITEM_ACTION,0,0,0, trig_preset,0,0,0,0,0,0, 0x010608 },
    { "Weapon",           ITEM_ACTION,0,0,0, trig_preset,0,0,0,0,0,0, 0x020508 },
    { "Vibration",        ITEM_ACTION,0,0,0, trig_preset,0,0,0,0,0,0, 0x030305 },
    { "Back",             ITEM_BACK },
};

/* ============ PAD VIEW ============ */
static u32 g_pad_snapshot = 0;

static const char *get_pad_hex(void) {
    static char b[12] = "0x00000000";
    const char *h = "0123456789ABCDEF";
    u32 v = g_pad_snapshot;
    for (int i = 0; i < 8; i++) b[2+i] = h[(v >> ((7-i)*4)) & 0xF];
    return b;
}

static Item scr_pad_items[] = {
    { "Raw Pad State", ITEM_HEADER },
    { "Bitmask:",  ITEM_INFO,0,0,0,0,0,0,0,0,0, get_pad_hex,0,0 },
    { "",          ITEM_HEADER },
    { "  UP   DOWN  LEFT  RIGHT", ITEM_INFO,0,0,0,0,0,0,0,0,0,0,"",0 },
    { "  L1 L2 L3  R1 R2 R3",     ITEM_INFO,0,0,0,0,0,0,0,0,0,0,"",0 },
    { "  \x18 \x19 \x1B \x1C",    ITEM_INFO,0,0,0,0,0,0,0,0,0,0,"",0 },
    { "  Options L3 R3 Touchpad", ITEM_INFO,0,0,0,0,0,0,0,0,0,0,"",0 },
    { "Back", ITEM_BACK },
};

/* ============ CONTROLLER MENU ============ */
static Item scr_controller_items[] = {
    { "DualSense", ITEM_HEADER },
    { "Lightbar",        ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_lightbar,0,0,0 },
    { "Vibration",       ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_vib,0,0,0 },
    { "Trigger Effects", ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_trig,0,0,0 },
    { "Pad State View",  ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_pad,0,0,0 },
    { "Back", ITEM_BACK },
};

/* ============ SYSTEM INFO ============ */
static char g_sysinfo[16][96];
static int  g_sysinfo_n = 0;

static void sys_refresh(void) {
    g_sysinfo_n = 0;
    char *b;
    #define NEXT() (b = g_sysinfo[g_sysinfo_n++])
    #define APP(k,v) do { NEXT(); int p=0; while(*k && p<30) b[p++]=*k++; b[p++]=':'; b[p++]=' '; \
        while(*v && p<90) b[p++]=*v++; b[p]=0; } while(0)

    APP("User ID",  ({static char t[16]; s_itoa(t, G_CTX.user_id); t;}));
    APP("Pad Handle",({static char t[16]; s_itoa(t, G_CTX.pad_h); t;}));
    APP("Audio Handle",({static char t[16]; s_itoa(t, G_CTX.audio_h); t;}));
    APP("Video Handle",({static char t[16]; s_itoa(t, G_CTX.video_h); t;}));

    {
        static char t[24];
        s_hex64(t, get_dm_size(&G_CTX));
        APP("Direct Mem", t);
    }
    {
        static char t[24];
        u64 ms = get_uptime_ms(&G_CTX);
        s_itoa(t, (int)(ms / 1000));
        int l = s_len(t); t[l++]=' '; t[l++]='s'; t[l]=0;
        APP("Uptime", t);
    }
    {
        static char t[24];
        s_itoa(t, (int)G_CTX.total_frames);
        APP("Frames", t);
    }
    {
        static char t[24];
        s_hex64(t, G_CTX.eboot_base);
        APP("EBOOT base", t);
    }
    {
        static char t[24];
        s_hex64(t, (u64)G_CTX.G);
        APP("Gadget", t);
    }
    #undef APP
    #undef NEXT
}

static const char *get_sys_line(void) { return 0; }

static Item scr_system_items[] = {
    { "System Information", ITEM_HEADER },
    { "Refresh on Enter", ITEM_INFO,0,0,0,0,0,0,0,0,0,0,"(this screen)",0 },
    { "Back", ITEM_BACK },
};

/* ============ VIDEO ============ */
static void vid_clear(u32 color) {
    if (G_CTX.fbs[0]) ui_clear(G_CTX.fbs[0], color);
    if (G_CTX.fbs[1]) ui_clear(G_CTX.fbs[1], color);
    video_flip(&G_CTX, 1);
    video_flip(&G_CTX, 1);
}
static void vid_black(Item *it) { (void)it; vid_clear(RGB(0,0,0)); }
static void vid_white(Item *it) { (void)it; vid_clear(RGB(255,255,255)); }
static void vid_red  (Item *it) { (void)it; vid_clear(RGB(255,0,0)); }
static void vid_grn  (Item *it) { (void)it; vid_clear(RGB(0,255,0)); }
static void vid_blu  (Item *it) { (void)it; vid_clear(RGB(0,0,255)); }
static void vid_gry  (Item *it) { (void)it; vid_clear(RGB(128,128,128)); }

static Item scr_video_items[] = {
    { "Video Out", ITEM_HEADER },
    { "Clear to Black", ITEM_ACTION,0,0,0, vid_black,0,0,0,0,0,0,0 },
    { "Clear to White", ITEM_ACTION,0,0,0, vid_white,0,0,0,0,0,0,0 },
    { "Clear to Red",   ITEM_ACTION,0,0,0, vid_red,  0,0,0,0,0,0,0 },
    { "Clear to Green", ITEM_ACTION,0,0,0, vid_grn,  0,0,0,0,0,0,0 },
    { "Clear to Blue",  ITEM_ACTION,0,0,0, vid_blu,  0,0,0,0,0,0,0 },
    { "Clear to Gray",  ITEM_ACTION,0,0,0, vid_gry,  0,0,0,0,0,0,0 },
    { "Back", ITEM_BACK },
};

/* ============ AUDIO ============ */
static void aud_tone_item(Item *it) { audio_tone(&G_CTX, (int)it->data, 250); }

static Item scr_audio_items[] = {
    { "Audio Out", ITEM_HEADER },
    { "Tone 220 Hz", ITEM_ACTION,0,0,0, aud_tone_item,0,0,0,0,0,0, 220 },
    { "Tone 440 Hz", ITEM_ACTION,0,0,0, aud_tone_item,0,0,0,0,0,0, 440 },
    { "Tone 880 Hz", ITEM_ACTION,0,0,0, aud_tone_item,0,0,0,0,0,0, 880 },
    { "Tone 1760 Hz",ITEM_ACTION,0,0,0, aud_tone_item,0,0,0,0,0,0, 1760 },
    { "Back", ITEM_BACK },
};

/* ============ NETWORK ============ */
static u8 g_net_buf[32];

static void net_udp_test(Item *it) {
    (void)it;
    ulog(&G_CTX, "[toolbox] UDP log test from toolbox!\n");
}

static void net_local_ip(Item *it) {
    (void)it;
    /* Create a UDP socket, connect to 8.8.8.8:80, getsockname -> local IP */
    if (!G_CTX.socket_fn || !G_CTX.getsockname_fn) return;
    s32 fd = (s32)NC(G_CTX.G, G_CTX.socket_fn, 2, 2, 0, 0,0,0,0);
    if (fd < 0) return;
    u8 sa[16]; m_set(sa, 0, 16);
    sa[0] = 16; sa[1] = 2;
    *(u16*)(sa+2) = (80 >> 8) | ((80 & 0xFF) << 8);
    *(u32*)(sa+4) = (8) | (8 << 8) | (8 << 16) | (8 << 24);
    if (G_CTX.setsockopt_fn) { /* skip */ }
    void *connect_fn = SYM(G_CTX.G, G_CTX.D, LIBKERNEL_HANDLE, "connect");
    if (connect_fn) NC(G_CTX.G, connect_fn, (u64)fd, (u64)sa, 16, 0,0,0);
    u8 out[16]; s32 outlen = 16;
    NC(G_CTX.G, G_CTX.getsockname_fn, (u64)fd, (u64)out, (u64)&outlen, 0,0,0);
    if (G_CTX.close_fn) NC(G_CTX.G, G_CTX.close_fn, (u64)fd, 0,0,0,0,0);
    u32 ip = *(u32*)(out+4);
    char b[32]; int p = 0;
    p += s_itoa(b+p, ip & 0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>8)&0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>16)&0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>24)&0xFF); b[p]=0;
    ulog(&G_CTX, "[toolbox] local IP: ");
    ulog(&G_CTX, b);
    ulog(&G_CTX, "\n");
}

static const char *get_local_ip_str(void) {
    static char b[20] = "—";
    if (!G_CTX.socket_fn || !G_CTX.getsockname_fn) return b;
    s32 fd = (s32)NC(G_CTX.G, G_CTX.socket_fn, 2, 2, 0, 0,0,0,0);
    if (fd < 0) return b;
    u8 sa[16]; m_set(sa, 0, 16);
    sa[0]=16; sa[1]=2;
    *(u16*)(sa+2) = (80 >> 8) | ((80 & 0xFF) << 8);
    *(u32*)(sa+4) = 0x08080808;
    void *connect_fn = SYM(G_CTX.G, G_CTX.D, LIBKERNEL_HANDLE, "connect");
    if (connect_fn) NC(G_CTX.G, connect_fn, (u64)fd, (u64)sa, 16, 0,0,0);
    u8 out[16]; s32 outlen = 16;
    NC(G_CTX.G, G_CTX.getsockname_fn, (u64)fd, (u64)out, (u64)&outlen, 0,0,0);
    if (G_CTX.close_fn) NC(G_CTX.G, G_CTX.close_fn, (u64)fd, 0,0,0,0,0);
    u32 ip = *(u32*)(out+4);
    int p = 0;
    p += s_itoa(b+p, ip & 0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>8)&0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>16)&0xFF); b[p++]='.';
    p += s_itoa(b+p, (ip>>24)&0xFF); b[p]=0;
    return b;
}

static Item scr_network_items[] = {
    { "Network Tools", ITEM_HEADER },
    { "Local IP:",      ITEM_INFO,0,0,0,0,0,0,0,0,0, get_local_ip_str,0,0 },
    { "UDP Log Test",   ITEM_ACTION,0,0,0, net_udp_test,0,0,0,0,0,0,0 },
    { "Back", ITEM_BACK },
};

/* ============ DEBUG ============ */
static Item scr_debug_items[] = {
    { "Debug Info", ITEM_HEADER },
    { "Back", ITEM_BACK },
};

/* ============ MAIN MENU ============ */
static Item scr_main_items[] = {
    { "LuaC0re Toolbox", ITEM_HEADER },
    { "Controller",  ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_controller,0,0,0 },
    { "System Info", ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_system,  0,0,0 },
    { "Video",       ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_video,   0,0,0 },
    { "Audio",       ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_audio,   0,0,0 },
    { "Network",     ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_network, 0,0,0 },
    { "Debug",       ITEM_SUBMENU,0,0,0,0,0,0,0, &scr_debug,   0,0,0 },
    { "",            ITEM_HEADER },
    { "Exit (R1 also)", ITEM_ACTION,0,0,0, 0,0,0,0,0,0,0,0xFFFFFFFF },
};

/* ============ SCREEN DEFINITIONS ============ */
#define CNT(a) (sizeof(a) / sizeof((a)[0]))

Screen scr_main          = { "LuaC0re Toolbox", scr_main_items,        CNT(scr_main_items),        0, 0,0,0,8 };
Screen scr_controller    = { "Controller",      scr_controller_items,  CNT(scr_controller_items),  &scr_main, 0,0,0,8 };
Screen scr_lightbar      = { "Lightbar",        scr_lightbar_items,    CNT(scr_lightbar_items),    &scr_controller, 0,0,0,12 };
Screen scr_lightbar_rgb  = { "Custom RGB",      scr_lightbar_rgb_items,CNT(scr_lightbar_rgb_items),&scr_lightbar, 0,0,0,8 };
Screen scr_vib           = { "Vibration",       scr_vib_items,         CNT(scr_vib_items),         &scr_controller, 0,0,0,10 };
Screen scr_trig          = { "Trigger Effects", scr_trig_items,        CNT(scr_trig_items),        &scr_controller, 0,0,0,8 };
Screen scr_pad           = { "Pad State",       scr_pad_items,         CNT(scr_pad_items),         &scr_controller, 0,0,0,10 };
Screen scr_system        = { "System Info",     scr_system_items,      CNT(scr_system_items),      &scr_main, 0,0,0,12 };
Screen scr_video         = { "Video",           scr_video_items,       CNT(scr_video_items),       &scr_main, 0,0,0,8 };
Screen scr_audio         = { "Audio",           scr_audio_items,       CNT(scr_audio_items),       &scr_main, 0,0,0,8 };
Screen scr_network       = { "Network",         scr_network_items,     CNT(scr_network_items),     &scr_main, 0,0,0,8 };
Screen scr_debug         = { "Debug",           scr_debug_items,       CNT(scr_debug_items),       &scr_main, 0,0,0,8 };

/* ============ MENU LOGIC ============ */
void menu_init(void) {
    g_screen = &scr_main;
    for (Screen *s[] = {&scr_main, &scr_controller, &scr_lightbar, &scr_lightbar_rgb,
                        &scr_vib, &scr_trig, &scr_pad, &scr_system, &scr_video,
                        &scr_audio, &scr_network, &scr_debug, 0}; *s; s++) {
        (*s)->cursor = 0; (*s)->scroll = 0;
    }
}

void menu_goto(Screen *s) {
    if (!s) return;
    g_screen = s;
    if (s->on_enter) s->on_enter();
    if (s == &scr_system) sys_refresh();
}

static int item_is_selectable(Item *it) {
    return it->kind != ITEM_HEADER;
}

static void move_cursor(int dir) {
    Screen *s = g_screen;
    int start = s->cursor;
    for (int n = 0; n < s->count; n++) {
        s->cursor += dir;
        if (s->cursor < 0) s->cursor = s->count - 1;
        if (s->cursor >= s->count) s->cursor = 0;
        if (item_is_selectable(&s->items[s->cursor])) break;
        if (s->cursor == start) break;
    }
    /* scroll */
    if (s->cursor < s->scroll) s->scroll = s->cursor;
    if (s->cursor >= s->scroll + s->visible) s->scroll = s->cursor - s->visible + 1;
}

void menu_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)raw; (void)c;
    Screen *s = g_screen;
    Item *it = &s->items[s->cursor];

    if (pressed & 0x10) move_cursor(-1);                          /* UP */
    if (pressed & 0x20) move_cursor( 1);                          /* DOWN */

    if (pressed & 0x40) { if (it->on_left) it->on_left(it); }     /* LEFT */
    if (pressed & 0x80) { if (it->on_right) it->on_right(it); }   /* RIGHT */

    if (pressed & 0x01) {                                         /* CROSS */
        if (it->kind == ITEM_SUBMENU) menu_goto(it->submenu);
        else if (it->kind == ITEM_BACK) menu_goto(s->parent);
        else if (it->kind == ITEM_ACTION) {
            if (it->on_confirm) it->on_confirm(it);
            else if (it->data == 0xFFFFFFFF) {                    /* Exit sentinel */
                extern void menu_request_exit(void);
                menu_request_exit();
            }
        }
    }
    if (pressed & 0x02) {                                         /* CIRCLE */
        if (s->parent) menu_goto(s->parent);
    }
}

void menu_tick(struct ctx *c) {
    /* Rainbow animation */
    if (g_lb_mode == 1) {
        static int acc = 0;
        acc++;
        if (acc >= 3) {
            acc = 0;
            g_lb_hue = (g_lb_hue + 6) & 0xFF;
            /* HSV -> RGB (S=1, V=1) */
            int h = g_lb_hue / 43;
            int f = (g_lb_hue - h * 43) * 6;
            int q = 255 - f, t = f;
            u8 r,g,b;
            switch (h) {
                case 0: r=255; g=t;   b=0;   break;
                case 1: r=q;   g=255; b=0;   break;
                case 2: r=0;   g=255; b=t;   break;
                case 3: r=0;   g=q;   b=255; break;
                case 4: r=t;   g=0;   b=255; break;
                default:r=255; g=0;   b=q;   break;
            }
            pad_set_lightbar(c, r, g, b);
        }
    }
    /* Vibration pulse */
    if (g_vib_pulse) {
        static int acc = 0;
        acc++;
        if (acc >= 8) {
            acc = 0;
            g_vib_phase ^= 1;
            u8 l = g_vib_phase ? g_vib_l : (g_vib_l / 3);
            u8 s = g_vib_phase ? g_vib_s : (g_vib_s / 3);
            pad_set_vibration(c, l, s);
        }
    }
    /* Pad state view */
    if (g_screen == &scr_pad) {
        g_pad_snapshot = pad_raw(c);
    }
    /* Update sysinfo values in-place */
    if (g_screen == &scr_system) sys_refresh();
}

/* ============ DRAWING ============ */
static const char *item_value_str(Item *it) {
    static char buf[32];
    if (it->kind == ITEM_SLIDER && it->get_info) return it->get_info();
    if (it->kind == ITEM_SLIDER) {
        if (it->value) { s_itoa(buf, *it->value); return buf; }
    }
    if (it->kind == ITEM_TOGGLE) {
        if (it->value) return *it->value ? "ON" : "OFF";
    }
    if (it->kind == ITEM_INFO) {
        if (it->get_info) return it->get_info();
        if (it->info) return it->info;
    }
    return 0;
}

void menu_draw(struct ctx *c, u32 *fb) {
    Screen *s = g_screen;
    ui_clear(fb, COL_BG);

    /* Header */
    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "LuaC0re", COL_ACCENT, 5);
    ui_str(fb, 60 + ui_str_w("LuaC0re ", 5), 22, "Toolbox", COL_TITLE, 5);
    ui_str_right(fb, SCR_W - 60, 30, s->title, COL_TEXT_DIM, 4);

    /* Path breadcrumb */
    {
        char path[200]; int p = 0;
        Screen *walk[8]; int n = 0;
        Screen *cur = s;
        while (cur && n < 8) { walk[n++] = cur; cur = cur->parent; }
        for (int i = n - 1; i >= 0; i--) {
            const char *t = walk[i]->title;
            while (*t && p < 190) path[p++] = *t++;
            if (i > 0) { path[p++] = ' '; path[p++] = '>'; path[p++] = ' '; }
        }
        path[p] = 0;
        ui_str(fb, 60, 110, path, COL_TEXT_DIM, 3);
    }

    /* Body */
    int y0 = 160, row_h = 62;
    int vis = s->visible;
    if (vis > s->count) vis = s->count;

    for (int i = 0; i < vis; i++) {
        int idx = s->scroll + i;
        if (idx >= s->count) break;
        Item *it = &s->items[idx];
        int y = y0 + i * row_h;
        int sel = (idx == s->cursor);

        if (it->kind == ITEM_HEADER) {
            ui_fill(fb, 60, y + 22, SCR_W - 120, 2, COL_HDR);
            ui_str(fb, 60, y + 4, it->label, COL_HDR, 3);
            continue;
        }

        u32 bg   = sel ? COL_SEL_BG : COL_PANEL;
        u32 fg   = sel ? 0xFFFFFFFF : COL_TEXT;
        u32 sub  = sel ? 0xFFFFE0B0 : COL_TEXT_DIM;

        ui_fill(fb, 60, y, SCR_W - 120, row_h - 8, bg);
        if (sel) {
            ui_fill(fb, 60, y, 6, row_h - 8, COL_TITLE);
        } else {
            ui_fill(fb, 60, y, 3, row_h - 8, COL_ACCENT_D);
        }

        /* Label */
        ui_str(fb, 90, y + 12, it->label, fg, 4);

        /* Right side: value / arrow */
        const char *vs = item_value_str(it);
        if (vs) {
            ui_str_right(fb, SCR_W - 100, y + 14, vs, sub, 4);
        } else if (it->kind == ITEM_SUBMENU) {
            ui_str_right(fb, SCR_W - 100, y + 12, ">", COL_ACCENT, 5);
        }
    }

    if (s->scroll > 0)
        ui_str_center(fb, y0 - 24, "^ more ^", COL_TEXT_DIM, 3);
    if (s->scroll + vis < s->count)
        ui_str_center(fb, y0 + vis * row_h + 6, "v more v", COL_TEXT_DIM, 3);

    /* Footer */
    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);

    /* raw pad buttons indicator */
    u32 raw = pad_raw(c);
    char pb[80]; int p = 0;
    const char *pre = "Pad: ";
    while (*pre) pb[p++] = *pre++;
    struct { u32 bit; const char *name; } btns[] = {
        {0x0001,"\x18"},{0x0002,"\x19"},{0x0004,"\x1B"},{0x0008,"\x1C"},
        {0x0010,"UP"},{0x0020,"DN"},{0x0040,"LT"},{0x0080,"RT"},
        {0x0100,"L2"},{0x0200,"R2"},{0x0400,"L1"},{0x0800,"R1"},
        {0,0}
    };
    for (int i = 0; btns[i].bit; i++) {
        if (!(raw & btns[i].bit)) continue;
        if (p > 60) break;
        pb[p++] = '['; 
        const char *n = btns[i].name; while (*n && p < 76) pb[p++] = *n++;
        pb[p++] = ']'; pb[p++] = ' ';
    }
    pb[p] = 0;
    if (p < 8) { s_cpy(pb + 5, "(none)"); pb[11] = 0; }
    ui_str(fb, 60, SCR_H - 44, pb, COL_TEXT, 3);

    /* Controls hint */
    ui_str_right(fb, SCR_W - 60, SCR_H - 44,
                 "D-Pad: Move   X: Select   O: Back   R1: Exit",
                 COL_TEXT_DIM, 3);
}
