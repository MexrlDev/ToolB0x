#!/usr/bin/env python3
"""
toolbox_launcher.py - LuaC0re Toolbox launcher (iPhone / desktop).

v3.2:
  - Terminal output is now 100% ASCII.
  - "Watching logs" prompt shows both PC (Ctrl+C) and phone (Stop) stop.
  - Fix (v3.1): only replace the exact PC_IP declaration line.
  - --debug-logs true|false argument (default true).
"""

import argparse
import datetime
import os
import platform
import re
import socket
import sys
import threading
import time

DEFAULT_CONSOLE_IP = "192.168.1.6" # Your luac0re ip (ps4 ip)
DEFAULT_LAUNCHER   = "toolbox_launcher.lua"
DEFAULT_SHELLCODE  = "toolbox.bin"

PAYLOAD_PORT      = 9026
LOG_PORT          = 9027
SC_PORT_LO        = 5001
SC_PORT_HI        = 5021
CHUNK             = 64 * 1024

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
    """UDP log listener - prints whatever the shellcode sends us."""

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
                    print("[log] WARN: cannot bind UDP "
                          + self.host + ":" + str(self.port)
                          + " (" + str(e) + ").")
                    if IS_WINDOWS:
                        print("[log] Windows: allow Python through the "
                              "firewall for UDP " + str(self.port) + ".")
                    s.close()
                    return
                time.sleep(0.5)

        s.settimeout(0.5)
        self.sock = s
        print("[log] UDP listening on " + self.host + ":" + str(self.port),
              flush=True)

        while not self._stop_event.is_set():
            try:
                data, addr = s.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break

            ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            msg = data.decode("utf-8", errors="replace").rstrip("\n")
            msg = msg.encode("ascii", errors="replace").decode("ascii")
            print("[" + ts + "] " + addr[0] + "  " + msg, flush=True)

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


def send_payload(host, filepath, port, pc_ip=None, retries=PAYLOAD_RETRIES):
    path = find_file(filepath)
    if not path:
        print("[!] Launcher not found: " + str(filepath))
        return False

    try:
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
    except UnicodeDecodeError:
        with open(path, "r") as f:
            text = f.read()

    replacement = pc_ip if pc_ip else ""

    decl_pattern = 'local PC_IP        = "__PC_IP__"'
    decl_new     = 'local PC_IP        = "' + replacement + '"'
    if decl_pattern in text:
        text = text.replace(decl_pattern, decl_new, 1)
        print('[1]   Decl line replaced: PC_IP = "' + replacement + '"')
    else:
        pattern = r'(local\s+PC_IP\s*=\s*)"__PC_IP__"'
        text, n = re.subn(pattern, r'\g<1>"' + replacement + '"', text, count=1)
        if n == 0:
            print("[!]   No PC_IP declaration found - file unchanged")

    data = text.encode("utf-8")

    print("[1] Sending " + os.path.basename(path)
          + " (" + format(len(data), ",") + " bytes) -> "
          + host + ":" + str(port))
    if replacement:
        print('[1]   Injected PC_IP = "' + replacement + '"')
    else:
        print('[1]   Debug logs disabled (PC_IP = "")')

    last_err = None
    for attempt in range(1, retries + 1):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        try:
            s.connect((host, port))
            s.sendall(data)
            s.close()
            if attempt > 1:
                print("[1] Sent on attempt " + str(attempt))
            return True
        except Exception as e:
            last_err = e
            try:
                s.close()
            except OSError:
                pass
            if attempt < retries:
                print("[1] Attempt " + str(attempt) + "/" + str(retries)
                      + " failed: " + str(e) + ". Retrying in "
                      + str(PAYLOAD_RETRY_DELAY) + "s...")
                time.sleep(PAYLOAD_RETRY_DELAY)

    print("[!] Payload send failed after " + str(retries)
          + " attempts: " + str(last_err))
    return False


def _send_shellcode_once(host, port, data, size):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.5)
    try:
        s.connect((host, port))
    except (ConnectionRefusedError, OSError):
        s.close()
        return False

    print("[sc] Connected to " + host + ":" + str(port)
          + ", sending " + format(size, ",") + " bytes")
    s.settimeout(None)
    try:
        sent = 0
        t0 = time.time()
        while sent < size:
            chunk = data[sent:sent + CHUNK]
            s.sendall(chunk)
            sent += len(chunk)
            pct = sent * 100 // size
            print("\r[sc] " + format(sent, ",") + "/" + format(size, ",")
                  + " (" + str(pct) + "%)", end="", flush=True)
        print()
        dt = max(time.time() - t0, 1e-6)
        print("[sc] Done - " + format(sent, ",") + " bytes in "
              + format(dt, ".1f") + "s ("
              + format(sent / dt / 1024, ".0f") + " KB/s)")
        s.close()
        return True
    except OSError as e:
        print("\n[!] Shellcode send broke at "
              + format(sent, ",") + "/" + format(size, ",")
              + ": " + str(e))
        try:
            s.close()
        except OSError:
            pass
        return False


