#include "app.h"

#define IOC_VOID       0x20000000u
#define CDIOCALLOW_MOD (IOC_VOID | ('c' << 8) | 82u)
#define CDIOCALLOW_OLD ((u32)(('c' << 8) | 82u))
#define CDIOCEJECT_MOD (IOC_VOID | ('c' << 8) | 88u)
#define CDIOCEJECT_OLD ((u32)(('c' << 8) | 88u))
#define CDIOCCLOSE_MOD (IOC_VOID | ('c' << 8) | 89u)
#define CDIOCCLOSE_OLD ((u32)(('c' << 8) | 89u))

#define O_RDONLY_    0x0000
#define O_RDWR_      0x0002
#define O_NONBLOCK_  0x0004
#define O_DIRECTORY_ 0x00020000

static int g_dumped = 0;

static const char *g_last_device = "";
const char *eject_last_device(void) { return g_last_device; }
void eject_reset(void) { g_last_device = ""; }

static const char *candidate_paths[] = {
    "/dev/cd0", "/dev/cd1", "/dev/cd",
    "/dev/acd0", "/dev/acd",
    "/dev/scd0", "/dev/scd",
    "/dev/rcd0", "/dev/rcd",
    "/dev/bd0", "/dev/bd", "/dev/bdvd0", "/dev/bdvd1",
    "/dev/sr0", "/dev/sr",
    "/dev/dvd0", "/dev/dvd",
    0
};

static u8 g_dents_buf[8192];

void eject_dump_dev(struct ctx *c) {
    if (g_dumped) return;
    g_dumped = 1;
    if (!c->kopen || !c->getdents || !c->kclose) return;
    ulog(c, "[toolbox] /dev dump begin\n");
    s32 dfd = (s32)NC(c->G, c->kopen, (u64)"/dev",
                     (u64)(O_RDONLY_ | O_DIRECTORY_), 0, 0, 0, 0);
    if (dfd < 0) return;
    for (;;) {
        s32 n = (s32)NC(c->G, c->getdents, (u64)dfd, (u64)g_dents_buf,
                        (u64)sizeof(g_dents_buf), 0, 0, 0);
        if (n <= 0) break;
        int off = 0;
        while (off < n) {
            u16 reclen = *(u16*)(g_dents_buf + off + 4);
            if (reclen == 0) break;
            const char *name = (const char *)(g_dents_buf + off + 8);
            if (name[0] != '.') {
                ulog(c, "[toolbox]   ");
                ulog(c, name);
                ulog(c, "\n");
            }
            off += reclen;
        }
    }
    NC(c->G, c->kclose, (u64)dfd, 0,0,0,0,0);
    ulog(c, "[toolbox] /dev dump end\n");
}

static s32 try_open(struct ctx *c, const char *path, u32 flags) {
    return (s32)NC(c->G, c->kopen, (u64)path, (u64)flags, 0, 0, 0, 0);
}

static s32 open_optical(struct ctx *c, const char **used_path) {
    for (int i = 0; candidate_paths[i]; i++) {
        const char *p = candidate_paths[i];
        s32 fd = try_open(c, p, O_RDWR_ | O_NONBLOCK_);
        if (fd < 0) fd = try_open(c, p, O_RDONLY_ | O_NONBLOCK_);
        if (fd >= 0) {
            if (used_path) *used_path = p;
            return fd;
        }
    }
    return -1;
}

static int ioctl_dual(struct ctx *c, void *ioctl_fn, s32 fd,
                      u32 modern, u32 legacy) {
    s32 r = (s32)NC(c->G, ioctl_fn, (u64)fd, (u64)modern, 0, 0, 0, 0);
    if (r == 0) return 0;
    r = (s32)NC(c->G, ioctl_fn, (u64)fd, (u64)legacy, 0, 0, 0, 0);
    return (r == 0) ? 0 : -1;
}

static int eject_common(struct ctx *c, u32 action_modern, u32 action_legacy) {
    g_last_device = "";

    void *ioctl_fn = SYM(c->G, c->D, LIBKERNEL_HANDLE, "ioctl");
    if (!ioctl_fn) return -3;

    const char *path = 0;
    s32 fd = open_optical(c, &path);
    if (fd < 0) {
        ulog(c, "[toolbox] eject: no optical device in /dev\n");
        eject_dump_dev(c);
        return -1;
    }
    g_last_device = path;
    ulog(c, "[toolbox] eject: opened ");
    ulog(c, path);
    ulog(c, "\n");

    (void)ioctl_dual(c, ioctl_fn, fd, CDIOCALLOW_MOD, CDIOCALLOW_OLD);
    int r = ioctl_dual(c, ioctl_fn, fd, action_modern, action_legacy);
    NC(c->G, c->kclose, (u64)fd, 0, 0, 0, 0, 0);
    return r;
}

int eject_disc(struct ctx *c) {
    return eject_common(c, CDIOCEJECT_MOD, CDIOCEJECT_OLD);
}

int eject_close(struct ctx *c) {
    return eject_common(c, CDIOCCLOSE_MOD, CDIOCCLOSE_OLD);
}
