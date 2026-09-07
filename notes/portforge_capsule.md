# PortForge context capsule — for the Win32 carrier pilot (Icy Tower)

Purpose: let a Win32 carrier pilot reuse PortForge's ideas and code without
re-reading `port_forge/`. Everything below is KNOWN (read from source/docs,
cited `pf/NN` = `port_forge/docs/NN-*.md`) unless marked INFERRED. Repo root
for all paths: `port_forge/` (submodule, branch `experimental/win32`, == `origin/main`
at capture time, 2026-09-07 per `docs/README.md`'s register).

**Headline fact for the pilot:** PortForge has NO Win32 or PE-format support
today. There is no `win32` entry in `config/platforms-v1.json` (KNOWN — it
lists only `dos-rm`, `dos-pm`, `win16`, `gbc`, `amiga`, `mac68k`, pf/46 §1),
no PE/COFF loader in `src/formats/` (KNOWN — that directory holds `mz.hpp`,
`ne.hpp`, `le.hpp`, `amiga_adf.hpp`, `amiga_hunk.hpp`, `mac_*.hpp` and
nothing PE-shaped), and no Windows-API service layer. The closest analog is
**Win16** (a real Windows API/message-loop runtime the framework already
forwards to a host UI — pf/19, pf/22, pf/23, pf/13, pf/21) and **DOS
protected-mode** (`src/platform/dos_pm/`, the only existing consumer of the
32-bit flat CPU core `src/arch/x86_32/`). Neither is Win32; both are the
nearest working precedent for "forward to something host-shaped instead of
reimplementing a whole OS," which is exactly win32_pilot.md's stated
strategy.

---

## A. Vocabulary

Source: pf/39 (glossary, normative), pf/41 (ADR), pf/72 (verdict), pf/77
(carrier/coastline), pf/81 (anchors). One line each; consult pf/39 for full
definitions.

- **Carrier** — the mechanically emitted, machine-shaped AOT execution plus
  its host. Regenerable, never hand-edited; the identity baseline clean work
  is measured against; a disposable reference; NOT the clean tree's ancestor.
  pf/77.
- **Oracle** — faithful execution of original program behavior, used as
  ground truth/evidence. May use PortForge and original media at runtime.
  pf/39.
- **Host** — the project's implementation of the host-service interface a
  carrier declares (`*_host.h`): OS/BIOS services, devices, the work clock.
  May not privately reimplement what the shared machine model owns. pf/27,
  pf/39.
- **Quarry** — a legacy reimplementation or hand-RE project used as a source
  of hypotheses, never an authority; a claim from it must be confirmed from
  this project's own bytes. pf/39.
- **Brick** — a hand-written native unit that replaces emitted blocks behind
  a seam and is verified against the carrier; lives in `src/<game>/`, no
  framework dependency, no guest address in its interface. pf/39, pf/89.
- **Island** — a connected set of bricks; islands coalesce. Cross-owner edge
  count is one progress metric among several. pf/39.
- **Coastline** — the remaining carrier/clean ownership boundary, split into
  CONTROL (execution crossings), DATA (state/effect crossings), TIME
  (timing/admission/sync dependencies). A standalone envelope needs all
  three at zero AND closed build/init/asset/persistence/output/environment
  dependencies. pf/39, pf/77 §4.
- **Engine flip** — the milestone where the clean main/game loop, not the
  carrier runtime, becomes product authority; carrier-backed behavior after
  it is a named transactional service, never an implicit re-entry. pf/39,
  pf/77 §6.
- **Replay identity vs execution identity** — `ReplayIdentity` names portable
  behavior (game/machine/boundary/assets); `ReplayExecutionIdentity` adds the
  exact implementation-plan digest, executable, runtime, devices, effects
  ABI, continuation/canonical schema. Two implementations can consume the
  same replay without sharing private state. pf/39, pf/41.
- **ContinuationState vs CanonicalState** — Continuation is complete private
  resumable state for one execution identity, opaque and non-comparable
  across implementations. Canonical state is a versioned, schema-owned
  comparison PROJECTION for one declared claim; equal canonical state never
  implies equal continuation. pf/39, pf/41.
- **Semantic point / boundary selector / boundary stamp** — a semantic point
  is a backend-neutral event (e.g. a gameplay tick); a `BoundarySelector` is
  `(point, point-local occurrence, phase)`; a `BoundaryStamp` is an observed
  selector plus machine time, global ordinal, outcome, recognition evidence.
  PC/instruction-count/frame/tick are recognition EVIDENCE, never the
  identity itself. pf/39, pf/41.
- **Heartbeat / counted coordinate** — a heartbeat is a periodic event the
  program itself produces from a device it programmed (e.g. a PIT reload it
  wrote); a counted coordinate is that event's occurrence count, MEASURED AT
  CAPTURE and copied, never derived/converted. It is semantic identity by the
  2026-09-01 ADR addendum. pf/39, pf/41 addendum, pf/42.
- **Native promotion / seam / brick manifest** — promotion replaces emitted
  blocks with clean code behind a GENERATED seam (`pf_seam_gen.py`) built
  from a hand-written manifest (`portforge-brick-manifest-v1`); the seam
  handles projection in, codec out, correspondence/continuation checks,
  refusals. pf/89.
- **First divergence / verdict** — a verdict states what was compared, the
  span, what it cannot see, its preconditions, that its inputs could change
  the answer; it is EQUAL or a NAMED first difference, never a percentage.
  pf/72.
- **Island / frontier / anchor** (execution) — the **frontier** is where a
  native run stops at a gap and captures a resumable artifact (pf/39, pf/69);
  an **anchor** (`pf-replay-anchor-v1`) is a full-state acceleration cache
  cut from an identity-sound root (cold boot or digest-checked snapshot) that
  is never itself a proof origin — every claim still traces to the root it
  was cut from. pf/81.
- **Work unit / work coordinate** — the oracle's instruction-class cost table
  unit (pf/54); `CoordinateKind::Work` is a counted (not converted) delivery
  coordinate finer than a heartbeat tick, used when gameplay cadence is a
  divisor of the hardware tick. pf/42 "The work coordinate".
- **Test tiers** — tier 0 (seconds, every brick change), tier 1 (commit,
  short whole-route recording + verdict), tier 2 (island merge/daily, all
  recordings), tier 3 (rare, named-reason only, forbidden in the inner
  loop). pf/73.
- **Replaceability / comparability** — the carrier's two standing
  requirements: any covered region must stay replaceable by an island; every
  quantity a verdict rests on must stay observable at the same coordinates on
  both roles. No emitter change may trade either away. pf/73 §2.

---

## B. Carrier philosophy and lessons learned

**"Scaffolding grew larger than the game" failure mode, and what was done.**
Measured directly on two hand-written brick promotions (pf/89 §1): a
call/return-region brick was 375 lines of recovered clean C++ behind **400
lines of hand-written seam**; a loop-region brick was 880 lines of clean C++
behind **731 lines of hand-written seam**. In both cases the seam — none of
it recovered semantics, all of it projection/codec/trace/correspondence
check/report — was comparable to or larger than the semantics it served. The
fix was not "write the seam better" but to make the seam **generated** from
five declarations that already exist (brick manifest, program model, layout
bindings, toolchain profile, block table): `tools/pf_seam_gen.py`,
`schemas/portforge-brick-manifest-v1.schema.json` (pf/89). The manifest is
YAML (not JSON) specifically because "its comments carry the evidence for
its numbers" (pf/89 §2) — this is a deliberate, reusable convention for a
Win32 pilot's own manifests.

The same failure mode recurs at the project-host layer: pf/76 §1 found both
DOS-family reference projects independently re-implementing the dispatch
loop, interrupt delivery, boundary/point-state writers, and five DOS/BIOS
service families as ~3,000+-line hand-written hosts (`native/ae_engine.h`,
`native/al_host.cpp`+`al_freerun.cpp`), each a **second implementation** of
something the framework already had, verified only by a differential that
existed the day it was written and free to drift after. pf/76 §2.1 measured
concretely that a transcribed-not-shared family loses arms silently (DOS
files missing 5 functions, guest clock missing 2 writes, XMS missing 7
functions — none detected by the existing differential because it compared
two roles both running the oracle). **Lesson for Win32: do not let the
project host duplicate anything the framework will eventually own — a
duplicate is "a duplicate with a date," listed and retired on a schedule, not
treated as done work** (pf/76 §2.1, §5).

**Generated vs hand-written, as a rule (pf/76's "port contract" table, DOS
instance, but the shape generalizes):**

| kind | example | who supplies it |
|---|---|---|
| emitted unit (blocks, dispatcher, seam, host header) | `pf_rm_emit` output | framework generates from the program + boundary profile |
| host service families (files, clock, video, PIC/PIT wiring) | `src/platform/dos/services/*` | ONE implementation, called by BOTH the interpreter and the shared engine — never transcribed per project |
| the engine (dispatch loop, tick edge, delivery rule, seam servicing) | `src/host/dos_rm_engine.hpp` | framework; project supplies only seam bodies and clean game code |
| presentation/operator path | `pf_main`, `pf_play`, `--window --sound` | framework; a project may add a diagnostic front end only if it changes nothing observable |
| replay/record | recorder, session replayer, counted coordinates, claims ledger | framework; project supplies recordings and gate declarations |
| the brick seam | `pf_seam_gen.py`, brick manifest schema | framework generates; project writes only the manifest and `src/<game>/` |

Rule, generalized in pf/76 §2: **a project may not privately implement any
surface the framework declares.** A missing capability goes upstream first,
then is adopted. This is a hard discipline point for a Win32 pilot working
inside `port_forge` on `experimental/win32`: build the Win32 host-service
layer, PE loader, and engine as framework code from day one, not as
project-local scaffolding, or it becomes exactly the debt pf/76 §1 measured
(6,209 lines, 13 of 19 surveyed mechanisms independently reimplemented).

**Replaceability and comparability (pf/73 §2), non-negotiable for any
carrier:** blocks are split, never fused (an emitter optimization that fuses
regions so no seam can be cut through them is refused categorically, even
for speed); everything a verdict rests on (declared points, heartbeats,
block-entry counts, interval effects, cost charges) must stay observable at
the same coordinates on both the interpreted and native-replaced roles. Two
measured regressions motivate this: an emitter once spent a declared point's
visit slot on its occurrence counter (comparability silently lost), and a
REP instruction charged atomically became a region no interrupt could land
inside (comparability lost at an unchosen seam) — quantified later at 2.30%
of admissions withheld inside an in-progress REP on a real route (pf/75,
cited by pf/73 §2).

**AOT/interpreter split, and what "faithful" means before semantics exist.**
PortForge's stated order (pf/77 §1-§2): start from a faithful INTERPRETER
oracle over the declared machine, generate an AOT carrier from what that
oracle observed with explicit residual fallback for anything unrecovered,
and treat semantic recovery as a SEPARATE, later, evidence-driven layer —
"PortForge should generate mechanically specified dispatch, capture,
comparison, codec, enforcement and accounting infrastructure. AI/human work
includes semantic meaning… A field name alone cannot generate a valid
migration protocol" (pf/77 §2). The carrier is not a step toward the clean
source tree architecturally; it is a disposable reference the clean tree is
measured against (pf/77 §1, pf/39 "Carrier").

---

## C. Replay model

**What is recorded.** `ReplayArtifactV2` (`portforge-replay-v2`, schema
`schemas/portforge-replay-v2.schema.json`) binds: program/launch/machine/
environment/boundary/asset identity; one boundary-profile id+digest; typed
`hold`/`pulse`/`edge`/`latch` input channels; one contiguous global timeline
of semantic selectors with optional deadlines; one cross-channel event
sequence referencing timeline ordinals; ordered canonical checkpoints with
explicit claim scopes; one terminal selector/outcome/reason (+ optional
canonical digest); required capability IDs; and section hashes over every
authoritative part (identity, profile, environment, channel, timeline,
event, checkpoint, terminal, capability). pf/42 "Artifact V2". Section
hashing uses one canonical wire serialization (sorted keys, 2-space indent,
`max_digits10` float formatting) so no producer/validator pair can disagree
about a hash. pf/42.

**Coordinate kinds** (`src/replay/input_script.hpp`, pf/39, pf/42): (1)
**timed / machine-time-input** — a fallback coordinate valid only when both
recording and replaying roles derive machine time identically; degrades
silently on held input if clocks differ (pf/42 "Counted coordinates" +
"machine-time-input"); (2) **counted heartbeat** (`CountedInputCoordinate`,
capability `counted-input-coordinate`) — measured at capture from a
program-produced periodic event (DOS-RM's instance: `dos.irq0-deliveries`,
the master-timer-interrupt delivery count), copied never converted, and
treated as semantic identity by the pf/41 2026-09-01 addendum; (3)
**work coordinate** (`CoordinateKind::Work`, capability
`counted-work-coordinate`) — finer than a heartbeat tick, the accumulated
instruction-class CPU work (pf/54), used when gameplay cadence divides the
hardware tick; written `HEX@work:W` with a guard syntax
(`+irq0:N`, `+point:N`) so a role that reaches `W` at the wrong tick reports
`GUARD FAILED` rather than replaying quietly. pf/42 "The work coordinate".

**How input is delivered.** ONE rule, in a header rather than duplicated per
consumer: `src/replay/input_delivery.hpp`. Central finding: **a blocked read
is a wait, not a request for the next event.** Handing over the next
scripted event the moment a program blocks (rather than at its true
coordinate) was measured to deliver events a median of 3.1s early (up to
5.2s) and to collapse every held-key duration to ~2ms regardless of the
recorded 48–102ms hold — silently changing gameplay outcomes that integrate
held input. pf/42 header comment. The companion **wait rule**
(`DosServices::park_blocked_input_wait`, cited pf/42 "The wait rule") charges
an idle span's cost ONCE, converted by the profile's rational rate and
rounded UP at the instant the wait ends — never accumulated by spinning, and
never segmented by however many times a front end polled it (measured: a
front end that cut one park ~2,000 times vs one that cut it once produced a
+2,028-unit divergence before the fix; 0 after). pf/42 "An idle span is
charged once".

**How nondeterminism is handled.** Everything that could affect a future
comparison is either (a) captured as a counted/timed event on the artifact,
(b) modeled as a shared canonical device (PIT, PIC, PC speaker — ONE
implementation both executors call, pf/76 §2.1), or (c) explicitly refused
(a role that cannot share the recording role's clock domain must refuse the
artifact rather than approximate — pf/42 "machine-time-input" consumer
obligation).

**How a replay is verified to reproduce.** Digests: canonical checkpoints
carry a `CanonicalState` digest with an explicit claim scope (one of
semantic-boundary / observable-interval / full-continuation /
strict-low-level-execution equivalence — pf/41, pf/42). `first_divergence()`
returns the last verified predecessor and first mismatch; localization uses
independently restored/replayed probes, never trusting a divergent endpoint.
pf/42 "Verification and divergence". The verdict FILE format
(`portforge-verdict-v1`, pf/72 §7) is the machine-readable form: `instrument`
(name/version/tier), `operands` (role/kind/id/digest — identical digests
across operands are REFUSED, pf/72 §1(a)), `span` (unit/from/to/count/covers:
session|prefix|segment|sample), `ending` (from `src/replay/run_ending.hpp`'s
enum, exported to `config/portforge-run-endings-v1.json` and `--check`ed for
drift), `verdict` (EQUAL|DIFFERS|REFUSED|INCOMPLETE|UNVERIFIED|
NOT-ESTABLISHED), `first_difference` (required when DIFFERS: where,
instruction?, component, both_sides), `negative_control` (pf/72 §8, a fault
deliberately injected to prove the comparator CAN fail), `cannot_see`
(named+countable), `preconditions`, `distinctness`, `skipped`, `inputs`.
`tools/pf_verdict.py check`/`show` are the one validator and one renderer.

**Replay anchors.** `pf-replay-anchor-v1` (pf/81) is a full-state
acceleration cache: a DOS-RM snapshot directory plus `anchor.json` naming
cold-start provenance, contract cursors, an image-identity check, and the
work clock restated for cross-check. It is NEVER a proof origin — every claim
still traces to the cold/snapshot root it was cut from, and certification
(`scripts/certify_replay_anchor.py`) runs the SAME route twice (cold vs from
anchor) and requires row-by-row equality of the point-state stream, final
memory digest, session counters, and ending token. Measured savings: a
session-role anchor skipping 83.8% of a recording's prefix gave a 6.08×
wall-time speedup with byte-identical suffix state (pf/81 §7).

**A recording replays on either executor.** pf/85 fixes that the SAME
recording (not an extracted flat route) is the operand for both the
interpreter and the emitted carrier: `src/host/carrier_replay.hpp` restores
the recording's own `base_snapshot`, samples arrivals via the emitted unit's
own `probe`/`probe_at`, and compares the `before` stamp of every occurrence
≥ 1 against the recording's captured `counted_work` and
`guest_instructions` — first field that parts is named by ordinal,
occurrence, and both values, no tolerance. This is DOS-specific machinery
today (`DosRmEngine`), but the PATTERN — "the recording is the operand for
every executor; nothing is extracted from it first" — is the reusable idea
for a Win32 carrier's own record/replay/verify path.

---

## D. Snapshot model

**What a snapshot contains** (DOS-RM instance, `pf-dos-rm-snapshot-v5`, cited
pf/81 §2): CPU state, guest instruction count, the versioned `devices` block
(PIC, PIT, speaker, etc.), and guest memory as 4 KiB pages each with a
sha256, plus one sha256 over the whole image. `pf::load_snapshot` /
`pf::load_snapshot_devices` read it. Win16's own snapshot
(`portforge-win16-snapshot-v2`, pf/23) additionally carries full protected
selector-memory state, NE/loader/API-thunk state, GDI object/DC state, and
one explicit resource binding (not a copied directory listing) — the shape
to imitate for a Win32 carrier, since Win32 state (heap, handle tables,
loaded-module list, thread/message state) is structurally closer to Win16's
problem than to DOS-RM's flat-memory one.

**Safepoint restriction.** A snapshot may only be taken at a declared
semantic SAFE POINT — evidence that a long operation completed before a
deadline, yielded with resumable continuation, exposed sufficient ordered
effects, or failed; "a hook boundary alone is not proof of deadline safety"
(pf/39 "Safe point", pf/41). Concretely for Win16: F11/F12 (start/finalize
recording; publish snapshot) both wait for
`LiveReplaySession::at_continuation_point()` with callback depth zero and no
in-flight paint/modal operation before capturing (pf/23).

**Restore transaction.** `src/core/restore_transaction.hpp`:
`RestoreTransactionContext` is a depth-counted RAII guard;
`with_restore_rollback(capture, operation, rollback)` runs an atomic
publication with a SEPARATELY represented rollback state (for formats whose
wire shape differs from a live in-memory rollback snapshot), and nested
publishers participate in the OUTER transaction rather than capturing an
incoherent mid-publication fragment. This is platform-generic (no DOS/Win16
dependency) and directly reusable.

**Certification that `restore→suffix == cold→suffix`.** This is the anchor
certification contract (§C above, pf/81 §6): "cold → anchor → suffix" is
compared row-by-row against "restore(anchor) → suffix" — same point-state
stream, same final memory digest, same session counters, same ending. The
Win16 analog (pf/23, "Focused executable tests…") persists a publication,
mutates the real composed message-retrieval path, restores its recording
draft, independently loads the machine attachment, and requires both
canonical projections AND event cursors to equal the pre-publication state.

---

## E. Native promotion + verification

**How a function is replaced.** `pf_rm_emit` (`tools/pf_rm_emit.cpp`, DOS-RM
instance, 8,696 lines) generates, per promoted region: a state struct + ALU/
memory preamble (`<prefix>_emitted.h`), block bodies + dispatcher
(`<prefix>_emitted.cpp`), a timer-tick record for the verify clock
(`<prefix>_ticks.cpp`), a host-vs-project differential
(`<prefix>_hostdiff.cpp`), and a per-block-exit verify harness comparing
registers/segments/flags/exit-address/full memory/EGA planes against the
oracle (`<prefix>_verify.cpp`). Every emitted control transfer out of the set
returns a PHYSICAL ADDRESS — the declared host boundary a real port
services. Unsupported semantics are EXCLUDED with the reason recorded, never
approximated (file header comment). The **native seam** (surface A of pf/76
§2) is: `_native_enter`/`_native_cost` externs, "origin-2" boundary-table
rows, a `vk_*` state preamble, and a project `<prefix>_host.h`. Blocks are
split, never fused (pf/73 §2) so a seam can always be cut through a covered
region.

**How lockstep/differential comparison finds first divergence.**
`tools/pf_rm_lockstep.cpp` (582 lines) compares interpreter vs native+hooks
at declared frame boundaries — legacy core CPU fields and main memory, a
MASKED comparison (`--domains`, `--exclude`, `--dead-stack N` for
provably-dead stack bytes below SP) because bit-identical is not required,
behavioral equivalence is. `tools/pf_rm_hookdiff.cpp` (1,709 lines) is the
finer instrument: verifies each native replacement at its OWN entry/return
boundary against the original bytes executed from the same entry, catching
state (especially FLAGS) the guest overwrites before the next frame boundary
— frame-lockstep is blind to that. Both write `portforge-verdict-v1`
records. The generic idea (`src/replay/differential_mirror.hpp`,
platform-generic, 221 lines) is: a candidate driver consumes the same
immutable artifact selectors/events as `ReplaySession` but applies its own
state-domain projection instead of comparing full-state checkpoints, and
explicitly cannot compare past a declared `NativeGap` (raises
`DifferentialNativeGap` rather than spinning).

**The verdict contract required to call a promotion verified** (pf/72,
pf/89 §7 "the claim"): equality or a NAMED first difference (never a
magnitude/percentage); run-length reporting with PERMANENT flagging for
non-reconverging divergence runs; explicit `cannot_see` list; a
**negative control** — pf/72 §8, adopted from NESRecomp's `gate3`: a
one-byte fault injected into ONE role at a chosen instruction must produce a
first difference AT THAT INSTRUCTION, naming the perturbed component, or the
comparator is proven blind and every prior EQUAL from it is suspect.
`negative_control:true` beside `EQUAL` is itself refused by `pf_verdict.py`
(`[negative-control-agreed]`) because that combination invalidates every
other result the instrument produced. `pf_rm_emit`'s generated
`<prefix>_hostdiff` implements the injector: `--inject-at ORDINAL --inject
reg:<r>|flag:<f>|mem:<addr>` over a CLOSED set, both arguments required
together.

**The seam generator's promotion claim** (pf/89 §7, the concrete "how do I
know a brick is verified" recipe reusable for Win32 bricks): four
preconditions — (1) the seam under test IS the one the manifest generates
(proved by regenerating and byte-comparing); (2) `published == attempted`
and `refusals == 0`; (3) no published value is read back from guest memory
(`certify`, a STATIC scan of the generated codec — no run required, checked
that a copyback mutant that launders a legacy read through the codec is
CAUGHT); (4) every calibration mutant was refused by the published half
(pf/72 §8's negative control, applied per-brick). The equality of two
declared-point streams is the VERDICT; the four items above are
PRECONDITIONS (pf/72 §1(d): a failed precondition is a refusal, never a
"pass" or a "warning").

---

## F. Win16 pilot specifics — the closest existing analog

**The Win16 API boundary: reimplementation, not forwarding.** Contrary to a
naive "forward to host Windows" reading, the Win16 platform does NOT forward
KERNEL/USER/GDI calls to the real Win32 API. It re-implements a deterministic
Win16 runtime from scratch inside PortForge: its own selector-based global
heap, its own coalescing local heap, its own GDI (typed handles, DCs,
palettes, BitBlt/StretchBlt, retained per-window surfaces), its own message
dispatch, its own deterministic instruction-derived clock, its own timers
(pf/19 "Implemented foundation"). What IS forwarded is PRESENTATION only: a
backend-neutral description (window styles, menu trees, cursor bitmaps,
scrollbar state) is handed to a Qt frontend, which projects it onto REAL
native `QWidget`/`QMenuBar`/`QScrollBar` objects and translates host
input/close events back into the guest's Win16 message queue (pf/19
"Composition", "Native window and menu presentation"). The guest program's
OWN API calls are still interpreted/executed against PortForge's model, never
against the real Windows KERNEL/USER/GDI. **This is the load-bearing
distinction for a Win32 pilot deciding between "forward Win32 calls to
Windows 11" (win32_pilot.md's stated approach) vs "reimplement a
deterministic Win32 API layer" (PortForge's actual precedent) — PortForge
has never built the "forward to a live host OS" version for any platform.**
No document in the repo describes forwarding calls to a live host OS as
implemented; it would be a genuinely new mechanism for the framework.

**The DLL-hook mechanism** (pf/13, DOS-PM/Krypton Egg, History status but the
ABI is live): `src/hooks/pf_plugin_abi.h`, pure C, `PF_ABI_VERSION 3`. One
hook per function, keyed by a stable ID (`pm.func.<entry-hex>`), category
`faithful|platform|cosmetic|mod`. A hook DLL returns `PF_HOOK_RETURN`
(replace and return — ONLY under `PF_HOOK_PROVEN_ATOMIC`: the hook's
complete declared cost must fit before every known guest-visible deadline or
the exact implementation runs instead), `PF_HOOK_CONTINUE_EXACT` (observe
then defer), or `PF_HOOK_ERROR`. `src/hooks/hook_host.hpp` is the
`LoadLibrary`-based loader with transactional descriptor validation (ABI
version, timing contract, known target, no duplicates). Deliberately
deferred forever until needed: mid-block hooks, hook chains/priorities, live
reload, typed ABI adapters, per-call variable cost models (pf/13 "Deferred
deliberately"). This ABI is DOS-PM-specific in its target addressing
(`pm.func.<entry-hex>`) but the ABI SHAPE (stable ID, one hook per function,
explicit atomic-cost proof gate, fail-to-exact) is directly reusable for a
Win32 pilot's own hook layer.

**Snapshots of host-side state.** Win16's canonical schema
(`pf-canonical-win16-v17` per pf/18 table; pf/19 traces v3→v9 additions)
tracks EVERY deterministic side effect a real Win16 program can have: CPU/x87,
selector table, all 4 MiB memory, loaded NE bytes, API thunk layout, logical
handles/windows/messages/timers/files/profile/DOS vectors, GDI object/DC/
palette state, raw window/bitmap surfaces, global-heap free ranges, DGROUP
local-heap blocks, dynamic-library handles, pending PCM/MIDI device state,
queued modal results. Nothing is "left to the host" as untracked state — this
is the discipline a Win32 pilot needs for whatever subset of Win32 (heap,
handles, window/message state, GDI or DirectX surface state) it chooses to
own deterministically rather than forward.

**What was hard, concretely (measured, not hypothesized):**
- **Presentation churn dwarfed guest execution cost.** At one 51.6M-instruction
  endpoint only 456 surfaces genuinely changed, but the naive frontend path
  produced 39,784 window upserts / 74,757 Qt configure calls / 23,248 paint
  events — coalescing identical `WindowDesc` values against Qt cut wall time
  from 14.6s to 13.1s and paint events 40×, at unchanged guest state and
  digest (pf/21 "Diagnosis").
- **Win16 has no DOS-style idle-poll parking.** The DOS `IdleParkDetector`
  proves a poll inert by identical port reads; Win16's `PeekMessage`/
  `GetMessage` polling loops are NOT inert — measured proof attempts on real
  SimAnt polling cycles found CPU and guest-memory state changing in 4/4
  bounded attempts (real bookkeeping happens between polls), so a
  component-proof parking detector (`ReplayWaitParker`) had to be built that
  compares normalized canonical state at two loop occurrences rather than
  assuming any poll is free (pf/21 "Why DOS parking cannot be copied").
  Applicable warning for Win32: message-pump idling is NOT free to skip by
  assumption; it must be proven per-loop.
- **Deterministic instruction rate is a per-project measured constant, not a
  framework default.** SimAnt's `--recording-instructions-per-ms` was swept
  and pinned at 2,000 (not a benchmark-derived "fast" number) because
  different rates cross different in-game timing BRANCHES, not just
  different speeds (pf/21 "Restartable GetMessage idle gate"). A Win32 pilot
  will need its own such sweep per game.
- **Dynamic API resolution (LoadLibrary/GetProcAddress) had to be modeled
  explicitly**, not just static-import dispatch — SimAnt resolves its actual
  sound/MIDI calls this way at runtime (pf/19 "Win16 multimedia
  presentation"). Win32 games do this far more (COM, plugin DLLs, delay
  loading), so this generalizes as a bigger problem for Win32.
- **Selector/handle allocator bugs were subtle and silent for a long time**
  (pf/19 "Later gameplay gaps"): Win16 LDT handles carry TI/RPL bits, and an
  allocator emitting descriptor-aligned values corrupted memory that looked
  like an unrelated "unmapped selector" crash far downstream. Analogous class
  of bug for Win32: HANDLE-value or pointer-tagging assumptions baked into a
  generated carrier.

**What the Win16 platform has NOT yet reached** (truthful limits, pf/18 §5,
§9; pf/22): canonical audio/complete video streams remain unclaimed; full
generated (lifted) execution exists as a proof of concept on ONE corpus
(SimAnt, closing to 100% generated / 0% interpreter fallback on a specific
458-event journal, pf/22) but is NOT a whole-game claim; detachment
(carrier-free product) is unclaimed. This sets a realistic ceiling for how
far a Win32 pilot working alone can expect to get before hitting the same
class of "closed on one journal, not the whole game" wall.

---

## G. Concrete reusable code inventory

| candidate | path | what it does | dependencies | verdict |
|---|---|---|---|---|
| x86-32 decoder | `src/arch/x86_32/decoder.hpp` (253 ln) | Length+control-flow-only decode of the 32-bit flat instruction subset `Interp32` executes (prefixes, ModRM/SIB, jcc/jmp/call/ret/int classification, direct targets). Explicitly NOT the execution authority — decode-only, used by static discovery/emitter. Unclassified opcodes `ok=false` (fail-loud). | none beyond `<cstdint>`/`<vector>` | **ADAPT.** Real 32-bit x86 decoding logic is directly reusable; it decodes the ISA, not DOS. But it is scoped to what `Interp32`/dos_re's `cpu386.py` needed (no SSE, limited 0F map, no far-call-through-gate semantics) — a real Win32 binary will hit opcodes this decoder returns unclassified for (extend, don't assume complete). |
| x86-32 interpreter | `src/arch/x86_32/interp.hpp` (1,468 ln) | Faithful interpreter: flat segmentation (selector bases via caller-supplied lookup, no descriptor tables/paging/privilege checks), 32-bit default operand/address size, software INT dispatched like a syscall via a program-installed IDT-like map (`Cpu32::idt`, populated by an emulated "INT 21h AH=25h"-style call), hardware IRQ polled/delivered with a 32-bit EFLAGS/CS/EIP frame, fail-loud on unimplemented opcodes (raises with byte+address). | `alu.hpp`, `machine.hpp`, `core/observe.hpp` | **ADAPT, substantially.** The ALU/flag/decode core is real x86-32 semantics and reusable. But its interrupt model (software INT as "syscall," a program-populated vector table) is DOS4GW-shaped, not Windows-shaped — real Win32 programs don't raise INT for OS services; they call imported DLL functions directly. The interrupt/service dispatch layer needs replacing with an IAT-call-interception model, not adapting. |
| x86-32 machine state | `src/arch/x86_32/machine.hpp` (410 ln) | `Machine32`: 8×32-bit GPRs, EIP, EFLAGS, 6 segment registers with a selector→base/limit map (`selector_bases`, `selector_limits` — explicitly "no descriptor tables, no paging, no privilege checks" per file header), x87 stand-in via `double` (documented as an accuracy caveat, not IEEE-80-bit), one flat little-endian memory buffer with an MMIO tap window, page-dirty tracking for incremental hashing, write-barrier/write-observer hooks. | none beyond core | **ADAPT.** The flat-memory + page-tracking + MMIO-tap machinery is architecture-generic and valuable as-is. The selector model is a deliberate DOS4GW-flat-selector simplification (flat LE selectors resolve to base 0) — real Win32 also runs flat-selector (Windows sets up FLAT CS/DS/ES/SS via the GDT once at process start), so this simplification may actually be CLOSE to correct for Win32's own segment usage, but it was never validated against real Windows GDT/LDT/TEB-via-FS behavior (FS in Win32 points at the TEB — this model has no concept of that). |
| x86-32 ALU | `src/arch/x86_32/alu.hpp` (229 ln) | Flag-exact ALU ops (add/sub/logic/shift/rotate with AF/PF/CF/OF/ZF/SF semantics), transcribed from `dos_re cpu386.py`. | none | **REUSE-AS-IS.** Pure ISA arithmetic/flags, no platform dependency. |
| Win16 replay adapter | `src/platform/win16/replay_adapter.hpp`, `live_cycle.hpp`, `exact_machine_adapter.hpp` | `ReplayRuntimeAdapter` implementation pattern: boot/advance-to-selector, typed channel delivery, full continuation capture/restore, canonical projection. `LiveMessageLoopCycle` is the message-loop-specific `SinglePointLiveCycle` analog. | `src/replay/*` | **ADAPT — as a PATTERN, not code.** Win32's own message loop (`GetMessage`/`DispatchMessage`) is structurally identical to Win16's; the adapter shape (before/after retrieve phases, deterministic idle-wake-on-timer-or-input) transfers directly even though every type underneath (selectors, NE structures) does not. |
| Replay input schedule | `src/replay/input_script.hpp` (516 ln), `input_delivery.hpp` (210 ln) | ONE coordinate-kind enum/parser/printer for recorded input (`+irq0:N`, `@work:W`, timed); ONE input-delivery-timing rule (blocked-read-is-a-wait, not next-event). Both explicitly platform-neutral text-format machinery, called out in `docs/README.md`'s authority map as the single implementation every consumer must share. | `core/json.hpp` indirectly for other schedule forms | **REUSE-AS-IS** for the mechanism/rule; **ADAPT** the coordinate-kind list — a Win32 pilot needs its own heartbeat definition (there's no `dos.irq0-deliveries` equivalent; a Win32 game's own high-resolution timer or vblank-equivalent would have to be identified per pf/41's addendum discipline). |
| Differential mirror | `src/replay/differential_mirror.hpp` (221 ln) | Platform-generic candidate-vs-artifact differential driver: consumes the same immutable ArtifactV2 selectors/events as `ReplaySession`, applies a caller-supplied state-domain projection instead of comparing full checkpoints, raises `DifferentialNativeGap` at a declared native frontier rather than spinning. | `artifact.hpp`, `runtime_adapter.hpp` | **REUSE-AS-IS.** No DOS/Win16 dependency; this is exactly the shape a Win32 lockstep/hookdiff tool would drive. |
| sha256 | `src/core/sha256.hpp` | Dependency-free FIPS 180-4 SHA-256, used for every artifact/page/content digest in the framework. | none | **REUSE-AS-IS.** |
| JSON | `src/core/json.hpp` | Minimal full JSON reader/writer, int64-preserving, the one (de)serialization primitive under every artifact. | none | **REUSE-AS-IS.** |
| File I/O | `src/core/io.hpp` | Whole-file read/write, fail-loud. Windows-specific note: pulls in two narrow `dllimport` declarations (`MoveFileExW`, `GetLastError`) directly rather than `<windows.h>`, specifically to avoid `windows.h`'s `far`/`pascal` macros leaking into NE/Win16 declarations. | none | **REUSE-AS-IS** (and its narrow-declaration trick is directly relevant: a Win32 pilot will fight the SAME `windows.h` macro-pollution problem, likely worse, since it needs the real Win32 headers for structure layouts). |
| Restore transaction | `src/core/restore_transaction.hpp` | `RestoreTransactionContext` (nesting-safe depth guard) + `with_restore_rollback` for atomic state publication with a differently-shaped rollback representation. | none | **REUSE-AS-IS.** |
| Machine time | `src/core/machine_time.hpp` | Rational event-clock arithmetic (`ClockAdvance`, checked add/multiply) plus the single canonical `kPitClockHz = 1,193,182` constant, consolidated here specifically because five prior copies had drifted. | none | **REUSE-AS-IS** for the rational-clock arithmetic; the PIT constant itself is DOS/PC-hardware-specific and not applicable to a Win32 pilot's own timing source (QueryPerformanceCounter / multimedia timer / vblank), but the LESSON (name a constant once, cite the authority, never let five copies exist) is directly worth following. |
| Observe | `src/core/observe.hpp` | Low-overhead execution observer interface (levels Replay/Coverage/Graph/Layers/Trace; event kinds for calls/returns/interrupts/frame boundaries), attachable to any interpreter with one null-check cost when unattached. | none | **REUSE-AS-IS.** |
| Native-gap | `src/core/native_gap.hpp` (not read in full, referenced by `differential_mirror.hpp`) | Represents a declared point where native/generated execution cannot continue and must hand back to interpretation. | core | **ADAPT** — the concept (explicit, named execution-frontier capture) is exactly right for Win32; the shape likely needs Win32-specific fields (e.g. current DLL-import-thunk state) once written against it. |
| Verdict record / run ending | `src/replay/verdict_record.hpp`, `src/replay/run_ending.hpp` | The `portforge-verdict-v1` writer/validator support and the `RunEnding` enum exported to `config/portforge-run-endings-v1.json`. | `core/json.hpp` | **REUSE-AS-IS.** Platform-neutral by design (pf/72 §7). |
| Hook ABI | `src/hooks/pf_plugin_abi.h` (143 ln), `hook_host.hpp` | Pure-C plugin ABI: one hook per stable function ID, atomic-cost-proof gate before replacement, `LoadLibrary`-based loader with transactional descriptor validation. | none (C, no STL across the boundary) | **ADAPT.** ABI shape reusable; target addressing (`pm.func.<entry-hex>`) and the "declared cost fits before every guest-visible deadline" proof obligation need a Win32-appropriate deadline model (Win32 has no hardware IRQ-driven deadlines the way DOS does — deadlines would likely become "before the next message-loop yield" or similar). |
| Build target registry / build driver | `build.py` (1,508 ln), `config/build-targets-v2.json` | Incremental g++-based build driver: content-digest-keyed incremental compilation (not mtime-based), parallel target pool, one authoritative target registry consumed by both `build.py` and CMake, skippable-test identity by enumerated input closure. | g++/gcc via `$CXX`/`$CC` env or `which` | **REUSE-AS-IS** as the framework's own build system — a Win32 pilot living inside `port_forge` should register its new targets in `config/build-targets-v2.json`, not invent a parallel build path. |
| CMake root | `CMakeLists.txt` | Header-only INTERFACE-library module graph mirroring `src/`'s directory layers (`pf_core`→`pf_arch_*`/`pf_replay`/`pf_host`→`pf_platform_*`→ concrete targets); `if(MSVC) /W4 /WX else -Wall -Wextra -Werror`. | CMake ≥3.20 | **REUSE-AS-IS / ADAPT.** The module-graph PATTERN is exactly what a `pf_platform_win32` INTERFACE library should follow (depends on `pf_core`, `pf_arch_x86_32`, `pf_formats`, `pf_devices`, `pf_replay`, `pf_hooks`, per the existing `pf_platform_dos_pm`/`pf_platform_win16` targets at lines 62–67). MSVC IS already a first-class compiler branch here — relevant since a real Win32 pilot may need MSVC-specific structure-layout fidelity that MinGW g++ does not guarantee. |
| Compiler flags | `scripts/compiler_profile.py` | `CXX_BASE_FLAGS = -std=c++17 -O2 -Wall -Wextra -Werror`; the framework's OWN build flags (not guest-toolchain profiles — those are a separate `portforge-toolchain-profile-v1` schema for MODELING a game's original compiler). | none | **REUSE-AS-IS.** |
| Seam generator | `tools/pf_seam_gen.py` (4,181 ln) | Generates a brick's projection/codec/seam/tier-0 gates from a hand-written YAML manifest against a program model, layout bindings, toolchain profile, and block table. | Python 3, `schemas/portforge-brick-manifest-v1.schema.json` | **ADAPT.** The generator's CONTRACT (manifest → generated seam, `validate`/`gate purity`/`gate publication`/`gate identity`/`certify`/`claim` subcommands) is directly reusable once a Win32 emitter exists to supply the block table and layout bindings it consumes; the generator itself has zero DOS-specific code paths visible in its purpose statement, but it currently only has DOS-RM fixtures/goldens (pf/89 §9 "the tool lands ahead of any project's first brick"). |
| DOS-RM emitter | `tools/pf_rm_emit.cpp` (8,696 ln) | Emits promoted x86-16 blocks as standalone C++ (state struct, block bodies+dispatcher, tick record, host-differential harness, per-block verify harness, optional split-form). | `src/recovery/block_promotion.hpp`, `src/recovery/discover.hpp`, `src/arch/x86_16/*` | **NOT-APPLICABLE directly** (x86-16 real-mode specific, DOS ABI-shaped) but **the single best worked example of the emitter CONTRACT a Win32 emitter must replicate**: generated-file DO-NOT-EDIT headers, the five generated-artifact kinds (emitted/ticks/hostdiff/verify/splitdiff), fail-loud exclusion of unsupported semantics with the reason recorded. Read it as a spec, don't port it. |
| Lockstep / hookdiff tools | `tools/pf_rm_lockstep.cpp` (582 ln), `tools/pf_rm_hookdiff.cpp` (1,709 ln) | Frame-boundary and per-hook differential verification tools, DOS-RM specific. | DOS-RM platform | **NOT-APPLICABLE directly; ADAPT the two-granularity idea** (frame-boundary comparison catches gross state; per-call-boundary comparison catches FLAGS/state overwritten before the next frame boundary — a Win32 pilot needs both granularities too). |
| `carrier_replay.hpp` | `src/host/carrier_replay.hpp` (1,272 ln) | The "recording is the operand for both executors" mechanism (§C above), bound tightly to `DosRmEngine`/`VkState`. | DOS-RM engine | **NOT-APPLICABLE directly; the PATTERN (§C, §E) is the reusable part.** |
| Registry / onboarding scripts | `tools/pf_project.py`, `scripts/capability_graph.py`, `scripts/project_contracts.py` (not fully read; referenced pf/18 §7, pf/46) | Capability-graph scaffold/status/doctor/plan/detachment tooling consumed by every platform onboarding. | control-plane only, no platform-specific code | **REUSE-AS-IS** as the onboarding mechanism itself; a Win32 pilot's FIRST real step per pf/46 is `pf_project.py platform scaffold . win32 bootable` after registering `win32` in `config/platforms-v1.json`. |

