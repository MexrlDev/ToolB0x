#include "app.h"
#include "ui.h"

struct ctx G_CTX;

/* ============================================================
 * Init
 * ============================================================ */
void ctx_init(struct ctx *c, u64 eboot_base, u64 dlsym_addr, struct ext_args *ext) {
    m_set(c, 0, sizeof(*c));
    c->eboot_base = eboot_base;
    c->G = (void *)(eboot_base + GADGET_OFFSET);
    c->D = (void *)dlsym_addr;
    c->video_h = -1;
    c->audio_h = -1;
    c->pad_h   = -1;
    c->ext     = ext;
    c->log_fd  = ext->log_fd;
    for (int i = 0; i < 16; i++) c->log_sa[i] = ext->log_addr[i];

    void *G = c->G, *D = c->D;
    c->usleep    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelUsleep");
    c->cancel    = SYM(G, D, LIBKERNEL_HANDLE, "scePthreadCancel");
    c->load_mod  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLoadStartModule");
    c->alloc_dm  = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelAllocateDirectMemory");
    c->map_dm    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelMapDirectMemory");
    c->dm_size   = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelGetDirectMemorySize");
    c->create_eq = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelCreateEqueue");
    c->wait_eq   = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelWaitEqueue");
    c->delete_eq = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelDeleteEqueue");
    c->mmap_fn   = SYM(G, D, LIBKERNEL_HANDLE, "mmap");
    c->munmap    = SYM(G, D, LIBKERNEL_HANDLE, "munmap");
    c->kopen     = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelOpen");
    c->kread     = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelRead");
    c->kwrite    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelWrite");
    c->kclose    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelClose");
    c->klseek    = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelLseek");
    c->clock_gettime = SYM(G, D, LIBKERNEL_HANDLE, "clock_gettime");
    c->getpid    = SYM(G, D, LIBKERNEL_HANDLE, "getpid");
    c->socket_fn = SYM(G, D, LIBKERNEL_HANDLE, "socket");
    c->bind_fn   = SYM(G, D, LIBKERNEL_HANDLE, "bind");
    c->listen_fn = SYM(G, D, LIBKERNEL_HANDLE, "listen");
    c->accept_fn = SYM(G, D, LIBKERNEL_HANDLE, "accept");
    c->send_fn   = SYM(G, D, LIBKERNEL_HANDLE, "send");
    c->recv_fn   = SYM(G, D, LIBKERNEL_HANDLE, "recv");
    c->sendto_fn = SYM(G, D, LIBKERNEL_HANDLE, "sendto");
    c->recvfrom_fn = SYM(G, D, LIBKERNEL_HANDLE, "recvfrom");
    c->close_fn  = SYM(G, D, LIBKERNEL_HANDLE, "close");
    c->setsockopt_fn = SYM(G, D, LIBKERNEL_HANDLE, "setsockopt");
    c->poll_fn   = SYM(G, D, LIBKERNEL_HANDLE, "poll");
    c->getsockname_fn = SYM(G, D, LIBKERNEL_HANDLE, "getsockname");

    c->module_info_from_addr = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelGetModuleInfoFromAddr");
    c->sys_sw_version        = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelGetSystemSwVersion");
    c->virtual_query         = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelVirtualQuery");
    c->mprotect              = SYM(G, D, LIBKERNEL_HANDLE, "sceKernelMprotect");

    s32 ime = (s32)NC(G, c->load_mod, (u64)"libSceImeDialog.sprx", 0,0,0,0,0);
    if (ime > 0) {
        c->ime_init       = SYM(G, D, ime, "sceImeDialogInit");
        c->ime_get_status = SYM(G, D, ime, "sceImeDialogGetStatus");
        c->ime_get_result = SYM(G, D, ime, "sceImeDialogGetResult");
        c->ime_term       = SYM(G, D, ime, "sceImeDialogTerm");
    }

    dbg_init(c);
}

void ulog(struct ctx *c, const char *msg) {
    if (c->log_fd < 0 || !c->sendto_fn) return;
    NC(c->G, c->sendto_fn, (u64)c->log_fd, (u64)msg, (u64)s_len(msg),
       0, (u64)c->log_sa, 16);
}

