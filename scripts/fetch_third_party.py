#!/usr/bin/env python3
"""Fetch third-party sources needed for the LIBRARIES coastline (win32_pilot.md
Sections 7b/7c; notes/library_compat_verdict.md Section 8 "Recommended
simplest path"; notes/library_boundary.md Section 2 "Build configuration
that must be reproduced") and for the compiler-fingerprint experiment
(notes/external_research.md Section 1 / Queued experiment A).

Currently fetches:
  - Allegro 4.4.3.1 (upstream liballeg/allegro5, tag 4.4.3.1), falling back
    to the AGS fork (adventuregamestudio/lib-allegro, tag
    v4.4.3.1-agspatch-3) only if the upstream clone fails.
  - With --toolchain: the archived TDM-GCC 4.4.1-tdm-1 and -tdm-2 (SJLJ)
    releases from SourceForge, assembled into complete mingw32 toolchain
    prefixes (gcc/g++ component tarballs + binutils/mingwrt/w32api
    extracted from the matching "tdm-mingw-1.908.0-4.4.1[-2].exe" NSIS
    bundle installer -- extracted with 7-Zip, never executed), plus a
    shallow worktree/clone of Allegro 4.4.1 (git tag) alongside the
    4.4.3.1 checkout.

Writes/updates third_party/MANIFEST.json with name/url/tag/commit/licence
for every fetched dependency. Idempotent: re-running with an already-correct
checkout (same tag, same commit) is a no-op; a checkout at the wrong commit
is re-fetched. The --toolchain step is idempotent by sha256: a file already
present with the recorded hash is not re-downloaded.

This script only ever touches paths under third_party/ (plus reading this
repo's own git history). It does not touch carrier/, src/, assets/, notes/.
"""
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
THIRD_PARTY = REPO_ROOT / "third_party"
MANIFEST_PATH = THIRD_PARTY / "MANIFEST.json"

ALLEGRO_UPSTREAM_URL = "https://github.com/liballeg/allegro5"
ALLEGRO_UPSTREAM_TAG = "4.4.3.1"
ALLEGRO_FORK_URL = "https://github.com/adventuregamestudio/lib-allegro"
ALLEGRO_FORK_TAG = "v4.4.3.1-agspatch-3"
ALLEGRO_DEST_DIRNAME = "allegro-4.4.3.1"
ALLEGRO_LICENCE = "Giftware License (zlib/libpng-like; see the checkout's LICENSE.txt / allegro/base.h)"


def run(cmd, cwd=None, check=True):
    print("+ " + " ".join(str(c) for c in cmd), file=sys.stderr)
    return subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True)


def git_ls_remote_tag(url, tag):
    """Return the commit hash a remote tag points at (dereferenced), or None.

    Deliberately does NOT pass `tag` as a refname pattern to `git ls-remote`:
    some git/protocol-v2 combinations omit the peeled (`^{}`) line for a
    single filtered ref, which would make an annotated tag resolve to the
    tag object instead of the commit it points at. Listing all tags and
    filtering in Python is the reliable path (verified against
    liballeg/allegro5, where filtered ls-remote non-deterministically
    dropped the peeled line across repeated calls).
    """
    try:
        proc = run(["git", "ls-remote", "--tags", url])
    except subprocess.CalledProcessError:
        return None
    commit = None
    for line in proc.stdout.splitlines():
        sha, ref = line.split("\t")
        if ref == f"refs/tags/{tag}":
            commit = sha
        elif ref == f"refs/tags/{tag}^{{}}":
            # annotated tag: the dereferenced object is the actual commit
            commit = sha
    return commit


def current_checkout_commit(dest):
    if not (dest / ".git").exists():
        return None
    try:
        proc = run(["git", "rev-parse", "HEAD"], cwd=dest)
    except subprocess.CalledProcessError:
        return None
    return proc.stdout.strip()


def _rmtree_force(path):
    """shutil.rmtree that tolerates Windows read-only bits git sets on
    objects under .git/objects/pack/."""
    import shutil
    import stat

    def onerror(func, p, exc_info):
        try:
            import os

            os.chmod(p, stat.S_IWRITE)
            func(p)
        except Exception:
            raise

    shutil.rmtree(path, onerror=onerror)


