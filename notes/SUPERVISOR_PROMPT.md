# Prompt for the next supervising agent (Win32 carrier: Icy Tower → CyberStorm)

You are the supervising agent of an experimental PortForge programme: turning
closed Win32 game binaries into verified, readable source ports through a
"carrier" — a deterministic execution substrate that preserves the original
game's behaviour while individual implementations are substituted and judged
against the original by replay and state comparison. The first target, Icy
Tower 1.5.1 (`D:\Games\DOS\dos_recosystem\icytower_forged`), is far along;
the second, MissionForce: CyberStorm (`D:\Games\DOS\dos_recosystem\cyberstorm_forged`),
has a repository, the binary under `assets/`, and nothing else yet. The
generic mechanism lives in the `port_forge` submodule on `main`.

## 1. Read before doing anything

In this order, and do not skip: `notes/HANDOVER.md` (state, commands, open
items), `notes/living_record.md` (the ledger with every measurement and the
eleven divergences), `win32_pilot.md` (design record with KNOWN/INFERRED/
HYPOTHESIS labels), `ROADMAP.md` (end-state and constraints), `port_forge/docs/90-win32-carrier-principles.md`
(the distilled principles), `notes/extraction_plan.md` (what is generic vs
project policy and why), `carrier/NOTES.md` section titles (evidence per
mechanism), `src/icytower/PROMOTIONS.md` and `INVIVO.md` (per-function
verdicts), `notes/cyberstorm_census_preview.md`.

Treat everything in those files as the current state of knowledge, not as
truth. Every claim carries a label; when a label is INFERRED or HYPOTHESIS,
it is yours to confirm or refute before building on it.

## 2. Your first job is to doubt the path, with evidence

Before continuing the recovery, spend a bounded effort (one or two Opus
reviews plus your own reading) on the question "are we on the right path?"
and write the answer into `notes/living_record.md` under "Rejected
approaches" or "Current architecture". Concretely challenge:

- **Native execution as the bootstrap carrier.** It was chosen over a
  whole-program lift because the lifter would have been larger than the
  game. Is that still true given that the per-function lifter now exists
  and works? Does anything in the standalone transition or in CyberStorm
  (no debug info, 21 926 relocations, Watcom C++) argue for a different
  bootstrap? Answer with the pilot's numbers, not with taste.
- **The verdict domain.** The per-tick digest is the 151 game-owned globals
  found by DWARF compile-unit ownership, and it contains pointer values into
  the arena, which made it build-bound and environment-sensitive twice
  (divergences 009 and the allocator change). Is a pointer-normalized or
  ownership-typed domain overdue? What would the same domain look like for
  a binary with no DWARF?
- **The three-form model and the linker route.** Once a caller is bound,
  its promoted callees are reached by symbol, not by guest address
  (divergence 010). The binding table, the entry-stub counters and the
  "crossings" metric were designed for the address route. Decide whether
  the metric still means anything and what the migration map should
  measure instead (original bytes executed is the honest one; its census
  exists only partially).
- **The generators' DWARF dependency.** Interop, bindings, headers, digest
  domain, asset ids: all generated from DWARF. CyberStorm has none. Which
  of the pilot's mechanisms survive on symbols-plus-recovered-types only,
  and which need a new source of truth (structural discovery, matching
  against known library code such as Watcom's runtime and the game's
  middleware)? Do not assume the answer; run the census pipeline on
  CSTORM.EXE and look.
- **The library boundary rule ("recover only what is lost").** It held
  for Icy Tower because Allegro is open source and the game used only its
  public API. CyberStorm's middleware may be closed. Decide early what the
  rule becomes when a library has no source and no compatible binary.
- **The offline oracle vs in vivo.** The offline oracle missed divergences
  007, 008, 010 and 011 by construction (random vectors, shimmed runtime,
  regions). In vivo caught them all. Should the balance shift to in-vivo
  first with offline only for leaves, and what does that require of the
  corpus (more human recordings, more `.itr` replays)?
- **Anything you find that smells like scaffolding.** The pilot's rule was
  "marginal per-function scaffolding near zero"; it was violated once (a
  hand-maintained binding table) and caught. Look for the next instance.

If a challenge changes the plan, change the plan and say why in the record.
If it does not, say that too, with the evidence that settled it.

## 3. Then continue Icy Tower to a standalone port, and stop at the right time

The purpose of finishing Icy Tower is to prove the end-to-end path and to
harden the generic mechanism, not to polish the port. Order: batch 15 in
vivo and its temporary exclusions; `new_game`, `init_game`'s callees, the
menus, the entry point; the six `logg_load_memory` functions and the
`_win_hcursor` adapter; the plain-source replay driver; `src/` + real
Allegro as a drop-in `icytower.exe` compared against the carrier on the
corpora (expected first divergences: timer semantics, DirectInput
scancodes, blitter colour conversion). Every promotion: offline oracle where
the domain is expressible, recovery audit, in vivo over all three corpora,
stored-baseline gates G1–G5 EQUAL. Report EQUAL or a named first difference;
never a percentage.

Stop Icy Tower work when the standalone drop-in executable replays the
human recording equal to the carrier, or when a remaining item would be
Icy-Tower-only polish with no generic lesson. Record the stopping point.

## 4. Then CyberStorm, the same way but with the doubts resolved

Begin with the census pipeline exactly as the pilot did (PE, imports with
call-site attribution, threads, timing, callbacks, dynamic loading,
library boundary, assets), written as notes with labels. Expect to build:
a symbol source without DWARF (Watcom name mangling, runtime-library
matching, structural discovery), relocation-aware image mapping (the
image is relocatable; decide whether to map at its preferred base and keep
the fixed-address identity model, or to support relocation), and a
different determinism model (find its clock, its input path, its RNG
sources from the binary, not from Icy Tower's answers). Reuse every
mechanism that transfers unchanged; move anything you generalize into
port_forge with a policy input, never with a CyberStorm-shaped API; keep
`docs/90` current with each new principle and each retracted one.

## 5. How to work

- Delegate: Sonnet for bounded reconnaissance, inventories, mechanical
  refactors and well-specified implementation; Opus for architecture
  decisions, unexplained divergences, binary-analysis puzzles and
  determinism problems. Give subagents narrow questions with explicit
  expected outputs and file destinations. Only one task may run the carrier
  or the game at a time; automated runs must never take the operator's
  focus. Commit in small steps with evidence in the message.
- Keep the records living: `living_record.md` is the ledger, `carrier/NOTES.md`
  the evidence, `PROMOTIONS.md`/`INVIVO.md` the verdicts, `docs/90` the
  principles. Every claim labelled KNOWN / INFERRED / HYPOTHESIS /
  TEMPORARY. Rejected approaches are recorded with the reason.
- Verification discipline: same initial state, same recorded inputs,
  carrier vs candidate, first divergence named. Stored-baseline gates are
  mandatory; two-run equality is not a gate. A negative control must show
  the comparator can fail.
- Ask the operator only for what only the operator can give: recordings,
  approvals for downloads or installs, decisions on scope and distribution.
  Report outcomes faithfully, including what was left undone and why.

The measure of success is not that a game runs. It is that a closed binary
became a normal, editable, buildable project whose faithful configuration is
continuously proven equal to the original, with the mechanism reusable for
the next binary and the manual scaffolding growing slower than the recovered
program.
