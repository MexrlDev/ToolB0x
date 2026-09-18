#include "screens.h"
#include "ui.h"

/* ---- palette ---- */
#define COL_BG       RGB(8,8,14)
#define COL_PANEL    RGB(20,20,28)
#define COL_BORDER   RGB(70,70,95)
#define COL_TEXT     RGB(230,230,240)
#define COL_TEXT_DIM RGB(120,120,140)
#define COL_ACCENT   RGB(0,180,255)
#define COL_ACCENT_D RGB(0,90,130)
#define COL_SEL_BG   RGB(0,110,200)
#define COL_TITLE    RGB(255,200,80)
#define COL_HDR      RGB(255,140,0)

Screen *g_screen;

/* ============ shared state ============ */
static int  g_rgb[3]  = {255, 100, 0};
static int  g_vib_l   = 128, g_vib_s = 128;
static int  g_lb_mode = 0;          /* 0=static 1=rainbow */
static int  g_lb_hue  = 0;
static int  g_vib_pulse = 0;
static int  g_vib_phase = 0;
static u32  g_pad_snapshot = 0;

static char g_local_ip[20] = "unknown";
static int  g_local_ip_valid = 0;

/* ============ helpers ============ */
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
    { .label = "Static Colors", .kind = ITEM_HEADER },
    { .label = "Off",    .kind = ITEM_ACTION, .on_confirm = act_lb_off },
    { .label = "Red",    .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0xFF0000 },
    { .label = "Green",  .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0x00FF00 },
    { .label = "Blue",   .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0x0000FF },
    { .label = "White",  .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0xFFFFFF },
    { .label = "Orange", .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0xFF8000 },
    { .label = "Purple", .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0xA020FF },
    { .label = "Cyan",   .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0x00FFFF },
    { .label = "Pink",   .kind = ITEM_ACTION, .on_confirm = act_lb_preset, .data = 0xFF40A0 },
    { .label = "Animated", .kind = ITEM_HEADER },
    { .label = "Rainbow Pulse",  .kind = ITEM_ACTION, .on_confirm = act_lb_rb },
    { .label = "Stop Animation", .kind = ITEM_ACTION, .on_confirm = act_lb_stop },
    { .label = "Custom RGB...",  .kind = ITEM_SUBMENU, .submenu = &scr_lightbar_rgb },
    { .label = "Back",           .kind = ITEM_BACK },
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
    { .label = "Left/Right to adjust RGB", .kind = ITEM_HEADER },
    { .label = "Red",   .kind = ITEM_SLIDER, .min=0,.max=255,.step=16,
      .on_left = rgb_dec_r, .on_right = rgb_inc_r, .get_info = get_rgb_r },
    { .label = "Green", .kind = ITEM_SLIDER, .min=0,.max=255,.step=16,
      .on_left = rgb_dec_g, .on_right = rgb_inc_g, .get_info = get_rgb_g },
    { .label = "Blue",  .kind = ITEM_SLIDER, .min=0,.max=255,.step=16,
      .on_left = rgb_dec_b, .on_right = rgb_inc_b, .get_info = get_rgb_b },
    { .label = "Back",  .kind = ITEM_BACK },
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
    { .label = "Presets", .kind = ITEM_HEADER },
    { .label = "Off",         .kind = ITEM_ACTION, .on_confirm = vib_stop },
    { .label = "Weak",        .kind = ITEM_ACTION, .on_confirm = vib_preset, .data = 0x5050 },
    { .label = "Medium",      .kind = ITEM_ACTION, .on_confirm = vib_preset, .data = 0x8080 },
    { .label = "Strong",      .kind = ITEM_ACTION, .on_confirm = vib_preset, .data = 0xFFFF },
    { .label = "Left Motor",  .kind = ITEM_ACTION, .on_confirm = vib_preset, .data = 0xFF00 },
    { .label = "Right Motor", .kind = ITEM_ACTION, .on_confirm = vib_preset, .data = 0x00FF },
    { .label = "Animated",    .kind = ITEM_HEADER },
    { .label = "Pulse On",    .kind = ITEM_ACTION, .on_confirm = vib_start_pulse },
    { .label = "Stop",        .kind = ITEM_ACTION, .on_confirm = vib_stop },
    { .label = "Back",        .kind = ITEM_BACK },
};

