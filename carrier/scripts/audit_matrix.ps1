# audit_matrix.ps1 - the determinism audit's per-channel experiment runner
# ("Environment isolation" pass, notes/determinism_audit.md). Each row runs the
# G1 workload once with a different DET_ISOLATE_OFF / DET_PERTURB_* setting and
# compares the resulting digest stream against a reference, so every audit
# verdict is a measured FIRST DIFFERING TICK (or EQUAL), never an assertion.
#
#   .\scripts\audit_matrix.ps1 -OutDir ..\artifacts\determinism_audit
#
# Assets are restored before every launch (carrier/NOTES.md convention).
param([string]$OutDir = "..\artifacts\determinism_audit")
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$base = @("--det", "--pace=fast", "--input=script", "--input-script", "scripts/newgame.txt",
          "--stop-at-tick", "1000", "--run-seconds", "90")

function Restore { & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\restore_assets.ps1" }

function Run {
    param([string]$Name, [hashtable]$Env = @{}, [string[]]$Extra = @())
    Restore
    foreach ($k in $Env.Keys) { Set-Item -Path "Env:$k" -Value $Env[$k] }
    & .\carrier.exe @base @Extra --digest-out "$OutDir/$Name.txt" --report "$OutDir/${Name}_report.json" 2>"$OutDir/$Name.stderr.txt" | Out-Null
    foreach ($k in $Env.Keys) { Remove-Item -Path "Env:$k" -ErrorAction SilentlyContinue }
}

function Cmp {
    param([string]$Label, [string]$A, [string]$B)
    $r = (python scripts/compare_digests.py "$OutDir/$A.txt" "$OutDir/$B.txt" | Out-String).Trim()
    "{0,-34} {1}" -f $Label, ($r -split "`n")[0]
}

# Reference: everything this pass isolates, ON.
Run -Name "iso_all"
# Everything OFF = the carrier as it behaved BEFORE this pass.
Run -Name "iso_none"  -Env @{ DET_ISOLATE_OFF = "ad,mouse,switch,window" }
# One channel isolated at a time (the rest left as before the pass).
Run -Name "only_ad"     -Env @{ DET_ISOLATE_OFF = "mouse,switch,window" }
Run -Name "only_mouse"  -Env @{ DET_ISOLATE_OFF = "ad,switch,window" }
Run -Name "only_switch" -Env @{ DET_ISOLATE_OFF = "ad,mouse,window" }
Run -Name "only_window" -Env @{ DET_ISOLATE_OFF = "ad,mouse,switch" }
# The cleaner orientation: exactly ONE isolation removed from the fully
# isolated reference, so each verdict names one channel with no confound.
Run -Name "no_ad"     -Env @{ DET_ISOLATE_OFF = "ad" }
Run -Name "no_mouse"  -Env @{ DET_ISOLATE_OFF = "mouse" }
Run -Name "no_switch" -Env @{ DET_ISOLATE_OFF = "switch" }
Run -Name "no_window" -Env @{ DET_ISOLATE_OFF = "window" }
# Reproducibility of the fully-isolated configuration.
Run -Name "iso_all2"
# Clock channel controls (item 4): clock()/QPC/timeGetTime must NOT reach the
# digest, time() MUST.
Run -Name "perturb_clock" -Env @{ DET_PERTURB_CLOCK = "123456" }
Run -Name "perturb_time"  -Env @{ DET_PERTURB_TIME = "86400" }
# Window mode variants (item 1).
Run -Name "win_hidden" -Extra @("--window=hidden")
Run -Name "win_normal" -Extra @("--window=normal")
# Real-time pace, three times (the audit's "real-time pace x3" channel).
Run -Name "pace_real1" -Extra @("--pace=real")
Run -Name "pace_real2" -Extra @("--pace=real")
Run -Name "pace_real3" -Extra @("--pace=real")
# Unrestored mutable files: three runs in a row with NO asset restore between
# them, to quantify the first differing tick the restore convention prevents.
& .\carrier.exe @base --digest-out "$OutDir/norestore1.txt" --report "$OutDir/norestore1_report.json" 2>"$OutDir/norestore1.stderr.txt" | Out-Null
& .\carrier.exe @base --digest-out "$OutDir/norestore2.txt" --report "$OutDir/norestore2_report.json" 2>"$OutDir/norestore2.stderr.txt" | Out-Null
& .\carrier.exe @base --digest-out "$OutDir/norestore3.txt" --report "$OutDir/norestore3_report.json" 2>"$OutDir/norestore3.stderr.txt" | Out-Null
# The window-activation positive control: the same workload plus one
# `switch out`/`switch in` pair, delivered from the script.
Restore
& .\carrier.exe --det --pace=fast --input=script --input-script scripts/newgame_switch.txt --stop-at-tick 1000 --run-seconds 90 --digest-out "$OutDir/switch_posctrl.txt" --report "$OutDir/switch_posctrl_report.json" 2>"$OutDir/switch_posctrl.stderr.txt" | Out-Null

Cmp "iso_all vs iso_all2"        "iso_all"  "iso_all2"
Cmp "iso_none vs iso_all"        "iso_none" "iso_all"
Cmp "only_ad vs iso_none"        "iso_none" "only_ad"
Cmp "only_mouse vs iso_none"     "iso_none" "only_mouse"
Cmp "only_switch vs iso_none"    "iso_none" "only_switch"
Cmp "only_window vs iso_none"    "iso_none" "only_window"
Cmp "no_ad vs iso_all"           "iso_all"  "no_ad"
Cmp "no_mouse vs iso_all"        "iso_all"  "no_mouse"
Cmp "no_switch vs iso_all"       "iso_all"  "no_switch"
Cmp "no_window vs iso_all"       "iso_all"  "no_window"
Cmp "perturb_clock vs iso_all"   "iso_all"  "perturb_clock"
Cmp "perturb_time vs iso_all"    "iso_all"  "perturb_time"
Cmp "win_hidden vs iso_all"      "iso_all"  "win_hidden"
Cmp "win_normal vs iso_all"      "iso_all"  "win_normal"
Cmp "pace_real1 vs iso_all"      "iso_all"  "pace_real1"
Cmp "pace_real2 vs pace_real1"   "pace_real1" "pace_real2"
Cmp "pace_real3 vs pace_real1"   "pace_real1" "pace_real3"
Cmp "norestore2 vs norestore1"   "norestore1" "norestore2"
Cmp "norestore3 vs norestore1"   "norestore1" "norestore3"
Cmp "switch_posctrl vs iso_all"  "iso_all"  "switch_posctrl"
