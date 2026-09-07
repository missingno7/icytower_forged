// main.cpp - carrier entry point: parse args, map the guest PE image,
// resolve its imports, install diagnostics, switch to the guest stack, and
// jump into icytower15.exe's real entry point.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "pe_image.hpp"
#include "imports.hpp"
#include "trace.hpp"
#include "wrappers.hpp"
#include "symbols.hpp"
#include "import_types.hpp"
#include "det.hpp"

// KNOWN (measured, see carrier/NOTES.md): the guest's own CRT startup
// (___mingw_CRTStartup -> __getmainargs, both inside the REAL msvcrt.dll we
// LoadLibrary'd) calls GetCommandLineA() through msvcrt.dll's OWN import
// table - bound by the normal Windows loader when msvcrt.dll was loaded,
// entirely independent of icytower15.exe's IAT. wrap_GetCommandLineA (see
// wrappers.cpp) only intercepts calls made through the GUEST's *own* IAT
// (e.g. _main's direct GetCommandLineA call) - it never sees this one. The
// game's get_executable_name() uses __argv[0] (populated by __getmainargs
// from that unintercepted call), which - without a fix - resolves to
// carrier.exe's own real invocation command line, sending replace_filename
// + chdir to carrier\ instead of assets\ (confirmed: this exact bug once
// made the game chdir to the carrier directory and then fail to find
// data\loading.dat). A runtime patch of the PEB's ProcessParameters->
// CommandLine was tried and measured to NOT work: GetCommandLineA on this
// host is served from a cached ANSI copy kernel32/ucrtbase establish once,
// very early - too early for anything our own process does at runtime to
// preempt (the same structural race documented for the guest-image
// reservation above).
//
// The only fix that actually lands before that caching happens is to make
// the CHILD's real, OS-assigned command line already be what the guest
// expects, from CreateProcess itself - see relaunch_as_reserved_child.
// carrier's OWN flags therefore travel via environment variables (PF_*,
// see options_to_env/options_from_env) instead of argv in the child.

// KNOWN (notes/binary_recon.md): icytower15.exe's ImageBase/SizeOfImage.
// pe_image_load() re-derives the real values from the file's own header
// for the actual mapping; this hardcoded copy exists only so the range can
// be reserved before the file is even opened - see relaunch_as_reserved_child.
#define GUEST_IMAGE_BASE_HINT 0x400000ul
#define GUEST_IMAGE_SIZE_HINT 0x38c000ul

// ---------------------------------------------------------------------
// Shutdown plumbing: both the ExitProcess/exit/_cexit/abort wrappers (see
// wrappers.cpp, called from a guest thread) and the --run-seconds watchdog
// (a carrier thread) need to flush the trace/report exactly once and then
// really end the process. carrier_shutdown() does the flush (idempotent);
// each caller decides how to terminate afterward.
// ---------------------------------------------------------------------
static volatile LONG g_shutdown_once = 0;
static char g_report_path[MAX_PATH] = "";

static void carrier_shutdown(const char* reason) {
    if (InterlockedCompareExchange(&g_shutdown_once, 1, 0) != 0) return;
    fprintf(stderr, "carrier_shutdown: %s\n", reason);
    trace_write_report(g_report_path);
    trace_close();
    det_shutdown();
    fflush(stderr);
}

static DWORD g_run_seconds = 0;
static DWORD WINAPI watchdog_proc(LPVOID) {
    Sleep(g_run_seconds * 1000);
    carrier_shutdown("--run-seconds watchdog elapsed");
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}

