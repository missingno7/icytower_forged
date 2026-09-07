# External research findings (2026-09-07) and their verification status

Source: an external research agent's report on open questions, incorporated
here with what the binary and the project evidence say about each item.
Labels: KNOWN (verified in the binary/our runs), INFERRED, UNVERIFIED (web
claim not yet checked), DECISION.

## 1. Original toolchain

- Web: TDM-GCC 4.4.1-tdm-1 and -tdm-2 (SJLJ) are archived on SourceForge
  ("TDM-GCC Old Releases / TDM-GCC 4.4 series"); contemporary forum posts
  say tdm-1 had a code-generation bug and users were told to move to tdm-2.
- **KNOWN from the binary**: the linked libgcc objects carry the build path
  `c:\crossdev\b4.4.1-tdm-1\build-sjlj\mingw32\libgcc` (6 occurrences), and
  every CU's include path is `c:/program/codeblocks/mingw/.../gcc/mingw32/4.4.1`
  (also an `e:` variant), i.e. the MinGW bundled with Code::Blocks. So the
  runtime is **4.4.1-tdm-1 SJLJ**; the compiler driver is INFERRED to be the
  same bundle (Code::Blocks 10.05 shipped TDM-GCC 4.4.1). The research
  agent's "tdm-2 is the strong candidate" is therefore probably wrong for
  this binary; the fingerprint experiment should try tdm-1 first.
- libogg objects were compiled by "GNU C 4.2.1-sjlj (mingw32-2)" (KNOWN,
  DW_AT_producer): a prebuilt libogg from an older MinGW, not rebuilt by the
  game's toolchain.
- DECISION: archive both tdm-1 and tdm-2 (download needs operator approval),
  plus Allegro 4.4.1 source (git tag, already fetchable), and run the
  code-generation fingerprint experiment on update_frame/is_solid and on
  Allegro CUs (timer.c, blit.c, color.c, cblit32.c) against the embedded
  bytes. Goal: strongest possible criterion (ii) evidence and, if it
  reproduces, a compiler for `src/` that matches the original code
  generation. Byte identity is not assumed.

## 2. `logg_load_memory`

- Web: upstream Allegro 4 `addons/logg` has no `logg_load_memory`; `alogg`
  (a different addon) had memory/callback streaming.
- KNOWN (notes/library_compat_verdict.md): the game's logg CU has 7 extra
  functions (667 B) beyond the 11-function upstream logg.c.
- DECISION: classify as LIKELY LOCAL/VENDORED EXTENSION, provenance
  unresolved, recovery burden small. Inspect the machine code against the
  `ov_open_callbacks` memory-callback pattern; if it is a thin wrapper over
  vorbisfile callbacks, recover a tiny clean implementation. Never a reason
  to recover Vorbis/logg. (Inspection task queued.)

## 3. Allegro 4.4.1 → 4.4.3.1 behaviour

- Web (changelog): Windows changes after 4.4.1 are localized: DirectInput
  keyboard ALT/TAB stuck-key fixes after Alt-Tab, DirectDraw lost-surface
  and fullscreen fixes, a GDI crash fix on repeated mode changes; nothing
  obvious in the win32 timer or DirectSound mixer.
- Matches our own prediction order for first divergences (win32_pilot.md
  §7c) and the header-level ABI verdict (0 ABI differences, KNOWN).
- DECISION: two roles. **4.4.1 + TDM-GCC = reference/reconstruction**
  (fingerprinting, criterion (ii)); **4.4.3.1 = standalone candidate**,
  accepted only through the replay oracle. Divergence hotspots are
  focus/presentation paths, which deterministic replay avoids by design.

## 4. The 2010 replay_checker (RaMMicHaeL / Ramen Software)

- Web: "Revealing the secrets of Icy Tower v1.3.1" (ramensoftware.com,
  2010-04); described as completely open source; no standard licence text
  found on the page.
- KNOWN: we have the source (assets/replay_checker, see
  notes/replay_checker_reference.md); it rejects our `ITR140` replays; its
  physics/floor code matches our findings where compared (x87/FNINIT, LCG,
  seed+keys model) and differs where 1.5.1 changed (line_intersect guards,
  rejump handling).
- DECISION: reference evidence only; a SAME/CHANGED/UNKNOWN comparison note
  against 1.5.1 is produced as functions are recovered (add_floor next).
  Nothing is copied into `src/` without oracle proof, and nothing is
  redistributed until its licence is established.

## 5. Icy Tower versions and community documentation

- Web: download.icy.pl archives historical versions; the community
  documents 1.3→1.4 floor-generation/replay changes, and features named
  rejump, custom gravity, game speed, floor length, slowdown detection;
  icy.pl glossary; allegro.cc depot page. No public `ITR140` spec found.
- KNOWN: our DWARF-derived `Treplay`/`Trecord` decode and the five
  difficulty fields (notes/replay_format.md, notes/layout_determinism.md)
  are currently the best 1.5.1 documentation; the feature names above map
  onto header fields we already see (rejump, gravity_modifier,
  floor_size_modifiers, start_speeds; slowdown = the QPC/clock telemetry).
- DECISION: use community names when naming recovered state; never override
  binary behaviour.

## 6. Remakes / reimplementations

- Web: at least one Allegro-based remake used extracted datafile art and
  RaMMicHaeL's physics knowledge.
- DECISION: secondary semantic evidence for asset and state names (our
  asset ids currently derive from datafile object names, KNOWN); catalogue
  when naming questions arise; not an oracle.

## 7. Licensing of assets and source ports

- Web: no public statement from Free Lunch Design / Johan Peitz / Apskeppet
  AB authorizing asset redistribution or third-party ports.
- KNOWN: assets/readme.txt forbids repackaging (notes/asset_census.md §7).
- DECISION (already in ROADMAP §8 and win32_pilot §7c): distribution-neutral
  architecture; default public model = clean source + open-source
  libraries + asset import tooling, user supplies the original install.

## 8. Headless verification and cnc-ddraw

- Web: cnc-ddraw has fullscreen/windowed/borderless/scaling modes but no
  documented no-window mode; Allegro 4's DirectInput keyboard uses
  DISCL_FOREGROUND | DISCL_NONEXCLUSIVE, so physical input is tied to the
  foreground window.
- KNOWN: matches our measurements (focus loss is a live channel; SendInput
  reaches DirectInput; deterministic replay never depends on physical
  DirectInput — input is injected through Allegro's key functions).
- DECISION (already the design, win32_pilot §4a/§9a): headless = parked
  physical input + injected input + presentation suppressed or captured
  above the backend (frame digest at `blit_to_screen`) + hidden window;
  cnc-ddraw stays a presentation backend and is not the source of
  determinism or headless semantics.

## Queued experiments from this research

- A. Archive TDM-GCC 4.4.1-tdm-1/-tdm-2 SJLJ and Allegro 4.4.1; compiler
  fingerprint experiment (needs download approval).
- B. replay_checker SAME/CHANGED/UNKNOWN note, produced alongside add_floor
  and player-physics recovery.
- C. `logg_load_memory` machine-code inspection (running).
- D. Roles for 4.4.1 vs 4.4.3.1 recorded in win32_pilot §7c.
- E. Headless model recorded; implementation follows the isolation task.
