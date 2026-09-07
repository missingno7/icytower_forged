#!/usr/bin/env python3
"""check_assets.py -- consistency check for the generated ASSET seam
(win32_pilot.md SS7c task brief, item 2).

Three independent checks, all read-only w.r.t. assets/ and src/:

  1. GENERATOR MATCHES ITS OWN OUTPUT: recompute the asset list straight
     from artifacts/asset_manifest.json + tools_recon/assets_manifest.py's
     CHAR_SLOT (the same inputs carrier/gen/gen_assets.py uses, imported
     from it rather than re-derived) and diff it, row by row, against what
     is actually sitting in src/icytower/assets_table.inc. Catches the
     generator and its output drifting apart (hand-edits, a stale run).

  2. CENSUS COVERAGE: every constant-index `data[N].dat` / `sfx[N].dat`
     site the census's ref-scanner found (artifacts/assets_extract/
     datafile_refs.json -- 42 distinct `data` indices, 22 distinct `sfx`
     indices, notes/asset_census.md SS3) maps to EXACTLY ONE row in the
     table (same family, same index). A site with zero matches means the
     manifest and the disassembly disagree about what object lives at
     that index; more than one match means the generator produced a
     duplicate id for one object.

  3. REAL-FILE ORDER: for every datafile family, parse the ACTUAL file
     under assets/ with tools_recon/assets_datafile.py (the same reader
     notes/asset_census.md SS4/SS7 calls "already written and validated")
     and confirm the table's index N really is object N in that file, by
     name -- not just by trusting the manifest that was built from the
     same reader earlier. Catches a stale manifest (assets/ changed since
     artifacts/asset_manifest.json was generated).

Usage: python carrier/gen/check_assets.py
Exit code: 0 if all three checks pass, 1 otherwise (mismatches printed).
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools_recon"))
import gen_assets as GA  # noqa: E402
import assets_manifest as AM  # noqa: E402
from assets_datafile import open_packfile, parse_datafile, flatten, PASSWORDS  # noqa: E402

MANIFEST = os.path.join(ROOT, "artifacts", "asset_manifest.json")
REFS = os.path.join(ROOT, "artifacts", "assets_extract", "datafile_refs.json")
TABLE_INC = os.path.join(ROOT, "src", "icytower", "assets_table.inc")
ASSETS_DIR = os.path.join(ROOT, "assets")

ROW_RE = re.compile(
    r'\{\s*(?P<id>ASSET_[A-Za-z0-9_]+)\s*,\s*'
    r'"(?P<datafile>[^"]*)"\s*,\s*'
    r'(?P<index>-?\d+)\s*,\s*'
    r'(?P<object_name>"(?:[^"\\]|\\.)*"|\(const char \*\)0)\s*,\s*'
    r'"(?P<type_fourcc>[^"]*)"\s*\}')


def c_unescape(lit):
    if lit == "(const char *)0":
        return None
    assert lit[0] == '"' and lit[-1] == '"', lit
    return lit[1:-1].replace('\\"', '"').replace('\\\\', '\\')


def parse_table_inc(path):
    text = open(path, encoding="utf-8").read()
    rows = []
    for m in ROW_RE.finditer(text):
        rows.append({
            "id": m.group("id"),
            "datafile": m.group("datafile"),
            "index": int(m.group("index")),
            "object_name": c_unescape(m.group("object_name")),
            "type_fourcc": m.group("type_fourcc"),
        })
    return rows


def check_generator_matches_output():
    print("== 1. generator output vs src/icytower/assets_table.inc ==")
    manifest = json.load(open(MANIFEST))
    expected = GA.build_assets(manifest)
    actual = parse_table_inc(TABLE_INC)

    problems = []
    if len(expected) != len(actual):
        problems.append("row count: expected %d, table has %d" %
                         (len(expected), len(actual)))

    for i, (e, a) in enumerate(zip(expected, actual)):
        if e.id_name != a["id"]:
            problems.append("row %d: id expected %s, table has %s" %
                             (i, e.id_name, a["id"]))
            continue
        if (e.family, e.index, e.object_name, e.type_fourcc) != \
           (a["datafile"], a["index"], a["object_name"], a["type_fourcc"]):
            problems.append(
                "row %d (%s): expected (%r,%r,%r,%r), table has (%r,%r,%r,%r)" % (
                    i, e.id_name, e.family, e.index, e.object_name, e.type_fourcc,
                    a["datafile"], a["index"], a["object_name"], a["type_fourcc"]))

    if problems:
        for p in problems[:50]:
            print("  MISMATCH:", p)
        if len(problems) > 50:
            print("  ... and %d more" % (len(problems) - 50))
    else:
        print("  OK: %d rows, generator output == src/icytower/assets_table.inc" % len(actual))
    return actual, problems


def check_census_coverage(actual):
    print("== 2. census constant-index sites -> exactly one table row each ==")
    refs = json.load(open(REFS))
    by_family_index = {}
    for row in actual:
        by_family_index.setdefault((row["datafile"], row["index"]), []).append(row["id"])

    fam_of_global = {"data": "data", "sfx": "sfx"}
    sites = {}
    for r in refs:
        fam = fam_of_global.get(r["global"])
        if fam is None:
            continue
        sites[(fam, r["index"])] = r["object"]

    data_sites = sorted(i for f, i in sites if f == "data")
    sfx_sites = sorted(i for f, i in sites if f == "sfx")
    print("  distinct data[N] indices: %d, distinct sfx[N] indices: %d "
          "(census: 42 + 22)" % (len(data_sites), len(sfx_sites)))

    problems = []
    for (fam, idx), census_obj in sorted(sites.items()):
        matches = by_family_index.get((fam, idx), [])
        if len(matches) != 1:
            problems.append("%s[%d] (census object %r): %d table rows match (expected 1): %s" %
                             (fam, idx, census_obj, len(matches), matches))

    if problems:
        for p in problems:
            print("  MISMATCH:", p)
    else:
        print("  OK: all %d sites map to exactly one asset id" % len(sites))
    return problems


def real_file_object_names(stem, rel):
    path = os.path.join(ASSETS_DIR, rel.replace("/", os.sep))
    pw = PASSWORDS.get(os.path.basename(path).lower())
    body, info = open_packfile(path, pw)
    objs = flatten(parse_datafile(body))
    names = {}
    for o in objs:
        if stem.startswith("char."):
            slot = AM.CHAR_SLOT.get(o["index"]) or o["name"]
            names[o["index"]] = (slot, o["name"], o["type"])
        else:
            names[o["index"]] = (o["name"], o["name"], o["type"])
    return names


def check_real_file_order(actual):
    print("== 3. table index N vs real-file object order (assets_datafile.py, read-only) ==")
    by_family = {}
    for row in actual:
        by_family.setdefault(row["datafile"], {})[row["index"]] = row

    problems = []
    checked_files = 0
    checked_objects = 0
    for stem, rel, _pwkey in AM.DATAFILES:
        family = GA.family_of(stem)
        table_rows = by_family.get(family, {})
        real = real_file_object_names(stem, rel)
        checked_files += 1

        if len(real) != len(table_rows):
            problems.append("%s (%s): real file has %d objects, table has %d rows" %
                             (family, rel, len(real), len(table_rows)))

        for idx, (expected_name, raw_name, type_fourcc) in sorted(real.items()):
            row = table_rows.get(idx)
            checked_objects += 1
            if row is None:
                problems.append("%s[%d]: real file has object %r, table has no row" %
                                 (family, idx, raw_name))
                continue
            if row["object_name"] != expected_name:
                problems.append("%s[%d]: real file object is %r (table expected %r), table row object_name is %r" %
                                 (family, idx, raw_name, expected_name, row["object_name"]))
            if row["type_fourcc"] != type_fourcc:
                problems.append("%s[%d] (%s): real file type is %r, table type_fourcc is %r" %
                                 (family, idx, expected_name, type_fourcc, row["type_fourcc"]))

    if problems:
        for p in problems[:50]:
            print("  MISMATCH:", p)
        if len(problems) > 50:
            print("  ... and %d more" % (len(problems) - 50))
    else:
        print("  OK: %d objects across %d real datafiles, all index/name/type match" %
              (checked_objects, checked_files))
    return problems


def main():
    actual, p1 = check_generator_matches_output()
    p2 = check_census_coverage(actual)
    p3 = check_real_file_order(actual)

    total = len(p1) + len(p2) + len(p3)
    print("---")
    print("check_assets: %d mismatch(es) (%d generator/output, %d census-coverage, "
          "%d real-file-order)" % (total, len(p1), len(p2), len(p3)))
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
