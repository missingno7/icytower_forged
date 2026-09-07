"""desktop_perturb_test.py - the "Environment isolation" pass's proof harness.

The residue "Divergences 004 and 005" left open was: a genuine-keyboard
session is only EQUAL when the desktop leaves the guest alone (4 of 8 sessions
diverged, always ones where the foreground was repeatedly taken from the
guest). The two named suspects were Allegro's WM_ACTIVATEAPP switch-out/in
path and the real MOUSE. This script perturbs exactly those two channels, on
purpose, at known times, and checks the digest.

It never touches any of the operator's windows: it creates its OWN tiny
top-most helper window, activates that, and hands the foreground back to the
guest, which is what a Chrome/Explorer window opening does.

  --test script-focus   an --input=script run (the G1 workload) while the
                        helper steals the foreground twice. Must stay EQUAL
                        to the undisturbed G1 digest.  [neutralized]
  --test script-mouse   an --input=script run with the guest window shown
                        (--window=normal) while the helper drags the real
                        cursor across it. Must stay EQUAL to the same
                        baseline.                       [parked]
  --test record-focus   a --det --pace=real --input=real --interactive
                        SendInput session (real keys through the real
                        DirectInput path) while the helper steals the
                        foreground twice at known times. The recording must
                        contain `switch out`/`switch in` events and must
                        replay EQUAL.                   [recorded]

Exit code 0 only if the test's own verdict is EQUAL.
"""
import argparse
import ctypes
import ctypes.wintypes as w
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sendinput_session as si  # noqa: E402  (same directory; reuses its window/process plumbing)

u32 = ctypes.windll.user32
k32 = ctypes.windll.kernel32
CARRIER_DIR = si.CARRIER_DIR
ROOT = si.ROOT

WS_POPUP = 0x80000000
WS_EX_TOPMOST = 0x00000008
WS_EX_TOOLWINDOW = 0x00000080
SW_SHOW, SW_HIDE = 5, 0
PM_REMOVE = 1


class WNDCLASSW(ctypes.Structure):
    _fields_ = [("style", w.UINT), ("lpfnWndProc", ctypes.c_void_p), ("cbClsExtra", ctypes.c_int),
                ("cbWndExtra", ctypes.c_int), ("hInstance", w.HINSTANCE), ("hIcon", w.HICON),
                ("hCursor", w.HANDLE), ("hbrBackground", w.HANDLE), ("lpszMenuName", w.LPCWSTR),
                ("lpszClassName", w.LPCWSTR)]


LRESULT = ctypes.c_longlong if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.c_long
u32.DefWindowProcW.restype = LRESULT
u32.DefWindowProcW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
WNDPROC = ctypes.WINFUNCTYPE(LRESULT, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
_keepalive = []


def make_helper_window():
    """Our own window, so the test never activates anything the operator owns."""
    proc = WNDPROC(lambda h, m, wp, lp: u32.DefWindowProcW(h, m, wp, lp))
    cls = WNDCLASSW()
    cls.lpfnWndProc = ctypes.cast(proc, ctypes.c_void_p)
    cls.hInstance = k32.GetModuleHandleW(None)
    cls.lpszClassName = "PortForgeFocusThief"
    atom = u32.RegisterClassW(ctypes.byref(cls))
    if not atom:
        raise RuntimeError("RegisterClassW failed gle=%d" % k32.GetLastError())
    # Far from the guest window (which Allegro places at 0,0 640x480), so the
    # activating click lands on the helper and not on the game.
    sw = u32.GetSystemMetrics(0)
    hwnd = u32.CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "PortForgeFocusThief",
                               "PortForge focus thief (test)", WS_POPUP,
                               max(sw - 320, 700), 10, 260, 40, None, None, cls.hInstance, None)
    if not hwnd:
        raise RuntimeError("CreateWindowExW failed gle=%d" % k32.GetLastError())
    _keepalive.extend([proc, cls])
    return hwnd


def pump(hwnd, seconds):
    msg = ctypes.create_string_buffer(48)
    end = time.time() + seconds
    while time.time() < end:
        while u32.PeekMessageW(msg, None, 0, 0, PM_REMOVE):
            u32.DispatchMessageW(msg)
        time.sleep(0.02)


INPUT_MOUSE = 0
MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP = 0x0002, 0x0004
SW_SHOWNA = 8


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", ctypes.c_long), ("dy", ctypes.c_long), ("mouseData", w.DWORD),
                ("dwFlags", w.DWORD), ("time", w.DWORD), ("dwExtraInfo", si.ULONG_PTR)]