void ulog_num(struct ctx *c, const char *prefix, u64 v) {
    char b[128]; int p = 0;
    while (*prefix && p < 90) b[p++] = *prefix++;
    p += s_hex64(b + p, v);
    b[p++] = '\n'; b[p] = 0;
    ulog(c, b);
}

void dbg_init(struct ctx *c) {
    m_set(&c->dbg, 0, sizeof(c->dbg));
    c->dbg.start_ms    = get_uptime_ms(c);
    c->dbg.fps_last_ms = c->dbg.start_ms;
    c->dbg.frame_time_min = 0xFFFFFFFF;
}

void dbg_reset(struct ctx *c) { dbg_init(c); }

void dbg_record_press(struct ctx *c, u32 mask) {
    u64 now = get_uptime_ms(c);
    c->dbg.pad_presses++;
    c->dbg.last_press_mask   = mask;
    c->dbg.last_press_ago_ms = 0;

    int h = c->dbg.press_hist_head;
    c->dbg.press_hist[h].mask = mask;
    c->dbg.press_hist[h].ms   = (u32)(now - c->dbg.start_ms);
    c->dbg.press_hist_head    = (h + 1) & 15;
    if (c->dbg.press_hist_count < 16) c->dbg.press_hist_count++;

    for (int i = 0; i < 16; i++)
        if (c->dbg.flash[i].bit == mask) { c->dbg.flash[i].until_ms = now + 500; return; }
    for (int i = 0; i < 16; i++)
        if (c->dbg.flash[i].bit == 0) {
            c->dbg.flash[i].bit = mask;
            c->dbg.flash[i].until_ms = now + 500;
            return;
        }
    c->dbg.flash[0].bit = mask;
    c->dbg.flash[0].until_ms = now + 500;
}

int ctx_video_up(struct ctx *c, u64 eboot_base) {
    void *G = c->G, *D = c->D;
    if (!c->usleep || !c->load_mod || !c->alloc_dm || !c->map_dm) return -1;
    if (c->cancel) {
        u64 gs = *(u64 *)(eboot_base + EBOOT_GS_THREAD);
        if (gs) NC(G, c->cancel, gs, 0,0,0,0,0);
    }
    NC(G, c->usleep, 200000, 0,0,0,0,0);

    s32 vid = (s32)NC(G, c->load_mod, (u64)"libSceVideoOut.sprx",0,0,0,0,0);
    if (vid < 0) return -2;
    c->vid_open  = SYM(G, D, vid, "sceVideoOutOpen");
    c->vid_close = SYM(G, D, vid, "sceVideoOutClose");
    c->vid_reg   = SYM(G, D, vid, "sceVideoOutRegisterBuffers");
    c->vid_flip  = SYM(G, D, vid, "sceVideoOutSubmitFlip");
    c->vid_rate  = SYM(G, D, vid, "sceVideoOutSetFlipRate");
    c->vid_evt   = SYM(G, D, vid, "sceVideoOutAddFlipEvent");
    if (!c->vid_open || !c->vid_close || !c->vid_reg || !c->vid_flip) return -3;

    s32 emu = *(s32 *)(eboot_base + EBOOT_VIDOUT);
    if (emu >= 0) NC(G, c->vid_close, (u64)emu, 0,0,0,0,0);
    NC(G, c->usleep, 80000, 0,0,0,0,0);

    c->video_h = (s32)NC(G, c->vid_open, 0xFF, 0, 0, 0, 0, 0);
    if (c->video_h < 0) return -4;

    if (c->create_eq) NC(G, c->create_eq, (u64)&c->eq, (u64)"tbq",0,0,0,0);
    if (c->vid_evt && c->eq) NC(G, c->vid_evt, c->eq, (u64)c->video_h,0,0,0,0);

    u64 total = c->dm_size ? NC(G, c->dm_size,0,0,0,0,0,0) : 0x300000000ULL;
    u64 phys = 0;
    NC(G, c->alloc_dm, 0, total, FB_TOTAL, 0x200000, 3, (u64)&phys);
    c->vmem = 0;
    NC(G, c->map_dm, (u64)&c->vmem, FB_TOTAL, 0x33, 0, phys, 0x200000);
    if (!c->vmem) return -5;

    c->fbs[0] = (u32*)c->vmem;
    c->fbs[1] = (u32*)((u8*)c->vmem + FB_ALIGNED);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        c->fbs[0][i] = RGB(8,8,12);
        c->fbs[1][i] = RGB(8,8,12);
    }

    u8 attr[64]; m_set(attr, 0, 64);
    *(u32*)(attr+0)  = 0x80000000;
    *(u32*)(attr+4)  = 1;
    *(u32*)(attr+12) = SCR_W;
    *(u32*)(attr+16) = SCR_H;
    *(u32*)(attr+20) = SCR_W;

    if (NC(G, c->vid_reg, (u64)c->video_h, 0, (u64)c->fbs, 2, (u64)attr, 0) != 0)
        return -6;
    if (c->vid_rate) NC(G, c->vid_rate, (u64)c->video_h, 0,0,0,0,0);
    c->active = 0;
    return 0;
}

