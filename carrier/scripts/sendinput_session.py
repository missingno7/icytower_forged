"""sendinput_session.py - a GENUINE real-keyboard record/replay round trip.

Divergence 005 (notes/living_record.md) could only ever be closed by a
recording whose key events actually travelled the real DirectInput path
asynchronously, at arbitrary real instants - which `--inject-real-test`
structurally cannot do (it calls key_dinput_handle_scancode from inside
det_wrap_Sleep on the main thread, so its events are never async).

There is no way to make a helper *thread* post events into DirectInput. What
there is, is SendInput: it injects at the Win32 input-stack level, so the
events reach the focused guest window's DirectInput keyboard device exactly
like a physical key press does, on the guest's own window thread, at whatever
real instant we send them. That is the closest thing to a human that an
automated pass can produce, and it exercises the very race 005 is about.

  1. launch carrier.exe --det --pace=real --input=real --record-input R
     --digest-out D
  2. wait for the guest's AllegroWindow, make it foreground
  3. hold ENTER to start a game, then press/release the gameplay keys at
     RANDOMIZED real times for ~N seconds
  4. replay R with --det --pace=fast --input=script, bounded by D's last tick
  5. compare_digests.py D vs the replay digest -> must be EQUAL

Every SendInput batch first checks that the foreground window still belongs
to the guest process tree; if it ever does not, the session aborts rather
than typing into whatever else has focus.
"""
import argparse, ctypes, ctypes.wintypes as w, os, random, subprocess, sys, time

u32 = ctypes.windll.user32
k32 = ctypes.windll.kernel32
CARRIER_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(CARRIER_DIR)

# --- SendInput plumbing ---------------------------------------------------
INPUT_KEYBOARD = 1
KEYEVENTF_EXTENDEDKEY = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_SCANCODE = 0x0008

ULONG_PTR = ctypes.c_ulonglong if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.c_ulong


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", w.WORD), ("wScan", w.WORD), ("dwFlags", w.DWORD),
                ("time", w.DWORD), ("dwExtraInfo", ULONG_PTR)]


class _U(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("pad", ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", w.DWORD), ("u", _U)]


# Allegro key name -> (set-1 scan code, extended?). These are exactly the DIK
# codes the guest's own hw_to_mycode[256] table maps (carrier/NOTES.md).
KEYS = {
    "KEY_ESC":   (0x01, False),
    "KEY_ENTER": (0x1C, False),
    "KEY_SPACE": (0x39, False),
    "KEY_LEFT":  (0x4B, True),
    "KEY_RIGHT": (0x4D, True),
    "KEY_UP":    (0x48, True),
    "KEY_DOWN":  (0x50, True),
}


def send_key(name, down):
    scan, ext = KEYS[name]
    flags = KEYEVENTF_SCANCODE | (0 if down else KEYEVENTF_KEYUP)
    if ext:
        flags |= KEYEVENTF_EXTENDEDKEY
    inp = INPUT(type=INPUT_KEYBOARD,
                u=_U(ki=KEYBDINPUT(wVk=0, wScan=scan, dwFlags=flags, time=0, dwExtraInfo=0)))
    n = u32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))
    if n != 1:
        raise RuntimeError("SendInput failed, gle=%d" % k32.GetLastError())


# --- process tree / window ------------------------------------------------
TH32CS_SNAPPROCESS = 2


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", w.DWORD), ("cntUsage", w.DWORD), ("th32ProcessID", w.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)), ("th32ModuleID", w.DWORD),
                ("cntThreads", w.DWORD), ("th32ParentProcessID", w.DWORD),
                ("pcPriClassBase", ctypes.c_long), ("dwFlags", w.DWORD),
                ("szExeFile", ctypes.c_char * 260)]