/* ============ TRIGGERS ============ */
static void trig_preset(Item *it) {
    int mode = (int)((it->data >> 24) & 0xFF);
    int p1   = (int)((it->data >> 16) & 0xFF);
    int p2   = (int)((it->data >> 8)  & 0xFF);
    int p3   = (int)( it->data        & 0xFF);
    pad_set_trigger(&G_CTX, mode, 2, p1, p2, p3);
}
static void trig_off(Item *it) { (void)it; pad_set_trigger(&G_CTX, 0, 2, 0, 0, 0); }

static Item scr_trig_items[] = {
    { .label = "Trigger Effects", .kind = ITEM_HEADER },
    { .label = "Off",             .kind = ITEM_ACTION, .on_confirm = trig_off },
    { .label = "Feedback Weak",   .kind = ITEM_ACTION, .on_confirm = trig_preset, .data = 0x010201 },
    { .label = "Feedback Medium", .kind = ITEM_ACTION, .on_confirm = trig_preset, .data = 0x010405 },
    { .label = "Feedback Strong", .kind = ITEM_ACTION, .on_confirm = trig_preset, .data = 0x010608 },
    { .label = "Weapon",          .kind = ITEM_ACTION, .on_confirm = trig_preset, .data = 0x020508 },
    { .label = "Vibration",       .kind = ITEM_ACTION, .on_confirm = trig_preset, .data = 0x030305 },
    { .label = "Back",            .kind = ITEM_BACK },
};

/* ============ PAD VIEW ============ */
static const char *get_pad_hex(void) {
    static char b[12] = "0x00000000";
    const char *h = "0123456789ABCDEF";
    u32 v = g_pad_snapshot;
    for (int i = 0; i < 8; i++) b[2+i] = h[(v >> ((7-i)*4)) & 0xF];
    return b;
}

/* Per-button live indicator strings.  Each getter has its own rotating
 * static buffer so the menu system can hold several references across
 * a single draw without them stomping each other. */
static char *btn_str(u32 bit, const char *name) {
    static char bufs[20][24];
    static int  next = 0;
    int idx = (next++) % 20;
    char *b = bufs[idx];
    int p = 0;
    int pressed = (g_pad_snapshot & bit) != 0;
    const char *tag = pressed ? "[X] " : "[ ] ";
    while (*tag && p < 22) b[p++] = *tag++;
    while (*name && p < 22) b[p++] = *name++;
    b[p] = 0;
    return b;
}

static const char *get_pd_cross(void)    { return btn_str(DS_CROSS,    "Cross");      }
static const char *get_pd_circle(void)   { return btn_str(DS_CIRCLE,   "Circle");     }
static const char *get_pd_triangle(void) { return btn_str(DS_TRIANGLE, "Triangle");   }
static const char *get_pd_square(void)   { return btn_str(DS_SQUARE,   "Square");     }
static const char *get_pd_up(void)       { return btn_str(DS_UP,       "Up");         }
static const char *get_pd_down(void)     { return btn_str(DS_DOWN,     "Down");       }
static const char *get_pd_left(void)     { return btn_str(DS_LEFT,     "Left");       }
static const char *get_pd_right(void)    { return btn_str(DS_RIGHT,    "Right");      }
static const char *get_pd_l1(void)       { return btn_str(DS_L1,       "L1");         }
static const char *get_pd_r1(void)       { return btn_str(DS_R1,       "R1");         }
static const char *get_pd_l2(void)       { return btn_str(DS_L2,       "L2");         }
static const char *get_pd_r2(void)       { return btn_str(DS_R2,       "R2");         }
static const char *get_pd_l3(void)       { return btn_str(DS_L3,       "L3");         }
static const char *get_pd_r3(void)       { return btn_str(DS_R3,       "R3");         }
static const char *get_pd_options(void)  { return btn_str(DS_OPTIONS,  "Options");    }
static const char *get_pd_share(void)    { return btn_str(DS_SHARE,    "Share/Create");}
static const char *get_pd_touch(void)    { return btn_str(DS_TOUCHPAD, "Touchpad");   }

static Item scr_pad_items[] = {
    { .label = "Raw Pad State",  .kind = ITEM_HEADER },
    { .label = "Bitmask:",       .kind = ITEM_INFO, .get_info = get_pad_hex },

    { .label = "Face Buttons",   .kind = ITEM_HEADER },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_cross    },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_circle   },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_triangle },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_square   },

    { .label = "D-Pad",          .kind = ITEM_HEADER },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_up       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_down     },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_left     },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_right    },

    { .label = "Shoulders",      .kind = ITEM_HEADER },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_l1       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_l2       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_r1       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_r2       },

    { .label = "Sticks / Misc",  .kind = ITEM_HEADER },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_l3       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_r3       },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_options  },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_share    },
    { .label = "",  .kind = ITEM_INFO, .get_info = get_pd_touch    },

    { .label = "Back",           .kind = ITEM_BACK },
};

