#!/usr/bin/env python3
"""gen_assets.py -- generate the ASSET coastline seam (win32_pilot.md SS7c,
notes/asset_census.md SS3/SS6, ROADMAP.md SS2).

Reads artifacts/asset_manifest.json (319 records, all three EMBEDDED
origins: EXE_RSRC, EXE_DATA, DATAFILE, EXTERNAL) and reuses tools_recon/
assets_manifest.py's CHAR_SLOT table for the character datafiles' game-
visible slot names (palette, frame01..frame15, snd_jump_lo, ...), so this
generator does not re-derive facts assets_manifest.py already established.

Scope: only manifest records with kind == "datafile_object" (257 of 319) --
the ones actually reached in game code through a `data[N].dat`/`sfx[N].dat`
numeric index (notes/asset_census.md SS3). The other 62 records (2 PE icons,
48 compiled-in .data/.rdata tables, 12 external plain files) are a different
EMBEDDED form with a different consumer (icons: Win32 resource API tables:
generator emits a C initialiser directly into src/; external files: already
plain files) and are out of scope for load_asset()/asset_bitmap() et al.

Outputs:
  src/icytower/assets.h            address-free `asset_id` enum + the
                                    5-function API declaration
                                    (BITMAP/SAMPLE/FONT/PALETTE/void* by id)
  src/icytower/assets_table.inc    {id, kind="datafile"/"sfx"/"loading"/
                                    "char:<name>", index N, object name,
                                    type_fourcc} rows, DATA only (no
                                    address) -- included by both worlds'
                                    implementation
  carrier/gen/pf_asset_bindings.h  carrier-world implementation: resolves
                                    an id to the SAME object in the SAME
                                    already-loaded DATAFILE the original
                                    game code reads (zero copy) for the
                                    "data"/"sfx" families (persistent
                                    globals `data`/`sfx`, already bound by
                                    pf_bindings_src.h), and to a privately
                                    lazy-loaded DATAFILE for "loading" and
                                    every "char:*" family (notes/
                                    asset_census.md SS3: neither has a
                                    persistent DATAFILE* global in the
                                    original -- see this file's own header
                                    comment)
  src/icytower/assets_standalone.c the standalone implementation: loads
                                    every datafile itself with the real
                                    Allegro load_datafile()+
                                    packfile_password(), using the
                                    passwords recovered from the binary
                                    (notes/asset_census.md SS5a)

ID naming rule (deterministic, purely mechanical from the manifest -- see
src/icytower/ASSETS.md "How ids are named" for the worked examples):

  ASSET_<FAMILY>_<OBJECT>

  FAMILY: DATA | LOADING | SFX for data.dat/loading.dat/sfx15.dat; for a
  character datafile, CHAR_<X> where X is the upper-cased first `_`-token
  of the character's directory name (harold_the_homeboy -> HAROLD,
  disco_dave -> DISCO, jungle_jane -> JUNGLE, wild_wendy -> WILD).

  OBJECT: the datafile object's own NAME property for data/loading/sfx15
  (already a symbolic identifier: TITLE, FONT_MONO, FLD_LOGO, ...) with a
  leading "S_" stripped for sfx15 (S_AIGHT -> AIGHT, so the id is
  ASSET_SFX_AIGHT, not ASSET_SFX_S_AIGHT); for a character datafile, the
  CHAR_SLOT-derived game-visible slot name (palette, frame01..frame15,
  snd_jump_lo, ...) instead of the raw grabber name ("000_PAL" etc, which
  carries no meaning). Either way: non-alnum/non-underscore runs become
  "_", and a trailing digit run gets a "_" inserted before it for
  readability (frame01 -> FRAME_01).

Usage:
    python carrier/gen/gen_assets.py
        [--manifest artifacts/asset_manifest.json]
        [--out-src src/icytower] [--out-gen carrier/gen]
"""
import argparse
import datetime
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

sys.path.insert(0, os.path.join(ROOT, "tools_recon"))
import assets_manifest as AM  # noqa: E402  (CHAR_SLOT, DATAFILES -- reused, not re-derived)

DEFAULT_MANIFEST = os.path.join(ROOT, "artifacts", "asset_manifest.json")
DEFAULT_OUT_SRC = os.path.join(ROOT, "src", "icytower")
DEFAULT_OUT_GEN = os.path.join(ROOT, "carrier", "gen")