def shallow_clone_tag(url, tag, dest):
    if dest.exists():
        _rmtree_force(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--branch", tag, "--depth", "1", url, str(dest)])
    submodules = list_submodules(dest)
    if submodules:
        run(["git", "submodule", "update", "--init", "--depth", "1"], cwd=dest)
    return current_checkout_commit(dest)


def list_submodules(dest):
    gitmodules = dest / ".gitmodules"
    return gitmodules.exists()


def load_manifest():
    if MANIFEST_PATH.exists():
        return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    return {"dependencies": []}


def save_manifest(manifest):
    MANIFEST_PATH.write_text(
        json.dumps(manifest, indent=2, sort_keys=False) + "\n", encoding="utf-8"
    )


def upsert_dependency(manifest, entry):
    deps = manifest.setdefault("dependencies", [])
    for i, d in enumerate(deps):
        if d.get("name") == entry["name"]:
            deps[i] = entry
            return
    deps.append(entry)


def fetch_allegro():
    dest = THIRD_PARTY / ALLEGRO_DEST_DIRNAME
    wanted_commit = git_ls_remote_tag(ALLEGRO_UPSTREAM_URL, ALLEGRO_UPSTREAM_TAG)
    used_url, used_tag = ALLEGRO_UPSTREAM_URL, ALLEGRO_UPSTREAM_TAG

    if wanted_commit is None:
        print(
            f"WARNING: upstream tag {ALLEGRO_UPSTREAM_TAG} not found at "
            f"{ALLEGRO_UPSTREAM_URL}; falling back to AGS fork "
            f"{ALLEGRO_FORK_URL} tag {ALLEGRO_FORK_TAG}",
            file=sys.stderr,
        )
        used_url, used_tag = ALLEGRO_FORK_URL, ALLEGRO_FORK_TAG
        wanted_commit = git_ls_remote_tag(used_url, used_tag)
        if wanted_commit is None:
            raise SystemExit(
                f"ERROR: could not resolve tag {used_tag} at {used_url} either; aborting"
            )

    existing_commit = current_checkout_commit(dest)
    if existing_commit == wanted_commit:
        print(
            f"third_party/{ALLEGRO_DEST_DIRNAME} already at {wanted_commit} "
            f"({used_url} tag {used_tag}) - skipping clone (idempotent)",
            file=sys.stderr,
        )
        commit = existing_commit
    else:
        if existing_commit is not None:
            print(
                f"third_party/{ALLEGRO_DEST_DIRNAME} at {existing_commit}, "
                f"want {wanted_commit} - re-cloning",
                file=sys.stderr,
            )
        try:
            commit = shallow_clone_tag(used_url, used_tag, dest)
        except subprocess.CalledProcessError as e:
            if used_url == ALLEGRO_UPSTREAM_URL:
                print(
                    f"WARNING: clone of upstream {ALLEGRO_UPSTREAM_URL}@{ALLEGRO_UPSTREAM_TAG} "
                    f"failed ({e}); falling back to AGS fork {ALLEGRO_FORK_URL}@{ALLEGRO_FORK_TAG}",
                    file=sys.stderr,
                )
                used_url, used_tag = ALLEGRO_FORK_URL, ALLEGRO_FORK_TAG
                commit = shallow_clone_tag(used_url, used_tag, dest)
            else:
                raise

    print(f"USED SOURCE: {used_url} tag {used_tag} commit {commit}", file=sys.stderr)

    return {
        "name": "allegro",
        "url": used_url,
        "tag": used_tag,
        "commit": commit,
        "licence": ALLEGRO_LICENCE,
        "dest": f"third_party/{ALLEGRO_DEST_DIRNAME}",
        "note": (
            "upstream liballeg/allegro5 used"
            if used_url == ALLEGRO_UPSTREAM_URL
            else "FALLBACK: upstream tag unavailable, used AGS fork "
            "adventuregamestudio/lib-allegro instead"
        ),
    }


# ---------------------------------------------------------------------------
# --toolchain: TDM-GCC 4.4.1-tdm-1 / -tdm-2 SJLJ + Allegro 4.4.1
# ---------------------------------------------------------------------------

SF_FILES = "https://sourceforge.net/projects/tdm-gcc/files"

# Component archives (gcc core + g++) per variant, from the SJLJ release
# directories. Prefer tarballs over the bundle installer's own zip copies,
# per the operator's instruction.
TOOLCHAIN_VARIANTS = {
    "tdm-1": {
        "sjlj_dir": f"{SF_FILES}/TDM-GCC%20Old%20Releases/TDM-GCC%204.4%20series/"
        "Previous%20Releases/4.4.1-tdm-1%20SJLJ",
        "core": "gcc-4.4.1-tdm-1-core.tar.gz",
        "gxx": "gcc-4.4.1-tdm-1-g++.tar.gz",
        "installer_url": f"{SF_FILES}/TDM-GCC%20Installer/Previous/1.908.0/"
        "Superceded/tdm-mingw-1.908.0-4.4.1.exe",
        "installer_name": "tdm-mingw-1.908.0-4.4.1.exe",
        "core_sha256": "f9de30fdc4cf00887e27b38461f3d4ff0f1f1cb3a720acb2f6835381e8933de3",
        "gxx_sha256": "acd5fe4168cb08fdbd3c5bd0db206f34e232fe4845bdb0e2445773b9343a8a5b",
        "installer_sha256": "b5ee3197de9e1f71bcfc5e99d2360dd73296be250424863a6f8bf8f1fa06a219",
    },
    "tdm-2": {
        "sjlj_dir": f"{SF_FILES}/TDM-GCC%20Old%20Releases/TDM-GCC%204.4%20series/"
        "4.4.1-tdm-2%20SJLJ",
        "core": "gcc-4.4.1-tdm-2-core.tar.gz",
        "gxx": "gcc-4.4.1-tdm-2-g++.tar.gz",
        "installer_url": f"{SF_FILES}/TDM-GCC%20Installer/Previous/1.908.0/"
        "tdm-mingw-1.908.0-4.4.1-2.exe",
        "installer_name": "tdm-mingw-1.908.0-4.4.1-2.exe",
        "core_sha256": "aad57f3fb2a5e78c6ba1809c04cbd599857ec36d0d0e03ae46113442d38a50ca",
        "gxx_sha256": "565e3b22f5493e80200bbf117f3299e3c04e70160d7d9c25e2ef9dc02062c93d",
        "installer_sha256": "8748bfc3a5600ea5739fb00c1d282bb5b97793d026af3bb34e37d943bc209173",
    },
}

# binutils/mingwrt/w32api sub-archives are identical between the tdm-1 and
# tdm-2 bundle installers (only the gcc core/g++ payload differs); recorded
# once, extracted from whichever installer was fetched.
TOOLCHAIN_RUNTIME_MEMBERS = [
    "binutils-2.19.1-mingw32-bin.tar.gz",
    "mingwrt-3.16-mingw32-dev.tar.gz",
    "mingwrt-3.16-mingw32-dll.tar.gz",
    "w32api-3.13-mingw32-dev.tar.gz",
]
TOOLCHAIN_RUNTIME_SHA256 = {
    "binutils-2.19.1-mingw32-bin.tar.gz": "42427d4e7adaacacddb4e58ec3a03f3724aa46db884c7a37635bec98ac80fb0c",
    "mingwrt-3.16-mingw32-dev.tar.gz": "917dbecaa3f2e6de95e63763b615f142c73da975605269747e0849bde9b917f7",
    "mingwrt-3.16-mingw32-dll.tar.gz": "627be0854993e71cfda35f9b9c579688fff2d0473e17d64c594f8103e15f59d1",
    "w32api-3.13-mingw32-dev.tar.gz": "7dbf01a06a0e21bd405fc397789bdbfdd21e69acb59383568a3d96ea1b1455a6",
}

ALLEGRO_441_TAG = "4.4.1"
ALLEGRO_441_DEST_DIRNAME = "allegro-4.4.1"


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def curl_download(url, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"+ curl -L -o {dest} '{url}/download'", file=sys.stderr)
    subprocess.run(
        ["curl", "-L", "--fail", "--max-time", "300", "-o", str(dest), f"{url}/download"],
        check=True,
    )


def ensure_file(url, dest, expected_sha256):
    """Download dest from url unless it already exists with the expected hash."""
    if dest.exists() and expected_sha256 and sha256_of(dest) == expected_sha256:
        print(f"{dest} already present with matching sha256 - skipping", file=sys.stderr)
        return
    curl_download(url, dest)
    if expected_sha256:
        actual = sha256_of(dest)
        if actual != expected_sha256:
            raise SystemExit(
                f"ERROR: {dest} sha256 mismatch: expected {expected_sha256}, got {actual}"
            )


def find_7z():
    for candidate in ("7z", "7z.exe"):
        path = shutil.which(candidate)
        if path:
            return path
    for fallback in ("C:/msys64/usr/lib/p7zip/7z.exe", "C:/msys64/usr/bin/7z"):
        if Path(fallback).exists():
            return fallback
    return None


def extract_nsis_installer(installer_path, out_dir):
    """Extract an NSIS installer's payload with 7-Zip WITHOUT running it."""
    out_dir.mkdir(parents=True, exist_ok=True)
    sevenzip = find_7z()
    if sevenzip is None:
        raise SystemExit(
            "ERROR: 7z not found (install p7zip via MSYS2: "
            "`pacman -S --noconfirm --needed p7zip`) - required to extract "
            f"{installer_path.name} without running it"
        )
    subprocess.run(
        [sevenzip, "x", "-y", f"-o{out_dir}", str(installer_path)],
        check=True,
        capture_output=True,
    )


def extract_tar_gz(archive, dest_dir):
    dest_dir.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "r:gz") as tf:
        tf.extractall(dest_dir)


