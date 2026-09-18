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

/* Exit flag */
static volatile int g_exit_now = 0;

void menu_request_exit(void) { g_exit_now = 1; }

__attribute__((section(".text._start")))
void _start(u64 eboot_base, u64 dlsym_addr, struct ext_args *ext) {
    u64 load_base = (u64)&_start;
    do_relocations(load_base);
    for (volatile char *p = __bss_start; p < __bss_end; p++) *p = 0;

    ext->step = 1;
    ctx_init(&G_CTX, eboot_base, dlsym_addr, ext);
    ulog(&G_CTX, "[toolbox] start\n");

    ext->step = 2;
    int vret = ctx_video_up(&G_CTX, eboot_base);
    if (vret != 0) {
        ext->status = -100 + vret;
        ext->step = 3;
        return;
    }
    ulog(&G_CTX, "[toolbox] video up\n");

    ext->step = 4;
    ctx_audio_up(&G_CTX);
    ulog(&G_CTX, "[toolbox] audio up\n");

    ext->step = 5;
    ctx_pad_up(&G_CTX);
    ulog(&G_CTX, "[toolbox] pad up\n");

    /* Reset lightbar to a friendly colour */
    pad_set_lightbar(&G_CTX, 0, 160, 255);
    pad_set_vibration(&G_CTX, 0, 0);

    ext->step = 6;
    menu_init();
    G_CTX.pad_prev = pad_raw(&G_CTX);

    /* Local debug counters persist across loop iterations */
    static u32 dbg_tick   = 0;
    static u32 retry_tick = 0;

    /* Main loop */
    while (!g_exit_now) {
        u32 raw = pad_raw(&G_CTX);
        u32 pressed = raw & ~G_CTX.pad_prev;
        G_CTX.pad_prev = raw;

        /* --- DEBUG: throttled raw pad dump --- */
        if (++dbg_tick >= 60) {
            dbg_tick = 0;
            ulog_num(&G_CTX, "[toolbox] raw=", (u64)raw);
        }

        /* --- DEBUG: retry pad handle grab if it failed --- */
        if (G_CTX.pad_h < 0 && ++retry_tick >= 120) {
            retry_tick = 0;
            ctx_pad_retry(&G_CTX);
        }

        /* R1 = hard exit */
        if (raw & 0x0800) break;

        menu_input(&G_CTX, raw, pressed);

        /* Tick + draw on current active framebuffer */
        menu_tick(&G_CTX);
        u32 *fb = G_CTX.fbs[G_CTX.active];
        menu_draw(&G_CTX, fb);

        video_flip(&G_CTX, 1);
        if (G_CTX.ext) G_CTX.ext->frame_count = G_CTX.total_frames;
    }

    ctx_cleanup(&G_CTX);
    ulog(&G_CTX, "[toolbox] clean exit\n");

    ext->status = 0;
    ext->step = 99;
    ext->frame_count = G_CTX.total_frames;
}