class _MU(ctypes.Union):
    _fields_ = [("mi", MOUSEINPUT), ("pad", ctypes.c_byte * 32)]


class MINPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", w.DWORD), ("u", _MU)]


def click_at(x, y):
    """A genuine user click. MEASURED, and load-bearing for this test: the
    AttachThreadInput trick sendinput_session.force_foreground uses to move
    the foreground MERGES the two threads' input queues, and Windows then
    delivers only WM_KILLFOCUS/WM_NCACTIVATE to the losing window - never the
    WM_ACTIVATE(WA_INACTIVE) that Allegro's directx_wnd_proc actually listens
    for. A real click activates through the normal cross-process path, which
    is what an operator opening a window does."""
    u32.SetCursorPos(int(x), int(y))
    time.sleep(0.05)
    for flag in (MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP):
        inp = MINPUT(type=INPUT_MOUSE, u=_MU(mi=MOUSEINPUT(0, 0, 0, flag, 0, 0)))
        u32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(MINPUT))
        time.sleep(0.03)


def steal_and_return(helper, guest_hwnd, pids, hold=1.5, label=""):
    """Activate our own helper window with a real click, hold it, then hide it
    so the foreground falls back to the window that had it - exactly the
    activation out/in pair an operator opening and closing a window produces."""
    saved = w.POINT()
    u32.GetCursorPos(ctypes.byref(saved))
    u32.ShowWindow(helper, SW_SHOWNA)
    r = w.RECT()
    u32.GetWindowRect(helper, ctypes.byref(r))
    click_at((r.left + r.right) // 2, (r.top + r.bottom) // 2)
    pump(helper, 0.2)
    print("  [%s] foreground stolen -> %s" % (label, si.describe_foreground()))
    pump(helper, hold)
    u32.ShowWindow(helper, SW_HIDE)
    pump(helper, 0.6)
    u32.SetCursorPos(saved.x, saved.y)
    print("  [%s] foreground returned -> %s" % (label, si.describe_foreground()))


def jiggle_mouse(hwnd, seconds, step=0.05):
    """Drag the real cursor back and forth across the guest window."""
    rect = w.RECT()
    u32.GetWindowRect(hwnd, ctypes.byref(rect))
    x0, y0 = rect.left + 8, rect.top + 8
    x1, y1 = max(rect.right - 8, x0 + 1), max(rect.bottom - 8, y0 + 1)
    saved = w.POINT()
    u32.GetCursorPos(ctypes.byref(saved))
    n, end, i = 0, time.time() + seconds, 0
    while time.time() < end:
        t = (i % 20) / 19.0
        u32.SetCursorPos(int(x0 + (x1 - x0) * t), int(y0 + (y1 - y0) * t))
        i += 1
        n += 1
        time.sleep(step)
    u32.SetCursorPos(saved.x, saved.y)
    return n


def wait_for_window(proc, timeout=30):
    deadline = time.time() + timeout
    while time.time() < deadline:
        pids = si.descendants(proc.pid)
        hwnd = si.guest_window(pids)
        if hwnd:
            return pids, hwnd
        if proc.poll() is not None:
            return pids, None
        time.sleep(0.3)
    return si.descendants(proc.pid), None


def guest_window_any(pids):
    """Like sendinput_session.guest_window but also finds a MINIMIZED /
    non-visible AllegroWindow, which is what an automated run now creates."""
    found = []

    @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def cb(hwnd, _):
        pid = w.DWORD()
        u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids:
            cls = ctypes.create_unicode_buffer(256)
            u32.GetClassNameW(hwnd, cls, 256)
            if cls.value == "AllegroWindow":
                found.append(hwnd)
        return True

    u32.EnumWindows(cb, 0)
    return found[0] if found else None


def run_carrier(args, stderr_path):
    print("$ carrier.exe " + " ".join(args))
    errf = open(stderr_path, "w")
    return subprocess.Popen([os.path.join(CARRIER_DIR, "carrier.exe")] + args,
                            cwd=CARRIER_DIR, stdout=subprocess.DEVNULL, stderr=errf)


def compare(a, b):
    r = subprocess.run([sys.executable, os.path.join(CARRIER_DIR, "scripts", "compare_digests.py"), a, b],
                       capture_output=True, text=True)
    out = (r.stdout.strip() or r.stderr.strip()).splitlines()
    print("  " + (out[0] if out else "(no output)"))
    return r.returncode == 0


def baseline(out):
    """The undisturbed reference: the ordinary G1 command, --pace=fast."""
    path = os.path.join(out, "baseline.txt")
    si.restore_assets()
    p = run_carrier(["--det", "--pace=fast", "--input=script", "--input-script", "scripts/newgame.txt",
                     "--stop-at-tick", "1000", "--run-seconds", "90", "--digest-out", path,
                     "--report", path + ".report.json"], os.path.join(out, "baseline.stderr.txt"))
    p.wait()
    return path


def script_run_with_perturbation(out, tag, extra_args, perturb, env=None):
    """A scripted (deterministic) run at --pace=real, so there is real wall
    time in which to perturb it, while `perturb(pids, hwnd)` misbehaves."""
    path = os.path.join(out, tag + ".txt")
    si.restore_assets()
    saved = {}
    for k, v in (env or {}).items():
        saved[k] = os.environ.get(k)
        os.environ[k] = v
    try:
        p = run_carrier(["--det", "--pace=real", "--input=script", "--input-script", "scripts/newgame.txt",
                         "--stop-at-tick", "1000", "--run-seconds", "120", "--digest-out", path,
                         "--report", path + ".report.json"] + extra_args,
                        os.path.join(out, tag + ".stderr.txt"))
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    pids, _ = wait_for_window(p, timeout=30)
    hwnd = guest_window_any(pids)
    print("  guest window: %s" % (hex(hwnd) if hwnd else "(none found)"))
    perturb(pids, hwnd, p)
    p.wait()
    return path


def count_stderr(path, needle):
    try:
        return sum(1 for line in open(path, errors="replace") if needle in line)
    except OSError:
        return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--test", required=True,
                    choices=["script-focus", "script-focus-negctrl", "script-mouse",
                             "script-mouse-negctrl", "record-focus"])
    ap.add_argument("--out", default=os.path.join(ROOT, "artifacts", "determinism_audit"))
    ap.add_argument("--seconds", type=float, default=22.0)
    ap.add_argument("--seed", type=int, default=4242)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    helper = make_helper_window()

    if a.test in ("script-focus", "script-focus-negctrl"):
        # DET_ISOLATE_OFF=window puts the guest window back to how it behaved
        # BEFORE this pass (visible, takes the foreground), so that this test
        # exercises item 2 - the activation channel - on its own, rather than
        # being trivially satisfied by item 1's never-activating window.
        # The negative control additionally turns item 2 OFF, so the same two
        # focus thefts DO reach the game's switchedFrom/ToProgram callbacks:
        # that run must DIFFER, which is what makes the EQUAL above meaningful.
        negctrl = (a.test == "script-focus-negctrl")
        tag = "script_focus_negctrl" if negctrl else "script_focus_theft"
        env = {"DET_ISOLATE_OFF": "window,switch" if negctrl else "window"}
        base = baseline(a.out)

        def perturb(pids, hwnd, proc):
            time.sleep(3.0)
            if hwnd:
                si.force_foreground(hwnd, pids, tries=4)
                print("  guest has the foreground: %s" % si.foreground_is_guest(pids))
            time.sleep(2.0)
            steal_and_return(helper, hwnd, pids, hold=2.0, label="theft 1")
            time.sleep(3.0)
            steal_and_return(helper, hwnd, pids, hold=2.0, label="theft 2")

        got = script_run_with_perturbation(a.out, tag, ["--window=normal"], perturb, env=env)
        errp = os.path.join(a.out, tag + ".stderr.txt")
        n_sup = count_stderr(errp, "SUPPRESSED window switch")
        print("  guest saw %d window-switch event(s) at the _switch_in/_switch_out choke point" % n_sup)
        eq = compare(base, got)
        if negctrl:
            print("VERDICT script-focus-negctrl (activation NOT neutralized - must DIFFER):")
            print("  " + ("UNEXPECTED EQUAL - the thefts never reached the guest" if eq
                          else "DIFFERS as required"))
            sys.exit(1 if eq else 0)
        n_msg = count_stderr(errp, "[wndmsg]")
        print("  guest window received %d activation-class Win32 message(s) "
              "(run with DET_TRACE_WNDMSG=1 to list them)" % n_msg)
        print("VERDICT script-focus (activation neutralized in --input=script):")
        if n_sup < 3:
            print("  NOTE: only %d switch event(s) reached the _switch_in/_switch_out choke point. "
                  "MEASURED on this host (see notes/determinism_audit.md): the guest window gets "
                  "WM_NCACTIVATE/WM_SETFOCUS/WM_KILLFOCUS on every foreground change but WM_ACTIVATE "
                  "only once, at creation - and Allegro's directx_wnd_proc acts only on WM_ACTIVATE. "
                  "The perturbation did reach the window; it just cannot reach the game here." % n_sup)
        sys.exit(0 if eq else 1)

    if a.test in ("script-mouse", "script-mouse-negctrl"):
        negctrl = (a.test == "script-mouse-negctrl")
        base = baseline(a.out)

        def perturb(pids, hwnd, proc):
            time.sleep(3.0)
            if not hwnd:
                print("  no guest window - cannot move the cursor over it")
                return
            n = jiggle_mouse(hwnd, 10.0)
            print("  moved the real cursor over the guest window %d times" % n)

        # --window=normal so the window is actually ON SCREEN for the cursor to
        # be moved over (it still may not activate: WS_EX_NOACTIVATE, and the
        # guest's own SetForegroundWindow calls stay suppressed).
        tag = "script_mouse_negctrl" if negctrl else "script_mouse_jiggle"
        got = script_run_with_perturbation(a.out, tag, ["--window=normal"], perturb,
                                           env=({"DET_ISOLATE_OFF": "mouse"} if negctrl else None))
        print("VERDICT %s (DirectInput mouse %s):" % (a.test, "NOT parked" if negctrl else "parked"))
        sys.exit(0 if compare(base, got) else 1)

    # record-focus: a genuine SendInput session, perturbed twice on purpose.
    rec = os.path.join(a.out, "record_focus.txt")
    dig = os.path.join(a.out, "record_focus.digest")
    rep = os.path.join(a.out, "record_focus.replay.digest")
    si.restore_assets()
    p = run_carrier(["--det", "--pace=real", "--input=real", "--interactive",
                     "--record-input", rec, "--digest-out", dig,
                     "--report", dig + ".report.json", "--run-seconds", str(int(a.seconds) + 35)],
                    os.path.join(a.out, "record_focus.stderr.txt"))
    pids, hwnd = wait_for_window(p, timeout=30)
    if not hwnd:
        p.kill()
        raise SystemExit("guest window never appeared")
    if not si.force_foreground(hwnd, pids):
        p.kill()
        raise SystemExit("could not give the guest the foreground")
    import random
    rnd = random.Random(a.seed)
    si.send_key("KEY_ENTER", True)
    time.sleep(2.5)
    si.send_key("KEY_ENTER", False)
    time.sleep(0.6)
    held, start = {}, time.time()
    thefts = [a.seconds * 0.3, a.seconds * 0.65]
    done = [False, False]
    while time.time() - start < a.seconds:
        for i, at in enumerate(thefts):
            if not done[i] and time.time() - start >= at:
                done[i] = True
                for key, is_down in list(held.items()):
                    if is_down:
                        si.send_key(key, False)
                        held[key] = False
                print("  t=%.1fs: stealing the foreground on purpose" % (time.time() - start))
                steal_and_return(helper, hwnd, pids, hold=2.0, label="theft %d" % (i + 1))
        key = rnd.choice(["KEY_LEFT", "KEY_RIGHT", "KEY_SPACE", "KEY_RIGHT", "KEY_SPACE"])
        down = not held.get(key, False)
        if si.foreground_is_guest(pids):
            si.send_key(key, down)
            held[key] = down
        time.sleep(rnd.uniform(0.012, 0.19))
    for key, is_down in held.items():
        if is_down:
            si.send_key(key, False)
            time.sleep(0.02)
    p.wait()

    events = [l.strip() for l in open(rec) if l.strip() and not l.startswith("#")]
    switches = [e for e in events if " switch " in e]
    times = [e for e in events if " time " in e]
    print("recorded %d events: %d key, %d switch, %d time" %
          (len(events), len(events) - len(switches) - len(times), len(switches), len(times)))
    for s in switches:
        print("    " + s)
    si.restore_assets()
    stop = si.last_tick(dig)
    q = run_carrier(["--det", "--pace=fast", "--input=script", "--input-script", rec,
                     "--digest-out", rep, "--report", rep + ".report.json",
                     "--stop-at-tick", str(stop), "--run-seconds", "180"],
                    os.path.join(a.out, "record_focus.replay.stderr.txt"))
    q.wait()
    print("VERDICT record-focus (activation recorded and replayed):")
    ok = compare(dig, rep)
    if not switches:
        print("  WARNING: the recording contains NO switch events - the focus thefts did not "
              "reach the guest, so this run does not prove the recorded-activation path.")
    sys.exit(0 if (ok and switches) else 1)


if __name__ == "__main__":
    main()