def fetch_toolchain_variant(variant_key, spec):
    dest_root = THIRD_PARTY / f"tdm-gcc-4.4.1-{variant_key}"
    dl = dest_root / "dl"
    prefix = dest_root / "mingw32"
    installer_extract = dest_root / "installer_extract"

    core_path = dl / spec["core"]
    gxx_path = dl / spec["gxx"]
    installer_path = dl / spec["installer_name"]

    ensure_file(f"{spec['sjlj_dir']}/{spec['core']}", core_path, spec["core_sha256"])
    ensure_file(f"{spec['sjlj_dir']}/{spec['gxx']}", gxx_path, spec["gxx_sha256"])
    ensure_file(spec["installer_url"], installer_path, spec["installer_sha256"])

    gcc_exe = prefix / "bin" / "gcc.exe"
    as_exe = prefix / "bin" / "as.exe"
    if not (gcc_exe.exists() and as_exe.exists()):
        prefix.mkdir(parents=True, exist_ok=True)
        extract_tar_gz(core_path, prefix)
        extract_tar_gz(gxx_path, prefix)

        extract_nsis_installer(installer_path, installer_extract)
        plugins_dir = installer_extract / "$PLUGINSDIR"
        for member in TOOLCHAIN_RUNTIME_MEMBERS:
            member_path = plugins_dir / member
            if not member_path.exists():
                raise SystemExit(f"ERROR: expected {member} inside {installer_path.name}")
            expected = TOOLCHAIN_RUNTIME_SHA256.get(member)
            if expected:
                actual = sha256_of(member_path)
                if actual != expected:
                    raise SystemExit(
                        f"ERROR: {member} (from {installer_path.name}) sha256 mismatch: "
                        f"expected {expected}, got {actual}"
                    )
            extract_tar_gz(member_path, prefix)

    proc = subprocess.run(
        [str(gcc_exe), "--version"], capture_output=True, text=True, check=True
    )
    version_line = proc.stdout.splitlines()[0].strip()
    expected_marker = "TDM-1" if variant_key == "tdm-1" else "TDM-2"
    if "4.4.1" not in version_line or expected_marker not in version_line:
        raise SystemExit(
            f"ERROR: {gcc_exe} --version = {version_line!r}, "
            f"expected 4.4.1 and {expected_marker}"
        )
    print(f"VERIFIED {variant_key}: {version_line}", file=sys.stderr)

    files = [
        {"name": spec["core"], "sha256": spec["core_sha256"], "size": core_path.stat().st_size},
        {"name": spec["gxx"], "sha256": spec["gxx_sha256"], "size": gxx_path.stat().st_size},
        {
            "name": spec["installer_name"],
            "sha256": spec["installer_sha256"],
            "size": installer_path.stat().st_size,
            "note": "NSIS bundle installer; extracted with 7-Zip for "
            "binutils/mingwrt/w32api only, never executed",
        },
    ]
    for member in TOOLCHAIN_RUNTIME_MEMBERS:
        member_path = plugins_dir = installer_extract / "$PLUGINSDIR" / member
        files.append(
            {
                "name": member,
                "sha256": TOOLCHAIN_RUNTIME_SHA256.get(member),
                "size": member_path.stat().st_size if member_path.exists() else None,
                "note": f"extracted from {spec['installer_name']}",
            }
        )

    return {
        "name": f"tdm-gcc-4.4.1-{variant_key}",
        "url": spec["sjlj_dir"],
        "installer_url": spec["installer_url"],
        "tag": f"4.4.1-{variant_key} SJLJ",
        "licence": "GPL/LGPL (GCC/binutils/mingwrt/w32api); see COPYING* in the checkout",
        "dest": f"third_party/tdm-gcc-4.4.1-{variant_key}",
        "gcc_version": version_line,
        "files": files,
    }