# datafile family metadata: logical name -> (relative path, password or None)
# Passwords are GAME DATA recovered from the binary (notes/asset_census.md
# SS5a): 'CHEESE' is pwd_garble_string(0x4bdb3c, 0x32) applied to the
# garbled .data string at 0x4bdb3c; '(c) Free Lunch Design' is read
# directly. Character datafiles ship with no password (KNOWN,
# datafile_summary in the manifest: "password": null for all four).
FAMILY_META = {}
for _stem, _rel, _pwkey in AM.DATAFILES:
    if _stem == "data":
        _fam, _pw = "data", "CHEESE"
    elif _stem == "sfx15":
        _fam, _pw = "sfx", "CHEESE"
    elif _stem == "loading":
        _fam, _pw = "loading", "(c) Free Lunch Design"
    else:
        assert _stem.startswith("char.")
        _fam, _pw = "char:" + _stem[len("char."):], None
    FAMILY_META[_fam] = (_rel, _pw)

# manifest origin.file (relative path) -> (assets_manifest.py stem, family)
FILE_TO_STEM = {rel: stem for stem, rel, _ in AM.DATAFILES}


def family_of(stem):
    if stem == "data":
        return "data"
    if stem == "sfx15":
        return "sfx"
    if stem == "loading":
        return "loading"
    assert stem.startswith("char.")
    return "char:" + stem[len("char."):]


def id_prefix_of(stem):
    if stem == "data":
        return "DATA"
    if stem == "sfx15":
        return "SFX"
    if stem == "loading":
        return "LOADING"
    assert stem.startswith("char.")
    charname = stem[len("char."):]
    first_tok = charname.split("_")[0]
    return "CHAR_" + first_tok.upper()


_NONALNUM_RE = re.compile(r'[^A-Za-z0-9_]+')
_DIGIT_SUFFIX_RE = re.compile(r'^(.*[^0-9_])(\d+)$')
_COLLAPSE_RE = re.compile(r'_+')


def sanitize_token(name):
    s = _NONALNUM_RE.sub('_', name).upper()
    s = _COLLAPSE_RE.sub('_', s).strip('_')
    m = _DIGIT_SUFFIX_RE.match(s)
    if m:
        s = m.group(1) + '_' + m.group(2)
    return s


def display_name(stem, index, object_name):
    """The name assets_table.inc's object_name column stores for this
    object: for a character datafile, the CHAR_SLOT game-visible slot
    name (not the raw, meaningless grabber name like "000_PAL" --
    CHAR_SLOT is itself derived from the disassembly, notes/
    asset_census.md SS3, not invented here); otherwise the datafile
    object's own NAME property, UNCHANGED (so this column always matches
    what a reader would find by opening the real file with
    tools_recon/assets_datafile.py -- carrier/gen/check_assets.py's
    real-file-order check cross-references this exact value).
    """
    if stem.startswith("char."):
        # CHAR_SLOT (tools_recon/assets_manifest.py) only names the 24
        # game-visible slots (palette, 15 frames, 8 sounds -- the indices
        # load_frames/load_sounds actually walk, notes/asset_census.md
        # SS3). 3 of the 4 character datafiles carry one extra trailing
        # object, grabber bookkeeping named "GrabberInfo" (type "info",
        # same runtime-irrelevant kind as data/GrabberInfo and
        # loading/GrabberInfo -- notes/asset_census.md SS4's "info"
        # row), outside that range; fall back to its own object name.
        slot = AM.CHAR_SLOT.get(index) or object_name
        assert slot is not None, "no name for %s index %d" % (stem, index)
        return slot
    return object_name or ("OBJ_%d" % index)


def object_part(stem, index, object_name):
    # The id's OBJECT token additionally strips sfx15's redundant "S_"
    # prefix (the FAMILY token already says SFX -- ASSET_SFX_AIGHT reads
    # better than ASSET_SFX_S_AIGHT); this is purely an id-readability
    # choice and must NOT leak into display_name()'s table column, which
    # stays traceable to the real file's own NAME property verbatim.
    name = display_name(stem, index, object_name)
    if stem == "sfx15" and name.startswith("S_"):
        name = name[2:]
    return sanitize_token(name)


class Asset:
    __slots__ = ("id_name", "family", "index", "object_name", "type_fourcc")

    def __init__(self, id_name, family, index, object_name, type_fourcc):
        self.id_name = id_name
        self.family = family
        self.index = index
        self.object_name = object_name
        self.type_fourcc = type_fourcc


