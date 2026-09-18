#!/usr/bin/env python3
"""
toolbox_launcher.py — LuaC0re Toolbox launcher (iPhone / desktop).

Mirrors the doom-ps launcher flow:
  1. Send toolbox_launcher.lua to LuaC0re on port 9026 (retries).
  2. TCP-scan ports 5001..5020 for the shellcode receiver.
  3. Stream toolbox.bin.
  4. Print live UDP debug logs on port 9027.

Cross-platform: Windows / Linux / macOS / iOS Pythonica / Android PyCode.
"""

import argparse, datetime, os, platform, socket, sys, threading, time

DEFAULT_CONSOLE_IP = "192.168.1.6"
DEFAULT_LAUNCHER   = "toolbox_launcher.lua"
DEFAULT_SHELLCODE  = "toolbox.bin"

PAYLOAD_PORT      = 9026
LOG_PORT          = 9027
SC_PORT_LO        = 5001
SC_PORT_HI        = 5021
CHUNK             = 64 * 1024

# Retry tuning (same idea as doom_launcher.py)
PAYLOAD_RETRIES       = 5
PAYLOAD_RETRY_DELAY   = 1.0
SHELLCODE_RETRIES     = 3
SHELLCODE_RETRY_DELAY = 1.5

IS_WINDOWS = os.name == "nt"
OS_NAME    = platform.system() or "Unknown"


def find_file(name, subdirs=("payloads", "lua", ".")):
    if os.path.isfile(name):
        return name
    for sub in subdirs:
        cand = os.path.join(sub, os.path.basename(name))
        if os.path.isfile(cand):
            return cand
    return None


def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        try:
            return socket.gethostbyname(socket.gethostname())
        except OSError:
            return "127.0.0.1"
    finally:
        s.close()


def make_udp_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    except OSError:
        pass
    if IS_WINDOWS and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
        try:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 0)
        except OSError:
            pass
    return s


class LogServer(threading.Thread):
    """UDP log listener — prints whatever the shellcode sends us.

    NOTE: `_stop` is an internal threading.Thread method — we must
    call our event `_stop_event` to avoid breaking Thread cleanup.
    """
    def __init__(self, port, host="0.0.0.0"):
        super().__init__(daemon=True)
        self.host, self.port = host, port
        self._stop_event = threading.Event()
        self.sock = None
        self.scport = None

    def run(self):
        s = make_udp_socket()
        for attempt in range(3):
            try:
                s.bind((self.host, self.port))
                break
            except OSError as e:
                if attempt == 2:
                    print(f"[log] WARN: cannot bind UDP "
                          f"{self.host}:{self.port} ({e}).")
                    if IS_WINDOWS:
                        print("[log] Windows: allow Python through Windows "
                              f"Defender Firewall for UDP {self.port}.")
                    s.close()
                    return
                time.sleep(0.5)

        s.settimeout(0.5)
        self.sock = s
        print(f"[log] UDP listening on {self.host}:{self.port}", flush=True)

        while not self._stop_event.is_set():
            try:
                data, addr = s.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break

            ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            msg = data.decode("utf-8", errors="replace").rstrip("\n")
            print(f"[{ts}] {addr[0]}  {msg}", flush=True)

            if msg.startswith("SCPORT "):
                try:
                    self.scport = int(msg.split()[1])
                except (ValueError, IndexError):
                    pass

        s.close()

    def stop(self):
        self._stop_event.set()
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass


# ============================================================
# Send toolbox_launcher.lua to LuaC0re (port 9026) — with retries
# ============================================================
def send_payload(host, filepath, port, retries=PAYLOAD_RETRIES):
    path = find_file(filepath)
    if not path:
        print(f"[!] Launcher not found: {filepath}")
        return False
    with open(path, "rb") as f:
        data = f.read()

    print(f"[1] Sending {os.path.basename(path)} "
          f"({len(data):,} bytes) -> {host}:{port}")

    last_err = None
    for attempt in range(1, retries + 1):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        try:
            s.connect((host, port))
            s.sendall(data)
            s.close()
            if attempt > 1:
                print(f"[1] Sent on attempt {attempt}")
            return True
        except Exception as e:
            last_err = e
            try:
                s.close()
            except OSError:
                pass
            if attempt < retries:
                print(f"[1] Attempt {attempt}/{retries} failed: {e}. "
                      f"Retrying in {PAYLOAD_RETRY_DELAY}s...")
                time.sleep(PAYLOAD_RETRY_DELAY)

    print(f"[!] Payload send failed after {retries} attempts: {last_err}")
    return False