void ctx_audio_up(struct ctx *c) {
    void *G = c->G, *D = c->D;
    s32 aud = (s32)NC(G, c->load_mod, (u64)"libSceAudioOut.sprx",0,0,0,0,0);
    if (aud < 0) { ulog(c, "[toolbox] libSceAudioOut load FAILED\n"); return; }
    c->aud_open  = SYM(G, D, aud, "sceAudioOutOpen");
    c->aud_out   = SYM(G, D, aud, "sceAudioOutOutput");
    c->aud_close = SYM(G, D, aud, "sceAudioOutClose");
    if (c->aud_close)
        for (int h = 0; h < 8; h++) NC(G, c->aud_close, (u64)h, 0,0,0,0,0);
    if (c->aud_open)
        c->audio_h = (s32)NC(G, c->aud_open, 0xFF, 0, 0,
                             SAMPLES_PER_BUF, SAMPLE_RATE, AUDIO_S16_STEREO);
    ulog_num(c, "[toolbox] audio_h=", (u64)(u32)c->audio_h);
}

void ctx_pad_up(struct ctx *c) {
    void *G = c->G, *D = c->D;
    s32 pad = (s32)NC(G, c->load_mod, (u64)"libScePad.sprx", 0,0,0,0,0);
    if (pad < 0) { ulog(c, "[toolbox] libScePad load FAILED\n"); return; }

    u32 real_user_id = 0;
    s32 usr = (s32)NC(G, c->load_mod, (u64)"libSceUserService.sprx", 0,0,0,0,0);
    if (usr > 0) {
        void *get_user = SYM(G, D, usr, "sceUserServiceGetInitialUser");
        if (get_user) {
            u32 uid = 0;
            s32 rc = (s32)NC(G, get_user, (u64)&uid, 0,0,0,0,0);
            if (rc == 0 && uid != 0) real_user_id = uid;
        }
    }
    if (real_user_id == 0) {
        real_user_id = (u32)c->ext->dbg[0];
        if (!real_user_id) real_user_id = 1;
    }
    c->user_id = (s32)real_user_id;

    c->pad_init         = SYM(G, D, pad, "scePadInit");
    c->pad_geth         = SYM(G, D, pad, "scePadGetHandle");
    c->pad_read         = SYM(G, D, pad, "scePadRead");
    c->pad_set_lightbar = SYM(G, D, pad, "scePadSetLightBar");
    c->pad_set_vib      = SYM(G, D, pad, "scePadSetVibration");

    static const char *trig_names[] = {
        "scePadSetTriggerEffect",
        "scePadSetTriggerEffectForController",
        "scePadSetTriggerEffectA",
        "scePadSetTriggerEffectB",
        "scePadSetTriggerEffectEx",
        0
    };
    c->pad_set_trigger = 0;
    c->pad_trigger_sym_used = "(none)";
    for (int i = 0; trig_names[i]; i++) {
        void *p = SYM(G, D, pad, trig_names[i]);
        if (p) {
            c->pad_set_trigger = p;
            c->pad_trigger_sym_used = trig_names[i];
            break;
        }
    }

    if (c->pad_init) (void)NC(G, c->pad_init, 0,0,0,0,0,0);
    if (c->usleep) NC(G, c->usleep, 50000, 0,0,0,0,0);
    if (c->pad_geth)
        c->pad_h = (s32)NC(G, c->pad_geth, (u64)c->user_id, 0,0,0,0,0);
}

