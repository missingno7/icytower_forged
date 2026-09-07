"""assets_refscan.py - find every compiled-in datafile index reference.

The game keeps two DATAFILE* globals:
    data  @ 0x004dd23c   (data/data.dat)
    sfx   @ 0x004dd240   (data/sfx15.dat)
plus per-character DATAFILE* fields.  Allegro's DATAFILE is 16 bytes
{void *dat; int type; long size; DATAFILE_PROPERTY *prop;} so `data[N].dat`
compiles to  mov 0x4dd23c,%r ; mov (N*16)(%r),%r .

This scans artifacts/disasm.txt for that pattern and maps N back to the object
name parsed from the datafile.
"""
import re, os, sys, json, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIS = os.path.join(ROOT, "artifacts", "disasm.txt")

GLOBALS = {"0x4dd23c": ("data", "data.dat"), "0x4dd240": ("sfx", "sfx15.dat")}

fn_re = re.compile(r"^([0-9a-f]{8}) <(\S+)>:")
ins_re = re.compile(r"^\s+([0-9a-f]+):\t[0-9a-f ]+\t(\S+)\s*(.*)$")


def names_for(datfile):
    p = os.path.join(ROOT, "artifacts/assets_extract",
                     "objects_%s.json" % datfile.replace(".dat", ""))
    if not os.path.exists(p):
        return {}
    rec = json.load(open(p))
    return {o["index"]: o["path"] for o in rec["objects"]}


def main():
    obj_names = {g: names_for(f) for g, (n, f) in
                 ((k, v) for k, v in GLOBALS.items())}
    obj_names = {k: names_for(v[1]) for k, v in GLOBALS.items()}
    hits = []
    cur_fn = None
    # state: register -> which datafile global it currently holds
    reg = {}
    with open(DIS, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = fn_re.match(line)
            if m:
                cur_fn = m.group(2)
                reg = {}
                continue
            m = ins_re.match(line)
            if not m:
                continue
            va, op, args = m.group(1), m.group(2), m.group(3)
            args = args.split("<")[0].strip()
            if op == "mov":
                mm = re.match(r"^(0x[0-9a-f]+),%(\w+)$", args)
                if mm and mm.group(1) in GLOBALS:
                    reg[mm.group(2)] = GLOBALS[mm.group(1)][0]
                    reg["_src_" + mm.group(2)] = mm.group(1)
                    continue
                # mov OFF(%src),%dst  -> data[OFF/16].dat
                mm = re.match(r"^(?:(0x[0-9a-f]+|-?\d+))?\(%(\w+)\),%(\w+)$", args)
                if mm:
                    src = mm.group(2)
                    if reg.get(src) in ("data", "sfx"):
                        off = int(mm.group(1), 0) if mm.group(1) else 0
                        gname = reg[src]
                        key = "0x4dd23c" if gname == "data" else "0x4dd240"
                        idx = off // 16
                        field = ["dat", "type", "size", "prop"][(off % 16) // 4]
                        hits.append({
                            "va": "0x" + va, "func": cur_fn, "global": gname,
                            "offset": off, "index": idx, "field": field,
                            "object": obj_names.get(key, {}).get(idx),
                        })
                    reg.pop(mm.group(3), None)
                    continue
                # anything else writing a register invalidates it
                mm = re.match(r".*,%(\w+)$", args)
                if mm:
                    reg.pop(mm.group(1), None)
            elif op in ("lea", "add", "sub", "xor", "pop", "call"):
                if op == "call":
                    reg = {}
                else:
                    mm = re.match(r".*%(\w+)$", args)
                    if mm:
                        reg.pop(mm.group(1), None)
    # second pass: getSampleFromOggDatafile(sfx, N) - sound objects by index
    sfx_names = obj_names["0x4dd240"]
    txt = open(DIS, encoding="utf-8", errors="replace").read().split("\n")
    snd = []
    cur_fn = None
    for i, l in enumerate(txt):
        m = fn_re.match(l)
        if m:
            cur_fn = m.group(2)
        if "<_getSampleFromOggDatafile>" not in l or "call" not in l:
            continue
        idx = None
        for j in range(i - 1, max(0, i - 8), -1):
            mm = re.search(r"movl\s+\$0x([0-9a-f]+),0x4\(%esp\)", txt[j])
            if mm:
                idx = int(mm.group(1), 16)
                break
        dest = None
        for j in range(i + 1, min(len(txt), i + 3)):
            mm = re.search(r"mov\s+%eax,(0x[0-9a-f]+)", txt[j])
            if mm:
                dest = mm.group(1)
                break
        snd.append({"va": "0x" + re.match(r"\s+([0-9a-f]+):", l).group(1),
                    "func": cur_fn, "global": "sfx", "index": idx,
                    "object": sfx_names.get(idx), "stored_to": dest,
                    "field": "sample"})
    hits.extend(snd)
    print("\ngetSampleFromOggDatafile(sfx, N) call sites:", len(snd))
    for s in snd:
        print("  %s %-22s sfx[%3s] %-16s -> %s" % (
            s["va"], s["func"], s["index"], s["object"], s["stored_to"]))

    out = os.path.join(ROOT, "artifacts/assets_extract/datafile_refs.json")
    json.dump(hits, open(out, "w"), indent=1)
    c = collections.Counter((h["global"], h["index"], h["object"]) for h in hits
                            if h["field"] == "dat")
    print("total datafile member reads:", len(hits))
    print("distinct (global,index) .dat reads:", len(c))
    for (g, i, name), n in sorted(c.items(), key=lambda x: (x[0][0], x[0][1])):
        print("%-5s data[%3d] %-24s x%d" % (g, i, name, n))
    print("wrote", out)


if __name__ == "__main__":
    main()
