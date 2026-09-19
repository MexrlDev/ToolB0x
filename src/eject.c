#include "app.h"

/* ============================================================
 * Optical drive control (FreeBSD CDIO interface)
 *
 * The PS4 / PS5 kernel is FreeBSD-derived, so the CD-ROM ioctls
 * come from <sys/cdio.h>.  We can't include that header here
 * (freestanding build), so we encode the values ourselves.
 *
 *   CDIOCALLOW  = _IO('c', 82)
 *   CDIOCEJECT  = _IO('c', 88)
 *   CDIOCCLOSE  = _IO('c', 89)
 *
 * FreeBSD's <sys/ioccom.h> defines
 *     _IO(g, n) = _IOC(IOC_VOID, g, n, 0)
 *               = (IOC_VOID) | ((g) << 8) | (n)
 * with IOC_VOID = 0x20000000.
 *
 * Older BSDs used a bare 16-bit encoding `(g << 8) | n`.  We try
 * the modern value first, then the legacy one.
 * ============================================================ */

#define IOC_VOID       0x20000000u

#define CDIOCALLOW_MOD (IOC_VOID | ('c' << 8) | 82u)   /* 0x20006352 */
#define CDIOCALLOW_OLD ((u32)(('c' << 8) | 82u))       /* 0x6352     */

#define CDIOCEJECT_MOD (IOC_VOID | ('c' << 8) | 88u)   /* 0x20006358 */
#define CDIOCEJECT_OLD ((u32)(('c' << 8) | 88u))       /* 0x6358     */

#define CDIOCCLOSE_MOD (IOC_VOID | ('c' << 8) | 89u)   /* 0x20006359 */
#define CDIOCCLOSE_OLD ((u32)(('c' << 8) | 89u))       /* 0x6359     */

#define O_RDONLY_    0x0000
#define O_NONBLOCK_  0x0004

static const char *g_last_device = "";

const char *eject_last_device(void) { return g_last_device; }
void eject_reset(void) { g_last_device = ""; }

/* Try each candidate path until one opens.  Returns fd or -1. */
static s32 open_optical(struct ctx *c, const char **used_path) {
    static const char *paths[] = {
        "/dev/cd0",
        "/dev/cd1",
        "/dev/cd",
        "/dev/bdvd0",
        "/dev/bdvd1",
        0
    };
    if (!c->kopen) return -1;

    for (int i = 0; paths[i]; i++) {
        s32 fd = (s32)NC(c->G, c->kopen,
                         (u64)paths[i],
                         (u64)(O_RDONLY_ | O_NONBLOCK_),
                         0, 0, 0, 0);
        ulog(c, "[toolbox] eject: try ");
        ulog(c, paths[i]);
        ulog_num(c, "  fd=", (u64)(s64)fd);
        if (fd >= 0) {
            if (used_path) *used_path = paths[i];
            return fd;
        }
    }
    return -1;
}

/* Call ioctl with the modern encoding first, then the legacy one.
 * Returns 0 if either succeeds, negative otherwise. */
static int ioctl_dual(struct ctx *c, void *ioctl_fn, s32 fd,
                      u32 modern, u32 legacy, const char *tag) {
    s32 r = (s32)NC(c->G, ioctl_fn, (u64)fd, (u64)modern, 0, 0, 0, 0);
    ulog_num(c, tag, (u64)(s64)r);
    if (r == 0) return 0;

    r = (s32)NC(c->G, ioctl_fn, (u64)fd, (u64)legacy, 0, 0, 0, 0);
    ulog_num(c, tag, (u64)(s64)r);
    return (r == 0) ? 0 : -1;
}

/* ------------------------------------------------------------
 * Common open / prepare / act / close pipeline.
 *
 *   allow  = best effort CDIOCALLOW (some drives need this before
 *            they'll accept CDIOCEJECT).
 *   action = CDIOCEJECT or CDIOCCLOSE.
 * ------------------------------------------------------------ */
static int eject_common(struct ctx *c, u32 action_modern, u32 action_legacy,
                        const char *tag) {
    g_last_device = "";

    void *ioctl_fn = SYM(c->G, c->D, LIBKERNEL_HANDLE, "ioctl");
    if (!ioctl_fn) {
        ulog(c, "[toolbox] eject: ioctl symbol not resolved\n");
        return -3;
    }

    const char *path = 0;
    s32 fd = open_optical(c, &path);
    if (fd < 0) {
        ulog(c, "[toolbox] eject: no optical device found\n");
        return -1;
    }
    g_last_device = path;

    /* Best-effort unlock.  Ignore failure. */
    (void)ioctl_dual(c, ioctl_fn, fd,
                     CDIOCALLOW_MOD, CDIOCALLOW_OLD,
                     "[toolbox] eject: CDIOCALLOW r=");

    int r = ioctl_dual(c, ioctl_fn, fd,
                       action_modern, action_legacy,
                       tag);

    NC(c->G, c->kclose, (u64)fd, 0, 0, 0, 0, 0);
    ulog(c, r == 0 ? "[toolbox] eject: OK\n"
                   : "[toolbox] eject: FAILED\n");
    return r;
}

int eject_disc(struct ctx *c) {
    ulog(c, "[toolbox] eject_disc\n");
    return eject_common(c, CDIOCEJECT_MOD, CDIOCEJECT_OLD,
                        "[toolbox] eject: CDIOCEJECT r=");
}

int eject_close(struct ctx *c) {
    ulog(c, "[toolbox] eject_close\n");
    return eject_common(c, CDIOCCLOSE_MOD, CDIOCCLOSE_OLD,
                        "[toolbox] eject: CDIOCCLOSE r=");
}