**Not present anywhere in the tree, and would have to be written from
scratch (INFERRED from the absence of any PE/COFF reference in
`src/formats/`, confirmed by `grep`):** a PE32/PE32+ loader (analogous to
`src/formats/ne.hpp` for NE or `src/formats/le.hpp` for LE), an import-table
(IAT) resolution and thunk-generation layer, any Win32 KERNEL32/USER32/GDI32
service model, and any x86-32 canonical-state schema that isn't DOS4GW-shaped
(`src/platform/dos_pm/canonical.hpp`'s `pf-canonical-pm-v12` is entirely
`Dos4gwHost`-specific — video mode/DAC/VGA/PIC/PIT/Sound-Blaster/DOS
allocator — none of it is Win32-relevant except as a canonical-schema-design
example to imitate).

---

## H. Contract obligations for a new "win32" platform

**pf/46 (onboarding) — the maintained progression, and where a pilot
realistically stops:**

```
bootable -> deterministic -> replayable -> observable -> liftable
-> replaceable -> detachable
```

Concrete required steps (pf/46 §1–§8): (1) register `win32` in
`config/platforms-v1.json` — `platform scaffold` REJECTS unknown platform
IDs; (2) `pf_project.py platform scaffold . win32 bootable` — creates
`portforge.capabilities.json` entries marked `scaffolded` (never edit a
claim to `conformant` to silence `doctor` — pf/46 §2 explicit warning); (3)
author a `portforge-boundary-profile-v1`/`v2` file declaring semantic
points, phases, channels, hold/pulse/edge/latch semantics, and (v2) the
declared machine's heartbeat census; (4) implement `ReplayRuntimeAdapter`
explicitly (boot, deadline-aware advance, fail-closed channel delivery,
complete `ContinuationState`, versioned `CanonicalState`, canonical
audio/video commits where claimed); (5) record a short deterministic V2
smoke corpus testing cold repeat, every channel semantic, rejection of
missing/extra/reordered/stale events, session capture/resume, checkpoint
mismatch, output-partition invariance, at least one negative control; (6)
populate an implementation catalog and resolve immutable execution plans;
(7) prove detachment SEPARATELY (a dependency-free plan is necessary but not
sufficient); (8) regenerate Atlas projections and document. **A platform is
"onboarded" only when the declared capability closure passes from a clean
checkout and the V2 artifact/session path is the ACTIVE workflow** (pf/46
"Final gate").