def build_assets(manifest):
    assets = []
    seen = {}
    for rec in manifest["assets"]:
        if rec["kind"] != "datafile_object":
            continue
        origin = rec["origin"]
        rel = origin["file"]
        stem = FILE_TO_STEM[rel]
        family = family_of(stem)
        prefix = id_prefix_of(stem)
        obj = object_part(stem, origin["index"], origin["object_name"])
        id_name = "ASSET_%s_%s" % (prefix, obj)
        if id_name in seen:
            # deterministic disambiguation; should not trigger for the
            # current 257-object manifest (checked by check_assets.py),
            # kept so the generator never silently drops an asset.
            n = 2
            while ("%s_%d" % (id_name, n)) in seen:
                n += 1
            id_name = "%s_%d" % (id_name, n)
        seen[id_name] = True
        dname = display_name(stem, origin["index"], origin["object_name"])
        assets.append(Asset(id_name, family, origin["index"],
                             dname, origin["type_fourcc"]))
    return assets


BANNER_SRC = """/* {fname} -- GENERATED FILE. DO NOT EDIT.
 * Produced by carrier/gen/gen_assets.py from:
 *   artifacts/asset_manifest.json (319 records; this file uses the 257
 *     kind=="datafile_object" ones)
 *   tools_recon/assets_manifest.py (CHAR_SLOT, DATAFILES -- reused, not
 *     re-derived)
 * Generated: {date}
 *
 * win32_pilot.md SS7c / notes/asset_census.md SS6 "Binding model": the
 * ASSET coastline, EMBEDDED ORIGINAL -> EXTRACTED FILE, one level below
 * the CODE coastline's ORIGINAL -> LIFTED -> NATIVE. This file is the
 * address-free half: an asset_id and the accessor prototypes, no
 * datafile index literal, no path string. See src/icytower/ASSETS.md.
 * Re-run gen_assets.py to regenerate; do not hand-edit.
 */
"""

BANNER_INC = """/* {fname} -- GENERATED FILE. DO NOT EDIT.
 * Produced by carrier/gen/gen_assets.py -- see assets.h's header comment
 * for inputs. DATA only (asset id, datafile family name, object index,
 * object name, type fourcc) -- no address, so this lives under src/
 * exactly like game_types.h's field-offset-free struct layouts do
 * (notes/asset_census.md SS6: "indices are DATA, not addresses").
 * Re-run gen_assets.py to regenerate; do not hand-edit.
 */
"""

BANNER_GEN = """/* {fname} -- GENERATED FILE. DO NOT EDIT.
 * Produced by carrier/gen/gen_assets.py.
 * Generated: {date}
 *
{extra}
 * Force-included (/FI), after pf_bindings_src.h and pf_lib_bindings.h,
 * only when address-free src/ is compiled INTO the carrier
 * (win32_pilot.md SS7a's trick, reused here for the ASSET coastline).
 * Re-run gen_assets.py to regenerate; do not hand-edit.
 */
"""


