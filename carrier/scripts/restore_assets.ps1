# restore_assets.ps1 - restore the pristine assets before a carrier run, the
# convention carrier/NOTES.md describes for every measured run:
#   assets/tower.cfg, assets/profiles/  <- artifacts/assets_backup/
#   assets/log.txt                      <- artifacts/log_original_baseline.txt
#
# MEASURED: carrier.exe relaunches itself as a child (NOTES.md fix #3), so a
# just-finished run can still be tearing down - and still rewriting
# assets/profiles/ - when the next run's restore starts. That race produced a
# single spurious G3 "cold vs post-rewind" difference in this project; wait
# for the process tree to be gone and verify the copy before returning.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

for ($i = 0; $i -lt 100; $i++) {
    if (-not (Get-Process carrier -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Milliseconds 100
}

for ($try = 1; $try -le 5; $try++) {
    try {
        if (Test-Path "$root\assets\profiles") {
            Remove-Item -Recurse -Force "$root\assets\profiles"
        }
        Copy-Item -Recurse -Force "$root\artifacts\assets_backup\profiles" "$root\assets\profiles"
        Copy-Item -Force "$root\artifacts\assets_backup\tower.cfg" "$root\assets\tower.cfg"
        Copy-Item -Force "$root\artifacts\log_original_baseline.txt" "$root\assets\log.txt"
        break
    } catch {
        if ($try -eq 5) { throw }
        Start-Sleep -Milliseconds 300
    }
}
