# Roadmap: long-term direction for the Icy Tower source port

Status: **architectural intent**, recorded 2026-09-07 at the operator's
request. This is the intended end-state and the constraints that keep it
reachable. It is not a work order: mechanics are decided later on evidence
and experiments (see `win32_pilot.md` for the current design and
`notes/living_record.md` for what exists). Near-term work stays on the
critical path in §9; decisions made now must simply not close the doors
below.

The core idea:

> PortForge turns a closed binary into a normal editable source-port
> project. The final user gets one ordinary source-port executable with
> optional enhancements selectable in-game, the original behaviour remains
> available as a faithful configuration, and the private PortForge carrier
> keeps verifying that configuration against the original executable.

---

## 1. Final goal: a normal editable game project

Not "an executable wrapped in PortForge", not "a decompiled binary with
recovery machinery around it". An ordinary, readable, buildable, modifiable
project:

```text
IcyTower/
├── src/            clean game source
├── assets/         explicit game assets/data
├── third_party/    normal external/upstream dependencies (or libs/)
├── build files
└── icytower.exe
```

The original executable mixes four ownership classes:

```text
game-owned machine code
third-party statically linked libraries
embedded/packed assets and resources
compiler/runtime support
```

PortForge separates them along three independent migration coastlines:

```text
CODE        original game machine code        -> clean recovered source
LIBRARIES   embedded third-party code         -> normal existing/upstream libraries
ASSETS      embedded/packed resources, data   -> explicit named assets/files
```

The original executable disappears from the runtime dependency graph once
all three coastlines have left their ORIGINAL/EMBEDDED forms.

Guiding principle: **recover what is actually lost; reuse what already
exists.** Allegro, Vorbis/Ogg, CRT code and any other identified upstream
code are never function-by-function recovery targets when compatible
source or binaries exist (evidence: `notes/library_boundary.md`).

## 2. The clean source is independent of the recovery mechanism

The final source contains ordinary semantic names and ordinary game logic.
It does not know about:

```text
guest addresses          carrier memory        oracle machinery
original binary offsets  lifted functions      binary extraction details
PF_* bindings
```

The carrier may temporarily bind clean names to original addresses while
recovery is in progress; those mappings live outside the clean source
(today: generated `carrier/gen/pf_bindings*.h`, enforced by
`scripts/check_native_layer.py`).

The same applies to assets: clean code refers to semantic asset identities,
never to original locations:

```text
ASSET_PLAYER_IDLE, ASSET_JUMP_SOUND, ASSET_MENU_BACKGROUND
   not: guest address 0x...., datafile object #37, raw blob offset ...
```

In the carrier an asset id may resolve to bytes embedded in the original
game; in the standalone build the same id resolves to a normal file.

Three analogous migration kinds, allowed to migrate independently:

```text
FUNCTION   ORIGINAL -> LIFTED -> NATIVE
LIBRARY    EMBEDDED ORIGINAL -> EXTERNAL/UPSTREAM
ASSET      EMBEDDED ORIGINAL -> EXTRACTED/NAMED FILE
```

## 3. The carrier remains useful after the standalone port exists

The carrier disappears as a runtime dependency but stays valuable as the
verification oracle:

```text
                same replay
                   |
        +----------+----------+
        |                     |
original + carrier       clean standalone port
        |                     |
        +----------+----------+
                   |
             compare behaviour
```

The carrier is a temporary runtime/migration substrate and a potentially
permanent executable specification of the original behaviour. The public
game project exists completely independently; private PortForge tooling can
still build it, run it, feed it deterministic workloads, and compare it
against the original oracle.

## 4. One normal user-facing build

Faithful, enhanced and modded behaviour do not require separate
executables. The intended experience is a conventional source port:

```text
icytower.exe
    ├── original-faithful behaviour by default
    └── optional enhancements selectable by the player
        through the game menu or an overlay/settings menu
```

Examples (UI and feature set decided much later):

```text
Options > Source Port Enhancements
  [ ] Widescreen            [ ] Controller support
  [ ] High-resolution render [ ] Enhanced HUD
  [ ] Modern scaling        [ ] Debug/telemetry overlay
  [ ] Modern audio backend  [ ] Gameplay tweak X
```

Canonical game logic and optional enhancements coexist in one maintainable
codebase without making original behaviour unreproducible:

