// print_globals.cpp - see print_globals.hpp. Walks the generated
// carrier/gen/it_print_globals.inc table (produced by
// carrier/gen/gen_print_globals.py from interop_index.json/it_types.h/
// it_types_check.c - the SAME generated interop metadata pf_inspect.py
// already decodes offline) to evaluate an arbitrary named-global expression
// against the GUEST's live memory at shutdown. No struct/global name is
// hard-coded in this file; everything it knows comes from the generated
// table.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include "print_globals.hpp"
#include "../gen/it_print_globals.inc"

namespace {

// Cheap committed-memory probe, same technique as bind.cpp's readable():
// a bad/uninitialized guest pointer produces a loud, printed marker instead
// of an access violation inside the printer.
bool readable(const void* p, size_t n) {
    static const char* cache_lo = nullptr;
    static const char* cache_hi = nullptr;
    const char* q = (const char*)p;
    if (!q) return false;
    if (q >= cache_lo && q + n <= cache_hi) return true;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    cache_lo = (const char*)mbi.BaseAddress;
    cache_hi = cache_lo + mbi.RegionSize;
    return q + n <= cache_hi;
}

const PfPGGlobal* find_global(const char* name) {
    for (int i = 0; i < kPGNumGlobals; ++i)
        if (strcmp(kPGGlobals[i].name, name) == 0) return &kPGGlobals[i];
    return nullptr;
}

const PfPGMember* find_member(const char* struct_name, const char* field) {
    for (int i = 0; i < kPGNumMembers; ++i)
        if (strcmp(kPGMembers[i].struct_name, struct_name) == 0 &&
            strcmp(kPGMembers[i].name, field) == 0)
            return &kPGMembers[i];
    return nullptr;
}

unsigned struct_size(const char* name) {
    for (int i = 0; i < kPGNumStructSizes; ++i)
        if (strcmp(kPGStructSizes[i].name, name) == 0) return kPGStructSizes[i].size;
    return 0;
}

unsigned elem_size(const PfPGVal& v) {
    if (v.ptr) return 4u;
    if (v.kind == 'S') return v.base_size;
    if (v.kind == 'P') return v.base_size;
    return 4u; // opaque fallback
}

// One evaluated location: the address a value of shape `v` lives at.
struct Loc { unsigned addr; PfPGVal v; bool ok; char err[128]; };

Loc fail(const char* fmt, const char* a) {
    Loc l; l.ok = false; l.addr = 0; memset(&l.v, 0, sizeof(l.v));
    _snprintf(l.err, sizeof(l.err), fmt, a); l.err[sizeof(l.err) - 1] = 0;
    return l;
}

// Reads a signed 32-bit int out of guest memory at `va` (used to resolve a
// `[global_name]` index). Returns false if unreadable or not a plausible
// integer-shaped global.
bool read_global_int(const char* name, int* out) {
    const PfPGGlobal* g = find_global(name);
    if (!g || g->v.kind != 'P' || g->v.ptr || g->v.ndims) return false;
    if (!readable((const void*)(uintptr_t)g->va, g->v.base_size)) return false;
    long long v = 0;
    memcpy(&v, (const void*)(uintptr_t)g->va, g->v.base_size < 8 ? g->v.base_size : 8);
    *out = (int)v;
    return true;
}

// Parses one `[...]` index token's contents: a decimal literal, or a bare
// global name (resolved by reading that global's live int value) - this is
// exactly what makes `ply[player_id]` generic rather than hard-coded.
bool resolve_index(const char* tok, int* out) {
    char* end = nullptr;
    long v = strtol(tok, &end, 10);
    if (end != tok && *end == 0) { *out = (int)v; return true; }
    return read_global_int(tok, out);
}

Loc do_index(const Loc& in, const char* idx_tok) {
    if (!in.ok) return in;
    int idx = 0;
    if (!resolve_index(idx_tok, &idx)) return fail("could not resolve index '%s'", idx_tok);
    Loc out = in;
    if (in.v.ndims >= 1) {
        // Direct array storage at `addr`: element N is addr + N*stride.
        PfPGVal elem = in.v;
        elem.ndims -= 1;
        if (elem.ndims >= 1) elem.dims[0] = elem.dims[1];
        elem.dims[elem.ndims] = 0;
        unsigned stride = elem_size(elem);
        out.addr = in.addr + (unsigned)idx * stride;
        out.v = elem;
        return out;
    }
    if (in.v.ptr > 0) {
        // Pointer: dereference then index the pointee (ptr[idx] shape).
        if (!readable((const void*)(uintptr_t)in.addr, 4)) return fail("unreadable pointer for '[%s]'", idx_tok);
        unsigned base = *(const unsigned*)(uintptr_t)in.addr;
        PfPGVal elem = in.v;
        elem.ptr -= 1;
        unsigned stride = elem_size(elem);
        out.addr = base + (unsigned)idx * stride;
        out.v = elem;
        return out;
    }
    return fail("'%s' is not an array or pointer, cannot index", idx_tok);
}

Loc do_member(const Loc& in, const char* field) {
    if (!in.ok) return in;
    unsigned base_addr = in.addr;
    PfPGVal cur = in.v;
    // Auto-dereference: a pointer-to-struct or a direct struct value both
    // reach `->field`/`.field` the same way here (ergonomic leniency - see
    // print_globals.hpp's comment).
    if (cur.ptr > 0) {
        if (!readable((const void*)(uintptr_t)base_addr, 4)) return fail("unreadable pointer before '.%s'", field);
        base_addr = *(const unsigned*)(uintptr_t)base_addr;
        cur.ptr -= 1;
    }
    if (cur.kind != 'S') return fail("not a struct before '.%s'", field);
    const PfPGMember* m = find_member(cur.base_name, field);
    if (!m) return fail("no such member '.%s'", field);
    Loc out;
    out.ok = true;
    out.addr = base_addr + m->offset;
    out.v = m->v;
    return out;
}

// ---------------------------------------------------------------------
// Value formatting - mirrors carrier/scripts/pf_inspect.py's format_value,
// with the same depth/array-preview limits, ported to C over the
// pre-resolved PfPGVal shape instead of a live type-string parse.
// ---------------------------------------------------------------------
void format_scalar(char* out, size_t n, const PfPGVal& v, unsigned addr) {
    if (!readable((const void*)(uintptr_t)addr, v.base_size)) {
        _snprintf(out, n, "<unreadable>"); out[n - 1] = 0; return;
    }
    const unsigned char* p = (const unsigned char*)(uintptr_t)addr;
    long long fixed_raw = 0; // only meaningful when v.is_fixed (32-bit signed)
    switch (v.prim_kind) {
    case 'F': { float f; memcpy(&f, p, 4); _snprintf(out, n, "%g", (double)f); break; }
    case 'D': {
        double d;
        if (v.base_size >= 8) memcpy(&d, p, 8); else { float f; memcpy(&f, p, 4); d = f; }
        _snprintf(out, n, "%g", d); break;
    }
    case 'U': {
        // Zero-initialized then partially overwritten by memcpy: correct
        // zero-extension on this (little-endian) target, no shifting needed.
        unsigned long long u = 0;
        memcpy(&u, p, v.base_size < 8 ? v.base_size : 8);
        _snprintf(out, n, "%llu", u); break;
    }
    default: { // 'I' (signed) and 'C' (plain char, signed on this compiler)
        long long s = 0;
        switch (v.base_size) {
        case 1: { signed char x; memcpy(&x, p, 1); s = x; break; }
        case 2: { short x; memcpy(&x, p, 2); s = x; break; }
        case 4: { int x; memcpy(&x, p, 4); s = x; fixed_raw = x; break; }
        default: { long long x = 0; memcpy(&x, p, v.base_size < 8 ? v.base_size : 8); s = x; break; }
        }
        _snprintf(out, n, "%lld", s); break;
    }
    }
    out[n - 1] = 0;
    if (v.is_fixed) {
        char tmp[64]; strncpy(tmp, out, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = 0;
        _snprintf(out, n, "%s (fixed 16.16 = %g)", tmp, (double)fixed_raw / 65536.0);
        out[n - 1] = 0;
    }
}

void format_value(char* out, size_t n, const PfPGVal& v, unsigned addr) {
    if (v.ndims >= 1) {
        PfPGVal elem = v; elem.ndims -= 1;
        if (elem.ndims >= 1) elem.dims[0] = elem.dims[1];
        int cnt = v.dims[0];
        // char[N] with no further dims prints as a bounded string.
        if (elem.ndims == 0 && elem.ptr == 0 && elem.kind == 'P' && elem.prim_kind == 'C' && !v.ptr) {
            int len = 0;
            char buf[121];
            while (len < 120 && len < cnt && readable((const void*)(uintptr_t)(addr + len), 1)) {
                char c = *(const char*)(uintptr_t)(addr + len);
                if (c == 0) break;
                buf[len++] = c;
            }
            buf[len] = 0;
            _snprintf(out, n, "\"%s\"", buf); out[n - 1] = 0;
            return;
        }
        unsigned stride = elem_size(elem);
        int shown = cnt < 8 ? cnt : 8;
        size_t off = 0;
        out[0] = '['; off = 1;
        for (int i = 0; i < shown && off + 4 < n; ++i) {
            char item[96];
            format_value(item, sizeof(item), elem, addr + i * stride);
            int w = _snprintf(out + off, n - off, "%s%s", i ? ", " : "", item);
            if (w > 0) off += (size_t)w;
        }
        _snprintf(out + off, n - off, "%s]", cnt > shown ? ", ..." : "");
        out[n - 1] = 0;
        return;
    }
    if (v.ptr > 0) {
        if (!readable((const void*)(uintptr_t)addr, 4)) { _snprintf(out, n, "<not in guest memory>"); out[n - 1] = 0; return; }
        unsigned p = *(const unsigned*)(uintptr_t)addr;
        if (v.kind == 'P' && v.prim_kind == 'C' && v.ptr == 1 && p) {
            char s[121]; int len = 0;
            while (len < 120 && readable((const void*)(uintptr_t)(p + len), 1)) {
                char c = *(const char*)(uintptr_t)(p + len);
                if (c == 0) break;
                s[len++] = c;
            }
            s[len] = 0;
            _snprintf(out, n, "0x%08x -> \"%s\"", p, s); out[n - 1] = 0;
            return;
        }
        _snprintf(out, n, "0x%08x", p); out[n - 1] = 0;
        return;
    }
    if (v.kind == 'P') { format_scalar(out, n, v, addr); return; }
    if (v.kind == 'S') {
        if (!readable((const void*)(uintptr_t)addr, 4)) { _snprintf(out, n, "<not in guest memory>"); out[n - 1] = 0; return; }
        _snprintf(out, n, "{struct %s @0x%08x, %u bytes - add ->field or .field to inspect a member}",
                  v.base_name, addr, v.base_size);
        out[n - 1] = 0;
        return;
    }
    _snprintf(out, n, "<opaque type '%s'>", v.base_name); out[n - 1] = 0;
}

// ---------------------------------------------------------------------
// Expression parsing: base_name ( '[' token ']' | ('->'|'.') field )*
// ---------------------------------------------------------------------
struct ParsedExpr { char text[192]; };

Loc eval_expr(const char* expr) {
    char buf[192];
    strncpy(buf, expr, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* p = buf;
    while (*p == ' ' || *p == '\t') ++p;
    char* start = p;
    while (isalnum((unsigned char)*p) || *p == '_') ++p;
    char save = *p; *p = 0;
    const PfPGGlobal* g = find_global(start);
    *p = save;
    if (!g) return fail("no such DWARF global '%s'", start);
    Loc cur; cur.ok = true; cur.addr = g->va; cur.v = g->v;

    while (*p) {
        if (*p == '[') {
            ++p;
            char* tstart = p;
            while (*p && *p != ']') ++p;
            if (*p != ']') return fail("unterminated '[' in '%s'", expr);
            char tok[64]; size_t len = (size_t)(p - tstart);
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, tstart, len); tok[len] = 0;
            ++p; // skip ']'
            cur = do_index(cur, tok);
        } else if (p[0] == '-' && p[1] == '>') {
            p += 2;
            char* fstart = p;
            while (isalnum((unsigned char)*p) || *p == '_') ++p;
            char ftok[64]; size_t len = (size_t)(p - fstart);
            if (len >= sizeof(ftok)) len = sizeof(ftok) - 1;
            memcpy(ftok, fstart, len); ftok[len] = 0;
            if (!len) return fail("expected a field name after '->' in '%s'", expr);
            cur = do_member(cur, ftok);
        } else if (p[0] == '.') {
            ++p;
            char* fstart = p;
            while (isalnum((unsigned char)*p) || *p == '_') ++p;
            char ftok[64]; size_t len = (size_t)(p - fstart);
            if (len >= sizeof(ftok)) len = sizeof(ftok) - 1;
            memcpy(ftok, fstart, len); ftok[len] = 0;
            if (!len) return fail("expected a field name after '.' in '%s'", expr);
            cur = do_member(cur, ftok);
        } else {
            return fail("unexpected character(s) '%s' in expression", p);
        }
        if (!cur.ok) return cur;
    }
    return cur;
}

char g_spec[1024] = "";

} // namespace

void print_globals_init(const char* spec) {
    if (!spec) { g_spec[0] = 0; return; }
    strncpy(g_spec, spec, sizeof(g_spec) - 1);
    g_spec[sizeof(g_spec) - 1] = 0;
}

static void for_each_expr(void (*fn)(const char* expr, const Loc& loc, const char* orig_type, void* ctx), void* ctx) {
    if (!g_spec[0]) return;
    char buf[1024];
    strncpy(buf, g_spec, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* tok = strtok(buf, ",");
    while (tok) {
        char* t = tok;
        while (*t == ' ' || *t == '\t') ++t;
        Loc loc = eval_expr(t);
        // Find the original declared type string for display, if this is a
        // plain top-level global (best-effort - a member-path expression
        // shows its final member's own orig_type instead, see below).
        const char* ot = "";
        const PfPGGlobal* g = find_global(t);
        if (g) ot = g->orig_type;
        fn(t, loc, ot, ctx);
        tok = strtok(nullptr, ",");
    }
}

void print_globals_run() {
    if (!g_spec[0]) return;
    struct Ctx {} ctx;
    for_each_expr([](const char* expr, const Loc& loc, const char* orig_type, void*) {
        if (!loc.ok) {
            printf("global %s = <error: %s>\n", expr, loc.err);
            return;
        }
        char val[256];
        format_value(val, sizeof(val), loc.v, loc.addr);
        printf("global %s = %s\n", expr, val);
        (void)orig_type;
    }, &ctx);
    fflush(stdout);
}

void print_globals_report_json(FILE* f) {
    if (!g_spec[0]) return;
    fprintf(f, "  \"print_globals\": [\n");
    bool first = true;
    struct Ctx { FILE* f; bool* first; } ctx{ f, &first };
    for_each_expr([](const char* expr, const Loc& loc, const char*, void* c) {
        Ctx* ctx = (Ctx*)c;
        if (!*ctx->first) fprintf(ctx->f, ",\n");
        *ctx->first = false;
        if (!loc.ok) {
            fprintf(ctx->f, "    { \"expr\": \"%s\", \"error\": \"%s\" }", expr, loc.err);
            return;
        }
        char val[256];
        format_value(val, sizeof(val), loc.v, loc.addr);
        // JSON-escape the (rare) case a formatted value contains a quote.
        char esc[512]; size_t o = 0;
        for (const char* c2 = val; *c2 && o + 2 < sizeof(esc); ++c2) {
            if (*c2 == '"' || *c2 == '\\') esc[o++] = '\\';
            esc[o++] = *c2;
        }
        esc[o] = 0;
        fprintf(ctx->f, "    { \"expr\": \"%s\", \"addr\": \"0x%08x\", \"value\": \"%s\" }",
                expr, loc.addr, esc);
    }, &ctx);
    fprintf(f, "\n  ],\n");
}
