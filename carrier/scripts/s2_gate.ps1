# s2_gate.ps1 - Stage S2 full gate set for one unit move.
# Runs G1/G2/G3 (gates.ps1), the human_test all-bound run, and the purity gate.
# Run from carrier\.  Usage:  powershell -File scripts\s2_gate.ps1 -Tag u1
param([string]$Tag = "s2", [string]$OutDir = "..\artifacts_s2")
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force $OutDir | Out-Null

& powershell -File "$PSScriptRoot\gates.ps1" -Tag $Tag -OutDir $OutDir

& powershell -File "$PSScriptRoot\restore_assets.ps1"
& .\carrier.exe --det --pace=fast --input=script --input-script ..\replays\human_test.txt `
    --stop-at-tick 2528 --run-seconds 120 --bind-file (Resolve-Path scripts\all_src.bindfile).Path `
    --digest-out "$OutDir/${Tag}_human.txt" 2>&1 | Out-Null
"G4: " + (python scripts/compare_digests.py "$OutDir/${Tag}_human.txt" ..\replays\human_test.digest | Out-String).Trim()

$p = (python ..\scripts\check_native_layer.py 2>&1 | Out-String).Trim()
"PURITY(exit $LASTEXITCODE): " + ($p -split "`n" | Select-Object -Last 1)
