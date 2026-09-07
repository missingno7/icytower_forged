#!/usr/bin/env python3
"""extract_assets.py -- build the local, gitignored assets_extracted/ cache
(notes/asset_census.md SS7 mode (b), src/icytower/ASSETS.md "Extraction and
the asset oracle", win32_pilot.md SS7c).

LICENCE NOTE (read before running this on someone else's behalf): Icy Tower's
own readme.txt/character scripts prohibit redistributing its art and sound
("Sounds and images contained in the datafile may not be reproduced without
permission from Free Lunch Design"). assets_extracted/ is therefore a LOCAL
CACHE ONLY -- gitignored (see .gitignore), never committed, never uploaded,
generated fresh on each machine from that machine's own copy of the game
(assets/, also gitignored). This script does not relax that: it only turns
the datafiles you already have into a friendlier on-disk layout for the
standalone build to read.

This is the project-owned half of the ASSETS coastline's extraction step:
all container-format knowledge (the packfile header, LZSS, the datafile
object tree, the PNG/PAL/FONT export rules) lives in the generic
port_forge/tools/pf_allegro4_datafile.py; this script only knows Icy
Tower's OWN facts -- which 7 datafiles exist, their passwords, and the
CHAR_SLOT game-visible slot names -- and those facts are not even hardcoded
here: they live in carrier/win32_policy.json's "assets" section (moved
there from tools_recon/assets_manifest.py's module constants, per this
task's brief), read at run time.

Output layout under assets_extracted/ (the "clean standalone layout" of
notes/asset_census.md SS7 mode (b)):
    gfx/<object>.png          data.dat + loading.dat BMP objects
    fonts/<object>.alfont[.json]   data.dat FONT objects
    palettes/<object>.pal     data.dat + loading.dat PAL objects
    sfx/<object>.ogg          sfx15.dat OGG objects
    characters/<name>/<slot>.{png,pal,ogg}   the 4 character datafiles
    misc/<family>_<object>.bin   grabber "info" bookkeeping objects
    manifest.json             every src/icytower/assets_table.inc row's
                               asset id -> its file under this tree

Usage:
    python scripts/extract_assets.py [--policy carrier/win32_policy.json]
"""
import argparse
import json
import os
import re
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
TOOLS_DIR = os.path.join(ROOT, "port_forge", "tools")
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)
import pf_allegro4_datafile as datafile_tool  # noqa: E402

POLICY_PATH = os.path.join(ROOT, "carrier", "win32_policy.json")
ASSETS_TABLE_INC = os.path.join(ROOT, "src", "icytower", "assets_table.inc")

# One row per src/icytower/assets_table.inc entry: { ASSET_..., "family", N, "name", "type" },
_ROW_RE = re.compile(
    r'\{\s*(?P<id>ASSET_[A-Z0-9_]+)\s*,\s*"(?P<family>[^"]*)"\s*,\s*'
    r'(?P<index>\d+)\s*,\s*"(?P<name>[^"]*)"\s*,\s*"(?P<type>[^"]*)"\s*\}')


def read_asset_table_rows():
    """Parse the GENERATED src/icytower/assets_table.inc (read-only) instead
    of re-deriving the id-naming rule: this script must map every id in
    src/icytower/assets.h to a file, and that file already carries the
    id<->(family,index,object_name,type) row it was generated from."""
    text = Path(ASSETS_TABLE_INC).read_text(encoding="utf-8")
    rows = [m.groupdict() for m in _ROW_RE.finditer(text)]
    if not rows:
        raise SystemExit("extract_assets: found 0 rows in %s -- did its format change?"
                          % ASSETS_TABLE_INC)
    for r in rows:
        r["index"] = int(r["index"])
    return rows