```text
all enhancements OFF                     -> faithful baseline
non-behavioural enhancements ON          -> same gameplay semantics,
                                            different presentation/platform behaviour
gameplay-changing enhancement ON         -> intentional behavioural divergence
```

A runtime/configuration distinction, not necessarily a build distinction.

## 5. Enhancements are classified by their effect on verification

**A. Observe-only** (debug/state overlay, telemetry, AI observer,
FPS/statistics, streaming integration, inspection tools): read state only.
Contract: canonical game state MUST remain equal to the original; full
replay equivalence expected.

**B. Presentation/platform** (widescreen, higher rendering resolution or
refresh rate, new renderer, controller support, scaling, new audio backend,
HUD derived from existing state, platform integrations):

```text
gameplay state     MUST match
RNG                MUST match
simulation events  MUST match
presentation       MAY differ
```

Enabling widescreen must not make the verifier give up; it verifies the
domains that are supposed to stay faithful.

**C. Gameplay-changing** (changed physics, new mechanics, enemies, modes,
scoring, different procedural generation): intentional divergence, with a
scoped contract rather than disabled verification:

```text
player physics      MAY DIFFER
menu logic          MUST MATCH
save parsing        MUST MATCH
unaffected RNG      MUST MATCH
unaffected systems  MUST MATCH
```

Long-term the private verifier may understand canonical domains
(gameplay, RNG, input, save/load, UI, renderer, audio, timing) and know,
from the enabled enhancements, which MUST MATCH, MAY DIFFER, or ARE
INTENTIONALLY MODIFIED. Not to be built now; preserved so that future
architecture never assumes equivalence is all-or-nothing. (The current
per-tick digest of game-owned globals is the first such domain.)

## 6. The faithful baseline must remain selectable

```text
icytower.exe + all behavioural enhancements disabled -> faithful baseline
```

That mode is what private PortForge verification continuously compares
against the original executable: a permanent regression target. A new
enhancement is introduced so that `enhancement OFF -> existing faithful
behaviour unchanged`, unless there is a strong reason otherwise.

## 7. The final public repository contains the game, not PortForge

```text
public icytower-source-port/
├── src/
├── include/
├── assets/              where redistribution is appropriate
├── third_party/
├── build files
├── README
└── source-port enhancement code
```

Purity criterion: can somebody clone it, build it, run it, enable/disable
enhancements, and modify the game without any PortForge-specific
knowledge? It must not need: carrier internals, lifter, guest-address maps,
DWARF recovery machinery, ORIGINAL/LIFTED dispatch, binary instrumentation,
private replay corpus, oracle snapshots, internal reverse-engineering
artifacts, promotion tooling.

```text
PRIVATE PORTFORGE  = factory + microscope + oracle + recovery process
PUBLIC SOURCE PORT = finished game + normal source-port enhancements
```

The private side may continuously check out, build and verify the public
repository's faithful configuration against the original. Separating the
repositories does not lose verification; it makes the verifier an external
test laboratory rather than part of the product.

## 8. Distribution possibilities (preserved, none chosen yet)

- **Drop-in compatibility mode**: a new `icytower.exe` placed into an
  original installation, using the existing files/layout (data files,
  config, profiles, replays, characters). A useful first standalone
  milestone.
- **Clean standalone layout**: `IcyTower/ {icytower.exe, assets/, config/,
  replays/, libraries/}`; enhancements through the normal options menu.
- **Source-port-only distribution**: if original assets cannot or should
  not be redistributed: clean source, open-source dependencies, enhancement
  code, and asset extraction/import tooling; the user supplies an original
  installation.

The technical architecture must not force the legal/distribution decision
prematurely (asset-side evidence: `notes/asset_census.md`).

## 9. Roadmap implication

Near-term work stays on proving that recovery scales:

```text
promote more game functions
keep marginal manual scaffolding near zero
complete deterministic replay
snapshots / oracles
library boundary
asset census
standalone transition
```

Decisions made now only have to avoid closing the doors above. Concretely,
constraints already honoured or to honour: address-free `src/` with a
purity gate; generated bindings outside the source; library API by upstream
name; asset ids instead of locations; the verdict domain expressed as named
state domains rather than a single all-or-nothing digest; one build with
runtime-configurable enhancements and a selectable faithful baseline.
