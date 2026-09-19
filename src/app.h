#ifndef APP_H
#define APP_H
#include "core.h"

/* ------------------------------------------------------------------
 * DualSense button bitmask (matches scePadRead DWORD).
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

/* Trigger effect command IDs (match SDK enum) */
#define TRIG_EFF_OFF          0
#define TRIG_EFF_FEEDBACK     1
#define TRIG_EFF_WEAPON       2
#define TRIG_EFF_VIBRATION    3
#define TRIG_EFF_SLOPE        4
#define TRIG_EFF_MULTI_FB     5
#define TRIG_EFF_MULTI_VIB    6

/* Which trigger(s) */
#define TRIG_L2    0
#define TRIG_R2    1
#define TRIG_BOTH  2

/* ------------------------------------------------------------------
 * Runtime debug state — everything on the Debug screen comes from here.
 * ------------------------------------------------------------------ */
struct debug_state {
    u64 start_ms;
    u64 last_frame_ms;

    /* Frame timing (milliseconds) */
    u32 frame_time_ms;
    u32 frame_time_min;
    u32 frame_time_max;
    u64 frame_time_sum;
    u32 frame_time_n;
    u32 draw_time_ms;
    u32 flip_time_ms;

    /* FPS (updated once per second) */
    u32 fps;
    u32 fps_frames;
    u64 fps_last_ms;

    /* Pad */
    u32 pad_ok;              /* successful read calls */
    u32 pad_err;             /* read returned <= 0 */
    u32 pad_disc;            /* read returned the disconnected flag */
    s32 last_read_ret;       /* raw scePadRead return value */
    u32 pad_presses;         /* rising edges since reset */
    u32 last_press_mask;
    u32 last_press_ago_ms;   /* ms since last edge */

    /* Video */
    u32 flips;

    /* Audio */
    u32 audio_submits;

    /* Press history ring (last 16 events) */
    struct { u32 mask; u32 ms; } press_hist[16];
    int press_hist_head;
    int press_hist_count;

    /* Per-button flash (press for 500 ms after releasing) */
    struct { u32 bit; u64 until_ms; } flash[16];
};

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

    /* Debug extras */
    struct debug_state dbg;
    u8  raw_buf[128];        /* last scePadRead buffer */
    u32 raw_buf_n;           /* how many bytes scePadRead actually filled */
};

extern struct ctx G_CTX;

/* Lifecycle */
void ctx_init      (struct ctx *c, u64 eboot_base, u64 dlsym_addr, struct ext_args *ext);
int  ctx_video_up  (struct ctx *c, u64 eboot_base);
void ctx_audio_up  (struct ctx *c);
void ctx_pad_up    (struct ctx *c);
void ctx_pad_retry (struct ctx *c);
void ctx_cleanup   (struct ctx *c);

/* Logging */
void ulog     (struct ctx *c, const char *msg);
void ulog_num (struct ctx *c, const char *prefix, u64 v);

/* Pad */
u32  pad_raw              (struct ctx *c);
int  pad_set_lightbar     (struct ctx *c, u8 r, u8 g, u8 b);
int  pad_set_vibration    (struct ctx *c, u8 large, u8 small);

/* Trigger effects */
int  pad_set_trigger_all_off (struct ctx *c);
int  pad_set_trigger_feedback(struct ctx *c, int which, u8 pos, u8 strength);
int  pad_set_trigger_weapon  (struct ctx *c, int which, u8 start, u8 end, u8 strength);
int  pad_set_trigger_vibrate (struct ctx *c, int which, u8 pos, u8 amp, u8 freq);
int  pad_set_trigger_slope   (struct ctx *c, int which, u8 sPos, u8 ePos, u8 sStr, u8 eStr);

/* Video */
void video_flip (struct ctx *c, int wait_vsync);

/* Misc */
u64  get_uptime_ms (struct ctx *c);
u64  get_dm_size   (struct ctx *c);
void audio_tone    (struct ctx *c, int freq, int ms);

/* Debug bookkeeping */
void dbg_init        (struct ctx *c);
void dbg_reset       (struct ctx *c);
void dbg_record_press(struct ctx *c, u32 mask);

#endif
