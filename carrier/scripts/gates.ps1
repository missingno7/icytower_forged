# gates.ps1 - runs G1/G2/G3 (carrier/NOTES.md gate contract), restoring the
# pristine assets before every carrier launch. Run from carrier\.
param([string]$Tag = "gate", [string]$OutDir = "..\artifacts_t3")
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force $OutDir | Out-Null
function Restore { & powershell -File "$PSScriptRoot\restore_assets.ps1" }
function Run { param([string[]]$a) Restore; & .\carrier.exe @a 2>&1 | Out-Null }

$c = @("--det","--pace=fast","--input=script","--input-script","scripts/newgame.txt","--stop-at-tick","1000","--run-seconds","60")

Run ($c + @("--digest-out","$OutDir/${Tag}_g1a.txt"))
Run ($c + @("--digest-out","$OutDir/${Tag}_g1b.txt"))
"G1: " + (python scripts/compare_digests.py "$OutDir/${Tag}_g1a.txt" "$OutDir/${Tag}_g1b.txt" | Out-String).Trim()

Run ($c + @("--bind","update_frame=src","--fn-digest-out","$OutDir/${Tag}_g2A.txt"))
Run ($c + @("--bind","update_frame=original","--fn-digest-out","$OutDir/${Tag}_g2B.txt"))
"G2: " + (python scripts/compare_fn_digests.py "$OutDir/${Tag}_g2A.txt" "$OutDir/${Tag}_g2B.txt" | Out-String).Trim()

Run ($c[0..7] + @("--run-seconds","120","--digest-out","$OutDir/${Tag}_g3R.txt","--snapshot-at-tick","400","--snapshot-out","$OutDir/${Tag}_snap","--restore-at-tick","700","--bind","update_frame=src","--fn-digest-out","$OutDir/${Tag}_g3F.txt"))
"G3a: " + (python scripts/certify_snapshot.py rewind "$OutDir/${Tag}_g3R.txt" --anchor 400 --cold "$OutDir/${Tag}_g1a.txt" | Out-String).Trim()
"G3b: " + (python scripts/certify_snapshot.py fn "$OutDir/${Tag}_g3F.txt" | Out-String).Trim()
$sz = (Get-ChildItem "$OutDir/${Tag}_snap" -Recurse | Measure-Object -Property Length -Sum).Sum
"SNAPSHOT bytes: $sz"

# G4 ("in-vivo pass, corpus gates, asset oracle" pass, carrier/NOTES.md): a
# STORED-BASELINE gate, not a binary-vs-itself gate like G1/G2/G3 above -
# the distinction carrier/NOTES.md "Stage 2" documents ("G4 is the gate
# that catches a whole-run shift" - a uniformly shifted run is internally
# self-consistent, so only a comparison against a digest recorded and
# committed in an EARLIER pass can see it). Every generated binding-table
# row (scripts/all_src.bindfile) bound at once, replacing the operator's
# own recording's original machine code end to end.
# --bind-file needs an ABSOLUTE path (carrier/NOTES.md "Milestone 12 at
# scale" part E: bind_init() opens it AFTER the carrier's own startup
# _chdir into assets\, unlike the CLI's other path options).
Run (@("--det","--pace=fast","--input=script","--input-script","..\replays\human_test.txt",
       "--stop-at-tick","2528","--run-seconds","300",
       "--bind-file",(Resolve-Path scripts\all_src.bindfile).Path,
       "--digest-out","$OutDir/${Tag}_g4_human.txt"))
"G4: " + (python scripts/compare_digests.py "$OutDir/${Tag}_g4_human.txt" ..\replays\human_test.digest | Out-String).Trim()

# G5 - the same STORED-BASELINE contract as G4, over the .itr corpus
# workload (carrier/scripts/play_itr.txt, plays profiles/MissingNO/
# replays/last_game.itr - carrier/NOTES.md "in-vivo pass, corpus gates,
# asset oracle"). This workload's own --stop-at-tick never fires (det.cpp's
# safepoint_hit is the only place that checks it, and no further safepoint
# occurs once play() ends and the game returns to an idle menu loop that
# never calls it again) - --run-seconds is the only real stopper, and the
# 157-line digest (T=394..T=550) is complete well within it.
Run (@("--det","--pace=fast","--input=script","--input-script","scripts\play_itr.txt",
       "--run-seconds","15","--digest-out","$OutDir/${Tag}_g5_itr_a.txt"))
Run (@("--det","--pace=fast","--input=script","--input-script","scripts\play_itr.txt",
       "--run-seconds","15","--digest-out","$OutDir/${Tag}_g5_itr_b.txt"))
"G5a (itr replayed twice, unbound): " + (python scripts/compare_digests.py "$OutDir/${Tag}_g5_itr_a.txt" "$OutDir/${Tag}_g5_itr_b.txt" | Out-String).Trim()
Run (@("--det","--pace=fast","--input=script","--input-script","scripts\play_itr.txt",
       "--run-seconds","15",
       "--bind-file",(Resolve-Path scripts\all_src.bindfile).Path,
       "--digest-out","$OutDir/${Tag}_g5_itr_allbound.txt"))
"G5b (itr all-bound vs replays/itr_last_game.digest): " + (python scripts/compare_digests.py "$OutDir/${Tag}_g5_itr_allbound.txt" ..\replays\itr_last_game.digest | Out-String).Trim()