/* ============ CONTROLLER ============ */
static Item scr_controller_items[] = {
    { .label = "DualSense",       .kind = ITEM_HEADER },
    { .label = "Lightbar",        .kind = ITEM_SUBMENU, .submenu = &scr_lightbar },
    { .label = "Vibration",       .kind = ITEM_SUBMENU, .submenu = &scr_vib },
    { .label = "Trigger Effects", .kind = ITEM_SUBMENU, .submenu = &scr_trig },
    { .label = "Pad State View",  .kind = ITEM_SUBMENU, .submenu = &scr_pad },
    { .label = "Back",            .kind = ITEM_BACK },
};

/* ============ SYSTEM INFO ============ */
#define SYSINFO_MAX 16
static char g_sysinfo[SYSINFO_MAX][96];
static int  g_sysinfo_n = 0;

static void sys_add_line(const char *key, const char *val) {
    if (g_sysinfo_n >= SYSINFO_MAX) return;
    char *b = g_sysinfo[g_sysinfo_n++];
    int p = 0;
    while (*key && p < 30) b[p++] = *key++;
    b[p++] = ':'; b[p++] = ' ';
    while (*val && p < 94) b[p++] = *val++;
    b[p] = 0;
}

static void sys_refresh(void) {
    g_sysinfo_n = 0;
    char t[24];

    s_itoa(t, G_CTX.user_id);        sys_add_line("User ID", t);
    s_itoa(t, G_CTX.pad_h);          sys_add_line("Pad Handle", t);
    s_itoa(t, G_CTX.audio_h);        sys_add_line("Audio Handle", t);
    s_itoa(t, G_CTX.video_h);        sys_add_line("Video Handle", t);

    s_hex64(t, get_dm_size(&G_CTX)); sys_add_line("Direct Mem", t);

    s_itoa(t, (int)(get_uptime_ms(&G_CTX) / 1000)); sys_add_line("Uptime (s)", t);
    s_itoa(t, (int)G_CTX.total_frames);             sys_add_line("Frames", t);

    s_hex64(t, G_CTX.eboot_base);    sys_add_line("EBOOT base", t);
    s_hex64(t, (u64)G_CTX.G);        sys_add_line("Gadget", t);
}

static const char *get_sys_line_0(void) { return g_sysinfo_n > 0 ? g_sysinfo[0] : ""; }
static const char *get_sys_line_1(void) { return g_sysinfo_n > 1 ? g_sysinfo[1] : ""; }
static const char *get_sys_line_2(void) { return g_sysinfo_n > 2 ? g_sysinfo[2] : ""; }
static const char *get_sys_line_3(void) { return g_sysinfo_n > 3 ? g_sysinfo[3] : ""; }
static const char *get_sys_line_4(void) { return g_sysinfo_n > 4 ? g_sysinfo[4] : ""; }
static const char *get_sys_line_5(void) { return g_sysinfo_n > 5 ? g_sysinfo[5] : ""; }
static const char *get_sys_line_6(void) { return g_sysinfo_n > 6 ? g_sysinfo[6] : ""; }
static const char *get_sys_line_7(void) { return g_sysinfo_n > 7 ? g_sysinfo[7] : ""; }
static const char *get_sys_line_8(void) { return g_sysinfo_n > 8 ? g_sysinfo[8] : ""; }
static const char *get_sys_line_9(void) { return g_sysinfo_n > 9 ? g_sysinfo[9] : ""; }

static Item scr_system_items[] = {
    { .label = "System Information", .kind = ITEM_HEADER },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_0 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_1 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_2 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_3 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_4 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_5 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_6 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_7 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_8 },
    { .label = "", .kind = ITEM_INFO, .get_info = get_sys_line_9 },
    { .label = "Back", .kind = ITEM_BACK },
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
    { .label = "Video Out", .kind = ITEM_HEADER },
    { .label = "Clear to Black", .kind = ITEM_ACTION, .on_confirm = vid_black },
    { .label = "Clear to White", .kind = ITEM_ACTION, .on_confirm = vid_white },
    { .label = "Clear to Red",   .kind = ITEM_ACTION, .on_confirm = vid_red   },
    { .label = "Clear to Green", .kind = ITEM_ACTION, .on_confirm = vid_grn   },
    { .label = "Clear to Blue",  .kind = ITEM_ACTION, .on_confirm = vid_blu   },
    { .label = "Clear to Gray",  .kind = ITEM_ACTION, .on_confirm = vid_gry   },
    { .label = "Back", .kind = ITEM_BACK },
};

