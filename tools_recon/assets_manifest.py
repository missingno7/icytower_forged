"""assets_manifest.py - build artifacts/asset_manifest.json.

One record per asset, giving the symbolic id the clean port will use, the
ORIGINAL location (exe VA/size, datafile+index+name, or external path), the
format, a sha256 of the original bytes, and the extraction rule that produces
the EXTRACTED FILE form.

Read-only with respect to assets/.
"""
import os, sys, json, hashlib, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools_recon"))
import assets_exe_read as X
from assets_datafile import open_packfile, parse_datafile, flatten, PASSWORDS

ASSETS = os.path.join(ROOT, "assets")
EXE = os.path.join(ASSETS, "icytower15.exe")

DATAFILES = [
    ("data", "data/data.dat", "data.dat"),
    ("loading", "data/loading.dat", "loading.dat"),
    ("sfx15", "data/sfx15.dat", "sfx15.dat"),
    ("char.harold_the_homeboy", "characters/harold_the_homeboy/harold.dat", None),
    ("char.disco_dave", "characters/disco_dave/dave.dat", None),
    ("char.jungle_jane", "characters/jungle_jane/jane.dat", None),
    ("char.wild_wendy", "characters/wild_wendy/wendy.dat", None),
]

# extraction rule per datafile object type
RULE = {
    "PNG ": ("png", "verbatim", ".png"),
    "OGG ": ("ogg_vorbis", "verbatim", ".ogg"),
    "DATA": ("opaque", "verbatim", ".bin"),
    "BMP ": ("allegro_bitmap", "bmp565_to_png24 (bit-exact round trip) "
             "or keep .albmp payload", ".png"),
    "RLE ": ("allegro_rle_sprite", "keep payload (.alrle) + manifest", ".alrle"),
    "PAL ": ("allegro_palette", "256x(r,g,b,filler) 6-bit -> .pal text or "
             "verbatim .pal", ".pal"),
    "FONT": ("allegro_font", "keep payload (.alfont) + manifest; a normal "
             "format needs a glyph sheet + metrics", ".alfont"),
    "SAMP": ("allegro_sample", "header -> RIFF/WAVE, PCM verbatim", ".wav"),
    "MIDI": ("allegro_midi", "tracks -> .mid", ".mid"),
    "info": ("grabber_metadata", "verbatim (grabber bookkeeping, not used at "
             "runtime)", ".bin"),
    "FILE": ("nested_datafile", "recurse", ""),
}

# game-visible slot names, from the disassembly (KNOWN)
CHAR_SLOT = {0: "palette"}
for i in range(1, 16):
    CHAR_SLOT[i] = "frame%02d" % i
CHAR_SLOT.update({16: "snd_jump_lo", 17: "snd_jump_med", 18: "snd_jump_hi",
                  19: "snd_yo", 20: "snd_wazup", 21: "snd_falling",
                  22: "snd_edge", 23: "music_bg"})

EXTERNAL = [
    ("gamepad.txt", "text/ini", "gamepad button -> action map, read at init"),
    ("characters/characters.txt", "text", "custom-character documentation, not read by the game"),
    ("readme.txt", "text", "licence / distribution terms"),
    ("ogg_license.txt", "text", "Xiph BSD licence"),
    ("itrcheck.txt", "text", "documentation of the -itrcheck replay verifier"),
    ("icytower.url", "text", "shortcut"),
    ("characters/_template/_template.png", "png", "template character sprite sheet"),
    ("characters/_template/_template.txt", "text/tagged", "template character script"),
    ("characters/harold_the_homeboy/harold_the_homeboy.txt", "text/tagged", "character script"),
    ("characters/disco_dave/disco_dave.txt", "text/tagged", "character script"),
    ("characters/jungle_jane/jungle_jane.txt", "text/tagged", "character script"),
    ("characters/wild_wendy/wild_wendy.txt", "text/tagged", "character script"),
]

