// pe_image.cpp - see pe_image.hpp for the contract.
#include "pe_image.hpp"
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#pragma comment(lib, "psapi.lib")

static void print_owner(void* addr) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        fprintf(stderr, "  VirtualQuery(%p) failed, gle=%lu\n", addr, GetLastError());
        return;
    }
    char mod_name[MAX_PATH] = "?";
    GetModuleFileNameA((HMODULE)mbi.AllocationBase, mod_name, sizeof(mod_name));
    fprintf(stderr,
        "  region at %p: BaseAddress=%p AllocationBase=%p State=0x%lx "
        "Protect=0x%lx Type=0x%lx RegionSize=0x%zx owner=%s\n",
        addr, mbi.BaseAddress, mbi.AllocationBase, mbi.State, mbi.Protect,
        mbi.Type, (size_t)mbi.RegionSize, mod_name);
}

bool pe_image_reserve_guest_range(unsigned long image_base, unsigned long size_of_image) {
    void* want = (void*)(uintptr_t)image_base;

    // Already reserved (typically by our own parent process via
    // VirtualAllocEx before resuming us - see relaunch_as_reserved_child in
    // main.cpp)? Then we're done; a second MEM_RESERVE call on the same
    // range would fail (MEM_RESERVE only succeeds on MEM_FREE memory).
    MEMORY_BASIC_INFORMATION existing;
    if (VirtualQuery(want, &existing, sizeof(existing)) != 0 &&
        existing.AllocationBase == want && existing.State == MEM_RESERVE &&
        existing.RegionSize >= size_of_image) {
        return true;
    }

    void* mem = VirtualAlloc(want, size_of_image, MEM_RESERVE, PAGE_NOACCESS);
    if (mem == want) return true;

    // Fallback for when nothing reserved the range ahead of time: the only
    // pre-existing occupant observed on this host is ucrtbase's lazy
    // locale/codepage table mapping (a small MEM_MAPPED view of a "*.nls"
    // file under System32\, e.g. C_852.NLS on a system whose codepage
    // isn't Latin-1). Evicting an arbitrary MEM_PRIVATE region is NOT safe
    // (measured: doing so once corrupted the heap and crashed the
    // process - it can be live CRT/heap bookkeeping, not a foreign file).
    // So this only ever unmaps MEM_MAPPED regions specifically backed by a
    // *.nls file; everything else is left alone, and a real loaded module
    // (MEM_IMAGE) simply fails the retry below.
    unsigned char* p = (unsigned char*)want;
    unsigned char* end = p + size_of_image;
    int guard = 0;
    while (p < end && guard++ < 256) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) break;
        if (mbi.State != MEM_FREE && mbi.Type == MEM_MAPPED) {
            char mapped_name[MAX_PATH] = "";
            GetMappedFileNameA(GetCurrentProcess(), mbi.AllocationBase, mapped_name, sizeof(mapped_name));
            size_t len = strlen(mapped_name);
            if (len >= 4 && _stricmp(mapped_name + len - 4, ".nls") == 0) {
                UnmapViewOfFile(mbi.AllocationBase);
            }
        }
        unsigned char* next = (unsigned char*)mbi.BaseAddress + mbi.RegionSize;
        if (next <= p) break; // non-advancing - stop rather than spin
        p = next;
    }

    mem = VirtualAlloc(want, size_of_image, MEM_RESERVE, PAGE_NOACCESS);
    return mem == want;
}