def fetch_allegro_441():
    dest = THIRD_PARTY / ALLEGRO_441_DEST_DIRNAME
    src = THIRD_PARTY / ALLEGRO_DEST_DIRNAME
    if not (src / ".git").exists():
        raise SystemExit(
            f"ERROR: {src} must exist first (run without --toolchain, or run "
            "the default Allegro 4.4.3.1 fetch) before adding the 4.4.1 worktree"
        )
    run(["git", "fetch", "--depth", "1", "origin", "tag", ALLEGRO_441_TAG], cwd=src)
    if not dest.exists():
        run(["git", "worktree", "add", str(dest), ALLEGRO_441_TAG], cwd=src)
    else:
        existing = current_checkout_commit(dest)
        print(
            f"third_party/{ALLEGRO_441_DEST_DIRNAME} already exists at {existing} - skipping",
            file=sys.stderr,
        )
    commit = current_checkout_commit(dest)
    print(f"VERIFIED allegro-4.4.1: commit {commit}", file=sys.stderr)
    return {
        "name": "allegro-4.4.1",
        "url": ALLEGRO_UPSTREAM_URL,
        "tag": ALLEGRO_441_TAG,
        "commit": commit,
        "licence": ALLEGRO_LICENCE,
        "dest": f"third_party/{ALLEGRO_441_DEST_DIRNAME}",
        "note": f"git worktree of third_party/{ALLEGRO_DEST_DIRNAME} at tag {ALLEGRO_441_TAG}",
    }


def fetch_toolchain():
    manifest = load_manifest()
    for variant_key, spec in TOOLCHAIN_VARIANTS.items():
        entry = fetch_toolchain_variant(variant_key, spec)
        upsert_dependency(manifest, entry)
    allegro_entry = fetch_allegro_441()
    upsert_dependency(manifest, allegro_entry)
    save_manifest(manifest)
    print(f"wrote {MANIFEST_PATH}", file=sys.stderr)


def main():
    THIRD_PARTY.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest()

    allegro_entry = fetch_allegro()
    upsert_dependency(manifest, allegro_entry)

    save_manifest(manifest)
    print(f"wrote {MANIFEST_PATH}", file=sys.stderr)

    if "--toolchain" in sys.argv[1:]:
        fetch_toolchain()


if __name__ == "__main__":
    main()
