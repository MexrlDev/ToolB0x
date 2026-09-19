#include "screens.h"
#include "ui.h"

/* Executable scratch buffer for user-submitted hex shellcode.
 * Since the whole .text segment is RWX (see linker.ld), a buffer
 * placed here is both writable and executable. */
__attribute__((section(".text"), aligned(16)))
u8 g_code_buf[0x10000];

/* Call arbitrary user code, saving callee-saved registers around it. */
u64 coderun_execute(void *code) {
    u64 ret;
    __asm__ volatile (
        "pushq %%rbp\n\t"
        "pushq %%rbx\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "callq *%1\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rbx\n\t"
        "popq %%rbp\n\t"
        : "=a"(ret)
        : "r"(code)
        : "memory"
    );
    return ret;
}

static int hex_char(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int parse_hex(const char *s, u8 *out, int max) {
    int n = 0;
    int hi = -1;
    while (*s) {
        char ch = *s;
        if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r') { s++; continue; }
        int v = hex_char(ch);
        if (v < 0) return -1;
        if (hi < 0) hi = v;
        else {
            if (n >= max) return -2;
            out[n++] = (u8)((hi << 4) | v);
            hi = -1;
        }
        s++;
    }
    if (hi >= 0) return -1;
    return n;
}

static char g_last_code[2048] =
    "48 C7 C0 2A 00 00 00 C3";

static void act_coderun(Item *it) {
    (void)it;

    int r = vkb_prompt(&G_CTX,
                       "Hex shellcode (send UDP :9031)",
                       g_last_code, g_last_code,
                       (int)sizeof(g_last_code), 512);
    if (r != 0) { notify_send(&G_CTX, "Cancelled", 0); return; }

    int n = parse_hex(g_last_code, g_code_buf, (int)sizeof(g_code_buf));
    if (n <= 0) {
        notify_send(&G_CTX, "Bad hex string", 0);
        return;
    }
    ulog_num(&G_CTX, "[toolbox] coderun bytes=", (u64)n);

    u64 ret = coderun_execute(g_code_buf);

    char buf[64]; int p = 0;
    const char *pre = "Returned: ";
    while (*pre) buf[p++] = *pre++;
    p += s_hex64(buf + p, ret);
    buf[p] = 0;
    ulog(&G_CTX, "[toolbox] coderun ");
    ulog(&G_CTX, buf);
    ulog(&G_CTX, "\n");
    notify_send(&G_CTX, buf, 0);
}

static Item scr_coderun_items[] = {
    { .label = "Code Runner", .kind = ITEM_HEADER },
    { .label = "Enter Hex Shellcode",  .kind = ITEM_ACTION, .on_confirm = act_coderun },

    { .label = "How it works",         .kind = ITEM_HEADER },
    { .label = "Opens keyboard.",      .kind = ITEM_INFO, .info = "Type hex bytes" },
    { .label = "UDP input supported",  .kind = ITEM_INFO, .info = "Port 9031 while open" },
    { .label = "Return value logged.", .kind = ITEM_INFO, .info = "and shown as toast" },
    { .label = "Example: 48C7C02A...", .kind = ITEM_INFO, .info = "= mov rax,42; ret" },
    { .label = "Send text from PC:",   .kind = ITEM_INFO, .info = "echo -n \"C3\" | nc -u <ip> 9031" },

    { .label = "Back", .kind = ITEM_BACK },
};

Screen scr_coderun = {
    .title = "Code Runner",
    .items = scr_coderun_items,
    .count = (int)(sizeof(scr_coderun_items) / sizeof(scr_coderun_items[0])),
    .visible = 8,
    .parent = &scr_main,
};