// ---------------------------------------------------------------------
// Vectored exception handler: the primary "where did it die" diagnostic.
// ---------------------------------------------------------------------
static LONG WINAPI veh_handler(EXCEPTION_POINTERS* ep) {
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    CONTEXT* ctx = ep->ContextRecord;
    char where[192];
    symbols_describe((unsigned long)(uintptr_t)er->ExceptionAddress, where, sizeof(where));

    fprintf(stderr, "\n=== UNHANDLED EXCEPTION ===\n");
    fprintf(stderr, "code=0x%08lx addr=0x%08lx (%s)\n",
            er->ExceptionCode, (unsigned long)(uintptr_t)er->ExceptionAddress, where);
    fprintf(stderr,
        "EIP=0x%08lx EAX=0x%08lx EBX=0x%08lx ECX=0x%08lx EDX=0x%08lx "
        "ESI=0x%08lx EDI=0x%08lx EBP=0x%08lx ESP=0x%08lx EFLAGS=0x%08lx\n",
        ctx->Eip, ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx,
        ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp, ctx->EFlags);

    // Best-effort EBP-chain stack walk (mingw gcc 4.4.1 keeps frame pointers
    // by default at this optimization level per notes/binary_recon.md - not
    // guaranteed, hence the readability guard on every hop).
    fprintf(stderr, "stack walk (EBP chain):\n");
    unsigned long* ebp = (unsigned long*)(uintptr_t)ctx->Ebp;
    for (int i = 0; i < 32 && ebp; ++i) {
        if (IsBadReadPtr(ebp, 8)) {
            fprintf(stderr, "  #%d ebp=0x%08lx <unreadable, stopping>\n", i, (unsigned long)(uintptr_t)ebp);
            break;
        }
        unsigned long ret = ebp[1];
        char d[192];
        symbols_describe(ret, d, sizeof(d));
        fprintf(stderr, "  #%d ebp=0x%08lx ret=0x%08lx (%s)\n", i, (unsigned long)(uintptr_t)ebp, ret, d);
        unsigned long* next = (unsigned long*)(uintptr_t)ebp[0];
        if (next <= ebp) break; // guards against a corrupt/cyclic chain
        ebp = next;
    }
    fflush(stderr);

    carrier_shutdown("unhandled exception");
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_CONTINUE_SEARCH; // unreachable
}

// ---------------------------------------------------------------------
// Guest stack switch (--guest-stack=fixed, the default). TEMPORARY
// mechanism whose only purpose is a deterministic guest stack address -
// see pf_launch_guest_fixed's comment for the SEH/TEB caveat.
// ---------------------------------------------------------------------
#define GUEST_STACK_BASE ((void*)0x0e000000)
#define GUEST_STACK_SIZE (2 * 1024 * 1024)

extern "C" void __cdecl pf_on_guest_return() {
    fprintf(stderr, "FATAL: guest entry point returned (should never happen - "
                     "the mingw entry always calls ExitProcess).\n");
    carrier_shutdown("guest entry returned unexpectedly");
    TerminateProcess(GetCurrentProcess(), 1);
}

// Win32 SEH validates that exception-registration frames lie within the
// TEB's stack limits (fs:[4]=StackBase/high, fs:[8]=StackLimit/low) - this
// is exactly what fibers do to run on an alternate stack. We swap those two
// TEB fields to the guest stack's bounds, switch esp, and call the guest
// entry point. It never returns in practice (the mingw entry always ends in
// ExitProcess, intercepted by wrap_ExitProcess); the return path below is a
// diagnostic-only fallback, not a real unwind.
extern "C" void __declspec(naked) pf_launch_guest_fixed(
    void* entry, void* guest_esp, void* stack_base_high, void* stack_limit_low) {
    __asm {
        mov eax, [esp+4]        // entry
        mov ecx, [esp+8]        // guest_esp
        mov edx, [esp+12]       // stack_base_high
        push esi
        push edi
        mov esi, fs:[4]         // save host StackBase
        mov edi, fs:[8]         // save host StackLimit
        mov fs:[4], edx
        mov edx, [esp+16+8]     // stack_limit_low (esp shifted by the 2 pushes above)
        mov fs:[8], edx
        mov esp, ecx            // switch onto the guest stack
        call eax                 // call the guest entry point - does not return
        mov fs:[4], esi          // (unreachable in practice) restore host TEB bounds
        mov fs:[8], edi
        call pf_on_guest_return  // noreturn
        int 3
    }
}

