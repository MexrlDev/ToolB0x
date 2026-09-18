#ifndef APP_H
#define APP_H
#include "core.h"

/* ------------------------------------------------------------------
 * DualSense button bitmask — matches the DWORD returned by scePadRead.
 * Order/values verified against libScePad + on-hardware testing.
 * ------------------------------------------------------------------ */
#define DS_SHARE     0x00000001
#define DS_L3        0x00000002
#define DS_R3        0x00000004
#define DS_OPTIONS   0x00000008
#define DS_UP        0x00000010
#define DS_RIGHT     0x00000020
#define DS_DOWN      0x00000040
#define DS_LEFT      0x00000080
#define DS_L2        0x00000100
#define DS_R2        0x00000200
#define DS_L1        0x00000400
#define DS_R1        0x00000800
#define DS_TRIANGLE  0x00001000
#define DS_CIRCLE    0x00002000
#define DS_CROSS     0x00004000
#define DS_SQUARE    0x00008000
#define DS_TOUCHPAD  0x00100000
#define DS_PAD_MASK  0x001FFFFF

struct ctx {
    void *G, *D;
    u64   eboot_base;

    void *usleep, *cancel, *load_mod;
    void *alloc_dm, *map_dm, *dm_size;
    void *create_eq, *wait_eq, *delete_eq;
    void *mmap_fn, *munmap;
    void *kopen, *kread, *kwrite, *kclose, *klseek;
    void *clock_gettime, *getpid;
    void *socket_fn, *bind_fn, *listen_fn, *accept_fn;
    void *send_fn, *recv_fn, *sendto_fn, *recvfrom_fn;
    void *close_fn, *setsockopt_fn, *poll_fn, *getsockname_fn;

    void *vid_open, *vid_close, *vid_reg, *vid_flip, *vid_rate, *vid_evt;
    s32   video_h;
    void *vmem;
    u32  *fbs[2];
    int   active;
    u64   eq;
    u32   total_frames;

    void *aud_open, *aud_out, *aud_close;
    s32   audio_h;

    void *pad_init, *pad_geth, *pad_read;
    void *pad_set_lightbar, *pad_set_vib, *pad_set_trigger;
    s32   pad_h;
    u32   pad_prev;

    s32   user_id;

    s32   log_fd;
    u8    log_sa[16];

    struct ext_args *ext;
};

extern struct ctx G_CTX;

void ctx_init      (struct ctx *c, u64 eboot_base, u64 dlsym_addr, struct ext_args *ext);
int  ctx_video_up  (struct ctx *c, u64 eboot_base);
void ctx_audio_up  (struct ctx *c);
void ctx_pad_up    (struct ctx *c);
void ctx_pad_retry (struct ctx *c);
void ctx_cleanup   (struct ctx *c);

void ulog     (struct ctx *c, const char *msg);
void ulog_num (struct ctx *c, const char *prefix, u64 v);

u32  pad_raw              (struct ctx *c);
int  pad_set_lightbar     (struct ctx *c, u8 r, u8 g, u8 b);
int  pad_set_vibration    (struct ctx *c, u8 large, u8 small);
int  pad_set_trigger      (struct ctx *c, int mode, int trig, int p1, int p2, int p3);

void video_flip (struct ctx *c, int wait_vsync);

u64  get_uptime_ms (struct ctx *c);
u64  get_dm_size   (struct ctx *c);

/* audio: play a square wave tone for `ms` milliseconds (blocking) */
void audio_tone (struct ctx *c, int freq, int ms);

#endif
