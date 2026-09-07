#!/usr/bin/env python3
"""Fetch third-party sources needed for the LIBRARIES coastline (win32_pilot.md
Sections 7b/7c; notes/library_compat_verdict.md Section 8 "Recommended
simplest path"; notes/library_boundary.md Section 2 "Build configuration
that must be reproduced").

Currently fetches:
  - Allegro 4.4.3.1 (upstream liballeg/allegro5, tag 4.4.3.1), falling back
    to the AGS fork (adventuregamestudio/lib-allegro, tag
    v4.4.3.1-agspatch-3) only if the upstream clone fails.

Writes/updates third_party/MANIFEST.json with name/url/tag/commit/licence
for every fetched dependency. Idempotent: re-running with an already-correct
checkout (same tag, same commit) is a no-op; a checkout at the wrong commit
is re-fetched.

This script only ever touches paths under third_party/ (plus reading this
repo's own git history). It does not touch carrier/, src/, assets/, notes/.
"""
import json
import subprocess
import sys
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


def main():
    THIRD_PARTY.mkdir(parents=True, exist_ok=True)
    manifest = load_manifest()

    allegro_entry = fetch_allegro()
    upsert_dependency(manifest, allegro_entry)

    save_manifest(manifest)
    print(f"wrote {MANIFEST_PATH}", file=sys.stderr)


if __name__ == "__main__":
    main()