RUNTIME_WRITTEN = [
    ("tower.cfg", "allegro packfile (LZSS, no password)",
     "options + hiscore tables + ad record; guarded by generate_options_checksum"),
    ("log.txt", "text", "append-only log written from log2file"),
    ("profiles/<name>/<name>.itp", "binary struct", "profile, guarded by generate_profile_checksum"),
    ("profiles/<name>/<name>_stats.txt", "text", "human-readable profile dump"),
    ("profiles/<name>/replays/*.itr", "binary struct", "replay, guarded by calc_replay_checksum"),
    ("screenshots/icytower_%04d.png", "png", "written by take_screenshot"),
    ("data/com/ads.csv", "csv", "ad listing cache (network)"),
    ("data/com/default.dat", "png/bitmap", "default ad image; its file_size_ex is baked into Toptions"),
    ("data/com/temp.dat", "binary", "ad download scratch file"),
]


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    man = {
        "schema": "portforge-asset-manifest/1",
        "target": "Icy Tower 1.5.1 (icytower15.exe)",
        "generated": datetime.date.today().isoformat(),
        "binding_model": {
            "EMBEDDED_ORIGINAL": "carrier build - load_asset(ID) returns the "
                                 "original bytes at the original location "
                                 "(exe VA, or the datafile the game already "
                                 "loaded); zero copying",
            "EXTRACTED_FILE": "standalone build - load_asset(ID) opens "
                              "assets/<clean_path>",
        },
        "assets": [],
    }

    # 1. PE resources
    import pefile
    pe = X.pe()
    pe.parse_data_directories()
    for t in getattr(pe, "DIRECTORY_ENTRY_RESOURCE").entries:
        for nm in t.directory.entries:
            for lang in nm.directory.entries:
                d = lang.data.struct
                data = pe.get_data(d.OffsetToData, d.Size)
                rn = nm.name.string.decode() if nm.name else str(nm.id)
                man["assets"].append({
                    "id": "exe/rsrc/%s" % rn, "kind": "icon",
                    "origin": {"mode": "EXE_RSRC",
                               "va": "0x%08x" % (pe.OPTIONAL_HEADER.ImageBase + d.OffsetToData),
                               "size": d.Size},
                    "format": "win32 resource (RT_ICON / RT_GROUP_ICON)",
                    "sha256": sha(data),
                    "extract": "verbatim -> assets/icon/%s.ico" % rn,
                    "referenced_by": "Allegro win_set_window / LoadIcon by name "
                                     "\"ALLEGRO_ICON\"",
                })

    # 2. compiled-in tables
    rows = json.load(open(os.path.join(ROOT, "artifacts/assets_extract/dwarf_tables.json")))
    cus = json.load(open(os.path.join(ROOT, "artifacts/dwarf_cus.json")))
    game_cu = set()
    for c in cus:
        n = (c.get("name") or "").replace("\\", "/")
        if "projects/icytower" in n.lower():
            game_cu.add(os.path.basename(n))
    for r in rows:
        if r["cu"] not in game_cu or r["section"] not in (".data", ".rdata"):
            continue
        if not r["size"]:
            continue
        b = X.read(r["va_int"], r["size"])
        man["assets"].append({
            "id": "exe/table/%s" % r["name"], "kind": "compiled_table",
            "origin": {"mode": "EXE_DATA", "va": r["va"], "size": r["size"],
                       "section": r["section"], "cu": r["cu"]},
            "format": r["type"], "sha256": sha(b),
            "extract": "generator emits a C initialiser into src/; no file",
            "referenced_by": "the C symbol name (already symbolic)",
        })

    # 3. datafile objects
    counts = {}
    for stem, rel, pwkey in DATAFILES:
        path = os.path.join(ASSETS, rel.replace("/", os.sep))
        pw = PASSWORDS.get(os.path.basename(path).lower())
        body, info = open_packfile(path, pw)
        objs = flatten(parse_datafile(body))
        counts[stem] = {"file": rel, "objects": len(objs),
                        "password": info["password"],
                        "packed": info["packed"],
                        "file_sha256": sha(open(path, "rb").read()),
                        "by_type": {}}
        for o in objs:
            counts[stem]["by_type"][o["type"]] = \
                counts[stem]["by_type"].get(o["type"], 0) + 1
            fmt, rule, ext = RULE.get(o["type"], ("unknown", "verbatim", ".bin"))
            geom = None
            if o["type"] == "BMP " and o["data"]:
                import struct as _s
                bpp, w, h = _s.unpack(">HHH", o["data"][:6])
                geom = {"bpp": bpp, "w": w, "h": h}
                counts[stem]["by_type"]["bpp%d" % bpp] = \
                    counts[stem]["by_type"].get("bpp%d" % bpp, 0) + 1
            slot = CHAR_SLOT.get(o["index"]) if stem.startswith("char.") else None
            man["assets"].append({
                "id": "%s/%s" % (stem, slot or o["name"] or "#%d" % o["index"]),
                "kind": "datafile_object",
                "origin": {"mode": "DATAFILE", "file": rel,
                           "index": o["index"], "object_name": o["name"],
                           "type_fourcc": o["type"],
                           "unpacked_offset": o["offset"], "size": o["size"]},
                "format": fmt, "geometry": geom,
                "sha256": sha(o["data"]) if o["data"] is not None else None,
                "extract": rule,
                "clean_path": "assets/%s/%s%s" % (stem.replace(".", "/"),
                                                  (slot or o["name"] or str(o["index"])).lower(), ext),
                "referenced_by": ("numeric index data[%d] compiled into .text"
                                  % o["index"]),
            })
    man["datafile_summary"] = counts

    # 4. external plain files
    for rel, fmt, meaning in EXTERNAL:
        p = os.path.join(ASSETS, rel.replace("/", os.sep))
        if not os.path.exists(p):
            continue
        b = open(p, "rb").read()
        man["assets"].append({
            "id": "file/%s" % rel, "kind": "external_file",
            "origin": {"mode": "EXTERNAL", "path": rel, "size": len(b)},
            "format": fmt, "sha256": sha(b),
            "extract": "already a plain file - copy verbatim",
            "referenced_by": "path string in .rdata", "meaning": meaning,
        })
    man["runtime_written"] = [{"path": p, "format": f, "meaning": m}
                              for p, f, m in RUNTIME_WRITTEN]

    out = os.path.join(ROOT, "artifacts", "asset_manifest.json")
    json.dump(man, open(out, "w"), indent=1)
    print("wrote", out, len(man["assets"]), "assets")
    for k, v in counts.items():
        print("  %-26s %-30s %3d objects pw=%s packed=%s %s" % (
            k, v["file"], v["objects"], v["password"], v["packed"], v["by_type"]))


if __name__ == "__main__":
    main()