// ---------------------------------------------------------------------
// Argument parsing.
// ---------------------------------------------------------------------
struct Options {
    char image[MAX_PATH];
    char cwd[MAX_PATH];
    char trace_imports[512]; // "all" | "none" | "Name1,Name2"
    char trace_out[MAX_PATH];
    char report[MAX_PATH];
    DDrawMode ddraw;
    bool count_imports;
    int run_seconds;
    bool guest_stack_fixed;
    // Milestones 5-7 (det.hpp) - see win32_pilot.md / carrier/NOTES.md.
    bool det_mode;
    bool pace_real;
    char input_script[MAX_PATH];
    char record_input[MAX_PATH];
    char digest_out[MAX_PATH];
    int stop_at_tick;
};

static void get_exe_dir(char* buf, size_t n) {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *slash = 0;
    strncpy(buf, path, n - 1);
    buf[n - 1] = 0;
}

static void get_parent_dir(const char* dir, char* buf, size_t n) {
    strncpy(buf, dir, n - 1);
    buf[n - 1] = 0;
    char* slash = strrchr(buf, '\\');
    if (slash) *slash = 0;
}

static void dirname_of(const char* path, char* buf, size_t n) {
    strncpy(buf, path, n - 1);
    buf[n - 1] = 0;
    char* slash = strrchr(buf, '\\');
    if (slash) *slash = 0; else buf[0] = 0;
}

// Default --image: <repo_root>\assets\icytower15.exe, where repo_root is
// carrier.exe's own parent directory (carrier.exe lives in <repo>\carrier\).
// Shared by parse_args (parent) and options_from_env (child fallback).
static void compute_default_image(char* buf, size_t n) {
    char exe_dir[MAX_PATH], repo_root[MAX_PATH];
    get_exe_dir(exe_dir, sizeof(exe_dir));
    get_parent_dir(exe_dir, repo_root, sizeof(repo_root));
    _snprintf(buf, n, "%s\\assets\\icytower15.exe", repo_root);
}

static void parse_args(int argc, char** argv, Options* o) {
    compute_default_image(o->image, sizeof(o->image));
    o->cwd[0] = 0; // resolved after --image is known, unless overridden
    strcpy(o->trace_imports, "none");
    o->trace_out[0] = 0;
    o->report[0] = 0;
    o->ddraw = DDrawMode::Local;
    o->count_imports = true;
    o->run_seconds = 0;
    o->guest_stack_fixed = true;
    o->det_mode = false;
    o->pace_real = false;
    o->input_script[0] = 0;
    o->record_input[0] = 0;
    o->digest_out[0] = 0;
    o->stop_at_tick = 0;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strncmp(arg, "--", 2) != 0) continue;
        const char* eq = strchr(arg, '=');
        char name[64];
        const char* value;
        char valbuf[MAX_PATH];
        if (eq) {
            size_t nlen = (size_t)(eq - arg - 2);
            if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
            memcpy(name, arg + 2, nlen);
            name[nlen] = 0;
            value = eq + 1;
        } else {
            strncpy(name, arg + 2, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            if (strcmp(name, "det") == 0) {
                value = "1"; // bare flag: --det (no value) means --det=1
            } else if (i + 1 < argc) {
                strncpy(valbuf, argv[++i], sizeof(valbuf) - 1);
                valbuf[sizeof(valbuf) - 1] = 0;
                value = valbuf;
            } else {
                value = "";
            }
        }

        if (strcmp(name, "image") == 0) { strncpy(o->image, value, sizeof(o->image) - 1); }
        else if (strcmp(name, "cwd") == 0) { strncpy(o->cwd, value, sizeof(o->cwd) - 1); }
        else if (strcmp(name, "trace-imports") == 0) { strncpy(o->trace_imports, value, sizeof(o->trace_imports) - 1); }
        else if (strcmp(name, "trace-out") == 0) { strncpy(o->trace_out, value, sizeof(o->trace_out) - 1); }
        else if (strcmp(name, "report") == 0) { strncpy(o->report, value, sizeof(o->report) - 1); }
        else if (strcmp(name, "ddraw") == 0) { o->ddraw = (_stricmp(value, "system") == 0) ? DDrawMode::System : DDrawMode::Local; }
        else if (strcmp(name, "count-imports") == 0) { o->count_imports = (_stricmp(value, "off") != 0 && _stricmp(value, "false") != 0); }
        else if (strcmp(name, "run-seconds") == 0) { o->run_seconds = atoi(value); }
        else if (strcmp(name, "guest-stack") == 0) { o->guest_stack_fixed = (_stricmp(value, "host") != 0); }
        else if (strcmp(name, "det") == 0) { o->det_mode = (_stricmp(value, "0") != 0 && _stricmp(value, "off") != 0 && _stricmp(value, "false") != 0); }
        else if (strcmp(name, "pace") == 0) { o->pace_real = (_stricmp(value, "real") == 0); }
        else if (strcmp(name, "input-script") == 0) { strncpy(o->input_script, value, sizeof(o->input_script) - 1); }
        else if (strcmp(name, "record-input") == 0) { strncpy(o->record_input, value, sizeof(o->record_input) - 1); }
        else if (strcmp(name, "digest-out") == 0) { strncpy(o->digest_out, value, sizeof(o->digest_out) - 1); }
        else if (strcmp(name, "stop-at-tick") == 0) { o->stop_at_tick = atoi(value); }
        else { fprintf(stderr, "warning: unknown option --%s\n", name); }
    }

    if (o->cwd[0] == 0) dirname_of(o->image, o->cwd, sizeof(o->cwd));
}

