# Divergence 001: log.txt line 16 — "%s%s" vs empty line

## Symptom

`artifacts/log_original_baseline.txt` line 15/16:
```
Malformed HTTP response:
<empty line>
```
`artifacts/log_carrier_run1.txt` (copied from `assets/log.txt` after run1) line 15/16:
```
Malformed HTTP response:
%s%s
```

## Q1: exact format string and call site (KNOWN)

Found via `artifacts/run1_trace.log`, filtering `log2file+0x77` (the `vsprintf`
call inside `log2file`, `main.c`, entered from `vfprintf` at `log2file+0x63`
then `vsprintf` at `log2file+0x77`, call site `0x0040dabb`/`0x0040dacf`,
`log2file` itself at `0x0040da58`). The trace line for this exact log line is:

```
551:537 tid=28488 msvcrt.dll!vsprintf ret=0x0040dacf (log2file+0x77)
  args=[0x004f89e8 0x004d494f 0x041fef50 0x041fef44 0x76df914c 0x76e04110]
```

Reading the format string from the image file at VA `0x004d494f`
(RVA `0xd494f`, in `.rdata`, file offset `0xd1e00+(0xd494f-0xd4000)=0xd234f`):

```
"Malformed HTTP response:\n%s"
```

**One `%s`, not `%s%s`.** The hint in the task description
(`0x004d6164`/`0x004d618e`) was checked and decoded to unrelated startup
strings ("Game started with the following commands:" / "   %s") — those are
different, earlier `log2file` calls (seq 181/190, tid 45240) reused by
`vsprintf` with the same call site; they are not the ad-fetch call. The
correct format pointer for the ad-fetch line is `0x004d494f`, found by
filtering trace entries to `tid=28488` (the pthread doing HTTP fetch, per
task description) and decoding each format pointer against the exe.

Call site: `artifacts/disasm.txt` line ~5962, inside `_extractHTTPResponse`
(httpget.c-equivalent, address range `0x405a91`-`0x405e2b`):

```
405dd6: mov  -0x834(%ebp),%eax   ; eax = raw HTTP response buffer (function's
                                  ; own arg, saved at entry: 405a9c mov %eax,-0x834(%ebp))
405ddc: mov  %eax,0x4(%esp)      ; single %s arg = raw response buffer
405de0: movl $0x4d494f,(%esp)    ; fmt = "Malformed HTTP response:\n%s"
405de7: call 40da58 <_log2file>
```

This is reached only when the earlier `sscanf(line, "HTTP/%s %d", ...)`
(format at `0x4d4944`, call at `405b77`) returns `!= 2` — i.e. the first line
of whatever bytes were received does not parse as an HTTP status line. In
that failure branch, the code logs the **entire raw response buffer
verbatim** via a single `%s`.

So line 15+16 together are exactly `"Malformed HTTP response:\n" + <raw
response buffer content>`, printed through one `%s`. There is no
`"%s%s"` format string anywhere in the game's `.rdata`; the literal text
`%s%s` seen in the carrier run's log is not a format-string escape/bug at
all — it is the **verbatim content of the HTTP response buffer**, printed
by the one `%s` specifier. This is a case of "argument text happens to
contain percent-escapes" being echoed as-is, not a nested/two-level printf.

Confirmed: baseline's argument was the empty string (0-byte response body,
or the buffer's first byte was `\0`), carrier run1's argument was the 4-byte
string `%s%s`, receieved as the raw bytes of whatever `www.icytower.com`
returned to `HTTPFetchInternal`/`extractHTTPResponse` (httpget.c, thread
`tid=28488`) at that particular moment.

## Q2: carrier bug vs. network difference (KNOWN, reproduced)

Reproduced both paths back-to-back on 2026-09-07, same machine, same network:

1. Backed up `assets/tower.cfg`, `assets/profiles/`, `assets/log.txt` to
   `artifacts/repro_backup_<timestamp>/`.