void ctx_pad_retry(struct ctx *c) {
    if (c->pad_h >= 0 || !c->pad_geth) return;
    if (c->pad_init) (void)NC(c->G, c->pad_init, 0,0,0,0,0,0);
    c->pad_h = (s32)NC(c->G, c->pad_geth, (u64)c->user_id, 0,0,0,0,0);
}

u32 pad_raw(struct ctx *c) {
    if (c->pad_h < 0 || !c->pad_read) return 0;
    m_set(c->raw_buf, 0, 128);
    c->raw_buf_n = 0;
    s32 n = (s32)NC(c->G, c->pad_read, (u64)c->pad_h, (u64)c->raw_buf, 1, 0, 0, 0);
    c->dbg.last_read_ret = n;
    if (n <= 0) { c->dbg.pad_err++; return 0; }
    if ((u32)n >= 0x80000000) { c->dbg.pad_err++; return 0; }
    c->raw_buf_n = (u32)n;
    u32 r = *(u32*)c->raw_buf;
    if (r & 0x80000000) { c->dbg.pad_disc++; return 0; }
    c->dbg.pad_ok++;
    return r & DS_PAD_MASK;
}

int pad_set_lightbar(struct ctx *c, u8 r, u8 g, u8 b) {
    if (c->pad_h < 0 || !c->pad_set_lightbar) return -1;
    struct { u8 r, g, b, x; } col = { r, g, b, 0 };
    return (s32)NC(c->G, c->pad_set_lightbar, (u64)c->pad_h, (u64)&col, 0,0,0,0);
}

int pad_set_vibration(struct ctx *c, u8 large, u8 small) {
    if (c->pad_h < 0 || !c->pad_set_vib) return -1;
    struct { u8 l, s, r[6]; } v = { large, small, {0,0,0,0,0,0} };
    return (s32)NC(c->G, c->pad_set_vib, (u64)c->pad_h, (u64)&v, 0,0,0,0);
}

static void trig_fill(u8 buf[32], int which, int cmd_id,
                      u8 p0, u8 p1, u8 p2, u8 p3) {
    m_set(buf, 0, 32);
    if (which == TRIG_L2 || which == TRIG_BOTH) buf[0] = (u8)cmd_id;
    if (which == TRIG_R2 || which == TRIG_BOTH) buf[1] = (u8)cmd_id;
    if (which == TRIG_L2 || which == TRIG_BOTH) {
        buf[0x08 + 0] = (u8)cmd_id;
        buf[0x08 + 1] = p0;
        buf[0x08 + 2] = p1;
        buf[0x08 + 3] = p2;
        buf[0x08 + 4] = p3;
    }
    if (which == TRIG_R2 || which == TRIG_BOTH) {
        buf[0x14 + 0] = (u8)cmd_id;
        buf[0x14 + 1] = p0;
        buf[0x14 + 2] = p1;
        buf[0x14 + 3] = p2;
        buf[0x14 + 4] = p3;
    }
}

static int trig_send(struct ctx *c, u8 buf[32]) {
    if (c->pad_h < 0 || !c->pad_set_trigger) return -1;
    return (s32)NC(c->G, c->pad_set_trigger, (u64)c->pad_h, (u64)buf, 0,0,0,0);
}