**pf/76 (port contract) — what would be required of a `win32` composition:**
the six-surface split (emitted unit / host services / engine / presentation
/ replay-and-record / brick seam) with ONE authority per surface and a
project supplying only the marked-project column. Its conformance gate
(`pf_project.py port-conformance PROJECT`, pf/76 §3) checks per surface that
the project's `native/` contains no private dispatch loop/delivery
rule/stream writer, every linked service family resolves to the shared
platform layer, and gates declare tiers. **This is DOS-RM/DOS-family
machinery today** (§2.1's amendment cites `src/platform/dos`,
`DosRmEngine`) — a `win32` platform would need its OWN equivalent surface
split before this gate could apply to it; nothing here auto-generalizes.

**pf/73 (test tiers) — directly applicable, platform-neutral, adopt as-is:**
the four-tier structure (§A above) and the two carrier standing requirements
(replaceability, comparability) apply verbatim to any carrier this framework
produces, DOS-specific or not. `tools/pf_gate_all.py`, `tools/pf_verdict.py`,
and the "blocks split never fused" emitter rule are the enforcement points a
Win32 emitter must also honor.

**pf/74 (cross-project gate) — likely PREMATURE for a solo pilot.** It gates
every `port_forge` change by every SIBLING project's own declared workflow
(`tools/pf_gate_all.py`). A Win32 pilot working on a feature branch inside
the same submodule does not need to satisfy other projects' gates until
landing changes that affect shared framework code; changes scoped to a new
`src/platform/win32/` tree are lower-risk for this gate than changes to
`src/core/`, `src/replay/`, or `src/host/`.