static void apply_trace_imports(const Options& o) {
    if (_stricmp(o.trace_imports, "all") == 0) {
        trace_enable_all();
        return;
    }
    if (_stricmp(o.trace_imports, "none") == 0 || o.trace_imports[0] == 0) return;

    char list[512];
    strncpy(list, o.trace_imports, sizeof(list) - 1);
    list[sizeof(list) - 1] = 0;
    char* tok = strtok(list, ",");
    while (tok) {
        for (int i = 0; i < kNumImports; ++i) {
            if (strcmp(g_import_table[i].name, tok) == 0) trace_enable_id(i);
        }
        tok = strtok(nullptr, ",");
    }
}

// carrier's own flags travel from parent to child via environment variables
// (inherited automatically - CreateProcessA's lpEnvironment is left null
// below) instead of argv, because the child's real OS-assigned command
// line is deliberately set to what the GUEST expects to see - see the
// comment above relaunch_as_reserved_child.
static void options_to_env(const Options& o) {
    SetEnvironmentVariableA("PF_CHILD", "1");
    SetEnvironmentVariableA("PF_IMAGE", o.image);
    SetEnvironmentVariableA("PF_CWD", o.cwd);
    SetEnvironmentVariableA("PF_TRACE_IMPORTS", o.trace_imports);
    SetEnvironmentVariableA("PF_TRACE_OUT", o.trace_out);
    SetEnvironmentVariableA("PF_REPORT", o.report);
    SetEnvironmentVariableA("PF_DDRAW", o.ddraw == DDrawMode::System ? "system" : "local");
    SetEnvironmentVariableA("PF_COUNT_IMPORTS", o.count_imports ? "1" : "0");
    char buf[16];
    _snprintf(buf, sizeof(buf), "%d", o.run_seconds);
    buf[sizeof(buf) - 1] = 0;
    SetEnvironmentVariableA("PF_RUN_SECONDS", buf);
    SetEnvironmentVariableA("PF_GUEST_STACK", o.guest_stack_fixed ? "fixed" : "host");
    SetEnvironmentVariableA("PF_DET", o.det_mode ? "1" : "0");
    SetEnvironmentVariableA("PF_PACE", o.pace_real ? "real" : "fast");
    SetEnvironmentVariableA("PF_INPUT_SCRIPT", o.input_script);
    SetEnvironmentVariableA("PF_RECORD_INPUT", o.record_input);
    SetEnvironmentVariableA("PF_DIGEST_OUT", o.digest_out);
    _snprintf(buf, sizeof(buf), "%d", o.stop_at_tick);
    buf[sizeof(buf) - 1] = 0;
    SetEnvironmentVariableA("PF_STOP_AT_TICK", buf);
}

