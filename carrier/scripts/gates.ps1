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