def clean_dest(family, meta, char_slot_names, index, object_name, type_fourcc, outdir):
    """Where one object's exported file lives under assets_extracted/."""
    ext = {"BMP ": ".png", "OGG ": ".ogg", "PAL ": ".pal", "FONT": ".alfont"}.get(type_fourcc)
    if family.startswith("char:"):
        char_dir = meta["char_dir"]
        slot = char_slot_names.get(str(index), object_name)
        if type_fourcc == "info":
            return os.path.join("misc", "%s_grabberinfo.bin" % char_dir)
        return os.path.join("characters", char_dir, slot + (ext or ".bin"))
    if type_fourcc == "info":
        return os.path.join("misc", "%s_%s.bin" % (family, object_name.lower() or index))
    name = object_name.lower()
    strip = meta.get("strip_object_prefix")
    if strip and name.upper().startswith(strip):
        name = name[len(strip):]
    if type_fourcc == "OGG ":
        return os.path.join("sfx", name + ".ogg")
    if type_fourcc == "FONT":
        return os.path.join("fonts", name + ".alfont")
    if type_fourcc == "PAL ":
        # every non-character datafile's palette is grabber-named "AAAPAL"
        # (notes/asset_census.md SS3: sorts it first on purpose), so the
        # object name alone collides across families -- prefix with the
        # family to keep data's and loading's palettes as separate files.
        return os.path.join("palettes", "%s_%s.pal" % (family, name))
    return os.path.join(meta.get("clean_dir", "gfx"), name + (ext or ".bin"))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--policy", default=POLICY_PATH)
    args = ap.parse_args()

    policy = json.loads(Path(args.policy).read_text(encoding="utf-8"))
    assets_policy = policy["assets"]
    families = assets_policy["families"]
    char_slot_names = assets_policy["char_slot_names"]
    assets_root = os.path.join(ROOT, assets_policy["root"])
    outdir = os.path.join(ROOT, assets_policy["output"])
    os.makedirs(outdir, exist_ok=True)

    rows = read_asset_table_rows()

    # Parse each of the 7 datafiles exactly once.
    parsed = {}
    for family, meta in families.items():
        path = os.path.join(assets_root, meta["path"].replace("/", os.sep))
        body, info = datafile_tool.open_packfile(path, meta["password"])
        objs = {o["index"]: o for o in datafile_tool.flatten(datafile_tool.parse_datafile(body))}
        palette = datafile_tool.palette_by_type(objs.values())
        parsed[family] = {"objs": objs, "palette": palette, "info": info, "path": path}

    id_manifest = {}
    by_type = {}
    ok = mismatch = 0

    for row in rows:
        family, index = row["family"], row["index"]
        meta = families.get(family)
        if meta is None:
            raise SystemExit("extract_assets: assets_table.inc references unknown family %r "
                              "-- add it to carrier/win32_policy.json's assets.families" % family)
        bundle = parsed[family]
        obj = bundle["objs"].get(index)
        if obj is None or obj["data"] is None:
            print("extract_assets: WARNING %s (%s#%d) has no payload, skipping"
                  % (row["id"], family, index))
            continue

        relpath = clean_dest(family, meta, char_slot_names, index, row["name"], row["type"], outdir)
        dest = os.path.join(outdir, relpath)
        os.makedirs(os.path.dirname(dest), exist_ok=True)

        typ = row["type"]
        geom = None
        if typ == "BMP ":
            geom = datafile_tool.bmp_payload_to_png(obj["data"], dest,
                                                     palette_rgb=bundle["palette"].get("PAL "))
            reconstruct = lambda d=dest, bpp=geom["bpp"]: datafile_tool.png_to_bmp_payload(d, bpp)
        elif typ == "FONT":
            Path(dest).write_bytes(obj["data"])
            Path(dest + ".json").write_text(
                json.dumps(datafile_tool.font_descriptor(obj["data"]), indent=1), encoding="utf-8")
            reconstruct = lambda d=dest: Path(d).read_bytes()
        else:
            Path(dest).write_bytes(obj["data"])
            reconstruct = lambda d=dest: Path(d).read_bytes()

        payload_sha = datafile_tool.sha256_bytes(obj["data"])
        export_sha = datafile_tool.sha256_bytes(Path(dest).read_bytes())
        round_trip_sha = datafile_tool.sha256_bytes(reconstruct())
        verified = round_trip_sha == payload_sha
        ok += verified
        mismatch += not verified
        if not verified:
            print("extract_assets: MISMATCH %s (%s#%d) -- round trip does not "
                  "reproduce the original object bytes" % (row["id"], family, index))

        id_manifest[row["id"]] = {
            "family": family, "index": index, "object_name": row["name"],
            "type_fourcc": typ, "file": relpath.replace(os.sep, "/"),
            "sha256_payload": payload_sha, "sha256_export": export_sha,
            "round_trip_verified": verified, "geometry": geom,
        }
        by_type[typ] = by_type.get(typ, 0) + 1

    manifest = {
        "schema": "icytower-assets-extracted-manifest/1",
        "note": "LOCAL CACHE -- see this script's own header comment; never commit "
                 "or redistribute the files this manifest describes.",
        "source_assets_dir": assets_policy["root"],
        "asset_count": len(id_manifest),
        "by_type": by_type,
        "assets": id_manifest,
    }
    manifest_path = os.path.join(outdir, "manifest.json")
    Path(manifest_path).write_text(json.dumps(manifest, indent=1), encoding="utf-8")

    print("extract_assets: %d ids extracted, %d verified round-trip, %d MISMATCH"
          % (len(id_manifest), ok, mismatch))
    print("  by type: %s" % by_type)
    print("  manifest: %s" % manifest_path)
    return 1 if mismatch else 0


if __name__ == "__main__":
    sys.exit(main())
