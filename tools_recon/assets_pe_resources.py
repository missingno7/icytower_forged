"""assets_pe_resources.py - enumerate PE .rsrc of icytower15.exe (read-only).

Usage: python tools_recon/assets_pe_resources.py [exe] [--json out.json]
"""
import sys, json, hashlib, os
import pefile

RT = {
    1: "RT_CURSOR", 2: "RT_BITMAP", 3: "RT_ICON", 4: "RT_MENU", 5: "RT_DIALOG",
    6: "RT_STRING", 7: "RT_FONTDIR", 8: "RT_FONT", 9: "RT_ACCELERATOR",
    10: "RT_RCDATA", 11: "RT_MESSAGETABLE", 12: "RT_GROUP_CURSOR",
    14: "RT_GROUP_ICON", 16: "RT_VERSION", 17: "RT_DLGINCLUDE",
    19: "RT_PLUGPLAY", 20: "RT_VXD", 21: "RT_ANICURSOR", 22: "RT_ANIICON",
    23: "RT_HTML", 24: "RT_MANIFEST",
}


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") \
        else os.path.join("assets", "icytower15.exe")
    pe = pefile.PE(exe, fast_load=True)
    pe.parse_data_directories()
    out = {"exe": exe, "sections": [], "resources": []}
    for s in pe.sections:
        out["sections"].append({
            "name": s.Name.decode(errors="replace").rstrip("\x00"),
            "va": hex(pe.OPTIONAL_HEADER.ImageBase + s.VirtualAddress),
            "vsize": s.Misc_VirtualSize,
            "rawsize": s.SizeOfRawData,
            "chars": hex(s.Characteristics),
        })
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        out["resources"] = None
    else:
        for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            tname = t.name.string.decode(errors="replace") if t.name else RT.get(t.id, str(t.id))
            for nm in getattr(t, "directory", None).entries if hasattr(t, "directory") else []:
                rname = nm.name.string.decode(errors="replace") if nm.name else nm.id
                for lang in nm.directory.entries:
                    d = lang.data.struct
                    data = pe.get_data(d.OffsetToData, d.Size)
                    out["resources"].append({
                        "type": tname, "name": rname, "lang": lang.id,
                        "rva": hex(d.OffsetToData),
                        "va": hex(pe.OPTIONAL_HEADER.ImageBase + d.OffsetToData),
                        "size": d.Size,
                        "sha256": hashlib.sha256(data).hexdigest(),
                        "head": data[:16].hex(),
                    })
    j = json.dumps(out, indent=2)
    print(j)
    if "--json" in sys.argv:
        p = sys.argv[sys.argv.index("--json") + 1]
        open(p, "w").write(j)


if __name__ == "__main__":
    main()