int pad_set_trigger_all_off(struct ctx *c) {
    u8 buf[32]; m_set(buf, 0, 32);
    return trig_send(c, buf);
}
int pad_set_trigger_feedback(struct ctx *c, int which, u8 pos, u8 strength) {
    u8 buf[32]; trig_fill(buf, which, TRIG_EFF_FEEDBACK, pos, strength, 0, 0);
    return trig_send(c, buf);
}
int pad_set_trigger_weapon(struct ctx *c, int which, u8 start, u8 end, u8 strength) {
    u8 buf[32]; trig_fill(buf, which, TRIG_EFF_WEAPON, start, end, strength, 0);
    return trig_send(c, buf);
}
int pad_set_trigger_vibrate(struct ctx *c, int which, u8 pos, u8 amp, u8 freq) {
    u8 buf[32]; trig_fill(buf, which, TRIG_EFF_VIBRATION, pos, amp, freq, 0);
    return trig_send(c, buf);
}
int pad_set_trigger_slope(struct ctx *c, int which, u8 sPos, u8 ePos, u8 sStr, u8 eStr) {
    u8 buf[32]; trig_fill(buf, which, TRIG_EFF_SLOPE, sPos, ePos, sStr, eStr);
    return trig_send(c, buf);
}

void video_flip(struct ctx *c, int wait_vsync) {
    if (c->video_h < 0 || !c->vid_flip) return;
    NC(c->G, c->vid_flip, (u64)c->video_h, (u64)c->active, 1,
       (u64)c->total_frames, 0, 0);
    if (wait_vsync && c->eq && c->wait_eq) {
        u8 evt[64]; s32 cnt = 0;
        NC(c->G, c->wait_eq, c->eq, (u64)evt, 1, (u64)&cnt, 0, 0);
    }
    c->active ^= 1;
    c->total_frames++;
    c->dbg.flips++;
}

u64 get_uptime_ms(struct ctx *c) {
    if (!c->clock_gettime) return 0;
    u64 ts[2] = {0,0};
    if (NC(c->G, c->clock_gettime, 4, (u64)ts, 0,0,0,0) != 0) return 0;
    return ts[0] * 1000ULL + ts[1] / 1000000ULL;
}

u64 get_dm_size(struct ctx *c) {
    if (!c->dm_size) return 0;
    return NC(c->G, c->dm_size, 0,0,0,0,0,0);
}

void audio_tone(struct ctx *c, int freq, int ms) {
    if (c->audio_h < 0 || !c->aud_out) return;
    static s16 buf[SAMPLES_PER_BUF * 2];
    u32 half = (u32)(SAMPLE_RATE / (freq * 2));
    if (half < 1) half = 1;
    u32 total = (u32)(SAMPLE_RATE * ms / 1000);
    u32 done = 0, phase = 0;
    while (done < total) {
        u32 n = total - done;
        if (n > SAMPLES_PER_BUF) n = SAMPLES_PER_BUF;
        for (u32 i = 0; i < n; i++) {
            s16 s = ((phase / half) & 1) ? -12000 : 12000;
            buf[i*2] = s; buf[i*2+1] = s;
            phase++;
        }
        NC(c->G, c->aud_out, (u64)c->audio_h, (u64)buf, 0,0,0,0);
        c->dbg.audio_submits++;
        done += n;
    }
}

/* ============================================================
 * Cleanup — restore the controller to its default state so
 * that when control returns to the game (or to the PS5 UI) the
 * DualSense looks normal again.
 *
 *   - Lightbar: soft PS5 blue (0, 0, 200) — this is what the
 *     console ships the controller with out of the box.
 *   - Vibration: off.
 *   - Triggers: no effect (all zeros struct).
 * ============================================================ */
void ctx_cleanup(struct ctx *c) {
    /* 1. Kill vibration */
    pad_set_vibration(c, 0, 0);

    /* 2. Give the lightbar back its default colour */
    pad_set_lightbar(c, 0, 0, 200);

    /* 3. Remove any trigger effect we may have installed */
    pad_set_trigger_all_off(c);

    /* Let the pad library flush these state changes */
    if (c->usleep) NC(c->G, c->usleep, 100000, 0,0,0,0,0);

    /* Audio teardown */
    if (c->aud_close && c->audio_h >= 0)
        NC(c->G, c->aud_close, (u64)c->audio_h, 0,0,0,0,0);

    /* Video teardown */
    if (c->fbs[0]) ui_clear(c->fbs[0], 0xFF000000);
    if (c->fbs[1]) ui_clear(c->fbs[1], 0xFF000000);
    if (c->vid_flip && c->video_h >= 0)
        NC(c->G, c->vid_flip, (u64)c->video_h, (u64)c->active, 1, 0,0,0);
    if (c->usleep) NC(c->G, c->usleep, 50000, 0,0,0,0,0);
    if (c->vid_close && c->video_h >= 0)
        NC(c->G, c->vid_close, (u64)c->video_h, 0,0,0,0,0);
    if (c->delete_eq && c->eq)
        NC(c->G, c->delete_eq, c->eq, 0,0,0,0,0);
}