**What is premature for a pilot, explicitly:** pf/76's full conformance gate
(needs a working `win32` engine/services split to even apply to); pf/77's
eight promotion obligations and coastline-zero standalone-envelope proof
(these govern moving FROM a working carrier TOWARD a clean source port —
not relevant until a carrier exists at all); pf/89's brick-manifest tier-0
gates (need a working emitter + block table first). The realistic FIRST
target per pf/46 is `bootable` → `deterministic` → a recorded V2 replay
corpus — i.e., get a PE loader + x86-32 interpreter + minimal Win32 service
stubs running one deterministic boot-to-menu recording before any brick,
seam, or detachment question is relevant.

---

## I. Build system facts

- **Compiler:** `g++`/`gcc` via `$CXX`/`$CC` env vars or `which g++`/`which
  gcc`, C++17, `-O2 -Wall -Wextra -Werror` (`scripts/compiler_profile.py`,
  `build.py` line 110-111). On Windows this is MinGW g++ — confirmed by
  Win16's documented build path: `qmake -o build-qt\Makefile pf_w16_qt.pro`
  then `mingw32-make -C build-qt` (pf/19 "Reproducible proof", "Native Qt
  build"). Qt 6.8.3/MinGW compiled successfully per that same doc.
- **CMake** (`CMakeLists.txt`) is a SECOND, parallel build description of
  the same target registry (`config/build-targets-v2.json`), explicitly kept
  in sync by `scripts/check_build_targets.py`. It DOES branch on MSVC
  (`if(MSVC) /W4 /WX else -Wall -Wextra -Werror`), so MSVC is a supported —
  if perhaps less-exercised — compiler path for this codebase. **INFERRED:**
  given real Win32 binaries (especially anything using MFC, COM, or
  MSVC-specific ABI quirks a source port might eventually need to match
  precisely) may be easier to validate structurally against MSVC's own
  headers/ABI, a Win32 pilot should confirm early whether it needs MSVC
  specifically for header/struct-layout fidelity or whether MinGW is
  sufficient.
- **Build target registry:** `config/build-targets-v2.json` is the single
  source of truth for executable roots, sources, host requirements
  (`hosts:` gating, e.g. Qt-required targets), and test classification; both
  `build.py` and CMake consume it, and `scripts/check_build_targets.py`
  rejects drift between the two build surfaces.
- **32-bit x86 builds:** the question is about the HOST build (does the
  framework compile a 32-bit executable) vs the GUEST architecture
  (`pf_arch_x86_32` models a 32-bit guest CPU, run under a normal 64-bit
  HOST build of PortForge's own tools). **INFERRED — not explicitly stated
  in any doc read:** nothing suggests PortForge's own tools are built as
  32-bit host binaries; `pf_arch_x86_32`/`pf_platform_dos_pm` are interpreter
  targets running a 32-bit GUEST inside an ordinary (almost certainly 64-bit)
  host process. Confirm with `config/build-targets-v2.json` directly if host
  bitness ever matters (e.g. for hook DLL ABI matching against a real Win32
  target process).
- **Qt usage:** Qt 6 Widgets is the presentation backend for Win16
  (`pf_w16_qt.pro`) and Mac68k (`pf_mac_qt.pro`); there is a third `.pro`,
  `qt_backend_projection_units.pro`, presumably shared Qt-backend unit tests
  (INFERRED from filename, not read). DOS-RM/DOS-PM use a different (INFERRED:
  non-Qt, likely native Win32 GDI/DirectSound via `src/host/waveout_audio.hpp`
  and `src/host/dos_video.hpp`'s "window half," per pf/76 §2 surface D
  description) presentation path — `pf_play` is named as the windowed/sound
  front end in pf/76 §2 surface D without naming its UI toolkit.
  `src/host/waveout_audio.hpp` is explicitly named as "the Win32 [audio
  output] one" (pf/18 authority-map row "audio presentation") — i.e. the
  framework ALREADY has one genuinely Win32-API-specific file
  (`waveOutOpen`/`waveOutWrite`-based), narrowly scoped to audio output, that
  a Win32 pilot's own presentation layer could study or reuse directly.

---

## J. Open questions / warnings for the Win32 pilot

1. **No PE loader exists.** `src/formats/` has MZ/NE/LE/Amiga/Mac readers
   and nothing PE-shaped (confirmed by direct grep for
   `IMAGE_DOS_HEADER`/`IMAGE_NT_HEADERS`/PE-signature strings — zero hits).
   This is the first and largest missing piece, full stop.
2. **The interrupt-as-syscall model in `Interp32` (§G) does not fit Win32.**
   DOS4GW/DOS-PM programs invoke OS services via software interrupts into a
   program-installed vector table; Win32 programs call imported DLL
   functions through the IAT. The x86-32 CPU core is reusable; its
   service-dispatch integration point is not — a Win32 interpreter/emitter
   needs IAT-call interception (detect a call/jmp to an IAT slot address,
   not an INT number) as its primary service-boundary mechanism.
3. **Win16's precedent is "reimplement deterministically," not "forward to
   host OS."** win32_pilot.md's stated strategy ("forward compatible OS
   functionality to the real host Windows wherever possible") is a
   DIFFERENT approach from anything PortForge has built before (§F). Forwarding
   to a live, non-deterministic host OS conflicts with the framework's
   replay-determinism contract (pf/41, pf/42) unless every forwarded call's
   result is captured as a recorded/replayable event — which is exactly the
   "migration timing service" / "residual execution" category pf/77 §5
   already names and bounds (a temporary, explicitly authoritative, COUNTED
   dependency, not silent passthrough). Decide explicitly, early, whether
   forwarded Win32 calls will be captured as replay events (framework-
   compatible) or left as an undeterministic escape hatch (framework-
   incompatible) — this affects nearly every other design choice.
4. **No canonical schema exists for Win32 state**, unlike DOS-RM/DOS-PM/
   Win16 which each have a versioned `pf-canonical-*` schema enumerating
   every deterministic guest-visible component. This must be authored
   before ANY replay/verification work is meaningful (pf/41 "Continuation is
   not canonical state").
5. **The x86-32 machine's segment model was validated against DOS4GW's flat
   selectors, never against real Windows GDT/LDT/TEB-via-FS behavior.** Win32
   uses FS to reach the TEB (Thread Environment Block) — this model has no
   TEB concept at all today (INFERRED from `Cpu32`/`Machine32` structure:
   segment bases are a flat lookup map with no per-thread structure).
6. **Deterministic timing source is unsolved for Win32.** DOS has the PIT;
   Win16's precedent used a swept, pinned, per-game instruction-rate
   constant instead of any hardware clock. A Win32 game likely uses
   `QueryPerformanceCounter`, `timeGetTime`, or vblank — none of which are
   naturally "a heartbeat the program produces from a device it programmed"
   in pf/41's addendum sense unless the game programs its own timer (e.g.
   `timeSetEvent`). This needs its own investigation per game before any
   heartbeat/counted-coordinate design is possible (pf/41 addendum, pf/42
   "The work coordinate").
7. **Presentation-churn cost (§F) will likely be WORSE for Win32** than
   Win16: modern Win32 games use far higher-frequency GDI/DirectX
   presentation than a Win3.x program, so the Qt-coalescing lesson (compare
   full backend-neutral descriptions before triggering a host repaint)
   should be built in from the start, not retrofitted after a performance
   audit.
8. **Multi-threading is a genuinely new problem.** Every existing PortForge
   platform (DOS-RM/PM, Win16, GBC, Amiga, Mac68k) is single-threaded by
   construction (16-bit DOS/Win16 cooperative model, or single-CPU retro
   machines). A real Win32 game may create OS threads; nothing in the
   replay/session architecture (`ReplaySession`, `LiveReplaySession`,
   `SinglePointLiveCycle`) has any multi-thread-aware coordinate concept.
   This is INFERRED to be a substantial, currently-unaddressed architectural
   gap rather than an adaptation of existing machinery.
9. **The hook ABI's atomic-cost-proof deadline model has no Win32 analog.**
   DOS-PM's hooks prove their cost fits before the next hardware-IRQ-derived
   deadline; Win32 has no equivalent periodic hard deadline in the same
   sense (a message pump yields voluntarily, it isn't interrupted). A Win32
   hook ABI needs its own definition of "safe to replace atomically."
10. **`src/host/io.hpp`'s `windows.h`-avoidance trick (§G) foreshadows a
    bigger problem.** The framework currently avoids including `<windows.h>`
    anywhere in shared code specifically to dodge Win16-incompatible macros
    (`far`, `pascal`). A Win32 pilot needs real Windows SDK headers for
    accurate struct layouts (`PEB`, `TEB`, `CONTEXT`, etc.) — expect macro
    collisions with existing NE/Win16 code if the two platforms are ever
    built in the same translation unit, and plan namespace/header isolation
    accordingly from the start.
