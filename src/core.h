#ifndef CORE_H
#define CORE_H

typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef long           s64;
typedef int            s32;
typedef short          s16;
typedef signed char    s8;

#define GADGET_OFFSET    0x31AA9
#define LIBKERNEL_HANDLE 0x2001
#define EBOOT_GS_THREAD  0x057F89B0
#define EBOOT_VIDOUT     0x02d695d0

#define SCR_W       1920
#define SCR_H       1080
#define FB_SIZE     (SCR_W * SCR_H * 4)
#define FB_ALIGNED  ((FB_SIZE + 0x1FFFFF) & ~0x1FFFFF)
#define FB_TOTAL    (FB_ALIGNED * 2)

#define SAMPLE_RATE      48000
#define SAMPLES_PER_BUF  1024
#define AUDIO_S16_STEREO 1

#define ARGB(a,r,g,b) (((u32)(a)<<24)|((u32)(r)<<16)|((u32)(g)<<8)|(u32)(b))
#define RGB(r,g,b)    ARGB(0xFF,(r),(g),(b))

struct ext_args {
    s64 status;          /* 0x00 */
    s64 step;            /* 0x08 */
    u32 frame_count;     /* 0x10 */
    u32 _pad;            /* 0x14 */
    s32 log_fd;          /* 0x18 */
    s32 pad_fd;          /* 0x1C */
    u8  log_addr[16];    /* 0x20 */
    u64 dbg[8];          /* 0x30 */
};

__attribute__((naked))
static u64 native_call(void *gadget, void *fn,
                       u64 a1, u64 a2, u64 a3,
                       u64 a4, u64 a5, u64 a6)
{
    __asm__ volatile (
        "pushq %%rbx\n\t"
        "movq %%rsi, %%rbx\n\t"
        "movq %%rdi, %%rax\n\t"
        "movq %%rdx, %%rdi\n\t"
        "movq %%rcx, %%rsi\n\t"
        "movq %%r8,  %%rdx\n\t"
        "movq %%r9,  %%rcx\n\t"
        "movq 16(%%rsp), %%r8\n\t"
        "movq 24(%%rsp), %%r9\n\t"
        "callq *%%rax\n\t"
        "popq %%rbx\n\t"
        "retq" ::: "memory"
    );
}

static void *resolve_sym(void *gadget, void *dlsym_fn, s32 handle, const char *name) {
    void *addr = 0;
    native_call(gadget, dlsym_fn, (u64)handle, (u64)name, (u64)&addr, 0, 0, 0);
    return addr;
}

#define NC  native_call
#define SYM resolve_sym

static inline int  s_len(const char *s)          { int n = 0; while (s[n]) n++; return n; }
static inline void s_cpy(char *d, const char *s) { while ((*d++ = *s++)); }
static inline void m_set(void *d, u8 v, u64 n)   { u8 *p = d; while (n--) *p++ = v; }
static inline void m_cpy(void *d, const void *s, u64 n) { u8 *dp = d; const u8 *sp = s; while (n--) *dp++ = *sp++; }
static inline int  s_cmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }

/* i32 -> decimal */
static inline int s_itoa(char *out, int v) {
    if (v == 0) { out[0] = '0'; out[1] = 0; return 1; }
    int neg = 0, p = 0; char tmp[12];
    if (v < 0) { neg = 1; v = -v; }
    while (v) { tmp[p++] = '0' + (v % 10); v /= 10; }
    int n = p; if (neg) out[0] = '-', out[1] = 0, p = 0; else p = 0;
    while (n) out[p++] = tmp[--n];
    out[p] = 0;
    return p;
}

static inline int s_hex64(char *out, u64 v) {
    const char *h = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; i++) out[2 + i] = h[(v >> ((15 - i) * 4)) & 0xF];
    out[18] = 0;
    return 18;
}

#endif