/* ============ AUDIO ============ */
static void aud_tone_item(Item *it) { audio_tone(&G_CTX, (int)it->data, 250); }

static Item scr_audio_items[] = {
    { .label = "Audio Out", .kind = ITEM_HEADER },
    { .label = "Tone 220 Hz",  .kind = ITEM_ACTION, .on_confirm = aud_tone_item, .data = 220  },
    { .label = "Tone 440 Hz",  .kind = ITEM_ACTION, .on_confirm = aud_tone_item, .data = 440  },
    { .label = "Tone 880 Hz",  .kind = ITEM_ACTION, .on_confirm = aud_tone_item, .data = 880  },
    { .label = "Tone 1760 Hz", .kind = ITEM_ACTION, .on_confirm = aud_tone_item, .data = 1760 },
    { .label = "Back", .kind = ITEM_BACK },
};

/* ============ NETWORK ============ */
static void net_udp_test(Item *it) {
    (void)it;
    ulog(&G_CTX, "[toolbox] UDP log test from toolbox!\n");
}

static void refresh_local_ip(void) {
    if (g_local_ip_valid) return;
    g_local_ip_valid = 1;
    s_cpy(g_local_ip, "unavailable");

    if (!G_CTX.socket_fn || !G_CTX.getsockname_fn) return;

    s32 fd = (s32)NC(G_CTX.G, G_CTX.socket_fn, 2, 2, 0, 0, 0, 0);   /* AF_INET, SOCK_DGRAM */
    if (fd < 0) return;

    u8 sa[16]; m_set(sa, 0, 16);
    sa[0] = 16; sa[1] = 2;
    *(u16*)(sa + 2) = (80 >> 8) | ((80 & 0xFF) << 8);         /* port 80, big-endian */
    *(u32*)(sa + 4) = 0x08080808;                              /* 8.8.8.8 */

    void *connect_fn = SYM(G_CTX.G, G_CTX.D, LIBKERNEL_HANDLE, "connect");
    if (connect_fn) NC(G_CTX.G, connect_fn, (u64)fd, (u64)sa, 16, 0, 0, 0);

    u8 out[16]; s32 outlen = 16;
    NC(G_CTX.G, G_CTX.getsockname_fn, (u64)fd, (u64)out, (u64)&outlen, 0, 0, 0);
    if (G_CTX.close_fn) NC(G_CTX.G, G_CTX.close_fn, (u64)fd, 0, 0, 0, 0, 0);

    u32 ip = *(u32*)(out + 4);
    int p = 0;
    p += s_itoa(g_local_ip + p, ip & 0xFF);         g_local_ip[p++] = '.';
    p += s_itoa(g_local_ip + p, (ip >> 8) & 0xFF);  g_local_ip[p++] = '.';
    p += s_itoa(g_local_ip + p, (ip >> 16) & 0xFF); g_local_ip[p++] = '.';
    p += s_itoa(g_local_ip + p, (ip >> 24) & 0xFF); g_local_ip[p]   = 0;
}

static const char *get_local_ip_str(void) {
    refresh_local_ip();
    return g_local_ip;
}

static Item scr_network_items[] = {
    { .label = "Network Tools",  .kind = ITEM_HEADER },
    { .label = "Local IP:",      .kind = ITEM_INFO, .get_info = get_local_ip_str },
    { .label = "UDP Log Test",   .kind = ITEM_ACTION, .on_confirm = net_udp_test },
    { .label = "Back",           .kind = ITEM_BACK },
};

/* ============ DEBUG ============ */
static Item scr_debug_items[] = {
    { .label = "Debug Info", .kind = ITEM_HEADER },
    { .label = "Nothing here yet — reserved", .kind = ITEM_INFO, .info = "More tools coming soon." },
    { .label = "Back", .kind = ITEM_BACK },
};

/* ============ MAIN ============ */
static void act_exit(Item *it) { (void)it; menu_request_exit(); }