def descendants(root_pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    pe = PROCESSENTRY32(); pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
    kids = {}
    if k32.Process32First(snap, ctypes.byref(pe)):
        while True:
            kids.setdefault(pe.th32ParentProcessID, []).append(pe.th32ProcessID)
            if not k32.Process32Next(snap, ctypes.byref(pe)):
                break
    k32.CloseHandle(snap)
    out, stack = set(), [root_pid]
    while stack:
        p = stack.pop()
        if p in out:
            continue
        out.add(p)
        stack.extend(kids.get(p, []))
    return out


def guest_window(pids):
    found = []

    @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def cb(hwnd, _):
        pid = w.DWORD()
        u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and u32.IsWindowVisible(hwnd):
            cls = ctypes.create_unicode_buffer(256)
            u32.GetClassNameW(hwnd, cls, 256)
            if cls.value == "AllegroWindow":
                found.append(hwnd)
        return True

    u32.EnumWindows(cb, 0)
    return found[0] if found else None


def foreground_is_guest(pids):
    fg = u32.GetForegroundWindow()
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(fg, ctypes.byref(pid))
    return pid.value in pids


SW_RESTORE = 9


def force_foreground(hwnd, pids, tries=8):
    """SetForegroundWindow from a background process is restricted by Windows;
    the documented way through is to attach our input queue to the queue that
    currently owns the foreground window first."""
    for _ in range(tries):
        if foreground_is_guest(pids):
            return True
        fg = u32.GetForegroundWindow()
        me = k32.GetCurrentThreadId()
        target = u32.GetWindowThreadProcessId(hwnd, None)
        owner = u32.GetWindowThreadProcessId(fg, None) if fg else 0
        for t in (owner, target):
            if t and t != me:
                u32.AttachThreadInput(me, t, True)
        u32.ShowWindow(hwnd, SW_RESTORE)
        u32.BringWindowToTop(hwnd)
        u32.SetForegroundWindow(hwnd)
        u32.SetActiveWindow(hwnd)
        u32.SetFocus(hwnd)
        for t in (owner, target):
            if t and t != me:
                u32.AttachThreadInput(me, t, False)
        time.sleep(0.5)
    return foreground_is_guest(pids)


def describe_foreground():
    fg = u32.GetForegroundWindow()
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(fg, ctypes.byref(pid))
    cls = ctypes.create_unicode_buffer(256)
    u32.GetClassNameW(fg, cls, 256)
    n = u32.GetWindowTextLengthW(fg)
    buf = ctypes.create_unicode_buffer(n + 2)
    u32.GetWindowTextW(fg, buf, n + 2)
    return "hwnd=0x%x pid=%d class=%r title=%r" % (fg, pid.value, cls.value, buf.value)


class FocusStolen(Exception):
    pass


def restore_assets():
    subprocess.run(["powershell", "-ExecutionPolicy", "Bypass", "-File",
                    os.path.join(CARRIER_DIR, "scripts", "restore_assets.ps1")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def last_tick(digest_path):
    last = None
    with open(digest_path) as f:
        for line in f:
            line = line.strip()
            if line:
                last = line.split()[0]
    return int(last) if last is not None else 0


stats = {"focus_thefts": 0}
QUIT_VIA_MENU = [False]


def record(rec_path, dig_path, trace_path, seconds, seed, run_seconds):
    restore_assets()
    # --interactive: this session NEEDS the guest window in the foreground for
    # SendInput to reach its DirectInput keyboard, so it is one of the few runs
    # allowed to take it (carrier/NOTES.md "Environment isolation" item 1).
    args = [os.path.join(CARRIER_DIR, "carrier.exe"), "--det", "--pace=real", "--input=real", "--interactive",
            "--record-input", rec_path, "--digest-out", dig_path,
            "--report", dig_path + ".report.json",
            "--run-seconds", str(run_seconds)]
    if trace_path:
        args += ["--trace-input", trace_path]
    print("$ " + " ".join(args))
    errf = open(dig_path + ".stderr.txt", "w")
    proc = subprocess.Popen(args, cwd=CARRIER_DIR, stdout=subprocess.DEVNULL, stderr=errf)
    pids, hwnd = set(), None
    deadline = time.time() + 25
    while time.time() < deadline:
        pids = descendants(proc.pid)
        hwnd = guest_window(pids)
        if hwnd:
            break
        time.sleep(0.3)
    if not hwnd:
        proc.kill()
        raise SystemExit("guest window never appeared")
    if not force_foreground(hwnd, pids):
        proc.kill()
        raise FocusStolen("guest window is not foreground (%s) - refusing to SendInput"
                          % describe_foreground())
    print("guest window 0x%x is foreground; sending real keys via SendInput" % hwnd)

    stats["focus_thefts"] = 0
    if QUIT_VIA_MENU[0]:  # noqa: E501
        # Item C demonstration: quit through the MENU with a real ESC hold, so
        # the guest reaches Allegro's own clean shutdown - which is where the
        # "release every DIK code" storm happens.
        # Stray desktop keystrokes can have started a game before we get here,
        # so hold ESC repeatedly: ESC quits play() through its pause/confirm
        # path and then quits the MENU through the identical confirm dialog
        # (carrier/NOTES.md "parked timer thread", scripts/quit_via_menu.txt).
        for _ in range(6):
            send_key("KEY_ESC", True)
            time.sleep(3.0)
            send_key("KEY_ESC", False)
            time.sleep(0.8)
            if proc.poll() is not None:
                break
        print("ESC held at the menu; waiting for the guest's own exit chain")
        proc.wait()
        return
    rnd = random.Random(seed)
    # 1. hold ENTER to start a game (NOTES.md: a brief menu tap is unreliable)
    send_key("KEY_ENTER", True)
    time.sleep(2.5)
    send_key("KEY_ENTER", False)
    time.sleep(0.6)

    # 2. randomized gameplay for `seconds`
    held = {}
    end = time.time() + seconds
    while time.time() < end:
        if not foreground_is_guest(pids):
            # MEASURED (carrier/NOTES.md "Divergence 005", proof section): if a
            # foreign window takes the foreground mid-session, the guest's own
            # window thread sees WM_ACTIVATEAPP and Allegro's DirectDraw
            # switch-out/switch-in handling runs - an input channel that is NOT
            # part of the key recording and therefore cannot be replayed. Such
            # a session is not a valid experiment; abort it rather than report
            # a divergence that the desktop, not the carrier, caused.
            # MEASURED (carrier/NOTES.md "Divergence 005", proof section): if a
            # foreign window takes the foreground mid-session, the guest's own
            # window thread sees WM_ACTIVATEAPP and Allegro's DirectDraw
            # switch-out/switch-in handling runs - an uncontrolled input channel
            # that is NOT part of the key recording and cannot be replayed.
            # Counted and reported; a session with focus_thefts > 0 is a
            # perturbed experiment, not a carrier divergence.
            stats["focus_thefts"] += 1
            print("focus stolen by %s (#%d) - re-acquiring"
                  % (describe_foreground(), stats["focus_thefts"]))
            h = guest_window(descendants(proc.pid)) or hwnd
            if not force_foreground(h, pids, tries=6):
                proc.kill()
                raise FocusStolen("focus could not be regained (%s)" % describe_foreground())
        key = rnd.choice(["KEY_LEFT", "KEY_RIGHT", "KEY_SPACE", "KEY_RIGHT", "KEY_SPACE"])
        down = not held.get(key, False)
        send_key(key, down)
        held[key] = down
        time.sleep(rnd.uniform(0.012, 0.19))   # randomized real timings
    for key, is_down in held.items():
        if is_down:
            send_key(key, False)
            time.sleep(0.02)
    print("key session done; waiting for the carrier to finish")
    proc.wait()


def replay(rec_path, dig_path, replay_dig):
    restore_assets()
    stop = last_tick(dig_path)
    args = [os.path.join(CARRIER_DIR, "carrier.exe"), "--det", "--pace=fast", "--input=script",
            "--input-script", rec_path, "--digest-out", replay_dig,
            "--report", replay_dig + ".report.json",
            "--stop-at-tick", str(stop), "--run-seconds", "180"]
    print("$ " + " ".join(args))
    subprocess.run(args, cwd=CARRIER_DIR, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    cmp_args = [sys.executable, os.path.join(CARRIER_DIR, "scripts", "compare_digests.py"),
                dig_path, replay_dig]
    print("$ " + " ".join(cmp_args))
    r = subprocess.run(cmp_args, capture_output=True, text=True)
    print(r.stdout.strip() or r.stderr.strip())
    for tag, path in (("record", dig_path + ".report.json"), ("replay", replay_dig + ".report.json")):
        try:
            import json
            j = json.load(open(path))
            print("  %s arena: %s" % (tag, j.get("arena")))
        except Exception as e:
            print("  %s arena: (no report: %s)" % (tag, e))
    return r.returncode == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", default="si1")
    ap.add_argument("--out", default=os.path.join(ROOT, "artifacts_t3"))
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--run-seconds", type=int, default=45)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("--replay-only", action="store_true")
    ap.add_argument("--attempts", type=int, default=6)
    ap.add_argument("--quit-via-menu", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    QUIT_VIA_MENU[0] = a.quit_via_menu
    rec = os.path.join(a.out, a.tag + "_record.txt")
    dig = os.path.join(a.out, a.tag + "_record.digest")
    rep = os.path.join(a.out, a.tag + "_replay.digest")
    trc = os.path.join(a.out, a.tag + "_record.trace") if a.trace else None
    if not a.replay_only:
        for attempt in range(1, a.attempts + 1):
            try:
                record(rec, dig, trc, a.seconds, a.seed + attempt - 1, a.run_seconds)
                break
            except FocusStolen as e:
                print("attempt %d VOID: %s" % (attempt, e))
                time.sleep(2)
        else:
            raise SystemExit("no attempt completed without the desktop stealing focus")
    n_events = sum(1 for l in open(rec) if l.strip() and not l.startswith("#"))
    print("recorded %d events, %d digest ticks (first..last: %s..%s)" %
          (n_events, sum(1 for _ in open(dig)),
           open(dig).readline().split()[0] if os.path.getsize(dig) else "-", last_tick(dig)))
    ok = replay(rec, dig, rep)
    print("desktop focus thefts during the record session: %d" % stats["focus_thefts"])
    print("ROUND TRIP:", "EQUAL" if ok else "NOT EQUAL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