static bool is_child_process() {
    char buf[8];
    return GetEnvironmentVariableA("PF_CHILD", buf, sizeof(buf)) > 0;
}

static void get_env_or(const char* name, char* out, size_t n, const char* fallback) {
    DWORD r = GetEnvironmentVariableA(name, out, (DWORD)n);
    if (r == 0 || r >= n) {
        strncpy(out, fallback, n - 1);
        out[n - 1] = 0;
    }
}

static void options_from_env(Options* o) {
    char default_image[MAX_PATH];
    compute_default_image(default_image, sizeof(default_image));
    get_env_or("PF_IMAGE", o->image, sizeof(o->image), default_image);
    char default_cwd[MAX_PATH];
    dirname_of(o->image, default_cwd, sizeof(default_cwd));
    get_env_or("PF_CWD", o->cwd, sizeof(o->cwd), default_cwd);
    get_env_or("PF_TRACE_IMPORTS", o->trace_imports, sizeof(o->trace_imports), "none");
    get_env_or("PF_TRACE_OUT", o->trace_out, sizeof(o->trace_out), "");
    get_env_or("PF_REPORT", o->report, sizeof(o->report), "");
    char ddraw_buf[16];
    get_env_or("PF_DDRAW", ddraw_buf, sizeof(ddraw_buf), "local");
    o->ddraw = (_stricmp(ddraw_buf, "system") == 0) ? DDrawMode::System : DDrawMode::Local;
    char count_buf[8];
    get_env_or("PF_COUNT_IMPORTS", count_buf, sizeof(count_buf), "1");
    o->count_imports = (strcmp(count_buf, "0") != 0);
    char run_buf[16];
    get_env_or("PF_RUN_SECONDS", run_buf, sizeof(run_buf), "0");
    o->run_seconds = atoi(run_buf);
    char stack_buf[16];
    get_env_or("PF_GUEST_STACK", stack_buf, sizeof(stack_buf), "fixed");
    o->guest_stack_fixed = (_stricmp(stack_buf, "host") != 0);

    char det_buf[8];
    get_env_or("PF_DET", det_buf, sizeof(det_buf), "0");
    o->det_mode = (strcmp(det_buf, "0") != 0);
    char pace_buf[8];
    get_env_or("PF_PACE", pace_buf, sizeof(pace_buf), "fast");
    o->pace_real = (_stricmp(pace_buf, "real") == 0);
    get_env_or("PF_INPUT_SCRIPT", o->input_script, sizeof(o->input_script), "");
    get_env_or("PF_RECORD_INPUT", o->record_input, sizeof(o->record_input), "");
    get_env_or("PF_DIGEST_OUT", o->digest_out, sizeof(o->digest_out), "");
    char stop_buf[16];
    get_env_or("PF_STOP_AT_TICK", stop_buf, sizeof(stop_buf), "0");
    o->stop_at_tick = atoi(stop_buf);
}

