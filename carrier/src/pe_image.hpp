// pe_image.hpp - minimal PE32 EXE loader for the icytower15.exe carrier.
//
// KNOWN (notes/binary_recon.md): icytower15.exe is PE32, ImageBase 0x400000,
// SizeOfImage 0x38c000, entry RVA 0x1110, NO base relocations, NO TLS
// directory. Because there are no relocations we never rebase; we just
// require the image base address to be free and fail loudly if not.
#pragma once
#include <windows.h>

struct PeImageInfo {
    HMODULE base;          // == requested ImageBase, cast to HMODULE
    unsigned long image_base; // numeric VA of the image base
    unsigned long size_of_image;
    unsigned long entry_va; // VA (not RVA) of the entry point
};

// Reserves [image_base, image_base+size_of_image) as MEM_RESERVE with no
// access, and nothing else. Call this as the VERY FIRST statement in
// main(), before any other work (before parsing args, before opening any
// file). On this host, Windows' heap/segment allocator and lazy locale
// (NLS) file mappings otherwise claim addresses inside the guest's fixed
// ImageBase range within milliseconds of process startup - observed: by
// the time pe_image_load() would normally run (after just symbol-table
// loading and a couple of fprintf calls), dozens of small MEM_MAPPED
// heap-segment/NLS allocations already tile the entire 0x400000-0x78c000
// range, because carrier.exe itself no longer lives there (see build.cmd's
// /BASE:0x10000000) and the OS's bottom-up allocator treats it as free
// space to hand out. Reserving the range up front (before any of that
// activity) blocks the OS from ever choosing it. pe_image_load() later
// just commits the already-owned reservation with real protection.
// TEMPORARY: hardcodes icytower15.exe's known ImageBase/SizeOfImage
// (notes/binary_recon.md) as constants in main.cpp, since this must run
// before the guest file is even opened.
bool pe_image_reserve_guest_range(unsigned long image_base, unsigned long size_of_image);

// Reads `path`, reserves+commits its preferred ImageBase, copies in headers
// and sections at their mapped RVAs. Fails loudly (prints diagnostics via
// stderr and returns false) if the base address is unavailable or the file
// isn't the expected shape. Does not run any code and does not touch the
// import directory - src/imports.cpp does that separately once this returns.
bool pe_image_load(const char* path, PeImageInfo* out);