# ============================================================
# Stream shellcode over TCP
# ============================================================
def _send_shellcode_once(host, port, data, size):
    """One attempt.  Returns True if the whole buffer was sent."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.5)
    try:
        s.connect((host, port))
    except (ConnectionRefusedError, OSError):
        s.close()
        return False

    print(f"[sc] Connected to {host}:{port}, sending {size:,} bytes")
    s.settimeout(None)
    try:
        sent = 0
        t0 = time.time()
        while sent < size:
            chunk = data[sent:sent + CHUNK]
            s.sendall(chunk)
            sent += len(chunk)
            pct = sent * 100 // size
            print(f"\r[sc] {sent:,}/{size:,} ({pct}%)", end="", flush=True)
        print()
        dt = max(time.time() - t0, 1e-6)
        print(f"[sc] Done — {sent:,} bytes in {dt:.1f}s "
              f"({sent / dt / 1024:.0f} KB/s)")
        s.close()
        return True
    except OSError as e:
        print(f"\n[!] Shellcode send broke at {sent:,}/{size:,}: {e}")
        try:
            s.close()
        except OSError:
            pass
        return False


def send_shellcode_to_port(host, port, path, retries=SHELLCODE_RETRIES):
    """Explicit-port override path (raw bytes, no framing)."""
    if not os.path.isfile(path):
        print(f"[!] Shellcode not found: {path}")
        return False
    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    for attempt in range(1, retries + 1):
        if attempt > 1:
            print(f"[sc] Retry {attempt}/{retries} in "
                  f"{SHELLCODE_RETRY_DELAY}s...")
            time.sleep(SHELLCODE_RETRY_DELAY)
        if _send_shellcode_once(host, port, data, size):
            return True
    print(f"[!] Shellcode failed after {retries} attempts")
    return False


def stream_shellcode(host, path,
                     port_lo=SC_PORT_LO, port_hi=SC_PORT_HI,
                     per_port_timeout=0.5, total_timeout=25,
                     retries=SHELLCODE_RETRIES):
    """Port-scan and stream, retrying the whole attempt on failure."""
    if not os.path.isfile(path):
        print(f"[!] Shellcode not found: {path}")
        return False
    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    for attempt in range(1, retries + 1):
        if attempt > 1:
            print(f"[sc] Retry {attempt}/{retries} in "
                  f"{SHELLCODE_RETRY_DELAY}s...")
            time.sleep(SHELLCODE_RETRY_DELAY)

        print(f"[sc] Scanning {host}:{port_lo}..{port_hi - 1} "
              f"for shellcode port")
        deadline = time.time() + total_timeout
        scan = 0
        found_and_sent = False

        while time.time() < deadline:
            scan += 1
            for port in range(port_lo, port_hi):
                if _send_shellcode_once(host, port, data, size):
                    found_and_sent = True
                    break
            if found_and_sent:
                return True
            if scan % 5 == 0:
                print(f"[sc]   ...no listener yet (attempt {attempt})")
            time.sleep(0.3)

        print(f"[sc] Port not found in {total_timeout}s "
              f"(attempt {attempt}/{retries})")

    print(f"[!] Shellcode failed after {retries} attempts")
    return False


# ============================================================
# Main
# ============================================================
def main():
    ap = argparse.ArgumentParser(
        description="LuaC0re Toolbox launcher (payload + shellcode)")
    ap.add_argument("host", nargs="?", default=DEFAULT_CONSOLE_IP,
                    help=f"console IP (default: {DEFAULT_CONSOLE_IP})")
    ap.add_argument("--launcher",  "-l", default=DEFAULT_LAUNCHER)
    ap.add_argument("--shellcode", "-s", default=DEFAULT_SHELLCODE)
    ap.add_argument("--payload-port", type=int, default=PAYLOAD_PORT)
    ap.add_argument("--log-port",     type=int, default=LOG_PORT)
    ap.add_argument("--local-ip",     default=None,
                    help="override local IP (for PC_IP in the .lua)")
    ap.add_argument("--no-log",       action="store_true",
                    help="disable the UDP log listener")
    ap.add_argument("--no-shellcode", action="store_true",
                    help="skip shellcode transfer (payload only)")
    ap.add_argument("--scport",       type=int, default=None,
                    help="skip TCP scan and use this shellcode port")
    ap.add_argument("--scport-wait",  type=int, default=25,
                    help="total seconds to spend finding the SC port")
    ap.add_argument("--shellcode-delay", type=float, default=1.0,
                    help="seconds to wait after payload send before SC")
    a = ap.parse_args()

    print("=" * 60)
    print(" LuaC0re Toolbox launcher")
    print(f" Host OS: {OS_NAME}")
    print("=" * 60)

    sc_path = None
    if not a.no_shellcode:
        sc_path = find_file(a.shellcode)
        if not sc_path:
            print(f"[!] Shellcode not found: {a.shellcode}")
            return 1
        print(f"[*] Shellcode: {sc_path} "
              f"({os.path.getsize(sc_path):,} bytes)")

    local_ip = a.local_ip or get_local_ip()
    print(f"[*] Console IP  : {a.host}")
    print(f"[*] Host IP     : {local_ip}")
    print(f"[*] toolbox_launcher.lua must have PC_IP = \"{local_ip}\"")
    if IS_WINDOWS and local_ip == "127.0.0.1":
        print("[*] Windows: no default route detected — pass --local-ip "
              "with your LAN address.")

    log_thread = None
    if not a.no_log:
        log_thread = LogServer(a.log_port)
        log_thread.start()
        time.sleep(0.2)

    print()

    # ---------- 1. Payload ----------
    if not send_payload(a.host, a.launcher, a.payload_port):
        if log_thread:
            log_thread.stop()
        return 1

    if a.shellcode_delay > 0:
        time.sleep(a.shellcode_delay)

    # ---------- 2. Shellcode ----------
    if sc_path:
        if a.scport is not None:
            print(f"[sc] Using --scport {a.scport} (override)")
            ok = send_shellcode_to_port(a.host, a.scport, sc_path)
        else:
            ok = stream_shellcode(a.host, sc_path,
                                  total_timeout=a.scport_wait)
        if not ok:
            if log_thread:
                log_thread.stop()
            return 1

    # ---------- 3. Watch logs ----------
    if log_thread:
        print("\n[*] Watching logs — Ctrl-C to quit.")
        try:
            while log_thread.is_alive():
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\n[*] Stopping…")
        finally:
            log_thread.stop()

    print("Done!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