static Item scr_main_items[] = {
    { .label = "LuaC0re Toolbox", .kind = ITEM_HEADER },
    { .label = "Controller",  .kind = ITEM_SUBMENU, .submenu = &scr_controller },
    { .label = "System Info", .kind = ITEM_SUBMENU, .submenu = &scr_system   },
    { .label = "Video",       .kind = ITEM_SUBMENU, .submenu = &scr_video    },
    { .label = "Audio",       .kind = ITEM_SUBMENU, .submenu = &scr_audio    },
    { .label = "Network",     .kind = ITEM_SUBMENU, .submenu = &scr_network  },
    { .label = "Debug",       .kind = ITEM_SUBMENU, .submenu = &scr_debug    },
    { .label = "",            .kind = ITEM_HEADER },
    { .label = "Exit (R1 also)", .kind = ITEM_ACTION, .on_confirm = act_exit },
};

#define CNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

Screen scr_main         = { .title="LuaC0re Toolbox", .items=scr_main_items,         .count=CNT(scr_main_items),         .visible=8,  .parent=0 };
Screen scr_controller   = { .title="Controller",      .items=scr_controller_items,   .count=CNT(scr_controller_items),   .visible=8,  .parent=&scr_main };
Screen scr_lightbar     = { .title="Lightbar",        .items=scr_lightbar_items,     .count=CNT(scr_lightbar_items),     .visible=12, .parent=&scr_controller };
Screen scr_lightbar_rgb = { .title="Custom RGB",      .items=scr_lightbar_rgb_items, .count=CNT(scr_lightbar_rgb_items), .visible=8,  .parent=&scr_lightbar };
Screen scr_vib          = { .title="Vibration",       .items=scr_vib_items,          .count=CNT(scr_vib_items),          .visible=10, .parent=&scr_controller };
Screen scr_trig         = { .title="Trigger Effects", .items=scr_trig_items,         .count=CNT(scr_trig_items),         .visible=8,  .parent=&scr_controller };
Screen scr_pad          = { .title="Pad State",       .items=scr_pad_items,          .count=CNT(scr_pad_items),          .visible=13, .parent=&scr_controller };
Screen scr_system       = { .title="System Info",     .items=scr_system_items,       .count=CNT(scr_system_items),       .visible=12, .parent=&scr_main };
Screen scr_video        = { .title="Video",           .items=scr_video_items,        .count=CNT(scr_video_items),        .visible=8,  .parent=&scr_main };
Screen scr_audio        = { .title="Audio",           .items=scr_audio_items,        .count=CNT(scr_audio_items),        .visible=8,  .parent=&scr_main };
Screen scr_network      = { .title="Network",         .items=scr_network_items,      .count=CNT(scr_network_items),      .visible=8,  .parent=&scr_main };
Screen scr_debug        = { .title="Debug",           .items=scr_debug_items,        .count=CNT(scr_debug_items),        .visible=8,  .parent=&scr_main };

/* ============ MENU LOGIC ============ */
void menu_init(void) {
    g_screen = &scr_main;
    Screen *all[] = {
        &scr_main, &scr_controller, &scr_lightbar, &scr_lightbar_rgb,
        &scr_vib, &scr_trig, &scr_pad, &scr_system, &scr_video,
        &scr_audio, &scr_network, &scr_debug
    };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        all[i]->cursor = 0;
        all[i]->scroll = 0;
    }
}

void menu_goto(Screen *s) {
    if (!s) return;
    g_screen = s;
    if (s == &scr_system) sys_refresh();
}

static int item_is_selectable(Item *it) { return it->kind != ITEM_HEADER; }

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
    if (s->cursor < s->scroll) s->scroll = s->cursor;
    if (s->cursor >= s->scroll + s->visible) s->scroll = s->cursor - s->visible + 1;
}

void menu_input(struct ctx *c, u32 raw, u32 pressed) {
    (void)raw; (void)c;
    Screen *s = g_screen;
    Item *it = &s->items[s->cursor];

    if (pressed & DS_UP)    move_cursor(-1);
    if (pressed & DS_DOWN)  move_cursor( 1);

    if (pressed & DS_LEFT)  { if (it->on_left)  it->on_left(it);  }
    if (pressed & DS_RIGHT) { if (it->on_right) it->on_right(it); }

    if (pressed & DS_CROSS) {
        if (it->kind == ITEM_SUBMENU)     menu_goto(it->submenu);
        else if (it->kind == ITEM_BACK)   menu_goto(s->parent);
        else if (it->kind == ITEM_ACTION) {
            if (it->on_confirm) it->on_confirm(it);
        }
    }
    if (pressed & DS_CIRCLE) {
        if (s->parent) menu_goto(s->parent);
    }
    if (pressed & DS_OPTIONS) {
        if (it->kind == ITEM_SUBMENU) menu_goto(it->submenu);
        else if (it->kind == ITEM_ACTION && it->on_confirm) it->on_confirm(it);
    }
}