/* ============================================================
 * Module info (static buffer, never on stack)
 * ============================================================ */
static u8 g_modinfo_buf[0x800];

int get_module_info_of_addr(struct ctx *c, u64 addr, struct module_info_simple *out) {
    m_set(out, 0, sizeof(*out));
    if (!c->module_info_from_addr) return -1;
    m_set(g_modinfo_buf, 0, sizeof(g_modinfo_buf));
    *(u64*)g_modinfo_buf = (u64)sizeof(g_modinfo_buf);

    s32 r = (s32)NC(c->G, c->module_info_from_addr,
                    addr, 1, (u64)g_modinfo_buf, 0, 0, 0);
    if (r != 0) return r;

    for (int i = 0; i < 31; i++) {
        char ch = (char)g_modinfo_buf[0x08 + i];
        out->name[i] = ch;
        if (!ch) break;
    }
    out->name[31] = 0;
    out->base  = *(u64*)(g_modinfo_buf + 0x160);
    out->valid = 1;
    return 0;
}

static u8 g_swver_buf[0x200];

u32 get_fw_version_int(struct ctx *c) {
    if (!c->sys_sw_version) return 0;
    m_set(g_swver_buf, 0, sizeof(g_swver_buf));
    (void)(s32)NC(c->G, c->sys_sw_version, (u64)g_swver_buf, 0,0,0,0,0);
    return *(u32*)g_swver_buf;
}

u8  mem_read8 (struct ctx *c, u64 a) { (void)c; return *(volatile u8 *)(u64)a; }
u16 mem_read16(struct ctx *c, u64 a) { (void)c; return *(volatile u16*)(u64)a; }
u32 mem_read32(struct ctx *c, u64 a) { (void)c; return *(volatile u32*)(u64)a; }
u64 mem_read64(struct ctx *c, u64 a) { (void)c; return *(volatile u64*)(u64)a; }
void mem_write8 (struct ctx *c, u64 a, u8 v)  { (void)c; *(volatile u8 *)(u64)a = v; }
void mem_write16(struct ctx *c, u64 a, u16 v) { (void)c; *(volatile u16*)(u64)a = v; }
void mem_write32(struct ctx *c, u64 a, u32 v) { (void)c; *(volatile u32*)(u64)a = v; }
void mem_write64(struct ctx *c, u64 a, u64 v) { (void)c; *(volatile u64*)(u64)a = v; }

/* ============================================================
 * OSK prompt
 * ============================================================ */
static u16 g_osk_text[128];
static u16 g_osk_prompt[64];

static void ascii_to_u16(u16 *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = (u16)(u8)src[i]; i++; }
    dst[i] = 0;
}

static int u16_to_ascii(char *dst, const u16 *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = (char)(src[i] & 0xFF); i++; }
    dst[i] = 0;
    return i;
}

