-- SPDX-License-Identifier: MIT
--[[
  toolbox_launcher.lua — Luac0re payload for LuaC0re Toolbox (v3.2)
]]

local PC_IP        = "__PC_IP__"
local LOG_PORT     = 9027
local SC_PORT_BASE = 5001
local SC_PORT_MAX  = 5020

local HAVE_LOGS = (PC_IP:match("^%d+%.%d+%.%d+%.%d+$") ~= nil)

init_dlsym()
sceMsgDialogTerminate()

local function htons(p) return ((p << 8) | (p >> 8)) & 0xFFFF end
local function inet_addr(s)
    local a,b,c,d = s:match("(%d+)%.(%d+)%.(%d+)%.(%d+)")
    return (d << 24) | (c << 16) | (b << 8) | a
end
local function make_sockaddr_in(port, ip)
    local sa = malloc(16)
    for i = 0,15 do write8(sa + i, 0) end
    write8(sa + 0, 16); write8(sa + 1, 2)
    write16(sa + 2, htons(port))
    if ip then write32(sa + 4, inet_addr(ip)) end
    return sa
end

-- Always create a valid socket so log_sock >= 0 inside ext_args
local log_sock = create_socket(AF_INET, SOCK_DGRAM, 0)
local log_sa   = nil

if HAVE_LOGS then
    log_sa = make_sockaddr_in(LOG_PORT, PC_IP)
end