void menu_tick(struct ctx *c) {
    if (g_lb_mode == 1) {
        static int acc = 0;
        acc++;
        if (acc >= 3) {
            acc = 0;
            g_lb_hue = (g_lb_hue + 6) & 0xFF;
            int h = g_lb_hue / 43;
            int f = (g_lb_hue - h * 43) * 6;
            int q = 255 - f, t = f;
            u8 r, g, b;
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
    if (g_screen == &scr_pad) g_pad_snapshot = pad_raw(c);
    if (g_screen == &scr_system) sys_refresh();
}

/* ============ DRAWING ============ */
static const char *item_value_str(Item *it) {
    static char buf[32];
    if (it->kind == ITEM_SLIDER) {
        if (it->get_info) return it->get_info();
        if (it->value) { s_itoa(buf, *it->value); return buf; }
    }
    if (it->kind == ITEM_TOGGLE) {
        if (it->value) return *it->value ? "ON" : "OFF";
    }
    if (it->kind == ITEM_INFO) {
        if (it->get_info) return it->get_info();
        if (it->info)     return it->info;
    }
    return 0;
}

void menu_draw(struct ctx *c, u32 *fb) {
    Screen *s = g_screen;
    ui_clear(fb, COL_BG);

    /* header */
    ui_fill(fb, 0, 0, SCR_W, 90, COL_PANEL);
    ui_fill(fb, 0, 90, SCR_W, 3, COL_ACCENT);
    ui_str(fb, 60, 22, "LuaC0re", COL_ACCENT, 5);
    ui_str(fb, 60 + ui_str_w("LuaC0re ", 5), 22, "Toolbox", COL_TITLE, 5);
    ui_str_right(fb, SCR_W - 60, 30, s->title, COL_TEXT_DIM, 4);

    /* breadcrumb */
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

    /* body */
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

        u32 bg  = sel ? COL_SEL_BG : COL_PANEL;
        u32 fg  = sel ? 0xFFFFFFFF : COL_TEXT;
        u32 sub = sel ? 0xFFFFE0B0 : COL_TEXT_DIM;

        ui_fill(fb, 60, y, SCR_W - 120, row_h - 8, bg);
        ui_fill(fb, 60, y, sel ? 6 : 3, row_h - 8, sel ? COL_TITLE : COL_ACCENT_D);

        ui_str(fb, 90, y + 12, it->label, fg, 4);

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

    /* footer */
    ui_fill(fb, 0, SCR_H - 70, SCR_W, 70, COL_PANEL);
    ui_fill(fb, 0, SCR_H - 73, SCR_W, 3, COL_ACCENT);

    u32 raw = pad_raw(c);
    char pb[80]; int p = 0;
    const char *pre = "Pad: ";
    while (*pre && p < 10) pb[p++] = *pre++;
    struct { u32 bit; const char *name; } btns[] = {
        {DS_CROSS,"X"},{DS_CIRCLE,"O"},{DS_TRIANGLE,"/\\"},{DS_SQUARE,"[]"},
        {DS_UP,"UP"},{DS_DOWN,"DN"},{DS_LEFT,"LT"},{DS_RIGHT,"RT"},
        {DS_L1,"L1"},{DS_R1,"R1"},{DS_L2,"L2"},{DS_R2,"R2"},
        {0,0}
    };
    for (int i = 0; btns[i].bit; i++) {
        if (!(raw & btns[i].bit)) continue;
        if (p > 66) break;
        pb[p++] = '[';
        const char *n = btns[i].name;
        while (*n && p < 74) pb[p++] = *n++;
        pb[p++] = ']'; pb[p++] = ' ';
    }
    if (p <= 5) { s_cpy(pb + 5, "(none)"); p = 11; }
    pb[p] = 0;
    ui_str(fb, 60, SCR_H - 44, pb, COL_TEXT, 3);

    ui_str_right(fb, SCR_W - 60, SCR_H - 44,
                 "D-Pad: Move   X: Select   O: Back   R1: Exit",
                 COL_TEXT_DIM, 3);
}