2. Ran the **original** `assets/icytower15.exe` directly
   (`Start-Process` + 10s wait + `Stop-Process -Force`, fullscreen via
   cnc-ddraw). Fresh `assets/log.txt` line 15/16 → copied to
   `artifacts/log_original_repro.txt`:
   ```
   Malformed HTTP response:
   <empty line>
   ```
3. Restored `tower.cfg`/`profiles`/`log.txt` from the backup so the carrier
   run starts from the same pre-run state.
4. Ran `carrier\carrier.exe --run-seconds 10` immediately after (same
   working directory, same network). Fresh `assets/log.txt` line 15/16 →
   copied to `artifacts/log_carrier_repro.txt`:
   ```
   Malformed HTTP response:
   <empty line>
   ```
5. Restored `tower.cfg`/`profiles`/`log.txt` from the backup again to leave
   `assets/` as found (`diff` confirmed `assets/log.txt` byte-identical to
   the pre-repro backup afterward).

**Both fresh runs today produced an empty line 16, matching each other and
matching the original baseline — the carrier did NOT reproduce the `%s%s`
divergence.** Since `HTTPFetchInternal`'s socket calls (`connect`/`send`/
`recv`/`gethostbyname` etc., WSOCK32.DLL) are left DIRECT/unwrapped per
`carrier/NOTES.md` (only `ExitProcess`/`exit`/`_cexit`/`abort`,
`GetModuleFileNameA`, `GetCommandLineA` are wrapped), the carrier does not
touch this code path at all — it's native guest code making real Winsock
calls straight through to the real network stack. The only thing that
changed between run1 (carrier, whenever it was captured) and today's paired
repro is **wall-clock time**, i.e. whatever `www.icytower.com` (or DNS/a
middlebox on the path to it) returned at that moment. This is external,
uncontrolled, non-deterministic network I/O — not a trampoline, argument-
passing, msvcrt-binding, or threading artifact of the carrier.

Conclusion: **the divergence is caused by the network response differing
between the two captures, not by the carrier.** `www.icytower.com` is a
long-defunct game-company endpoint (site domain still resolves per the log's
"Could not fetch... (0)" pattern showing a connect/read outcome, not a DNS
failure); its response content is apparently unstable/junk over time —
run1 happened to receive 4 bytes of literal text `%s%s` (plausibly a stray
fragment from a parked-domain ad server, malformed CDN response, or
mid-transfer truncation), while the baseline and both of today's repro runs
received an effectively empty body.

## Q3: what the carrier must record/suppress for determinism (INFERRED)

The ad-fetch path (`HTTPFetchInternal`/`extractHTTPResponse`, httpget.c,
`tid=28488`) is a genuine external-I/O nondeterminism source, structurally
like a clock or RNG read: identical guest code, identical carrier, differing
only by what bytes a live socket handed back. For deterministic replay the
carrier would need to, for this channel specifically:

- **Record**, on a real/reference run: the raw bytes returned by `recv()`
  (or equivalently the fully assembled response buffer passed into
  `extractHTTPResponse`) on `tid=28488`'s socket, keyed by call sequence.
- **Suppress/replay**, on later runs: intercept the WSOCK32 socket calls
  this thread makes (`connect`, `send`, `recv`, `closesocket`, and
  `gethostbyname`/DNS if used) and feed back the recorded bytes instead of
  hitting the real network — i.e. move this specific channel from DIRECT to
  WRAP status (it currently is not wrapped at all, per `carrier/NOTES.md`'s
  "Wrappers actually installed" list).
- Because the response feeds straight into a single `%s` with no further
  parsing/sanitization by the game on the malformed-response path, whatever
  bytes are replayed will reproduce the exact log text (including
  degenerate cases like a response that happens to contain `%`-characters)
  as long as the recorded buffer's exact byte length and content are
  preserved (no need to special-case malformed/empty responses — same
  replay mechanism as a well-formed response).
- This is scoped narrowly: only `tid=28488`'s ad-fetch socket traffic is
  implicated by this divergence; no evidence here that any other DIRECT
  import is a determinism risk.