// TEMPORARY, structural: on this host, by the time ANY of our own code can
// run - even a custom linker /ENTRY point that preempts mainCRTStartup -
// ntdll has already memory-mapped several locale/codepage files (this
// system's locale is Czech/852: C_852.NLS, l_intl.nls, locale.nls) plus a
// handful of unnamed pagefile-backed sections into the low address range
// that carrier.exe's own image vacated (it links at /BASE:0x10000000, see
// build.cmd, specifically to free 0x400000-0x78c000 for the guest). That
// happens during the OS loader's own process bring-up, strictly before any
// entry point (default or custom) is invoked - measured empirically (see
// carrier/NOTES.md): both "reserve as the first line of main()" and a
// custom /ENTRY point that reserved before calling mainCRTStartup still
// lost the race. Evicting those mappings after the fact is unsafe once our
// own heap has grown into the same range (an earlier attempt corrupted the
// heap and crashed).
//
// The only reliable way to win this race is to never run any code in the
// target process before the reservation exists: create a suspended copy of
// ourselves, reserve the guest range in ITS address space from here (the
// parent) via VirtualAllocEx, then resume it. Nothing - not even ntdll's
// process-init - executes in a CREATE_SUSPENDED process until its first
// thread is resumed, so this cannot lose. The parent then just proxies the
// child's exit code. This is the standard "create-suspended /
// VirtualAllocEx / resume" bootstrap technique.
// `o` is already fully resolved (parsed from the parent's own argv). The
// child's real OS-assigned command line is set to the guest's own expected
// argv[0] (see the big comment above) - carrier's flags reach the child via
// environment variables instead (options_to_env/options_from_env), which
// CreateProcessA's default lpEnvironment=null inherits automatically.
static int relaunch_as_reserved_child(const Options& o) {
    options_to_env(o);

    char self_path[MAX_PATH];
    GetModuleFileNameA(nullptr, self_path, MAX_PATH);
    char cmdline[MAX_PATH + 2];
    _snprintf(cmdline, sizeof(cmdline), "\"%s\"", o.image);
    cmdline[sizeof(cmdline) - 1] = 0;

    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(self_path, cmdline, nullptr, nullptr, FALSE,
                         CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "relaunch_as_reserved_child: CreateProcessA FAILED, gle=%lu\n", GetLastError());
        return 1;
    }

    void* mem = VirtualAllocEx(pi.hProcess, (void*)GUEST_IMAGE_BASE_HINT, GUEST_IMAGE_SIZE_HINT,
                                MEM_RESERVE, PAGE_NOACCESS);
    if (mem != (void*)GUEST_IMAGE_BASE_HINT) {
        fprintf(stderr,
            "relaunch_as_reserved_child: VirtualAllocEx reservation FAILED (got %p, gle=%lu) - "
            "resuming anyway, pe_image_load will report the real conflict.\n",
            mem, GetLastError());
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

int main(int argc, char** argv) {
    if (!is_child_process()) {
        Options o;
        parse_args(argc, argv, &o);
        return relaunch_as_reserved_child(o);
    }

    // Belt-and-suspenders: the parent already reserved the guest range in
    // this process before our first thread ever ran. This should be a
    // trivial success (VirtualAlloc on our own existing reservation just
    // succeeds); kept as a fallback in case the parent's VirtualAllocEx
    // failed for some other reason.
    bool reserved = pe_image_reserve_guest_range(GUEST_IMAGE_BASE_HINT, GUEST_IMAGE_SIZE_HINT);

    Options o;
    options_from_env(&o);
    if (!reserved) {
        fprintf(stderr, "carrier: warning - could not pre-reserve the guest image range "
                         "at process start; pe_image_load may fail below.\n");
    }
    g_run_seconds = (DWORD)o.run_seconds;
    strncpy(g_report_path, o.report, sizeof(g_report_path) - 1);

    char exe_dir[MAX_PATH], repo_root[MAX_PATH], functions_json[MAX_PATH];
    get_exe_dir(exe_dir, sizeof(exe_dir));
    get_parent_dir(exe_dir, repo_root, sizeof(repo_root));
    _snprintf(functions_json, sizeof(functions_json), "%s\\artifacts\\functions.json", repo_root);
    symbols_load(functions_json);

    trace_init(o.trace_out[0] ? o.trace_out : nullptr);
    apply_trace_imports(o);

    AddVectoredExceptionHandler(1, veh_handler);
    // Registered AFTER veh_handler so it runs FIRST (AddVectoredExceptionHandler
    // prepends when FirstHandler=1): det_veh_handler claims our own hardware
    // breakpoints (EXCEPTION_SINGLE_STEP) and returns CONTINUE_SEARCH for
    // everything else, falling through to veh_handler's fatal-crash diagnostics
    // unchanged. See det.hpp.
    AddVectoredExceptionHandler(1, det_veh_handler);

    DetOptions det_opt;
    det_opt.det_mode = o.det_mode;
    det_opt.pace_real = o.pace_real;
    det_opt.input_script = o.input_script[0] ? o.input_script : nullptr;
    det_opt.record_input = o.record_input[0] ? o.record_input : nullptr;
    det_opt.digest_out = o.digest_out[0] ? o.digest_out : nullptr;
    det_opt.stop_at_tick = o.stop_at_tick;
    det_init(det_opt, carrier_shutdown);

    fprintf(stderr, "carrier: image=%s\n", o.image);
    fprintf(stderr, "carrier: cwd=%s\n", o.cwd);
    fprintf(stderr, "carrier: ddraw=%s count_imports=%d guest_stack=%s run_seconds=%d\n",
            o.ddraw == DDrawMode::Local ? "local" : "system", o.count_imports,
            o.guest_stack_fixed ? "fixed" : "host", o.run_seconds);

    PeImageInfo info;
    if (!pe_image_load(o.image, &info)) {
        fprintf(stderr, "carrier: failed to map guest image, aborting.\n");
        return 1;
    }
    fprintf(stderr, "carrier: mapped image_base=0x%08lx size=0x%08lx entry=0x%08lx\n",
            info.image_base, info.size_of_image, info.entry_va);

    if (!SetCurrentDirectoryA(o.cwd)) {
        fprintf(stderr, "carrier: SetCurrentDirectoryA('%s') failed, gle=%lu\n", o.cwd, GetLastError());
    }

    char assets_dir[MAX_PATH];
    dirname_of(o.image, assets_dir, sizeof(assets_dir));
    wrappers_set_guest_image_path(o.image);
    wrappers_set_carrier_hmodule(GetModuleHandleA(nullptr));
    wrappers_set_shutdown_hook(carrier_shutdown);

    ImportsConfig icfg;
    icfg.assets_dir = assets_dir;
    icfg.ddraw_mode = o.ddraw;
    icfg.count_imports = o.count_imports;
    if (!imports_init(icfg)) {
        fprintf(stderr, "carrier: some imports failed to resolve - continuing anyway "
                         "(the guest will crash if it actually calls one of them).\n");
    }
    det_arm_main_thread(); // no-op unless digest/record/stop-at-tick asked for a breakpoint

    HANDLE watchdog = nullptr;
    if (o.run_seconds > 0) {
        watchdog = CreateThread(nullptr, 0, watchdog_proc, nullptr, 0, nullptr);
    }

    fprintf(stderr, "carrier: entering guest at 0x%08lx (guest-stack=%s)\n",
            info.entry_va, o.guest_stack_fixed ? "fixed" : "host");
    fflush(stderr);

    if (o.guest_stack_fixed) {
        void* mem = VirtualAlloc(GUEST_STACK_BASE, GUEST_STACK_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!mem) {
            fprintf(stderr, "carrier: VirtualAlloc(guest stack @ %p) FAILED gle=%lu\n",
                    GUEST_STACK_BASE, GetLastError());
            return 1;
        }
        if (mem != GUEST_STACK_BASE) {
            fprintf(stderr, "carrier: guest stack landed at %p instead of requested %p, aborting.\n",
                    mem, GUEST_STACK_BASE);
            return 1;
        }
        void* top = (void*)((uintptr_t)mem + GUEST_STACK_SIZE - 16);
        void* base_high = (void*)((uintptr_t)mem + GUEST_STACK_SIZE);
        pf_launch_guest_fixed((void*)(uintptr_t)info.entry_va, top, base_high, mem);
    } else {
        typedef void (*EntryFn)();
        ((EntryFn)(void*)(uintptr_t)info.entry_va)();
    }

    // Unreachable: the guest always exits through wrap_ExitProcess.
    fprintf(stderr, "carrier: guest entry call returned to main() - unexpected.\n");
    carrier_shutdown("main() fallthrough");
    (void)watchdog;
    return 0;
}
