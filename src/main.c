#include "core.h"
#include "app.h"
#include "ui.h"
#include "screens.h"

extern char __bss_start[];
extern char __bss_end[];
extern char __rela_start[];
extern char __rela_end[];

typedef struct { u64 r_offset; u64 r_info; s64 r_addend; } Elf64_Rela;
#define ELF64_R_TYPE(i) ((u32)((i) & 0xffffffffU))
#define R_X86_64_RELATIVE 8

static int do_relocations(u64 load_base) {
    Elf64_Rela *r = (Elf64_Rela *)__rela_start;
    Elf64_Rela *e = (Elf64_Rela *)__rela_end;
    int n = 0;
    while (r < e) {
        if (ELF64_R_TYPE(r->r_info) == R_X86_64_RELATIVE) {
            *(u64 *)(load_base + r->r_offset) = load_base + r->r_addend;
            n++;
        }
        r++;
    }
    return n;
}

static volatile int g_exit_now = 0;
void menu_request_exit(void) { g_exit_now = 1; }

#define TOOLBOX_BUILD_TAG  "[toolbox] BUILD v6-kernel-mem (2025)\n"

__attribute__((section(".text._start")))
void _start(u64 eboot_base, u64 dlsym_addr, struct ext_args *ext) {
    u64 load_base = (u64)&_start;
    do_relocations(load_base);
    for (volatile char *p = __bss_start; p < __bss_end; p++) *p = 0;

    ext->step = 1;
    ctx_init(&G_CTX, eboot_base, dlsym_addr, ext);
    ulog(&G_CTX, TOOLBOX_BUILD_TAG);
    ulog(&G_CTX, "[toolbox] start\n");

    ext->step = 2;
    int vret = ctx_video_up(&G_CTX, eboot_base);
    if (vret != 0) { ext->status = -100 + vret; ext->step = 3; return; }
    ulog(&G_CTX, "[toolbox] video up\n");

    ext->step = 4;
    ctx_audio_up(&G_CTX);
    ulog(&G_CTX, "[toolbox] audio up\n");

    ext->step = 5;
    ctx_pad_up(&G_CTX);
    ulog(&G_CTX, "[toolbox] pad up\n");

    pad_set_lightbar(&G_CTX, 0, 160, 255);
    pad_set_vibration(&G_CTX, 0, 0);
    pad_set_trigger_all_off(&G_CTX);

    ext->step = 6;
    menu_init();
    G_CTX.pad_prev = pad_raw(&G_CTX);

    static u32 dbg_tick   = 0;
    static u32 retry_tick = 0;

    while (!g_exit_now) {
        u64 t0 = get_uptime_ms(&G_CTX);

        u32 raw     = pad_raw(&G_CTX);
        u32 pressed = raw & ~G_CTX.pad_prev;

        if (pressed) {
            dbg_record_press(&G_CTX, pressed);
            ulog_num(&G_CTX, "[toolbox] press=", (u64)pressed);
        }
        if (++dbg_tick >= 600) {
            dbg_tick = 0;
            ulog_num(&G_CTX, "[toolbox] raw=", (u64)raw);
        }
        if (G_CTX.pad_h < 0 && ++retry_tick >= 120) {
            retry_tick = 0;
            ctx_pad_retry(&G_CTX);
        }

        /* ---- L1 + R1 escape hatch: force back to main menu ---- */
        int on_main = (g_screen == &scr_main);
        if (!on_main && (raw & (DS_L1 | DS_R1)) == (DS_L1 | DS_R1)) {
            menu_goto(&scr_main);
            G_CTX.pad_prev = raw;   /* consume the edge so R1 release doesn't fire */
            ulog(&G_CTX, "[toolbox] L1+R1 -> main\n");
            menu_draw(&G_CTX, G_CTX.fbs[G_CTX.active]);
            video_flip(&G_CTX, 1);
            continue;
        }

        /* ---- R1 = exit ONLY on main menu ---- */
        if (on_main && (pressed & DS_R1)) {
            ulog(&G_CTX, "[toolbox] R1 exit (main menu)\n");
            break;
        }

        G_CTX.pad_prev = raw;

        /* ---- Draw ---- */
        u64 t_draw_start = get_uptime_ms(&G_CTX);
        menu_input(&G_CTX, raw, pressed);
        menu_tick(&G_CTX);
        menu_draw(&G_CTX, G_CTX.fbs[G_CTX.active]);
        u64 t_draw_end = get_uptime_ms(&G_CTX);

        video_flip(&G_CTX, 1);
        u64 t_flip_end = get_uptime_ms(&G_CTX);

        u32 draw_ms = (u32)(t_draw_end - t_draw_start);
        u32 flip_ms = (u32)(t_flip_end - t_draw_end);
        u32 frame_ms = (u32)(t_flip_end - t0);

        G_CTX.dbg.draw_time_ms  = draw_ms;
        G_CTX.dbg.flip_time_ms  = flip_ms;
        G_CTX.dbg.frame_time_ms = frame_ms;
        if (frame_ms < G_CTX.dbg.frame_time_min) G_CTX.dbg.frame_time_min = frame_ms;
        if (frame_ms > G_CTX.dbg.frame_time_max) G_CTX.dbg.frame_time_max = frame_ms;
        G_CTX.dbg.frame_time_sum += frame_ms;
        G_CTX.dbg.frame_time_n++;

        G_CTX.dbg.fps_frames++;
        if (t_flip_end - G_CTX.dbg.fps_last_ms >= 1000) {
            G_CTX.dbg.fps = G_CTX.dbg.fps_frames * 1000
                          / (u32)(t_flip_end - G_CTX.dbg.fps_last_ms);
            G_CTX.dbg.fps_frames  = 0;
            G_CTX.dbg.fps_last_ms = t_flip_end;
        }
        if (G_CTX.dbg.pad_presses) {
            G_CTX.dbg.last_press_ago_ms = (u32)(t_flip_end - t0)
                + G_CTX.dbg.last_press_ago_ms;
            if (G_CTX.dbg.last_press_ago_ms > 60000)
                G_CTX.dbg.last_press_ago_ms = 60000;
        }

        if (G_CTX.ext) G_CTX.ext->frame_count = G_CTX.total_frames;
    }

    ctx_cleanup(&G_CTX);
    ulog(&G_CTX, "[toolbox] clean exit\n");
    ext->status = 0;
    ext->step = 99;
    ext->frame_count = G_CTX.total_frames;
}