local function ulog(m)
    if HAVE_LOGS and log_sock >= 0 and log_sa then
        syscall.sendto(log_sock, m .. "\n", #m + 1, 0, log_sa, 16)
    end
end

if HAVE_LOGS then
    ulog("toolbox_launcher: starting v3.2 (logs -> " .. PC_IP .. ":" .. tostring(LOG_PORT) .. ")")
end

-- ============================================================
-- Ensure module loader is available
-- ============================================================
if not sceKernelLoadStartModule then
    sceKernelLoadStartModule = func_wrap(dlsym(LIBKERNEL_HANDLE, "sceKernelLoadStartModule"))
end

-- ============================================================
-- Query real initial user id
-- ============================================================
local real_uid = 0
do
    local libUser = sceKernelLoadStartModule("libSceUserService.sprx", 0, 0, 0, 0, 0)
    ulog("libSceUserService handle=" .. tostring(libUser))
    if libUser and libUser > 0 then
        local getInit = dlsym(libUser, "sceUserServiceGetInitialUser")
        if getInit then
            ulog("sceUserServiceGetInitialUser addr=0x" .. string.format("%x", getInit))
            local uid_buf = malloc(8)
            write32(uid_buf, 0)
            local r = func_wrap(getInit)(uid_buf)
            real_uid = read32(uid_buf)
            ulog("GetInitialUser ret=" .. tostring(r) .. " uid=" .. tostring(real_uid))
        end
    end
end
if real_uid == 0 then
    real_uid = 1
    ulog("using fallback uid=1")
end

-- ============================================================
-- Memory — try mmap first, JIT fallback
-- ============================================================
local SC_TARGET = 0x100000          -- 1 MB
local rw, rx    = 0, 0
local SC_SIZE   = SC_TARGET

do
    local PROT_RWX      = 0x7
    local MAP_PRIV_ANON = 0x1002
    local m = syscall.mmap(0, SC_TARGET, PROT_RWX, MAP_PRIV_ANON, -1, 0)
    ulog("mmap RWX 1MB -> 0x" .. string.format("%x", m or 0))
    if m and m > 0x10000 then
        rw = m; rx = m
        ulog("PRIMARY mmap RWX at 0x" .. string.format("%x", m))
    end
end

if rw == 0 then
    local function jit_alloc(size)
        local bfd  = jit_malloc(8)
        local rwfd = jit_malloc(8)
        local rxfd = jit_malloc(8)
        local rwa  = jit_malloc(8)
        local rxa  = malloc(8)
        local nm   = jit_malloc(8)
        if bfd == 0 or rwfd == 0 or rxfd == 0 or rwa == 0 or nm == 0 then
            return 0, 0
        end
        jit_write_buffer(nm, "nv4b")
        jit_sceKernelJitCreateSharedMemory(nm, size, 7, bfd)
        local handle = jit_read32(bfd)
        if handle == 0 then return 0, 0 end
        jit_sceKernelJitCreateAliasOfSharedMemory(handle, PROT_READ|PROT_WRITE, rwfd)
        jit_sceKernelJitCreateAliasOfSharedMemory(handle, PROT_READ|PROT_EXECUTE, rxfd)
        jit_sceKernelJitMapSharedMemory(jit_read32(rwfd), PROT_READ|PROT_WRITE, rwa)
        local rw_try = jit_read64(rwa)
        if rw_try == 0 then return 0, 0 end
        local mfd = jit_send_recv_fd(jit_read32(rxfd), NEW_JIT_SOCK, NEW_MAIN_SOCK)
        sceKernelJitMapSharedMemory(mfd, PROT_READ|PROT_EXECUTE, rxa)
        local rx_try = read64(rxa)
        if rx_try == 0 then return 0, 0 end
        return rw_try, rx_try
    end

    local JIT_SIZES = {0x100000, 0xC0000, 0x80000}
    for _, size in ipairs(JIT_SIZES) do
        ulog("JIT try 0x" .. string.format("%x", size))
        local r, x = jit_alloc(size)
        if r ~= 0 then
            rw, rx = r, x
            SC_SIZE = size
            ulog("JIT ok at 0x" .. string.format("%x", r) .. " size 0x" .. string.format("%x", size))
            break
        end
    end
end

if rw == 0 then
    ulog("FATAL: no RWX memory >= 512 KB")
    error("shellcode memory allocation failed (need 512 KB)")
end

ulog("shellcode dest rw=0x" .. string.format("%x", rw) ..
     " rx=0x" .. string.format("%x", rx) ..
     " size=0x" .. string.format("%x", SC_SIZE))

-- ============================================================
-- Shellcode port scan 5001..5020
-- ============================================================
local srv, sc_port = -1, 0
for p = SC_PORT_BASE, SC_PORT_MAX do
    local s = create_socket(AF_INET, SOCK_STREAM, 0)
    if s >= 0 then
        local reuse = malloc(4); write32(reuse, 1)
        syscall.setsockopt(s, 0xFFFF, 0x0004, reuse, 4)
        local sa2 = make_sockaddr_in(p)
        if syscall.bind(s, sa2, 16) == 0 and syscall.listen(s, 1) == 0 then
            srv, sc_port = s, p
            break
        end
        syscall.close(s)
    end
end
if srv < 0 then
    ulog("FATAL: no free shellcode port in " .. SC_PORT_BASE .. ".." .. SC_PORT_MAX)
    error("shellcode port allocation failed")
end
ulog("SCPORT " .. tostring(sc_port))

-- ============================================================
-- Receive shellcode over TCP
-- ============================================================
local function receive_shellcode(dest, srv_fd, max_size)
    local sa = make_sockaddr_in(sc_port)
    local alen = malloc(8); write32(alen, 16)
    ulog("awaiting shellcode on port " .. tostring(sc_port))
    local cfd = syscall.accept(srv_fd, sa, alen)
    if cfd < 0 then syscall.close(srv_fd); error("accept failed") end

    local total, err_msg = 0, nil
    while total < max_size do
        local n = syscall.read(cfd, dest + total, max_size - total)
        if n == 0 then break end
        if n < 0 then
            err_msg = "read error " .. tostring(n) .. " at offset " .. tostring(total)
            break
        end
        total = total + n
    end
    syscall.close(cfd)
    syscall.close(srv_fd)
    if err_msg then error(err_msg) end
    ulog("shellcode received " .. total .. " / " .. max_size .. " bytes")
    return total
end

local n = receive_shellcode(rw, srv, SC_SIZE)
if n < 0x4000 then
    error("short receive: got " .. tostring(n) .. " bytes, expected >= 16 KB")
end

-- ============================================================
-- ext_args layout
-- ============================================================
ulog("userId passed to shellcode = " .. tostring(real_uid))

local ext = malloc(0x80)
memset(ext, 0, 0x80)
write64(ext + 0x00, 0xDEAD)
write32(ext + 0x18, log_sock)   -- Valid socket file descriptor
write32(ext + 0x1C, -1)
if log_sa then
    for i = 0, 15 do write8(ext + 0x20 + i, read8(log_sa + i)) end
end
write64(ext + 0x30, real_uid)

-- ============================================================
-- Jump into shellcode
-- ============================================================
ulog("entering shellcode at 0x" .. string.format("%x", rx))
func_wrap(rx)(EBOOT_BASE, SCE_KERNEL_DLSYM, ext)

local frames = read32(ext + 0x10)
ulog("toolbox returned, frames=" .. tostring(frames))