def send_shellcode_to_port(host, port, path, retries=SHELLCODE_RETRIES):
    if not os.path.isfile(path):
        print("[!] Shellcode not found: " + str(path))
        return False
    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    for attempt in range(1, retries + 1):
        if attempt > 1:
            print("[sc] Retry " + str(attempt) + "/" + str(retries)
                  + " in " + str(SHELLCODE_RETRY_DELAY) + "s...")
            time.sleep(SHELLCODE_RETRY_DELAY)
        if _send_shellcode_once(host, port, data, size):
            return True
    print("[!] Shellcode failed after " + str(retries) + " attempts")
    return False


def stream_shellcode(host, path,
                     port_lo=SC_PORT_LO, port_hi=SC_PORT_HI,
                     per_port_timeout=0.5, total_timeout=25,
                     retries=SHELLCODE_RETRIES):
    if not os.path.isfile(path):
        print("[!] Shellcode not found: " + str(path))
        return False
    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    for attempt in range(1, retries + 1):
        if attempt > 1:
            print("[sc] Retry " + str(attempt) + "/" + str(retries)
                  + " in " + str(SHELLCODE_RETRY_DELAY) + "s...")
            time.sleep(SHELLCODE_RETRY_DELAY)

        print("[sc] Scanning " + host + ":" + str(port_lo)
              + ".." + str(port_hi - 1) + " for shellcode port")
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
                print("[sc]   ...no listener yet (attempt "
                      + str(attempt) + ")")
            time.sleep(0.3)

        print("[sc] Port not found in " + str(total_timeout)
              + "s (attempt " + str(attempt) + "/" + str(retries) + ")")

    print("[!] Shellcode failed after " + str(retries) + " attempts")
    return False


def str_to_bool(v):
    if isinstance(v, bool):
        return v
    return str(v).strip().lower() in ("1", "true", "yes", "on", "y", "t")


def main():
    ap = argparse.ArgumentParser(
        description="LuaC0re Toolbox launcher (payload + shellcode)")
    ap.add_argument("host", nargs="?", default=DEFAULT_CONSOLE_IP,
                    help="console IP (default: " + DEFAULT_CONSOLE_IP + ")")
    ap.add_argument("--launcher",  "-l", default=DEFAULT_LAUNCHER)
    ap.add_argument("--shellcode", "-s", default=DEFAULT_SHELLCODE)
    ap.add_argument("--payload-port", type=int, default=PAYLOAD_PORT)
    ap.add_argument("--log-port",     type=int, default=LOG_PORT)
    ap.add_argument("--debug-logs", type=str_to_bool, default=True,
                    metavar="true|false",
                    help="capture UDP debug logs from the console "
                         "(default: true).")
    ap.add_argument("--local-ip",     default=None,
                    help="override auto-detected host LAN IP")
    ap.add_argument("--no-shellcode", action="store_true")
    ap.add_argument("--scport",       type=int, default=None)
    ap.add_argument("--scport-wait",  type=int, default=25)
    ap.add_argument("--shellcode-delay", type=float, default=1.0)
    a = ap.parse_args()

    print("=" * 60)
    print(" LuaC0re Toolbox launcher (v3.2)")
    print(" Host OS: " + OS_NAME)
    print(" Debug logs: " + ("ENABLED" if a.debug_logs else "DISABLED"))
    print("=" * 60)

    sc_path = None
    if not a.no_shellcode:
        sc_path = find_file(a.shellcode)
        if not sc_path:
            print("[!] Shellcode not found: " + str(a.shellcode))
            return 1
        print("[*] Shellcode: " + sc_path + " ("
              + format(os.path.getsize(sc_path), ",") + " bytes)")

    pc_ip = ""
    if a.debug_logs:
        pc_ip = a.local_ip or get_local_ip()
        if pc_ip == "127.0.0.1":
            print("[*] WARN: no LAN route detected - pass --local-ip")

    print("[*] Console IP  : " + str(a.host))
    if pc_ip:
        print("[*] Host IP     : " + pc_ip)
    else:
        print("[*] Host IP     : (logs disabled - no UDP listener)")

    log_thread = None
    if a.debug_logs and pc_ip:
        log_thread = LogServer(a.log_port)
        log_thread.start()
        time.sleep(0.2)

    print()

    if not send_payload(a.host, a.launcher, a.payload_port, pc_ip=pc_ip):
        if log_thread:
            log_thread.stop()
        return 1

    if a.shellcode_delay > 0:
        time.sleep(a.shellcode_delay)

    if sc_path:
        if a.scport is not None:
            print("[sc] Using --scport " + str(a.scport) + " (override)")
            ok = send_shellcode_to_port(a.host, a.scport, sc_path)
        else:
            ok = stream_shellcode(a.host, sc_path,
                                  total_timeout=a.scport_wait)
        if not ok:
            if log_thread:
                log_thread.stop()
            return 1

    if log_thread:
        print()
        print("[*] Watching logs. To stop:")
        print("[*]   PC    -> press Ctrl+C")
        print("[*]   Phone -> press the Stop button")
        try:
            while log_thread.is_alive():
                time.sleep(0.5)
        except KeyboardInterrupt:
            print()
            print("[*] Stopping...")
        finally:
            log_thread.stop()
    else:
        print()
        print("[*] No debug listener - exiting.")

    print("Done!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