bool pe_image_load(const char* path, PeImageInfo* out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "pe_image_load: cannot open '%s'\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* file_buf = (unsigned char*)malloc(file_size);
    if (!file_buf || fread(file_buf, 1, file_size, f) != (size_t)file_size) {
        fprintf(stderr, "pe_image_load: short read on '%s'\n", path);
        fclose(f);
        return false;
    }
    fclose(f);

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)file_buf;
    if (file_size < (long)sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        fprintf(stderr, "pe_image_load: not an MZ file\n");
        free(file_buf);
        return false;
    }
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(file_buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        fprintf(stderr, "pe_image_load: not a PE32/x86 image\n");
        free(file_buf);
        return false;
    }

    void* want_base = (void*)(uintptr_t)nt->OptionalHeader.ImageBase;
    SIZE_T size_of_image = nt->OptionalHeader.SizeOfImage;

    // Our parent process (see relaunch_as_reserved_child in main.cpp)
    // already reserved this exact range via VirtualAllocEx before this
    // process's first thread ever ran - see that function's comment for
    // why user-mode code inside this process can never win that race on
    // this host. Committing an *already-reserved* range must be a
    // MEM_COMMIT-only call - MEM_RESERVE|MEM_COMMIT combined on memory
    // that isn't MEM_FREE fails with ERROR_INVALID_ADDRESS (measured). Try
    // COMMIT-only first (the expected case); fall back to the combined
    // RESERVE|COMMIT call for the case where nothing pre-reserved the range.
    //
    // TEMPORARY: everything RWX. The image has no .reloc so it must load at
    // ImageBase exactly; we don't yet split protections per-section (that
    // would need PAGE_EXECUTE_READ for .text/.rdata, PAGE_READWRITE for
    // .data/.bss) - fine for a first carrier, revisit once it runs cleanly.
    void* mapped = VirtualAlloc(want_base, size_of_image, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mapped) {
        mapped = VirtualAlloc(want_base, size_of_image,
                               MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!mapped) {
        fprintf(stderr,
            "pe_image_load: VirtualAlloc(%p, 0x%zx) FAILED (gle=%lu) - the "
            "guest image base is not free in this process.\n",
            want_base, (size_t)size_of_image, GetLastError());
        print_owner(want_base);
        free(file_buf);
        return false;
    }
    if (mapped != want_base) {
        fprintf(stderr,
            "pe_image_load: got %p instead of requested %p (image has no "
            ".reloc section, cannot run rebased) - freeing and failing.\n",
            mapped, want_base);
        VirtualFree(mapped, 0, MEM_RELEASE);
        free(file_buf);
        return false;
    }

    // Copy headers (everything up to SizeOfHeaders), then each section at
    // its mapped RVA. VirtualAlloc'd memory starts zero-filled, so any tail
    // past a section's raw data (.bss growth, or a shorter file image than
    // VirtualSize) is already zero - no explicit zero-fill pass is needed,
    // but we assert the intent here for anyone reading this later.
    memcpy(mapped, file_buf, nt->OptionalHeader.SizeOfHeaders);

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        unsigned char* dst = (unsigned char*)mapped + sec->VirtualAddress;
        if (sec->SizeOfRawData > 0 && sec->PointerToRawData > 0) {
            memcpy(dst, file_buf + sec->PointerToRawData, sec->SizeOfRawData);
        }
        // Bytes from SizeOfRawData..Misc.VirtualSize (the .bss tail, or any
        // section whose virtual size exceeds its file size) are already
        // zero courtesy of VirtualAlloc's fresh pages - see comment above.
    }

    // Extract everything we still need from `nt` (which points INTO
    // file_buf) before freeing file_buf - it's a large (~3.6 MB)
    // allocation, and free() on an allocation that size is typically
    // served straight back to the OS via VirtualFree(MEM_RELEASE),
    // unmapping it immediately. Reading through `nt` after free() here was
    // a genuine bug during bring-up (use-after-free -> access violation,
    // see carrier/NOTES.md) - keep entry-point extraction strictly before
    // the free() below.
    unsigned long entry_va = (unsigned long)(uintptr_t)mapped + nt->OptionalHeader.AddressOfEntryPoint;

    free(file_buf);

    out->base = (HMODULE)mapped;
    out->image_base = (unsigned long)(uintptr_t)mapped;
    out->size_of_image = (unsigned long)size_of_image;
    out->entry_va = entry_va;
    return true;
}