def c_str(s):
    if s is None:
        return "(const char *)0"
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def write_assets_h(path, assets, date):
    lines = [BANNER_SRC.format(fname="assets.h", date=date)]
    lines.append("#ifndef ICYTOWER_ASSETS_H")
    lines.append("#define ICYTOWER_ASSETS_H")
    lines.append("")
    lines.append("/* One id per datafile object reached through a data[N]/sfx[N]/")
    lines.append(" * char-datafile[N] index in the original game (notes/asset_census.md")
    lines.append(" * SS3). Enum order == src/icytower/assets_table.inc row order, so a")
    lines.append(" * plain asset_id value is also a valid index into asset_table[]. */")
    lines.append("typedef enum asset_id {")
    for a in assets:
        lines.append("    %s," % a.id_name)
    lines.append("    ASSET_COUNT")
    lines.append("} asset_id;")
    lines.append("")
    lines.append("/* BITMAP/SAMPLE/FONT/PALETTE come from allegro_types.h regardless of")
    lines.append(" * which world this compiles into (it already self-skips its own body")
    lines.append(" * under ICYTOWER_BINDINGS_ACTIVE and reuses it_types.h's layouts then). */")
    lines.append('#include "allegro_types.h"')
    lines.append("")
    lines.append("#ifndef ICYTOWER_BINDINGS_ACTIVE")
    lines.append("/* Standalone-world prototypes only: in the carrier world,")
    lines.append(" * carrier/gen/pf_asset_bindings.h (force-included ahead of this file)")
    lines.append(" * already supplied these five names as real functions -- see its own")
    lines.append(" * header comment for why redeclaring them here would conflict")
    lines.append(" * (extern-then-static linkage clash), the same reasoning")
    lines.append(" * src/README.md documents for allegro_api.h. */")
    lines.append("")
    lines.append("/* EMBEDDED ORIGINAL (carrier) / EXTRACTED, drop-in original folder")
    lines.append(" * (standalone) -- win32_pilot.md SS7c, notes/asset_census.md SS6/SS7.")
    lines.append(" * `id` never carries an address or a path; both worlds resolve it")
    lines.append(" * through src/icytower/assets_table.inc. */")
    lines.append("BITMAP *asset_bitmap(asset_id id);")
    lines.append("SAMPLE *asset_sample(asset_id id);")
    lines.append("FONT *asset_font(asset_id id);")
    lines.append("PALETTE *asset_palette(asset_id id);")
    lines.append("void *asset_object(asset_id id);")
    lines.append("")
    lines.append("#endif /* !ICYTOWER_BINDINGS_ACTIVE */")
    lines.append("")
    lines.append("#endif /* ICYTOWER_ASSETS_H */")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_assets_table_inc(path, assets):
    lines = [BANNER_INC.format(fname="assets_table.inc")]
    lines.append("#ifndef ICYTOWER_ASSETS_TABLE_INC")
    lines.append("#define ICYTOWER_ASSETS_TABLE_INC")
    lines.append("")
    lines.append('#include "assets.h"')
    lines.append("")
    lines.append("struct asset_table_row {")
    lines.append("    asset_id id;")
    lines.append('    const char *datafile;    /* "data" | "sfx" | "loading" | "char:<name>" */')
    lines.append("    int index;                /* object index N inside that datafile -- DATA, never an address */")
    lines.append("    const char *object_name;   /* datafile object NAME property, or (character datafiles) the CHAR_SLOT slot name */")
    lines.append('    const char *type_fourcc;   /* Allegro DATAFILE object type: "BMP ", "PAL ", "FONT", "OGG ", "info", ... */')
    lines.append("};")
    lines.append("")
    lines.append("static const struct asset_table_row asset_table[ASSET_COUNT] = {")
    for a in assets:
        oname = a.object_name
        if oname is None:
            oname_c = "(const char *)0"
        else:
            oname_c = c_str(oname)
        lines.append("    { %s, %s, %d, %s, %s }," % (
            a.id_name, c_str(a.family), a.index, oname_c, c_str(a.type_fourcc)))
    lines.append("};")
    lines.append("")
    lines.append("/* Per-datafile-family path + password, keyed by asset_table_row.datafile.")
    lines.append(" * Passwords are game data recovered from the binary")
    lines.append(" * (notes/asset_census.md SS5a) -- CLEARLY MARKED here, the one table both")
    lines.append(" * implementations read instead of each carrying its own copy. */")
    lines.append("struct asset_datafile_family_row {")
    lines.append("    const char *name;")
    lines.append("    const char *path;")
    lines.append("    const char *password;    /* (const char *)0 if the datafile is unencrypted */")
    lines.append("};")
    lines.append("")
    lines.append("static const struct asset_datafile_family_row asset_datafile_family[] = {")
    # deterministic order: data, sfx, loading, then the four char: families
    order = ["data", "sfx", "loading"] + sorted(k for k in FAMILY_META if k.startswith("char:"))
    for fam in order:
        rel, pw = FAMILY_META[fam]
        lines.append("    { %s, %s, %s }," % (c_str(fam), c_str(rel), c_str(pw)))
    lines.append("};")
    lines.append("#define ASSET_DATAFILE_FAMILY_COUNT "
                  "(sizeof(asset_datafile_family) / sizeof(asset_datafile_family[0]))")
    lines.append("")
    lines.append("#endif /* ICYTOWER_ASSETS_TABLE_INC */")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_pf_asset_bindings_h(path, date):
    extra = (
        " * Carrier-world implementation of src/icytower/assets.h's 5-function\n"
        " * API (win32_pilot.md SS7c, notes/asset_census.md SS6 \"Binding model\"):\n"
        " * an asset_id resolves to the SAME object in the SAME DATAFILE the\n"
        " * original game code already reads -- zero copy, zero re-parsing.\n"
        " *\n"
        " * Two families (\"data\", \"sfx\") have a PERSISTENT DATAFILE* global\n"
        " * the original init_game() fills once and keeps for the rest of the\n"
        " * process (notes/asset_census.md SS3): `data` @0x4dd23c, `sfx`\n"
        " * @0x4dd240, already bound to those addresses under their plain names\n"
        " * by pf_bindings_src.h (force-included ahead of this file) -- so this\n"
        " * file reads them exactly as game code would: `data[N].dat`.\n"
        " *\n"
        " * The other five families (\"loading\", and every \"char:<name>\") have\n"
        " * NO persistent global in the original: `load_datafile(\"data/loading.dat\")`\n"
        " * fills a local used once for the splash and never kept; a character's\n"
        " * datafile only ever passes through the single, ephemeral `custom.df`\n"
        " * slot while that one character happens to be selected in the menu, and\n"
        " * is overwritten the moment a different character is chosen (KNOWN,\n"
        " * game_types.h's Tcustom -- one struct, not an array). There is\n"
        " * therefore no single \"the object the game already loaded\" for those\n"
        " * five families to bind to zero-copy the way \"data\"/\"sfx\" can.\n"
        " *\n"
        " * DESIGN DECISION (disclosed in src/icytower/ASSETS.md): this file\n"
        " * gives each of those five families its own PRIVATE, lazily-loaded,\n"
        " * cached-forever DATAFILE*, loaded through the real embedded Allegro\n"
        " * loader (load_datafile/packfile_password, bound to their original\n"
        " * addresses by pf_lib_bindings.h -- force-included ahead of this file\n"
        " * too) at the same relative path and with the same password the\n"
        " * original uses (src/icytower/assets_table.inc's asset_datafile_family[]).\n"
        " * This reads the exact same bytes through the exact same decoder the\n"
        " * original ships, so it is still \"zero re-implementation\", just not\n"
        " * \"the identical in-memory object at every moment\" -- the honest\n"
        " * fact for these five families is that the original itself doesn't\n"
        " * keep one either.\n"
        " *\n"
        " * OGG-type objects (all sample-kind objects in this game -- no shipped\n"
        " * datafile uses the SAMP type, notes/asset_census.md SS2a) need a real\n"
        " * decode, not a cast: asset_sample() calls the embedded libvorbis addon's\n"
        " * own `logg_load_memory` (pf_lib_bindings.h, VA=0x420688 -- the exact\n"
        " * function every game family already funnels OGG objects through) on\n"
        " * the object's raw payload. SAMP-type objects, if any datafile ever\n"
        " * ships one, are returned as a zero-copy cast like every other kind.\n"
    )
    lines = [BANNER_GEN.format(fname="pf_asset_bindings.h", date=date, extra=extra)]
    lines.append("#ifndef PF_ASSET_BINDINGS_H")
    lines.append("#define PF_ASSET_BINDINGS_H")
    lines.append("")
    lines.append("#include <string.h>")
    lines.append('#include "assets.h"')
    lines.append('#include "assets_table.inc"')
    lines.append("")
    lines.append("/* one cache slot per asset_datafile_family[] row, same index -- only")
    lines.append(" * the \"loading\" and \"char:*\" rows ever populate theirs, since")
    lines.append(" * \"data\"/\"sfx\" are special-cased below to the persistent globals and")
    lines.append(" * never reach this array at all. */")
    lines.append("static DATAFILE *pf_asset_cache[ASSET_DATAFILE_FAMILY_COUNT];")
    lines.append("")
    lines.append("static DATAFILE *pf_asset_family(const char *family)")
    lines.append("{")
    lines.append("    unsigned i;")
    lines.append("")
    lines.append('    if (!strcmp(family, "data"))')
    lines.append("        return data;   /* persistent global, plain name via pf_bindings_src.h */")
    lines.append('    if (!strcmp(family, "sfx"))')
    lines.append("        return sfx;    /* persistent global, plain name via pf_bindings_src.h */")
    lines.append("")
    lines.append("    for (i = 0; i < ASSET_DATAFILE_FAMILY_COUNT; i++) {")
    lines.append("        if (strcmp(asset_datafile_family[i].name, family) != 0)")
    lines.append("            continue;")
    lines.append("        if (!pf_asset_cache[i]) {")
    lines.append("            packfile_password(asset_datafile_family[i].password);   /* (const char *)0 -- unencrypted */")
    lines.append("            pf_asset_cache[i] = load_datafile(asset_datafile_family[i].path);")
    lines.append("            packfile_password((const char *)0);")
    lines.append("        }")
    lines.append("        return pf_asset_cache[i];")
    lines.append("    }")
    lines.append("    return (DATAFILE *)0;")
    lines.append("}")
    lines.append("")
    lines.append("static const struct asset_table_row *pf_asset_row(asset_id id)")
    lines.append("{")
    lines.append("    return &asset_table[id];")
    lines.append("}")
    lines.append("")
    lines.append("static void *pf_asset_raw(asset_id id)")
    lines.append("{")
    lines.append("    const struct asset_table_row *row = pf_asset_row(id);")
    lines.append("    DATAFILE *base = pf_asset_family(row->datafile);")
    lines.append("    return base[row->index].dat;")
    lines.append("}")
    lines.append("")
    lines.append("/* static, like every other name pf_bindings_src.h/pf_lib_bindings.h")
    lines.append(" * introduce: this header may be force-included ahead of more than one")
    lines.append(" * native .c file's compile, and each gets its own internal-linkage")
    lines.append(" * copy instead of colliding at link time (the same reason")
    lines.append(" * it_globals.h emits a `static X *g_name_p` per name). assets.h never")
    lines.append(" * declares these with external linkage in this world (guarded by")
    lines.append(" * ICYTOWER_BINDINGS_ACTIVE), so there is no extern/static clash. */")
    lines.append("static BITMAP *asset_bitmap(asset_id id) { return (BITMAP *)pf_asset_raw(id); }")
    lines.append("static FONT *asset_font(asset_id id) { return (FONT *)pf_asset_raw(id); }")
    lines.append("static PALETTE *asset_palette(asset_id id) { return (PALETTE *)pf_asset_raw(id); }")
    lines.append("static void *asset_object(asset_id id) { return pf_asset_raw(id); }")
    lines.append("")
    lines.append("static SAMPLE *asset_sample(asset_id id)")
    lines.append("{")
    lines.append("    const struct asset_table_row *row = pf_asset_row(id);")
    lines.append("    DATAFILE *base = pf_asset_family(row->datafile);")
    lines.append("    DATAFILE *obj = &base[row->index];")
    lines.append('    if (!strncmp(row->type_fourcc, "OGG", 3))')
    lines.append("        return logg_load_memory(obj->dat, (it_orig_size_t)obj->size);")
    lines.append("    return (SAMPLE *)obj->dat;   /* SAMP-type object: already a real SAMPLE*, zero copy */")
    lines.append("}")
    lines.append("")
    lines.append("#endif /* PF_ASSET_BINDINGS_H */")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def write_assets_standalone_c(path, date):
    lines = []
    lines.append("/* assets_standalone.c -- standalone-world implementation of")
    lines.append(" * src/icytower/assets.h's 5-function API (win32_pilot.md SS7c,")
    lines.append(" * notes/asset_census.md SS6 \"Binding model\", SS7 mode (a) \"drop-in,")
    lines.append(" * original folder\"). GENERATED FILE -- produced by")
    lines.append(" * carrier/gen/gen_assets.py, generated %s. Re-run it to regenerate;" % date)
    lines.append(" * do not hand-edit.")
    lines.append(" *")
    lines.append(" * Loads every one of the 7 datafiles (data.dat, loading.dat, sfx15.dat,")
    lines.append(" * the 4 character .dat files) itself, lazily, with the REAL Allegro")
    lines.append(" * load_datafile()/packfile_password() (allegro_api.h -- address-free,")
    lines.append(" * same declarations this port would use against a real <allegro.h>,")
    lines.append(" * see that file's own header comment): once Allegro is built from")
    lines.append(" * source (win32_pilot.md SS7c stage S3) these resolve to the real")
    lines.append(" * library instead of a guest address, so this file needs no carrier")
    lines.append(" * header at all -- it is plain source, exactly like every other file")
    lines.append(" * in this directory.")
    lines.append(" *")
    lines.append(" * Passwords: GAME DATA recovered from the binary (notes/asset_census.md")
    lines.append(" * SS5a) -- 'CHEESE' for data.dat/sfx15.dat, '(c) Free Lunch Design' for")
    lines.append(" * loading.dat, no password for the (unencrypted) character datafiles.")
    lines.append(" * CLEARLY MARKED, single source of truth: src/icytower/")
    lines.append(" * assets_table.inc's asset_datafile_family[] table (shared with the")
    lines.append(" * carrier implementation, carrier/gen/pf_asset_bindings.h, so the two")
    lines.append(" * never carry independently-typo-able copies).")
    lines.append(" *")
    lines.append(" * OGG-type objects (every sample-kind object this game ships) are")
    lines.append(" * decoded through the real Allegro logg addon's logg_load_memory(),")
    lines.append(" * the same upstream function the carrier binds to its original")
    lines.append(" * address (carrier/gen/pf_asset_bindings.h's own header comment).")
    lines.append(" */")
    lines.append('#include "allegro_api.h"')
    lines.append('#include "assets_table.inc"')
    lines.append("#include <string.h>")
    lines.append("")
    lines.append("/* one cache slot per asset_datafile_family[] row, same order */")
    lines.append("static DATAFILE *pf_standalone_cache[ASSET_DATAFILE_FAMILY_COUNT];")
    lines.append("")
    lines.append("static DATAFILE *assets_standalone_family(const char *family)")
    lines.append("{")
    lines.append("    unsigned i;")
    lines.append("    for (i = 0; i < ASSET_DATAFILE_FAMILY_COUNT; i++) {")
    lines.append("        if (strcmp(asset_datafile_family[i].name, family) != 0)")
    lines.append("            continue;")
    lines.append("        if (!pf_standalone_cache[i]) {")
    lines.append("            packfile_password(asset_datafile_family[i].password);")
    lines.append("            pf_standalone_cache[i] = load_datafile(asset_datafile_family[i].path);")
    lines.append("            packfile_password((const char *)0);")
    lines.append("        }")
    lines.append("        return pf_standalone_cache[i];")
    lines.append("    }")
    lines.append("    return (DATAFILE *)0;")
    lines.append("}")
    lines.append("")
    lines.append("static void *assets_standalone_raw(asset_id id)")
    lines.append("{")
    lines.append("    const struct asset_table_row *row = &asset_table[id];")
    lines.append("    DATAFILE *base = assets_standalone_family(row->datafile);")
    lines.append("    return base[row->index].dat;")
    lines.append("}")
    lines.append("")
    lines.append("BITMAP *asset_bitmap(asset_id id) { return (BITMAP *)assets_standalone_raw(id); }")
    lines.append("FONT *asset_font(asset_id id) { return (FONT *)assets_standalone_raw(id); }")
    lines.append("PALETTE *asset_palette(asset_id id) { return (PALETTE *)assets_standalone_raw(id); }")
    lines.append("void *asset_object(asset_id id) { return assets_standalone_raw(id); }")
    lines.append("")
    lines.append("SAMPLE *asset_sample(asset_id id)")
    lines.append("{")
    lines.append("    const struct asset_table_row *row = &asset_table[id];")
    lines.append("    DATAFILE *base = assets_standalone_family(row->datafile);")
    lines.append("    DATAFILE *obj = &base[row->index];")
    lines.append('    if (!strncmp(row->type_fourcc, "OGG", 3))')
    lines.append("        return logg_load_memory(obj->dat, (it_orig_size_t)obj->size);")
    lines.append("    return (SAMPLE *)obj->dat;")
    lines.append("}")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    ap.add_argument("--out-src", default=DEFAULT_OUT_SRC)
    ap.add_argument("--out-gen", default=DEFAULT_OUT_GEN)
    args = ap.parse_args()

    manifest = json.load(open(args.manifest))
    assets = build_assets(manifest)
    date = datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")

    write_assets_h(os.path.join(args.out_src, "assets.h"), assets, date)
    write_assets_table_inc(os.path.join(args.out_src, "assets_table.inc"), assets)
    write_pf_asset_bindings_h(os.path.join(args.out_gen, "pf_asset_bindings.h"), date)
    write_assets_standalone_c(os.path.join(args.out_src, "assets_standalone.c"), date)

    by_family = {}
    for a in assets:
        by_family[a.family] = by_family.get(a.family, 0) + 1
    print("wrote %d asset ids" % len(assets))
    for fam in sorted(by_family):
        print("  %-28s %3d" % (fam, by_family[fam]))


if __name__ == "__main__":
    main()