int osk_prompt(struct ctx *c, const char *title, const char *initial,
               char *out_ascii, int out_len, int max_len) {
    (void)title;
    if (!c->ime_init || !c->ime_get_status || !c->ime_get_result || !c->ime_term)
        return -100;

    if (max_len > 120) max_len = 120;
    ascii_to_u16(g_osk_text,   initial ? initial : "", 128);
    ascii_to_u16(g_osk_prompt, initial ? initial : "", 64);

    u8 param[256];
    m_set(param, 0, 256);

    *(u32*)(param + 0x00) = (u32)c->user_id;
    *(u32*)(param + 0x04) = 0;
    *(u64*)(param + 0x08) = 0;
    *(u64*)(param + 0x10) = 0;
    *(u64*)(param + 0x18) = (u64)g_osk_text;
    *(u64*)(param + 0x20) = (u64)g_osk_prompt;
    *(u32*)(param + 0x28) = 0;
    *(u32*)(param + 0x2C) = 0;
    *(u32*)(param + 0x30) = 0;
    *(u32*)(param + 0x34) = 0;
    *(u32*)(param + 0x38) = 0;
    *(u32*)(param + 0x3C) = 0;
    *(u16*)(param + 0x40) = (u16)max_len;
    *(u16*)(param + 0x42) = 0;
    *(u32*)(param + 0x44) = 0;
    *(u32*)(param + 0x48) = 0;

    s32 r = (s32)NC(c->G, c->ime_init, (u64)param, 0, 0,0,0,0);
    ulog_num(c, "[toolbox] osk_init ret=", (u64)(u32)r);
    if (r != 0) return -2;

    int timeout_ms = 60000;
    int waited = 0;
    for (;;) {
        s32 st = (s32)NC(c->G, c->ime_get_status, 0,0,0,0,0,0);
        if (st == 2) break;
        if (st < 0) {
            NC(c->G, c->ime_term, 0,0,0,0,0,0);
            ulog_num(c, "[toolbox] osk_status err=", (u64)(s64)st);
            return -3;
        }
        if (waited >= timeout_ms) {
            NC(c->G, c->ime_term, 0,0,0,0,0,0);
            return -4;
        }
        NC(c->G, c->usleep, 33333, 0,0,0,0,0);
        waited += 33;
    }

    u8 result[64]; m_set(result, 0, 64);
    NC(c->G, c->ime_get_result, (u64)result, 0,0,0,0,0);
    u32 end_status = *(u32*)result;

    NC(c->G, c->ime_term, 0,0,0,0,0,0);
    ulog_num(c, "[toolbox] osk_end=", (u64)end_status);

    if (end_status != 1) return -5;
    u16_to_ascii(out_ascii, g_osk_text, out_len);
    return 0;
}

/* ============================================================
 * System notification
 * ============================================================ */
static u8 g_notify_buf[0xC30];

int notify_send(struct ctx *c, const char *msg, const char *icon_uri) {
    if (!msg) return -1;
    if (!c->kopen || !c->kwrite || !c->kclose) return -2;

    int mlen = s_len(msg);
    if (mlen > 1023) mlen = 1023;

    int ilen = icon_uri ? s_len(icon_uri) : 0;
    if (ilen > 1023) ilen = 1023;

    m_set(g_notify_buf, 0, sizeof(g_notify_buf));

    *(u32*)(g_notify_buf + 0x00) = 0;
    *(u32*)(g_notify_buf + 0x10) = 0xFFFFFFFF;
    *(u32*)(g_notify_buf + 0x28) = 0;
    *(u32*)(g_notify_buf + 0x2C) = 1;

    for (int i = 0; i < mlen; i++)
        g_notify_buf[0x2D + i] = (u8)msg[i];
    g_notify_buf[0x2D + mlen] = 0;

    if (icon_uri && ilen > 0) {
        for (int i = 0; i < ilen; i++)
            g_notify_buf[0x42D + i] = (u8)icon_uri[i];
        g_notify_buf[0x42D + ilen] = 0;
    }

    s32 fd = (s32)NC(c->G, c->kopen, (u64)"/dev/notification0",
                     (u64)0x0001, 0, 0, 0, 0);
    if (fd < 0) {
        fd = (s32)NC(c->G, c->kopen, (u64)"/dev/notification",
                     (u64)0x0001, 0, 0, 0, 0);
    }
    if (fd < 0) {
        ulog_num(c, "[toolbox] notify open failed ret=", (u64)(s64)fd);
        return -3;
    }

    s32 w = (s32)NC(c->G, c->kwrite, (u64)fd, (u64)g_notify_buf,
                    (u64)sizeof(g_notify_buf), 0, 0, 0);
    NC(c->G, c->kclose, (u64)fd, 0,0,0,0,0);

    if (w <= 0) {
        ulog_num(c, "[toolbox] notify write failed ret=", (u64)(s64)w);
        return -4;
    }
    ulog(c, "[toolbox] notify sent\n");
    return 0;
}
