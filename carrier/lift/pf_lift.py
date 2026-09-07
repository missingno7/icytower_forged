#!/usr/bin/env python3
"""pf_lift.py -- per-function x86-32 -> C lifter for the Icy Tower Win32 carrier.

Produces the LIFTED form (win32_pilot.md SS3) of one game function: a C file
defining `lifted_<name>` with EXACTLY the prototype the generated interop header
gives for that VA, so the carrier can bind it at the original address.

Design (win32_pilot.md SS3 "LIFTED form constraints"):
  * memory is the ORIGINAL memory: every memory operand becomes a plain typed
    access at the computed address, through PF_MEM(addr) which defaults to the
    identity.  No memory abstraction, no CPU struct.
  * registers are C locals; ESP/EBP are modelled symbolically (frame model),
    everything else is a uint32 local.
  * flags are computed only where consumed; the reaching flag definition is
    resolved by a dataflow pass and the condition is expanded exactly.
  * x87 is an 8-entry array of pf_x87_t plus a top index.  pf_x87_t is a
    typedef (currently `double`) -- see pf_rt.h; that typedef IS the hypothesis
    under test.
  * anything not proven is a hard refusal with address + mnemonic.  No silent
    fallback, ever.
"""

import argparse
import hashlib
import json
import os
import re
import sys

import pefile
import capstone
from capstone.x86 import (
    X86_OP_REG, X86_OP_IMM, X86_OP_MEM,
)

GEN = "pf_lift.py"

# --------------------------------------------------------------------------
# refusals
# --------------------------------------------------------------------------


class Refusal(Exception):
    def __init__(self, addr, mnem, why):
        self.addr, self.mnem, self.why = addr, mnem, why
        Exception.__init__(self, "REFUSED at 0x%08x (%s): %s" % (addr, mnem, why))


def refuse(ins, why):
    raise Refusal(ins.address if ins is not None else 0,
                  ("%s %s" % (ins.mnemonic, ins.op_str)).strip() if ins is not None else "-",
                  why)


# --------------------------------------------------------------------------
# register model
# --------------------------------------------------------------------------

R32 = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"]
R16 = {"ax": "eax", "cx": "ecx", "dx": "edx", "bx": "ebx",
       "sp": "esp", "bp": "ebp", "si": "esi", "di": "edi"}
R8L = {"al": "eax", "cl": "ecx", "dl": "edx", "bl": "ebx"}
R8H = {"ah": "eax", "ch": "ecx", "dh": "edx", "bh": "ebx"}
GPR = set(R32) - {"esp", "ebp"}          # registers that become plain locals
SYMREG = {"esp", "ebp"}                  # registers that are frame-symbolic only


def vname(r):
    return "r_" + r


def reg_parts(name):
    """-> (full32, kind) with kind in {32,16,'L','H'} or None if not a GPR."""
    if name in R32:
        return name, 32
    if name in R16:
        return R16[name], 16
    if name in R8L:
        return R8L[name], "L"
    if name in R8H:
        return R8H[name], "H"
    return None, None


# --------------------------------------------------------------------------
# interop header parsing
# --------------------------------------------------------------------------

TYPEDEF_RE = re.compile(
    r"typedef\s+(?P<ret>[^;]*?)\s*\(\s*__(?P<conv>cdecl|stdcall)\s*\*\s*"
    r"PFN_(?P<name>\w+)\s*\)\s*\((?P<args>.*?)\)\s*;", re.S)
VA_RE = re.compile(r"/\*\s*(?P<name>\w+)\s+VA=(?P<va>0x[0-9a-fA-F]+)\s+size=(?P<size>\d+)")


def split_params(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def load_protos(interop_dir):
    """Parse PFN_ typedefs out of every it_funcs*.h under interop_dir."""
    protos = {}
    for fn in ("it_funcs.h",):
        p = os.path.join(interop_dir, fn)
        if not os.path.exists(p):
            continue
        text = open(p, "r", errors="replace").read()
        vamap = {}
        for m in VA_RE.finditer(text):
            vamap[m.group("name")] = (int(m.group("va"), 16), int(m.group("size")))
        for m in TYPEDEF_RE.finditer(text):
            name = m.group("name")
            if name not in vamap:
                continue
            args = m.group("args").strip()
            params = [] if args in ("", "void") else split_params(args)
            protos[name] = {
                "name": name,
                "va": vamap[name][0],
                "size": vamap[name][1],
                "ret": m.group("ret").strip(),
                "conv": m.group("conv"),
                "params": params,
                "unspecified_params": args == "",
            }
    return protos


PTR_RE = re.compile(r"\*\s*$")
FLOAT_TYPES = {"float", "double"}
FOUR_BYTE_KEYWORDS = {"int", "unsigned", "signed", "long", "short", "char",
                      "float", "fixed", "_Bool", "size_t", "unsigned int",
                      "unsigned long", "unsigned char", "unsigned short"}


def type_size(t):
    """Stack slot size for a by-value cdecl parameter.  None = not proven."""
    t = t.strip()
    if "*" in t or t.endswith("]"):
        return 4
    base = t.replace("const", "").replace("volatile", "").strip()
    if base in ("double", "long long", "unsigned long long", "__int64", "long double"):
        return 8
    toks = set(base.split())
    if base in FOUR_BYTE_KEYWORDS or (toks and toks <= {"int", "unsigned", "signed",
                                                        "long", "short", "char",
                                                        "float", "fixed"}):
        return 4
    return None


def ret_kind(t):
    t = t.strip()
    if t == "void":
        return "void"
    if "*" in t:
        return "ptr"
    base = t.replace("const", "").replace("volatile", "").strip()
    if base in ("float", "double", "long double"):
        return "fp"
    if base in ("long long", "unsigned long long", "__int64"):
        return "i64"
    if type_size(base) == 4:
        return "i32"
    return None


# --------------------------------------------------------------------------
# import prototypes (call *0x514xxx)  -- small hand table, see README
# --------------------------------------------------------------------------

IMPORT_PROTOS = {
    "rand":   ("int", []),
    "srand":  ("void", ["unsigned int"]),
    "abs":    ("int", ["int"]),
    "sin":    ("double", ["double"]),
    "cos":    ("double", ["double"]),
    "tan":    ("double", ["double"]),
    "atan2":  ("double", ["double", "double"]),
    "sqrt":   ("double", ["double"]),
    "pow":    ("double", ["double", "double"]),
    "floor":  ("double", ["double"]),
    "ceil":   ("double", ["double"]),
    "fabs":   ("double", ["double"]),
    "memset": ("void *", ["void *", "int", "unsigned int"]),
    "memcpy": ("void *", ["void *", "const void *", "unsigned int"]),
    "strlen": ("unsigned int", ["const char *"]),
}


# --------------------------------------------------------------------------
# flag model
# --------------------------------------------------------------------------

class FlagDef(object):
    __slots__ = ("addr", "kind", "w", "nocf")

    def __init__(self, addr, kind, w, nocf=False):
        self.addr, self.kind, self.w, self.nocf = addr, kind, w, nocf

    def key(self):
        return (self.kind, self.w, self.nocf)

    # Value semantics are REQUIRED, not a nicety: flag_def_of() builds a fresh
    # object every time an instruction is visited, so with identity hashing the
    # reaching-definition sets never stop growing and prop_flags spins forever
    # on any function with a back edge.  (None of the four lifted functions has
    # one, which is why this only showed up when scanning all 253 game
    # functions.)  A definition is identified by its instruction address.
    def _id(self):
        return (self.addr, self.kind, self.w, self.nocf)

    def __eq__(self, other):
        return isinstance(other, FlagDef) and self._id() == other._id()

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self._id())


CC_NEEDS = {
    "e": "Z", "z": "Z", "ne": "Z", "nz": "Z",
    "s": "S", "ns": "S",
    "b": "C", "c": "C", "nae": "C", "ae": "C", "nb": "C", "nc": "C",
    "a": "CZ", "nbe": "CZ", "be": "CZ", "na": "CZ",
    "g": "ZSO", "nle": "ZSO", "le": "ZSO", "ng": "ZSO",
    "ge": "SO", "nl": "SO", "l": "SO", "nge": "SO",
}


def flag_exprs(fd, ins):
    """-> dict of flag name -> C expression, for this flag definition."""
    sb = fd.w - 1
    Z = "(pf_fres == 0u)"
    S = "((pf_fres >> %d) & 1u)" % sb
    if fd.kind == "sub":
        C = "(pf_fa < pf_fb)"
        O = "((((pf_fa ^ pf_fb) & (pf_fa ^ pf_fres)) >> %d) & 1u)" % sb
    elif fd.kind == "add":
        C = "(pf_fres < pf_fa)"
        O = "((((pf_fa ^ pf_fres) & (pf_fb ^ pf_fres)) >> %d) & 1u)" % sb
    elif fd.kind == "logic":
        C, O = "0u", "0u"
    elif fd.kind == "sahf":
        return {"Z": "((pf_fa >> 6) & 1u)", "S": "((pf_fa >> 7) & 1u)",
                "C": "(pf_fa & 1u)", "O": None}
    elif fd.kind == "fcomi":
        return {"Z": "((pf_fa >> 6) & 1u)", "S": None,
                "C": "(pf_fa & 1u)", "O": None}
    elif fd.kind == "muls":
        # IMUL/MUL set CF = OF = "the upper half is not a sign/zero extension
        # of the lower half"; SF/ZF are architecturally undefined.
        return {"Z": None, "S": None, "C": "(pf_fa & 1u)", "O": "(pf_fa & 1u)"}
    else:
        return {"Z": None, "S": None, "C": None, "O": None}
    if fd.nocf:
        C = None
    return {"Z": Z, "S": S, "C": C, "O": O}


def cond_expr(fd, cc, ins):
    need = CC_NEEDS.get(cc)
    if need is None:
        refuse(ins, "condition code '%s' not supported" % cc)
    fe = flag_exprs(fd, ins)
    for f in need:
        if fe.get(f) is None:
            refuse(ins, "condition '%s' needs %s, undefined for flag-def kind '%s' at 0x%08x"
                   % (cc, f, fd.kind, fd.addr))
    Z, S, C, O = fe["Z"], fe["S"], fe["C"], fe["O"]
    if cc in ("e", "z"):
        return Z
    if cc in ("ne", "nz"):
        return "!" + Z
    if cc == "s":
        return S
    if cc == "ns":
        return "!" + S
    if cc in ("b", "c", "nae"):
        return C
    if cc in ("ae", "nb", "nc"):
        return "!" + C
    if cc in ("a", "nbe"):
        return "(!%s && !%s)" % (C, Z)
    if cc in ("be", "na"):
        return "(%s || %s)" % (C, Z)
    if cc in ("g", "nle"):
        return "(!%s && (%s) == (%s))" % (Z, S, O)
    if cc in ("le", "ng"):
        return "(%s || (%s) != (%s))" % (Z, S, O)
    if cc in ("ge", "nl"):
        return "((%s) == (%s))" % (S, O)
    if cc in ("l", "nge"):
        return "((%s) != (%s))" % (S, O)
    refuse(ins, "condition code '%s' not supported" % cc)


# --------------------------------------------------------------------------
# lifter
# --------------------------------------------------------------------------

class Lifter(object):

    def __init__(self, image, base, func, protos, iat, globals_idx, sha):
        self.image, self.base = image, base
        self.func = func
        self.protos = protos
        self.iat = iat                  # va -> import name
        self.globals_idx = globals_idx  # sorted [(va, name, type)]
        self.sha = sha
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.ins = {}                   # addr -> insn
        self.order = []
        self.used = set()
        self.mem_hits = []
        self.ext_calls = []
        self.x87 = False
        self.has_fldcw = False
        self.x87_arith_count = 0
        self.x87_fistp_count = 0
        self.frame_lo = 0
        self.argbytes = 0

    # -- decoding ----------------------------------------------------------

    def decode(self):
        va, size = self.func["va"], self.func["size"]
        code = self.image[va - self.base: va - self.base + size]
        cur = va
        while cur < va + size:
            got = list(self.md.disasm(code[cur - va: cur - va + 16], cur, 1))
            if not got:
                raise Refusal(cur, "??", "capstone could not decode")
            i = got[0]
            if i.address + i.size > va + size:
                raise Refusal(i.address, i.mnemonic, "instruction crosses the function end")
            self.ins[i.address] = i
            self.order.append(i.address)
            cur += i.size
        # A function that reloads the FPU control word can reach a rounding x87
        # operation with a non-default PC/RC; pf_lift then guards every such
        # operation at run time (see cw_guard).
        self.has_fldcw = any(
            len(x.bytes) >= 2 and x.bytes[0] == 0xD9 and x.bytes[1] < 0xC0
            and ((x.bytes[1] >> 3) & 7) == 5 for x in self.ins.values())

    # -- control flow ------------------------------------------------------

    def targets(self, i):
        """-> (list_of_branch_targets, falls_through, is_terminator)"""
        m = i.mnemonic
        if m == "jmp":
            if i.operands and i.operands[0].type == X86_OP_IMM:
                return [i.operands[0].imm], False, True
            refuse(i, "indirect jmp / switch table -- out of scope")
        if m.startswith("j"):
            if i.operands and i.operands[0].type == X86_OP_IMM:
                return [i.operands[0].imm], True, True
            refuse(i, "indirect conditional branch")
        if m in ("ret", "retn", "retf", "iret"):
            return [], False, True
        if m in ("loop", "loope", "loopne", "jecxz", "jcxz"):
            refuse(i, "loop/jecxz not supported")
        return [], True, False

    def build_blocks(self):
        leaders = {self.func["va"]}
        for a in self.order:
            i = self.ins[a]
            tg, ft, term = self.targets(i)
            for t in tg:
                if t not in self.ins:
                    refuse(i, "branch target 0x%08x outside the function" % t)
                leaders.add(t)
            if term:
                nxt = a + i.size
                if nxt in self.ins:
                    leaders.add(nxt)
        self.leaders = sorted(leaders)
        self.block_of = {}
        self.blocks = {}
        cur = None
        for a in self.order:
            if a in leaders:
                cur = a
                self.blocks[cur] = []
            self.block_of[a] = cur
            self.blocks[cur].append(a)
        # successors
        self.succ = {}
        for b, body in self.blocks.items():
            last = self.ins[body[-1]]
            tg, ft, term = self.targets(last)
            s = list(tg)
            if ft:
                n = body[-1] + last.size
                if n in self.ins:
                    s.append(n)
                elif not term:
                    refuse(last, "falls off the end of the function")
            self.succ[b] = s
        # reachability
        seen, stack = set(), [self.func["va"]]
        while stack:
            b = stack.pop()
            if b in seen:
                continue
            seen.add(b)
            stack.extend(self.succ[b])
        self.reach = seen
        self.emit_order = [b for b in self.leaders if b in seen]

    # -- esp/ebp frame propagation ----------------------------------------

    def prop_frame(self):
        entry = self.func["va"]
        state = {entry: (0, None, None)}      # esp_off, ebp_off, saved_ebp_off
        work = [entry]
        self.blk_state = {}
        esp_min = 0
        while work:
            b = work.pop()
            st = state[b]
            self.blk_state[b] = st
            cur = st
            for a in self.blocks[b]:
                cur = self.frame_step(self.ins[a], cur)
                # the deepest point may be reached INSIDE a block (an outgoing
                # `push` between two calls), not only at a block entry.
                if cur[0] < esp_min:
                    esp_min = cur[0]
            for s in self.succ[b]:
                if s in state:
                    if state[s] != cur:
                        refuse(self.ins[self.blocks[s][0]],
                               "inconsistent stack state at block entry: %r vs %r"
                               % (state[s], cur))
                else:
                    state[s] = cur
                    work.append(s)
        self.frame_lo = min([0, esp_min] + [v[0] for v in state.values()])

    def frame_step(self, i, st):
        """Advance (esp_off, ebp_off, saved_ebp_off) over instruction i."""
        esp, ebp, sav = st
        m, ops = i.mnemonic, i.operands
        if m == "push":
            esp -= 4
            if ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == "ebp":
                if ebp is not None:
                    refuse(i, "second 'push ebp' -- frame model does not cover it")
                sav = esp
        elif m == "pop":
            if ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == "ebp":
                if sav is None or esp != sav:
                    refuse(i, "'pop ebp' does not match the saved-ebp slot")
                ebp, sav = None, None
            esp += 4
        elif m == "leave":
            if ebp is None or sav is None:
                refuse(i, "'leave' without a modelled frame")
            esp = ebp
            if esp != sav:
                refuse(i, "'leave' lands on 0x%x, saved ebp at 0x%x" % (esp, sav))
            ebp, sav = None, None
            esp += 4
        elif m == "mov" and ops[0].type == X86_OP_REG and \
                i.reg_name(ops[0].reg) == "ebp" and ops[1].type == X86_OP_REG and \
                i.reg_name(ops[1].reg) == "esp":
            ebp = esp
        elif m in ("sub", "add") and ops[0].type == X86_OP_REG and \
                i.reg_name(ops[0].reg) == "esp":
            if ops[1].type != X86_OP_IMM:
                refuse(i, "non-immediate adjustment of esp")
            esp += (-ops[1].imm if m == "sub" else ops[1].imm)
        elif m in ("ret", "retn"):
            if esp != 0:
                refuse(i, "'ret' with esp offset %d (unbalanced frame)" % esp)
        else:
            # any *other* use of esp/ebp must be a memory base, checked in emit
            for o in ops:
                if o.type == X86_OP_REG and i.reg_name(o.reg) in SYMREG:
                    refuse(i, "unsupported use of %s as a value" % i.reg_name(o.reg))
        return (esp, ebp, sav)

    # -- flag dataflow -----------------------------------------------------

    def flag_def_of(self, i):
        m = i.mnemonic
        w = (i.operands[0].size * 8) if i.operands else 32
        if m == "add":
            return FlagDef(i.address, "add", w)
        if m in ("sub", "cmp"):
            return FlagDef(i.address, "sub", w)
        if m in ("adc", "sbb"):
            # the VALUE is modelled exactly (carry-in comes from the reaching
            # definition); the flags ADC/SBB produce are not, so consuming them
            # is a refusal rather than an approximation.
            return FlagDef(i.address, "undef", w)
        if m == "neg":
            return FlagDef(i.address, "sub", w)
        if m in ("and", "or", "xor", "test"):
            return FlagDef(i.address, "logic", w)
        if m == "inc":
            return FlagDef(i.address, "add", w, nocf=True)
        if m == "dec":
            return FlagDef(i.address, "sub", w, nocf=True)
        if m == "sahf":
            return FlagDef(i.address, "sahf", 32)
        if m in ("fcomi", "fcomip", "fucomi", "fucomip"):
            return FlagDef(i.address, "fcomi", 32)
        if m in ("imul", "mul"):
            return FlagDef(i.address, "muls", w)
        # NOT does not touch EFLAGS; everything below leaves at least one
        # consumed flag undefined, so consuming them is a refusal.
        if m in ("shl", "sal", "shr", "sar", "rol", "ror",
                 "idiv", "div", "bt", "bts"):
            return FlagDef(i.address, "undef", w)
        return None

    def flag_use_of(self, i):
        m = i.mnemonic
        if m in ("adc", "sbb"):
            return "c"                      # consumes CF only
        if m.startswith("j") and m not in ("jmp",):
            return m[1:]
        if m.startswith("set"):
            return m[3:]
        if m.startswith("cmov"):
            return m[4:]
        return None

    def prop_flags(self):
        entry = self.func["va"]
        inset = {entry: frozenset()}
        work = [entry]
        while work:
            b = work.pop()
            cur = set(inset[b])
            for a in self.blocks[b]:
                fd = self.flag_def_of(self.ins[a])
                if fd is not None:
                    cur = {fd}
            cur = frozenset(cur)
            for s in self.succ[b]:
                nxt = inset.get(s, frozenset()) | cur
                if s not in inset or nxt != inset[s]:
                    inset[s] = nxt
                    work.append(s)
        # resolve at consumers
        self.reaching = {}      # consumer addr -> FlagDef
        self.needed = set()     # addrs of flag defs that must be materialised
        for b in self.emit_order:
            cur = set(inset.get(b, frozenset()))
            for a in self.blocks[b]:
                i = self.ins[a]
                cc = self.flag_use_of(i)
                if cc is not None:
                    if not cur:
                        refuse(i, "no reaching flag definition")
                    keys = set(fd.key() for fd in cur)
                    if len(keys) != 1:
                        refuse(i, "conflicting reaching flag definitions %r" % (keys,))
                    fd = sorted(cur, key=lambda d: d.addr)[0]
                    self.reaching[a] = fd
                    for d in cur:
                        self.needed.add(d.addr)
                fd2 = self.flag_def_of(i)
                if fd2 is not None:
                    cur = {fd2}

    # -- emission helpers --------------------------------------------------

    def U(self, n):
        self.used.add(n)
        return n

    def mask(self, w):
        return {8: " & 0xFFu", 16: " & 0xFFFFu", 32: ""}[w]

    def reg_read(self, i, name):
        full, kind = reg_parts(name)
        if full is None:
            refuse(i, "unsupported register '%s'" % name)
        if full in SYMREG:
            refuse(i, "read of frame-symbolic register '%s'" % name)
        v = self.U(vname(full))
        if kind == 32:
            return v
        if kind == 16:
            return "PF_GET16(%s)" % v
        if kind == "L":
            return "PF_GET8L(%s)" % v
        return "PF_GET8H(%s)" % v

    def reg_write(self, i, name, expr):
        full, kind = reg_parts(name)
        if full is None or full in SYMREG:
            refuse(i, "unsupported register destination '%s'" % name)
        v = self.U(vname(full))
        if kind == 32:
            return "%s = %s;" % (v, expr)
        if kind == 16:
            return "PF_SET16(%s, %s);" % (v, expr)
        if kind == "L":
            return "PF_SET8L(%s, %s);" % (v, expr)
        return "PF_SET8H(%s, %s);" % (v, expr)

    def sym_for(self, addr):
        best = None
        for va, nm, ty in self.globals_idx:
            if va <= addr:
                best = (va, nm, ty)
            else:
                break
        if best is None or addr - best[0] > 4096:
            return None
        return "%s%s" % (best[1], "" if addr == best[0] else "+%d" % (addr - best[0]))

    def mem_ea(self, i, o, st):
        """-> ('frame', off) | ('abs', c_expr, const_base_or_None)"""
        mm = o.mem
        if mm.segment != 0:
            refuse(i, "segment override")
        bname = i.reg_name(mm.base) if mm.base else None
        iname = i.reg_name(mm.index) if mm.index else None
        disp = mm.disp
        if bname in SYMREG:
            if iname is not None:
                refuse(i, "indexed access through %s" % bname)
            esp, ebp, sav = st
            off = (esp if bname == "esp" else ebp)
            if off is None:
                refuse(i, "frame base %s has no modelled offset" % bname)
            return ("frame", off + disp)
        if iname in SYMREG:
            refuse(i, "%s used as an index register" % iname)
        terms = []
        if bname:
            terms.append(self.reg_read(i, bname))
        if iname:
            terms.append("%s * %du" % (self.reg_read(i, iname), mm.scale)
                         if mm.scale != 1 else self.reg_read(i, iname))
        cbase = None
        if disp or not terms:
            cbase = disp & 0xFFFFFFFF
            terms.append("0x%xu" % cbase)
        expr = "(unsigned int)(%s)" % " + ".join(terms)
        return ("abs", expr, cbase)

    def mem_setup(self, i, o, st):
        """Emit the address computation; return an accessor prefix tuple."""
        ea = self.mem_ea(i, o, st)
        if ea[0] == "frame":
            off = ea[1]
            if off >= 4:
                k = off - 4
                if k + o.size > self.argbytes:
                    refuse(i, "argument slot at frame offset %d is outside the "
                              "prototype's %d argument bytes" % (off, self.argbytes))
                return ("arg", k)
            if off >= 0:
                refuse(i, "access to the return address slot")
            if off < self.frame_lo:
                refuse(i, "frame access below the modelled frame")
            return ("stk", off - self.frame_lo)
        disp = ea[2]
        inimg = disp is not None and 0x400000 <= disp < 0x800000
        self.mem_hits.append({
            "at": "0x%08x" % i.address, "size": o.size,
            "kind": "absolute" if o.mem.base == 0 else "register-relative",
            "address": ("0x%08x" % disp) if inimg else None,
            "displacement": ("0x%x" % disp) if (disp is not None and not inimg) else None,
            "symbol": self.sym_for(disp) if inimg else None,
            "expr": ea[1]})
        self.body.append("    %s = %s;" % (self.U("pf_ea"), ea[1]))
        return ("mem", None)

    def mem_read(self, acc, size, fp=False):
        kind, k = acc
        sfx = {1: "8", 2: "16", 4: "32", 8: "64"}[size]
        if fp:
            sfx = {4: "F32", 8: "F64"}[size]
        if kind == "arg":
            return "PF_A%s(%d)" % (sfx, k)
        if kind == "stk":
            return "PF_S%s(%d)" % (sfx, k)
        return "PF_R%s(pf_ea)" % sfx

    def mem_write(self, i, acc, size, expr, fp=False):
        kind, k = acc
        sfx = {1: "8", 2: "16", 4: "32", 8: "64"}[size]
        if fp == "bits":
            sfx = {4: "B32", 8: "B64"}[size]
        elif fp:
            sfx = {4: "F32", 8: "F64"}[size]
        if kind == "arg":
            refuse(i, "write to an incoming argument slot -- refused (the caller's "
                      "copy would not be updated)")
        if kind == "stk":
            return "PF_SW%s(%d, %s);" % (sfx, k, expr)
        return "PF_W%s(pf_ea, %s);" % (sfx, expr)

    def src_read(self, i, o, st, fp=False):
        if o.type == X86_OP_REG:
            return self.reg_read(i, i.reg_name(o.reg))
        if o.type == X86_OP_IMM:
            return "0x%xu" % (o.imm & ((1 << (o.size * 8)) - 1))
        if o.type == X86_OP_MEM:
            acc = self.mem_setup(i, o, st)
            return self.mem_read(acc, o.size, fp)
        refuse(i, "unsupported operand type %d" % o.type)

    # -- main emission -----------------------------------------------------

    def emit(self):
        """Emit blocks in address order.  Every fall-through is made an explicit
        `goto`, so the emitted control flow does not depend on layout; a label is
        emitted only where something jumps to it (MSVC C4102 otherwise)."""
        self.body = []
        targeted = set()
        for b in self.emit_order:
            last = self.ins[self.blocks[b][-1]]
            tg, ft, term = self.targets(last)
            targeted.update(tg)
            if ft:
                targeted.add(self.blocks[b][-1] + last.size)
        for b in self.emit_order:
            st = self.blk_state[b]
            if b in targeted:
                self.body.append("L_%08x:" % b)
                self.body.append("    ;")
            else:
                self.body.append("    /* block 0x%08x */" % b)
            for a in self.blocks[b]:
                i = self.ins[a]
                self.body.append("    /* %08x  %-22s %s %s */"
                                 % (a, i.bytes.hex(), i.mnemonic, i.op_str))
                self.emit_ins(i, st)
                st = self.frame_step(i, st)
            last = self.ins[self.blocks[b][-1]]
            tg, ft, term = self.targets(last)
            if ft:
                nxt = self.blocks[b][-1] + last.size
                if nxt not in self.ins:
                    refuse(last, "falls off the end of the function")
                self.body.append("    goto L_%08x;" % nxt)
        return self.body

    def emit_flagdef(self, i, kind, a, bexpr, res, w):
        """Materialise pf_fa/pf_fb/pf_fres if this definition is consumed."""
        if i.address not in self.needed:
            return res
        m = self.mask(w)
        if kind in ("sub", "add"):
            self.body.append("    %s = %s;" % (self.U("pf_fa"), a))
            self.body.append("    %s = %s;" % (self.U("pf_fb"), bexpr))
            self.body.append("    %s = (pf_fa %s pf_fb)%s;"
                             % (self.U("pf_fres"), "-" if kind == "sub" else "+", m))
        elif kind == "logic":
            self.body.append("    %s = %s;" % (self.U("pf_fres"), res))
        elif kind in ("sahf", "fcomi"):
            self.body.append("    %s = %s;" % (self.U("pf_fa"), a))
        return "pf_fres" if kind in ("sub", "add", "logic") else res

    def emit_ins(self, i, st):
        m = i.mnemonic
        ops = i.operands
        E = self.body.append

        if m in ("nop", "fnop"):
            return
        if m in ("push", "pop", "leave") or \
           (m == "mov" and ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == "ebp"
                and ops[1].type == X86_OP_REG and i.reg_name(ops[1].reg) == "esp") or \
           (m in ("sub", "add") and ops and ops[0].type == X86_OP_REG
                and i.reg_name(ops[0].reg) == "esp"):
            return self.emit_stack(i, st)
        if m == "ret" or m == "retn":
            return self.emit_ret(i)
        if m == "mov":
            return self.emit_mov(i, st)
        if m in ("movzx", "movsx"):
            return self.emit_movx(i, st)
        if m == "lea":
            return self.emit_lea(i, st)
        if m in ("add", "sub", "and", "or", "xor", "cmp", "test", "adc", "sbb"):
            return self.emit_alu(i, st)
        if m in ("imul", "mul"):
            return self.emit_mul(i, st)
        if m in ("inc", "dec", "neg", "not"):
            return self.emit_un(i, st)
        if m in ("shl", "sal", "shr", "sar"):
            return self.emit_shift(i, st)
        if m == "cdq":
            E("    %s = (%s & 0x80000000u) ? 0xFFFFFFFFu : 0u;"
              % (self.U("r_edx"), self.U("r_eax")))
            return
        if m == "cwde":
            E("    %s = (unsigned int)(int)(short)(%s & 0xFFFFu);"
              % (self.U("r_eax"), self.U("r_eax")))
            return
        if m in ("idiv", "div"):
            return self.emit_div(i, st)
        if m.startswith("set"):
            fd = self.reaching[i.address]
            expr = cond_expr(fd, m[3:], i)
            if ops[0].type == X86_OP_REG:
                E("    " + self.reg_write(i, i.reg_name(ops[0].reg),
                                          "(%s) ? 1u : 0u" % expr))
            else:
                acc = self.mem_setup(i, ops[0], st)
                E("    " + self.mem_write(i, acc, 1, "(%s) ? 1u : 0u" % expr))
            return
        if m == "jmp":
            E("    goto L_%08x;" % ops[0].imm)
            return
        if m.startswith("j"):
            fd = self.reaching[i.address]
            E("    if (%s) goto L_%08x;" % (cond_expr(fd, m[1:], i), ops[0].imm))
            return
        if m == "sahf":
            self.emit_flagdef(i, "sahf", "PF_GET8H(%s)" % self.U("r_eax"), None, None, 32)
            return
        if m == "call":
            return self.emit_call(i, st)
        if m[0] == "f":
            return self.emit_x87(i, st)
        refuse(i, "instruction not supported by pf_lift")

    def emit_stack(self, i, st):
        m, ops = i.mnemonic, i.operands
        esp, ebp, sav = st
        if m == "push":
            if ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == "ebp":
                self.body.append("    /* frame: saved ebp (symbolic, not stored) */")
                return
            if ops[0].size != 4:
                refuse(i, "non-dword push")
            v = self.src_read(i, ops[0], st)
            off = esp - 4
            if off < self.frame_lo:
                refuse(i, "push below the modelled frame")
            self.body.append("    PF_SW32(%d, %s);" % (off - self.frame_lo, v))
            return
        if m == "pop":
            if ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == "ebp":
                self.body.append("    /* frame: restore ebp (symbolic) */")
                return
            if ops[0].type != X86_OP_REG or ops[0].size != 4:
                refuse(i, "unsupported pop operand")
            self.body.append("    " + self.reg_write(i, i.reg_name(ops[0].reg),
                                                     "PF_S32(%d)" % (esp - self.frame_lo)))
            return
        # leave / esp adjustment: purely symbolic
        self.body.append("    /* frame: %s (symbolic) */" % m)

    def emit_ret(self, i):
        rk = self.rkind
        if rk == "void":
            self.body.append("    return;")
        elif rk == "i32":
            self.body.append("    return (%s)%s;" % (self.rtype, self.U("r_eax")))
        elif rk == "ptr":
            self.body.append("    return (%s)(size_t)%s;" % (self.rtype, self.U("r_eax")))
        elif rk == "i64":
            self.body.append("    return (%s)(((unsigned long long)%s << 32) | %s);"
                             % (self.rtype, self.U("r_edx"), self.U("r_eax")))
        elif rk == "fp":
            self.body.append("    return (%s)PF_TOF64(PF_ST(0));" % self.rtype)
        else:
            refuse(i, "return type '%s' not supported" % self.rtype)

    def emit_mov(self, i, st):
        ops = i.operands
        src = self.src_read(i, ops[1], st)
        if ops[0].type == X86_OP_REG:
            self.body.append("    " + self.reg_write(i, i.reg_name(ops[0].reg), src))
        elif ops[0].type == X86_OP_MEM:
            acc = self.mem_setup(i, ops[0], st)
            self.body.append("    " + self.mem_write(i, acc, ops[0].size, src))
        else:
            refuse(i, "unsupported mov destination")

    def emit_movx(self, i, st):
        ops = i.operands
        src = self.src_read(i, ops[1], st)
        w = ops[1].size * 8
        if i.mnemonic == "movsx":
            src = "(unsigned int)(int)(%s)((%s)%s)" % (
                {8: "signed char", 16: "short"}[w], {8: "unsigned char", 16: "unsigned short"}[w], src)
        self.body.append("    " + self.reg_write(i, i.reg_name(ops[0].reg), src))

    def emit_lea(self, i, st):
        ops = i.operands
        ea = self.mem_ea(i, ops[1], st)
        if ea[0] == "frame":
            refuse(i, "lea of a frame address -- the frame is not at its original address")
        self.body.append("    " + self.reg_write(i, i.reg_name(ops[0].reg), ea[1]))

    def emit_alu(self, i, st):
        m, ops = i.mnemonic, i.operands
        w = ops[0].size * 8
        msk = self.mask(w)
        cop = {"add": "+", "sub": "-", "and": "&", "or": "|", "xor": "^",
               "cmp": "-", "test": "&", "adc": "+", "sbb": "-"}[m]
        kind = self.flag_def_of(i).kind
        cin = None
        if m in ("adc", "sbb"):
            # UNVERIFIED PATH: no lifted function so far contains ADC/SBB.
            cin = cond_expr(self.reaching[i.address], "c", i)
        # source first (immediates / registers are side-effect free either way;
        # a memory operand may only appear on one side)
        if ops[1].type == X86_OP_MEM:
            b = self.src_read(i, ops[1], st)
            a = (self.reg_read(i, i.reg_name(ops[0].reg)) if ops[0].type == X86_OP_REG
                 else refuse(i, "two memory operands"))
            dst = ("reg", i.reg_name(ops[0].reg))
        elif ops[0].type == X86_OP_MEM:
            acc = self.mem_setup(i, ops[0], st)
            a = self.mem_read(acc, ops[0].size)
            b = self.src_read(i, ops[1], st)
            dst = ("mem", acc)
        else:
            a = self.reg_read(i, i.reg_name(ops[0].reg))
            b = self.src_read(i, ops[1], st)
            dst = ("reg", i.reg_name(ops[0].reg))
        if cin is not None:
            res = "((%s %s %s %s ((%s) ? 1u : 0u))%s)" % (a, cop, b, cop, cin, msk)
            self.body.append("    /* UNVERIFIED PATH: %s value model has no test "
                             "coverage; its flags are a refusal */" % m)
        else:
            res = "((%s %s %s)%s)" % (a, cop, b, msk)
        res = self.emit_flagdef(i, kind, a, b, res, w)
        if m in ("cmp", "test"):
            if i.address not in self.needed:
                self.body.append("    /* %s: flags unused */" % m)
            return
        if dst[0] == "reg":
            self.body.append("    " + self.reg_write(i, dst[1], res))
        else:
            self.body.append("    " + self.mem_write(i, dst[1], ops[0].size, res))

    def emit_mul(self, i, st):
        """IMUL (1-, 2- and 3-operand) and MUL (1-operand).

        The low half is computed in UNSIGNED arithmetic -- it is bit-identical
        to the signed product and C's signed overflow is undefined.  The 64-bit
        product is materialised only when CF/OF are actually consumed, or when
        the one-operand form needs the upper half in EDX."""
        m, ops = i.mnemonic, i.operands
        E = self.body.append
        need = i.address in self.needed
        if ops[0].size != 4:
            refuse(i, "%s with a %d-bit operand -- only the dword forms are proven"
                   % (m, ops[0].size * 8))
        if len(ops) == 1:                              # EDX:EAX = EAX * r/m32
            src = self.src_read(i, ops[0], st)
            self.U("r_eax"); self.U("r_edx")
            if m == "imul":
                E("    %s = (long long)(int)r_eax * (long long)(int)(%s);"
                  % (self.U("pf_prod"), src))
                E("    r_eax = (unsigned int)((unsigned long long)pf_prod & 0xFFFFFFFFu);")
                E("    r_edx = (unsigned int)((unsigned long long)pf_prod >> 32);")
                if need:
                    E("    %s = (pf_prod != (long long)(int)r_eax) ? 1u : 0u;"
                      % self.U("pf_fa"))
            else:
                E("    %s = (unsigned long long)r_eax * (unsigned long long)(%s);"
                  % (self.U("pf_uprod"), src))
                E("    r_eax = (unsigned int)(pf_uprod & 0xFFFFFFFFull);")
                E("    r_edx = (unsigned int)(pf_uprod >> 32);")
                if need:
                    E("    %s = (r_edx != 0u) ? 1u : 0u;" % self.U("pf_fa"))
            return
        if m == "mul":
            refuse(i, "MUL with more than one operand does not exist")
        if ops[0].type != X86_OP_REG:
            refuse(i, "imul with a memory destination")
        if len(ops) == 2:
            a = self.reg_read(i, i.reg_name(ops[0].reg))
            b = self.src_read(i, ops[1], st)
        else:                                          # imul r32, r/m32, imm
            a = self.src_read(i, ops[1], st)
            b = self.src_read(i, ops[2], st)
        if need:
            E("    %s = (long long)(int)(%s) * (long long)(int)(%s);"
              % (self.U("pf_prod"), a, b))
            E("    %s = (pf_prod != (long long)(int)(unsigned int)(unsigned long long)"
              "pf_prod) ? 1u : 0u;" % self.U("pf_fa"))
            E("    " + self.reg_write(i, i.reg_name(ops[0].reg),
                                      "(unsigned int)((unsigned long long)pf_prod "
                                      "& 0xFFFFFFFFull)"))
        else:
            E("    " + self.reg_write(i, i.reg_name(ops[0].reg),
                                      "((%s) * (%s))" % (a, b)))

    def emit_un(self, i, st):
        m, ops = i.mnemonic, i.operands
        w = ops[0].size * 8
        msk = self.mask(w)
        if ops[0].type == X86_OP_REG:
            a = self.reg_read(i, i.reg_name(ops[0].reg))
            dst = ("reg", i.reg_name(ops[0].reg))
        else:
            acc = self.mem_setup(i, ops[0], st)
            a = self.mem_read(acc, ops[0].size)
            dst = ("mem", acc)
        if m == "not":
            res = "((~%s)%s)" % (a, msk)
        elif m == "inc":
            res = self.emit_flagdef(i, "add", a, "1u", "((%s + 1u)%s)" % (a, msk), w)
        elif m == "dec":
            res = self.emit_flagdef(i, "sub", a, "1u", "((%s - 1u)%s)" % (a, msk), w)
        else:   # neg  == sub(0, a)
            res = self.emit_flagdef(i, "sub", "0u", a, "((0u - %s)%s)" % (a, msk), w)
        if dst[0] == "reg":
            self.body.append("    " + self.reg_write(i, dst[1], res))
        else:
            self.body.append("    " + self.mem_write(i, dst[1], ops[0].size, res))

    def emit_shift(self, i, st):
        m, ops = i.mnemonic, i.operands
        if i.address in self.needed:
            refuse(i, "flags of a shift are consumed -- not modelled")
        w = ops[0].size * 8
        if ops[0].type != X86_OP_REG:
            refuse(i, "shift with a memory destination")
        a = self.reg_read(i, i.reg_name(ops[0].reg))
        if len(ops) < 2:
            cnt = "1u"
        elif ops[1].type == X86_OP_IMM:
            cnt = "%du" % (ops[1].imm & 31)
        elif ops[1].type == X86_OP_REG and i.reg_name(ops[1].reg) == "cl":
            cnt = "(PF_GET8L(%s) & 31u)" % self.U("r_ecx")
        else:
            refuse(i, "unsupported shift count operand")
        if m in ("shl", "sal"):
            res = "((%s << %s)%s)" % (a, cnt, self.mask(w))
        elif m == "shr":
            res = "((%s%s) >> %s)" % (a, self.mask(w), cnt)
        else:
            styp = {8: "signed char", 16: "short", 32: "int"}[w]
            res = "((unsigned int)(((%s)(%s)) >> %s)%s)" % (styp, a, cnt, self.mask(w))
        self.body.append("    " + self.reg_write(i, i.reg_name(ops[0].reg), res))

    def emit_div(self, i, st):
        ops = i.operands
        if ops[0].size != 4:
            refuse(i, "non-dword divide")
        d = self.src_read(i, ops[0], st)
        self.U("r_eax"); self.U("r_edx"); self.U("pf_num")
        B = self.body.append
        if i.mnemonic == "idiv":
            B("    pf_num = (long long)(((unsigned long long)r_edx << 32) | r_eax);")
            B("    pf_den = (long long)(int)(%s);" % d)
            B("    if (pf_den == 0) PF_TRAP(0x%08xu, \"idiv #DE: divisor is zero\");"
              % i.address)
            B("    pf_quo = pf_num / pf_den;")
            B("    if (pf_quo < -2147483648LL || pf_quo > 2147483647LL)")
            B("        PF_TRAP(0x%08xu, \"idiv #DE: quotient does not fit in 32 bits\");"
              % i.address)
            B("    r_eax = (unsigned int)(int)pf_quo;")
            B("    r_edx = (unsigned int)(int)(pf_num % pf_den);")
        else:
            B("    pf_num = (long long)((((unsigned long long)r_edx) << 32) "
              "| (unsigned long long)r_eax);")
            B("    pf_den = (long long)(unsigned long long)(unsigned int)(%s);" % d)
            B("    if (pf_den == 0) PF_TRAP(0x%08xu, \"div #DE: divisor is zero\");"
              % i.address)
            B("    pf_quo = (long long)((unsigned long long)pf_num "
              "/ (unsigned long long)pf_den);")
            B("    if ((unsigned long long)pf_quo > 0xFFFFFFFFull)")
            B("        PF_TRAP(0x%08xu, \"div #DE: quotient does not fit in 32 bits\");"
              % i.address)
            B("    r_eax = (unsigned int)pf_quo;")
            B("    r_edx = (unsigned int)((unsigned long long)pf_num "
              "% (unsigned long long)pf_den);")

    # -- calls -------------------------------------------------------------

    def emit_call(self, i, st):
        """UNVERIFIED PATH: none of the pilot candidates contains a call."""
        ops = i.operands
        esp = st[0]
        if ops[0].type == X86_OP_IMM:
            tgt = ops[0].imm
            proto = None
            for p in self.protos.values():
                if p["va"] == tgt:
                    proto = p
                    break
            if proto is None:
                refuse(i, "call 0x%08x has no prototype in the interop header" % tgt)
            tname = "PFN_%s" % proto["name"]
            callee = "((%s)0x%xu)" % (tname, tgt)
            label = proto["name"]
        elif ops[0].type == X86_OP_MEM and ops[0].mem.base == 0 and ops[0].mem.index == 0:
            slot = ops[0].mem.disp & 0xFFFFFFFF
            nm = self.iat.get(slot)
            if nm is None:
                refuse(i, "indirect call through 0x%08x which is not an IAT slot" % slot)
            if nm not in IMPORT_PROTOS:
                refuse(i, "import '%s' has no prototype in pf_lift's IMPORT_PROTOS table" % nm)
            ret, params = IMPORT_PROTOS[nm]
            proto = {"ret": ret, "params": params, "conv": "cdecl", "name": nm}
            callee = "(*(%s (__cdecl **)(%s))0x%xu)" % (
                ret, ", ".join(params) if params else "void", slot)
            label = "IAT:" + nm
        else:
            refuse(i, "unsupported indirect call form")
        args, off = [], esp
        for p in proto["params"]:
            sz = type_size(p)
            if sz is None:
                refuse(i, "call argument type '%s' has no proven stack size" % p)
            if sz != 4:
                refuse(i, "call argument type '%s' is %d bytes -- not supported" % (p, sz))
            if off < self.frame_lo:
                refuse(i, "call argument below the modelled frame")
            cast = "(%s)(size_t)" % p if "*" in p else "(%s)" % p
            args.append("%sPF_S32(%d)" % (cast, off - self.frame_lo))
            off += 4
        self.ext_calls.append({"at": "0x%08x" % i.address, "target": label,
                               "argc": len(proto["params"])})
        expr = "%s(%s)" % (callee, ", ".join(args))
        self.body.append("    /* UNVERIFIED PATH: pf_lift call lowering has no test coverage */")
        rk = ret_kind(proto["ret"])
        if rk == "void":
            self.body.append("    %s;" % expr)
        elif rk == "ptr":
            self.body.append("    %s = (unsigned int)(size_t)%s;" % (self.U("r_eax"), expr))
        elif rk == "i32":
            self.body.append("    %s = (unsigned int)%s;" % (self.U("r_eax"), expr))
        elif rk == "fp":
            self.x87 = True
            self.body.append("    PF_PUSH(PF_FF64(%s));" % expr)
        else:
            refuse(i, "call return type '%s' not supported" % proto["ret"])

    # -- x87 ---------------------------------------------------------------

    def x87_bytes(self, i):
        b = i.bytes
        if b[0] < 0xD8 or b[0] > 0xDF:
            refuse(i, "x87 instruction with a prefix -- not supported")
        return b[0], b[1]

    def emit_x87(self, i, st):
        self.x87 = True
        E = self.body.append
        esc, modrm = self.x87_bytes(i)
        reg = (modrm >> 3) & 7
        rm = modrm & 7
        ops = i.operands

        if modrm < 0xC0:
            # memory form
            o = ops[0]
            acc = self.mem_setup(i, o, st)
            if esc == 0xD9 and reg == 0:                       # fld m32fp
                E("    PF_PUSH(PF_FF32(%s));" % self.mem_read(acc, 4, fp=True)); return
            if esc == 0xDD and reg == 0:                       # fld m64fp
                E("    PF_PUSH(PF_FF64(%s));" % self.mem_read(acc, 8, fp=True)); return
            if esc == 0xD9 and reg in (2, 3):                  # fst/fstp m32fp
                self.cw_guard(i)
                E("    " + self.mem_write(i, acc, 4, "PF_ST(0)", fp="bits"))
                if reg == 3:
                    E("    PF_POP();")
                return
            if esc == 0xDD and reg in (2, 3):                  # fst/fstp m64fp
                self.cw_guard(i)
                E("    " + self.mem_write(i, acc, 8, "PF_ST(0)", fp="bits"))
                if reg == 3:
                    E("    PF_POP();")
                return
            if esc == 0xD9 and reg == 5:                       # fldcw m16
                E("    %s = %s;" % (self.U("pf_fcw"), self.mem_read(acc, 2))); return
            if esc == 0xD9 and reg == 7:                       # fnstcw m16
                E("    " + self.mem_write(i, acc, 2, self.U("pf_fcw"))); return
            if esc == 0xDB and reg == 0:                       # fild m32int
                E("    PF_PUSH(PF_FI32(%s));" % self.mem_read(acc, 4)); return
            if esc == 0xDF and reg == 0:                       # fild m16int
                E("    PF_PUSH(PF_FI16(%s));" % self.mem_read(acc, 2)); return
            if esc == 0xDF and reg == 5:                       # fild m64int
                E("    PF_PUSH(PF_FI64(%s));" % self.mem_read(acc, 8)); return
            if esc in (0xDB, 0xDF) and reg in (2, 3):          # fist/fistp
                self.x87_fistp_count += 1
            if esc == 0xDB and reg in (2, 3):                  # fist/fistp m32int
                E("    " + self.mem_write(i, acc, 4,
                                          "PF_TOI32(PF_ST(0), %s)" % self.U("pf_fcw")))
                if reg == 3:
                    E("    PF_POP();")
                return
            if esc == 0xDF and reg in (2, 3):                  # fist/fistp m16int
                E("    " + self.mem_write(i, acc, 2,
                                          "PF_TOI16(PF_ST(0), %s)" % self.U("pf_fcw")))
                if reg == 3:
                    E("    PF_POP();")
                return
            if esc in (0xD8, 0xDC) and reg in (0, 1, 4, 5, 6, 7):
                sz = 4 if esc == 0xD8 else 8
                src = "%s(%s)" % ("PF_FF32" if sz == 4 else "PF_FF64",
                                  self.mem_read(acc, sz, fp=True))
                return self.x87_arith(i, reg, "PF_ST(0)", src, "PF_ST(0)")
            if esc in (0xD8, 0xDC) and reg in (2, 3):          # fcom/fcomp mem
                sz = 4 if esc == 0xD8 else 8
                E("    %s = pf_fcmp(PF_ST(0), %s(%s));"
                  % (self.U("pf_fsw"), "PF_FF32" if sz == 4 else "PF_FF64",
                     self.mem_read(acc, sz, fp=True)))
                if reg == 3:
                    E("    PF_POP();")
                return
            if esc in (0xDA, 0xDE) and reg in (0, 1, 4, 5, 6, 7):
                # FIADD/FIMUL/FISUB/FISUBR/FIDIV/FIDIVR: DA = m32int, DE = m16int.
                # ST(0) is the destination, so /5 and /7 are the reversed forms.
                sz = 4 if esc == 0xDA else 2
                src = "%s(%s)" % ("PF_FI32" if sz == 4 else "PF_FI16",
                                  self.mem_read(acc, sz))
                return self.x87_arith(i, reg, "PF_ST(0)", src, "PF_ST(0)")
            if esc in (0xDA, 0xDE) and reg in (2, 3):          # ficom/ficomp mem
                sz = 4 if esc == 0xDA else 2
                E("    %s = pf_fcmp(PF_ST(0), %s(%s));"
                  % (self.U("pf_fsw"), "PF_FI32" if sz == 4 else "PF_FI16",
                     self.mem_read(acc, sz)))
                if reg == 3:
                    E("    PF_POP();")
                return
            refuse(i, "x87 memory form esc=%02X /%d not supported" % (esc, reg))

        # register form
        if esc == 0xD9:
            if 0xC0 <= modrm <= 0xC7:                          # fld st(i)
                E("    PF_PUSH(PF_ST(%d));" % rm); return
            if 0xC8 <= modrm <= 0xCF:                          # fxch st(i)
                E("    { pf_x87_t pf_t = PF_ST(0); PF_ST(0) = PF_ST(%d); PF_ST(%d) = pf_t; }"
                  % (rm, rm)); return
            if modrm == 0xE0:
                E("    PF_ST(0) = PF_NEG(PF_ST(0));"); return   # fchs
            if modrm == 0xE1:
                E("    PF_ST(0) = PF_ABS(PF_ST(0));"); return   # fabs
            if modrm == 0xE8:
                E("    PF_PUSH(PF_ONE);"); return               # fld1
            if modrm == 0xEE:
                E("    PF_PUSH(PF_ZERO);"); return              # fldz
            if modrm == 0xFA:
                refuse(i, "fsqrt: neither x87 backend implements a correctly "
                          "rounded square root -- refused rather than approximated")
            if modrm == 0xE4:                                   # ftst
                E("    %s = pf_fcmp(PF_ST(0), PF_ZERO);" % self.U("pf_fsw")); return
            refuse(i, "x87 D9 %02X not supported" % modrm)
        if esc == 0xD8:
            if 0xD0 <= modrm <= 0xDF:                          # fcom/fcomp st(i)
                E("    %s = pf_fcmp(PF_ST(0), PF_ST(%d));" % (self.U("pf_fsw"), rm))
                if modrm >= 0xD8:
                    E("    PF_POP();")
                return
            return self.x87_arith(i, (modrm - 0xC0) >> 3, "PF_ST(0)", "PF_ST(%d)" % rm, "PF_ST(0)")
        if esc == 0xDC:
            return self.x87_arith(i, (modrm - 0xC0) >> 3, "PF_ST(%d)" % rm, "PF_ST(0)",
                                  "PF_ST(%d)" % rm, reversed_dc=True)
        if esc == 0xDE:
            if modrm == 0xD9:                                  # fcompp
                E("    %s = pf_fcmp(PF_ST(0), PF_ST(1));" % self.U("pf_fsw"))
                E("    PF_POP(); PF_POP();"); return
            self.x87_arith(i, (modrm - 0xC0) >> 3, "PF_ST(%d)" % rm, "PF_ST(0)",
                           "PF_ST(%d)" % rm, reversed_dc=True)
            E("    PF_POP();"); return
        if esc == 0xDD:
            if 0xC0 <= modrm <= 0xC7:                          # ffree
                E("    /* ffree st(%d): tag word not modelled */" % rm); return
            if 0xD0 <= modrm <= 0xD7:                          # fst st(i)
                E("    PF_ST(%d) = PF_ST(0);" % rm); return
            if 0xD8 <= modrm <= 0xDF:                          # fstp st(i)
                E("    PF_ST(%d) = PF_ST(0);" % rm)
                E("    PF_POP();"); return
            if 0xE0 <= modrm <= 0xEF:                          # fucom / fucomp st(i)
                E("    %s = pf_fcmp(PF_ST(0), PF_ST(%d));" % (self.U("pf_fsw"), rm))
                if modrm >= 0xE8:
                    E("    PF_POP();")
                return
            refuse(i, "x87 DD %02X not supported" % modrm)
        if esc == 0xDA:
            if modrm == 0xE9:                                  # fucompp
                E("    %s = pf_fcmp(PF_ST(0), PF_ST(1));" % self.U("pf_fsw"))
                E("    PF_POP(); PF_POP();"); return
            refuse(i, "x87 DA %02X not supported" % modrm)
        if esc == 0xDB:
            if modrm == 0xE3:
                E("    %s = 0u; %s = 0u;" % (self.U("pf_ftop"), self.U("pf_fsw"))); return
            if 0xE8 <= modrm <= 0xEF or 0xF0 <= modrm <= 0xF7:  # fucomi / fcomi
                self.emit_flagdef(i, "fcomi", "pf_fcmp2eflags(pf_fcmp(PF_ST(0), PF_ST(%d)))" % rm,
                                  None, None, 32)
                return
            refuse(i, "x87 DB %02X not supported" % modrm)
        if esc == 0xDF:
            if modrm == 0xE0:                                  # fnstsw ax
                E("    PF_SET16(%s, PF_FSW());" % self.U("r_eax")); return
            if 0xE8 <= modrm <= 0xF7:                          # fucomip / fcomip
                self.emit_flagdef(i, "fcomi", "pf_fcmp2eflags(pf_fcmp(PF_ST(0), PF_ST(%d)))" % rm,
                                  None, None, 32)
                E("    PF_POP();"); return
            refuse(i, "x87 DF %02X not supported" % modrm)
        refuse(i, "x87 escape %02X %02X not supported" % (esc, modrm))

    def cw_guard(self, i):
        """A function that reloads the control word may reach a rounding x87
        operation with a non-default PC/RC.  Neither backend models that, so
        emit an explicit run-time trap rather than approximating silently."""
        if self.has_fldcw:
            self.body.append("    PF_CW_ARITH(0x%08xu, %s);"
                             % (i.address, self.U("pf_fcw")))

    def x87_arith(self, i, sub, a, b, dst, reversed_dc=False):
        ops = {0: "PF_ADD", 1: "PF_MUL", 4: "PF_SUB", 5: "PF_SUB",
               6: "PF_DIV", 7: "PF_DIV"}
        if sub not in ops:
            refuse(i, "x87 arithmetic sub-opcode %d not supported" % sub)
        # /4 = SUB, /5 = SUBR, /6 = DIV, /7 = DIVR for the D8/DA/DE-memory
        # (ST0-dest) forms; the DC/DE register (STi-dest) forms swap the
        # meaning of /4 vs /5 and /6 vs /7.
        rev = sub in (5, 7)
        if reversed_dc:
            rev = sub in (4, 6)
        op = ops[sub]
        x, y = (b, a) if rev else (a, b)
        self.x87_arith_count += 1
        self.cw_guard(i)
        self.body.append("    %s = %s(%s, %s);" % (dst, op, x, y))

    # -- file assembly -----------------------------------------------------

    def run(self):
        name = self.func["name"]
        proto = self.protos.get(name)
        if proto is None:
            raise Refusal(self.func["va"], "-",
                          "no PFN_%s typedef in the interop header" % name)
        if proto["va"] != self.func["va"]:
            raise Refusal(self.func["va"], "-", "interop VA 0x%x != requested VA 0x%x"
                          % (proto["va"], self.func["va"]))
        self.rtype = proto["ret"]
        self.rkind = ret_kind(self.rtype)
        if self.rkind is None:
            raise Refusal(self.func["va"], "-", "unsupported return type '%s'" % self.rtype)
        # argument frame
        self.params = []
        off = 0
        for k, p in enumerate(proto["params"]):
            sz = type_size(p)
            if sz is None:
                raise Refusal(self.func["va"], "-",
                              "parameter %d type '%s' has no proven stack size" % (k, p))
            self.params.append((p, "a%d" % k, off, sz))
            off += (sz + 3) & ~3
        self.argbytes = off

        self.decode()
        self.build_blocks()
        self.prop_frame()
        self.prop_flags()
        body = self.emit()

        framebytes = ((-self.frame_lo) + 7) & ~7
        decls = []
        for r in R32:
            if vname(r) in self.used:
                decls.append("    unsigned int %s = 0u;" % vname(r))
        if "pf_ea" in self.used:
            decls.append("    unsigned int pf_ea = 0u;")
        for v in ("pf_fa", "pf_fb", "pf_fres"):
            if v in self.used:
                decls.append("    unsigned int %s = 0u;" % v)
        if "pf_num" in self.used:
            decls.append("    long long pf_num = 0, pf_den = 0, pf_quo = 0;")
        if "pf_prod" in self.used:
            decls.append("    long long pf_prod = 0;")
        if "pf_uprod" in self.used:
            decls.append("    unsigned long long pf_uprod = 0;")
        if self.x87:
            decls.append("    pf_x87_t pf_fr[8];")
            decls.append("    unsigned int pf_ftop = 0u;")
            decls.append("    unsigned int pf_fsw = 0u;")
        if "pf_fcw" in self.used:
            decls.append("    unsigned int pf_fcw = PF_CW_INIT;")
        if framebytes:
            decls.append("    union { double d[%d]; unsigned char b[%d]; } pf_stk;"
                         % (max(1, framebytes // 8), framebytes))
        if self.argbytes:
            decls.append("    union { double d[%d]; unsigned char b[%d]; } pf_arg;"
                         % (max(1, (self.argbytes + 7) // 8), self.argbytes))

        pre = []
        if self.x87:
            pre.append("    memset(pf_fr, 0, sizeof pf_fr);")
        if framebytes:
            pre.append("    memset(pf_stk.b, 0, sizeof pf_stk.b);")
        for (ty, nm, o, sz) in self.params:
            pre.append("    PF_CT_ASSERT(sizeof(%s) == %d);" % (nm, sz))
            pre.append("    memcpy(pf_arg.b + %d, &%s, %d);" % (o, nm, sz))

        plist = ", ".join("%s %s" % (ty, nm) for (ty, nm, o, sz) in self.params) \
            or ("void" if not proto["unspecified_params"] else "void")

        hdr = self.header(len(self.ins), len(self.emit_order), framebytes)
        out = [hdr]
        out.append('#include "pf_rt.h"')
        out.append('#include "it_types.h"')
        out.append('#include "it_funcs.h"')
        out.append('#include "it_globals.h"')
        out.append("%s __cdecl lifted_%s(%s)" % (self.rtype, name, plist)
                   if proto["conv"] == "cdecl" else
                   "%s __stdcall lifted_%s(%s)" % (self.rtype, name, plist))
        out.append("{")
        out.extend(decls)
        out.append("")
        out.extend(pre)
        out.append("")
        out.extend(body)
        out.append("}")
        out.append("")
        text = "\n".join(out) + "\n"

        meta = {
            "generator": GEN,
            "image_sha256": self.sha,
            "function": name,
            "va": "0x%08x" % self.func["va"],
            "size": self.func["size"],
            "prototype": "%s __%s (%s)" % (self.rtype, proto["conv"], ", ".join(proto["params"])),
            "instructions_decoded": len(self.ins),
            "instructions_lifted": sum(len(self.blocks[b]) for b in self.emit_order),
            "blocks_total": len(self.blocks),
            "blocks_reachable": len(self.emit_order),
            "blocks": ["0x%08x" % b for b in self.emit_order],
            "unreachable_padding": ["0x%08x" % b for b in self.leaders if b not in self.reach],
            "frame_bytes": framebytes,
            "arg_bytes": self.argbytes,
            "uses_x87": self.x87,
            # Facts the carrier needs to pick an x87 backend (README SS6b):
            # a function whose only FP arithmetic is exact (a power-of-two
            # scaling) can run on `double`; one that divides or multiplies
            # non-powers-of-two and then truncates cannot.
            "x87_arith_ops": self.x87_arith_count,
            "x87_fistp_ops": self.x87_fistp_count,
            "x87_control_word_used": self.has_fldcw,
            "external_calls": self.ext_calls,
            "memory_ranges": self.mem_hits,
            "refusals": [],
            "generated_lines": len(out),
        }
        return text, meta

    def header(self, nins, nblk, framebytes):
        f = self.func
        return ("/* GENERATED FILE -- DO NOT EDIT.\n"
                " * generator : %s\n"
                " * image     : icytower15.exe sha256=%s\n"
                " * function  : %s  VA=0x%08x  size=%d bytes\n"
                " * lifted    : %d instructions in %d reachable basic blocks\n"
                " *\n"
                " * LIFTED form (win32_pilot.md SS3).  Memory is the ORIGINAL memory:\n"
                " * every access goes through PF_MEM(addr), which is the identity by\n"
                " * default, so this object file is bindable at the original address.\n"
                " *\n"
                " * x87 HYPOTHESIS (win32_pilot.md SS3): 80-bit x87 intermediates are\n"
                " * modelled as `pf_x87_t`, currently typedef'd to `double` in pf_rt.h.\n"
                " * That typedef is the bet under test; swap it for a software 80-bit\n"
                " * type if the oracle comparison ever disagrees.\n"
                " */" % (GEN, self.sha, f["name"], f["va"], f["size"], nins, nblk))


# --------------------------------------------------------------------------
# runtime header
# --------------------------------------------------------------------------

PF_RT_H = r'''/* GENERATED FILE -- DO NOT EDIT.  Produced by @GEN@.
 *
 * Runtime support for the LIFTED form (win32_pilot.md SS3).  Deliberately
 * tiny: no CPU struct, no memory abstraction.  PF_MEM() is the ONE seam --
 * it is the identity in the carrier (the original image is mapped at its
 * original base) and can be redefined by an offline harness to point at an
 * in-process copy of the image.
 *
 * The x87 model has TWO interchangeable backends behind one macro API
 * (PF_ADD/PF_MUL/PF_TOI32/...), selected at COMPILE TIME:
 *
 *   default        pf_x87_t = double          (the original HYPOTHESIS)
 *   -DPF_X87_SOFT  pf_x87_t = software 80-bit extended (pf_x87_soft.h)
 *
 * No generated .c file changes between the two.
 */
#ifndef PF_RT_H
#define PF_RT_H

#include <string.h>

/* ---- the one memory seam ------------------------------------------------ */
#ifndef PF_MEM
#define PF_MEM(a) ((void *)(size_t)(unsigned int)(a))
#endif

#define PF_R8(a)    (*(unsigned char  *)PF_MEM(a))
#define PF_R16(a)   (*(unsigned short *)PF_MEM(a))
#define PF_R32(a)   (*(unsigned int   *)PF_MEM(a))
#define PF_R64(a)   (*(unsigned long long *)PF_MEM(a))
#define PF_RF32(a)  (*(float  *)PF_MEM(a))
#define PF_RF64(a)  (*(double *)PF_MEM(a))
#define PF_W8(a,v)   (PF_R8(a)   = (unsigned char )(v))
#define PF_W16(a,v)  (PF_R16(a)  = (unsigned short)(v))
#define PF_W32(a,v)  (PF_R32(a)  = (unsigned int  )(v))
#define PF_W64(a,v)  (PF_R64(a)  = (unsigned long long)(v))
#define PF_WF32(a,v) (PF_RF32(a) = (float )(v))
#define PF_WF64(a,v) (PF_RF64(a) = (double)(v))

/* ---- partial-register access -------------------------------------------- */
#define PF_GET8L(r)   ((r) & 0xFFu)
#define PF_GET8H(r)   (((r) >> 8) & 0xFFu)
#define PF_GET16(r)   ((r) & 0xFFFFu)
#define PF_SET8L(r,v) ((r) = ((r) & 0xFFFFFF00u) | ((unsigned int)(v) & 0xFFu))
#define PF_SET8H(r,v) ((r) = ((r) & 0xFFFF00FFu) | (((unsigned int)(v) & 0xFFu) << 8))
#define PF_SET16(r,v) ((r) = ((r) & 0xFFFF0000u) | ((unsigned int)(v) & 0xFFFFu))

/* ---- compile-time assertion (used on every argument slot) --------------- */
#define PF_CT_ASSERT(e) do { typedef char pf_ct_[(e) ? 1 : -1]; \
                             (void)sizeof(pf_ct_); } while (0)

/* ---- hard refusal at run time ------------------------------------------- */
#ifndef PF_TRAP
extern void pf_trap(unsigned int va, const char *why);
#define PF_TRAP(va, why) pf_trap((va), (why))
#endif

/* ---- x87 control word ---------------------------------------------------
 * KNOWN: Icy Tower's ___mingw_CRTStartup (0x401020) calls __fpreset
 * (0x4b2850), which is a bare FNINIT.  FNINIT leaves CW = 0x037F, i.e.
 * PC = 11 (64-bit significand = full extended precision) and RC = 00
 * (round to nearest even).  That is the control word a lifted function
 * inherits, so PF_CW_INIT is 0x037F and NOT the MSVC/CRT 0x027F.
 *
 * GCC 4.4 casts a floating value to int with the classic idiom
 *     fnstcw save ; ax = save ; ah = 0x0C ; fldcw trunc ; fistp ; fldcw save
 * (0x0C in the high byte = RC 11 "toward zero", PC 00).  pf_lift models the
 * control word explicitly, so FISTP really truncates there.
 */
#define PF_CW_INIT  0x037Fu

/* Rounding-mode field, 2 bits: 00 nearest-even, 01 -inf, 10 +inf, 11 zero. */
#define PF_CW_RC(cw) (((cw) >> 10) & 3u)

/* pf_lift emits this before every rounding x87 operation in a function that
 * contains an FLDCW.  Neither backend models a non-default PC/RC for
 * ARITHMETIC (only FIST/FISTP consult RC), so reaching one is a hard trap
 * rather than a silent approximation. */
#define PF_CW_ARITH(va, cw) do { if (((cw) & 0x0F00u) != 0x0300u) \
        PF_TRAP((va), "x87 arithmetic under a non-default control word"); \
    } while (0)

/* ---- x87 backend -------------------------------------------------------- */
#if defined(PF_X87_SOFT)
#include "pf_x87_soft.h"
#else

/* HYPOTHESIS backend (win32_pilot.md SS3): the 80-bit x87 register stack is
 * modelled with `double`.  Every generated file goes through the macros
 * below, so -DPF_X87_SOFT swaps the whole model without regenerating. */
typedef double pf_x87_t;

#define PF_FI16(x)  ((pf_x87_t)(short)(x))
#define PF_FI32(x)  ((pf_x87_t)(int)(x))
#define PF_FI64(x)  ((pf_x87_t)(long long)(x))
#define PF_FF32(x)  ((pf_x87_t)(float)(x))
#define PF_FF64(x)  ((pf_x87_t)(double)(x))
#define PF_ZERO     ((pf_x87_t)0.0)
#define PF_ONE      ((pf_x87_t)1.0)
#define PF_ADD(a,b) ((pf_x87_t)((a) + (b)))
#define PF_SUB(a,b) ((pf_x87_t)((a) - (b)))
#define PF_MUL(a,b) ((pf_x87_t)((a) * (b)))
#define PF_DIV(a,b) ((pf_x87_t)((a) / (b)))
#define PF_NEG(a)   ((pf_x87_t)(-(a)))
#define PF_TOF32(v) ((float )(v))
#define PF_TOF64(v) ((double)(v))

/* Raw bit patterns for FST/FSTP.  These return an INTEGER, never a double:
 * the 32-bit cdecl ABI returns a double in ST(0), and FLD of a signalling NaN
 * quiets it, so a `double`-returning conversion helper would mangle an SNaN
 * that the original code merely copies from memory to memory. */
static unsigned long long pf_bits64(pf_x87_t v)
{ union { double d; unsigned long long u; } c; c.d = (double)v; return c.u; }
static unsigned int pf_bits32(pf_x87_t v)
{ union { float f; unsigned int u; } c; c.f = (float)v; return c.u; }

static pf_x87_t pf_abs(pf_x87_t v) { return v < (pf_x87_t)0 ? -v : v; }
#define PF_ABS(v) pf_abs(v)

/* FPU status-word condition bits, exactly as FCOM/FUCOM set them:
 *   ST0 > src -> 0, < -> C0 (0x0100), = -> C3 (0x4000), unordered -> 0x4500. */
static unsigned int pf_fcmp(pf_x87_t a, pf_x87_t b)
{
    if (a > b)  return 0x0000u;
    if (a < b)  return 0x0100u;
    if (a == b) return 0x4000u;
    return 0x4500u;
}

/* Round to an integral value in the CURRENT rounding mode. */
static double pf_round_rc(double v, unsigned int cw)
{
    double f, d;
    unsigned int rc = PF_CW_RC(cw);
    if (v != v) return v;                                  /* NaN */
    if (v >= 9.2233720368547758e18 || v <= -9.2233720368547758e18)
        return v;                                          /* already integral / inf */
    f = (double)(long long)v;                              /* toward zero */
    if (f == v) return v;
    if (rc == 0u) {
        d = v - f;
        if (d >  0.5) return f + 1.0;
        if (d < -0.5) return f - 1.0;
        if (d ==  0.5) return (((long long)f) & 1) ? f + 1.0 : f;
        if (d == -0.5) return (((long long)f) & 1) ? f - 1.0 : f;
        return f;
    }
    if (rc == 1u) return (v < 0.0) ? f - 1.0 : f;           /* toward -inf */
    if (rc == 2u) return (v > 0.0) ? f + 1.0 : f;           /* toward +inf */
    return f;                                               /* toward zero */
}

/* FIST/FISTP.  An out-of-range or NaN source yields the x87 "integer
 * indefinite" 0x80000000 / 0x8000 with the invalid exception masked, which is
 * what the hardware (and the unicorn oracle) stores. */
static unsigned int pf_toi32(pf_x87_t v, unsigned int cw)
{
    double r = pf_round_rc((double)v, cw);
    if (!(r >= -2147483648.0 && r <= 2147483647.0)) return 0x80000000u;
    return (unsigned int)(int)r;
}
static unsigned int pf_toi16(pf_x87_t v, unsigned int cw)
{
    double r = pf_round_rc((double)v, cw);
    if (!(r >= -32768.0 && r <= 32767.0)) return 0x8000u;
    return (unsigned int)(int)r & 0xFFFFu;
}
#define PF_TOI32(v, cw) pf_toi32((v), (cw))
#define PF_TOI16(v, cw) pf_toi16((v), (cw))

#endif /* backend */

/* FCOMI-family: the same comparison delivered in EFLAGS (ZF=bit6, PF=bit2,
 * CF=bit0) instead of the status word. */
static unsigned int pf_fcmp2eflags(unsigned int sw)
{
    switch (sw & 0x4500u) {
    case 0x0000u: return 0x00u;                 /* >  : ZF=0 PF=0 CF=0 */
    case 0x0100u: return 0x01u;                 /* <  : CF=1 */
    case 0x4000u: return 0x40u;                 /* =  : ZF=1 */
    default:      return 0x45u;                 /* unordered: ZF PF CF */
    }
}

/* ---- lifted-function-local macros ---------------------------------------
 * These expand INSIDE a lifted function and name its locals (pf_fr, pf_ftop,
 * pf_fsw, pf_fcw, pf_stk, pf_arg) directly.  That is what keeps a lifted
 * function re-entrant without a CPU struct: the "machine state" is ordinary
 * C locals.
 */
#define PF_ST(i)   pf_fr[(pf_ftop + (unsigned int)(i)) & 7u]
/* PF_PUSH must evaluate its argument BEFORE moving the top index: `fld st(0)`
 * reads ST(0) relative to the OLD top. */
#define PF_PUSH(v) do { pf_x87_t pf_pv_ = (v); pf_ftop = (pf_ftop - 1u) & 7u; \
                        pf_fr[pf_ftop] = pf_pv_; } while (0)
#define PF_POP()   do { pf_ftop = (pf_ftop + 1u) & 7u; } while (0)
#define PF_FSW()   ((pf_fsw & 0x4500u) | ((pf_ftop & 7u) << 11))

/* FST/FSTP m32fp / m64fp: store the raw bit pattern (see pf_bits64 above). */
#define PF_WB32(a,v)  (PF_R32(a) = pf_bits32(v))
#define PF_WB64(a,v)  (PF_R64(a) = pf_bits64(v))
#define PF_SWB32(k,v) (PF_S32(k) = pf_bits32(v))
#define PF_SWB64(k,v) (PF_S64(k) = pf_bits64(v))

/* incoming arguments: a byte-exact copy of the cdecl argument block */
#define PF_A8(k)   (*(unsigned char      *)(pf_arg.b + (k)))
#define PF_A16(k)  (*(unsigned short     *)(pf_arg.b + (k)))
#define PF_A32(k)  (*(unsigned int       *)(pf_arg.b + (k)))
#define PF_A64(k)  (*(unsigned long long *)(pf_arg.b + (k)))
#define PF_AF32(k) (*(float  *)(pf_arg.b + (k)))
#define PF_AF64(k) (*(double *)(pf_arg.b + (k)))

/* locals / saved registers / outgoing pushes: the scratch frame */
#define PF_S8(k)   (*(unsigned char      *)(pf_stk.b + (k)))
#define PF_S16(k)  (*(unsigned short     *)(pf_stk.b + (k)))
#define PF_S32(k)  (*(unsigned int       *)(pf_stk.b + (k)))
#define PF_S64(k)  (*(unsigned long long *)(pf_stk.b + (k)))
#define PF_SF32(k) (*(float  *)(pf_stk.b + (k)))
#define PF_SF64(k) (*(double *)(pf_stk.b + (k)))
#define PF_SW8(k,v)   (PF_S8(k)   = (unsigned char )(v))
#define PF_SW16(k,v)  (PF_S16(k)  = (unsigned short)(v))
#define PF_SW32(k,v)  (PF_S32(k)  = (unsigned int  )(v))
#define PF_SW64(k,v)  (PF_S64(k)  = (unsigned long long)(v))
#define PF_SWF32(k,v) (PF_SF32(k) = (float )(v))
#define PF_SWF64(k,v) (PF_SF64(k) = (double)(v))

#endif /* PF_RT_H */
'''.replace("@GEN@", GEN)


PF_X87_SOFT_H = r'''/* GENERATED FILE -- DO NOT EDIT.  Produced by @GEN@.
 *
 * Software 80-bit x87 extended type -- the FALLBACK named in win32_pilot.md
 * SS3 ("softfloat x87 is the fallback").  Selected with -DPF_X87_SOFT; not one
 * generated .c file changes between the two backends.
 *
 * Layout: sign, 15-bit biased exponent (bias 16383) and a 64-bit significand
 * with an EXPLICIT integer bit -- the real 80-bit register format.
 * Arithmetic: add / sub / mul / div, all round-to-nearest-even at 64 bits,
 * which is exactly what CW = 0x037F (PC = 11, RC = 00) selects on hardware.
 * FIST/FISTP honour the current RC field of the modelled control word.
 *
 * Deliberately NOT modelled, as hard traps rather than silent approximations:
 * extended denormals and extended-range underflow (unreachable -- an extended
 * denormal needs |x| < 2^-16382 and every value entering these functions comes
 * from a 32-bit int, a float or a double), FSQRT, and the exception flags.
 */
#ifndef PF_X87_SOFT_H
#define PF_X87_SOFT_H

typedef struct pf_x87_s {
    unsigned long long m;      /* significand, bit63 = explicit integer bit */
    unsigned short     se;     /* bit15 = sign, bits 0..14 = biased exponent */
    unsigned short     pad;
} pf_x87_t;

#define PF_XBIAS 16383
#define PF_XTOP  0x8000000000000000ull

static pf_x87_t pf_mk(unsigned int s, int be, unsigned long long m)
{
    pf_x87_t r;
    r.m = m;
    r.se = (unsigned short)((s ? 0x8000u : 0u) | ((unsigned int)be & 0x7FFFu));
    r.pad = 0;
    return r;
}
static unsigned int pf_sgn(pf_x87_t v)  { return (unsigned int)(v.se >> 15) & 1u; }
static int          pf_bex(pf_x87_t v)  { return (int)(v.se & 0x7FFFu); }
static int pf_is_nan (pf_x87_t v) { return pf_bex(v) == 0x7FFF && (v.m << 1) != 0ull; }
static int pf_is_inf (pf_x87_t v) { return pf_bex(v) == 0x7FFF && (v.m << 1) == 0ull; }
static int pf_is_zero(pf_x87_t v) { return pf_bex(v) == 0 && v.m == 0ull; }

static pf_x87_t pf_inf (unsigned int s) { return pf_mk(s, 0x7FFF, PF_XTOP); }
static pf_x87_t pf_zero(unsigned int s) { return pf_mk(s, 0, 0ull); }
/* the x87 "real indefinite": -QNaN, significand 0xC000000000000000 */
static pf_x87_t pf_indef(void) { return pf_mk(1u, 0x7FFF, 0xC000000000000000ull); }

static void pf_unpack(pf_x87_t v, unsigned int *s, int *be, unsigned long long *m)
{
    *s = pf_sgn(v);
    *be = pf_bex(v);
    *m = v.m;
    if (*be == 0 && v.m != 0ull)
        PF_TRAP(0u, "software x87: extended denormal operand is not modelled");
}

/* ---- 128-bit helpers ---------------------------------------------------- */
typedef struct { unsigned long long hi, lo; } pf_u128;

static pf_u128 pf_mul64(unsigned long long a, unsigned long long b)
{
    unsigned long long a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
    unsigned long long b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
    unsigned long long p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    unsigned long long mid = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
    pf_u128 r;
    r.lo = (p00 & 0xFFFFFFFFull) | (mid << 32);
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

/* right shift with a sticky OR of every bit shifted out */
static pf_u128 pf_shr(pf_u128 v, int n, int *sticky)
{
    pf_u128 r;
    if (n <= 0) return v;
    if (n >= 128) {
        if (v.hi || v.lo) *sticky = 1;
        r.hi = 0ull; r.lo = 0ull;
        return r;
    }
    if (n >= 64) {
        if (v.lo) *sticky = 1;
        r.hi = 0ull; r.lo = v.hi;
        n -= 64;
        if (n) {
            if (r.lo & ((1ull << n) - 1ull)) *sticky = 1;
            r.lo >>= n;
        }
        return r;
    }
    if (v.lo & ((1ull << n) - 1ull)) *sticky = 1;
    r.lo = (v.lo >> n) | (v.hi << (64 - n));
    r.hi = v.hi >> n;
    return r;
}

/* Normalise (hi:lo) so bit127 is set, round to 64 bits (nearest, ties to
 * even) and pack.  Value = (hi:lo) * 2^(be - PF_XBIAS - 127); `sticky` says
 * that nonzero bits exist below bit 0 of the 128-bit window. */
static pf_x87_t pf_round128(unsigned int s, int be, pf_u128 v, int sticky)
{
    unsigned long long m;
    int rbit;
    if (v.hi == 0ull && v.lo == 0ull) return pf_zero(s);
    while ((v.hi & PF_XTOP) == 0ull) {
        v.hi = (v.hi << 1) | (v.lo >> 63);
        v.lo <<= 1;
        be--;
    }
    m = v.hi;
    rbit = (int)((v.lo >> 63) & 1ull);
    if ((v.lo << 1) != 0ull) sticky = 1;
    if (rbit && (sticky || (m & 1ull))) {
        m++;
        if (m == 0ull) { m = PF_XTOP; be++; }
    }
    if (be >= 0x7FFF) return pf_inf(s);
    if (be <= 0) PF_TRAP(0u, "software x87: extended-range underflow is not modelled");
    return pf_mk(s, be, m);
}

/* ---- conversions in ----------------------------------------------------- */
static pf_x87_t pf_from_u64(unsigned int s, unsigned long long u)
{
    int be = PF_XBIAS + 63;
    if (u == 0ull) return pf_zero(0u);
    while ((u & PF_XTOP) == 0ull) { u <<= 1; be--; }
    return pf_mk(s, be, u);
}
static pf_x87_t pf_from_i64(long long v)
{
    unsigned long long u = (v < 0) ? (unsigned long long)(-(v + 1)) + 1ull
                                   : (unsigned long long)v;
    return pf_from_u64((v < 0) ? 1u : 0u, u);
}
static pf_x87_t pf_from_i32(int v)   { return pf_from_i64((long long)v); }
static pf_x87_t pf_from_i16(short v) { return pf_from_i64((long long)v); }

static pf_x87_t pf_from_f64(double d)
{
    union { double d; unsigned long long u; } cv;
    unsigned int s;
    int de;
    unsigned long long mf;
    cv.d = d;
    s  = (unsigned int)(cv.u >> 63) & 1u;
    de = (int)((cv.u >> 52) & 0x7FFull);
    mf = cv.u & 0xFFFFFFFFFFFFFull;
    if (de == 0x7FF) {
        if (mf == 0ull) return pf_inf(s);
        return pf_mk(s, 0x7FFF, PF_XTOP | (mf << 11));       /* NaN payload kept */
    }
    if (de == 0) {
        pf_x87_t t;
        if (mf == 0ull) return pf_zero(s);
        t = pf_from_u64(s, mf);                              /* value == mf */
        return pf_mk(s, pf_bex(t) - 1074, t.m);              /* * 2^-1074 */
    }
    return pf_mk(s, de - 1023 + PF_XBIAS, PF_XTOP | (mf << 11));
}
static pf_x87_t pf_from_f32(float f) { return pf_from_f64((double)f); }

/* ---- conversions out ---------------------------------------------------- */
/* Pack the extended value into an IEEE format with `ew` exponent bits and
 * `mw` significand bits, round-to-nearest-even -- the one routine behind both
 * FST m64fp (11, 52) and FST m32fp (8, 23).  Returns the BIT PATTERN, never a
 * float: the 32-bit cdecl ABI returns floating point in ST(0), and FLD of a
 * signalling NaN quiets it, which would silently mangle an SNaN that the
 * original code only copies from memory to memory. */
static unsigned long long pf_pack(pf_x87_t v, int ew, int mw)
{
    unsigned long long s = (unsigned long long)pf_sgn(v), dm, mmask, mtop;
    int be = pf_bex(v), de, sh, rbit, sticky;
    int bias = (1 << (ew - 1)) - 1, emax = (1 << ew) - 1;
    unsigned long long m = v.m;
    mmask = (1ull << mw) - 1ull;
    mtop = 1ull << mw;
    if (be == 0x7FFF) {
        if ((m << 1) == 0ull)
            return (s << (ew + mw)) | ((unsigned long long)emax << mw);
        dm = (m >> (63 - mw)) & mmask;
        if (dm == 0ull) dm = mtop >> 1;
        return (s << (ew + mw)) | ((unsigned long long)emax << mw) | dm;
    }
    if (be == 0) {
        if (m != 0ull) PF_TRAP(0u, "software x87: extended denormal to IEEE");
        return s << (ew + mw);
    }
    de = be - PF_XBIAS + bias;
    sh = 63 - mw;
    if (de <= 0) {                                  /* IEEE subnormal */
        sh = (63 - mw) + (1 - de);
        de = 0;
        if (sh > 64) return s << (ew + mw);         /* below half the smallest */
    }
    if (sh >= 64) {
        dm = 0ull;
        rbit = (int)((m >> 63) & 1ull);
        sticky = ((m << 1) != 0ull);
    } else {
        dm = m >> sh;
        rbit = (int)((m >> (sh - 1)) & 1ull);
        sticky = ((m & ((1ull << (sh - 1)) - 1ull)) != 0ull);
    }
    if (rbit && (sticky || (dm & 1ull))) dm++;
    if (de == 0) {
        if (dm & mtop) { de = 1; dm &= mmask; }
        return (s << (ew + mw)) | ((unsigned long long)de << mw) | dm;
    }
    if (dm & (mtop << 1)) { dm >>= 1; de++; }
    if (de >= emax) return (s << (ew + mw)) | ((unsigned long long)emax << mw);
    return (s << (ew + mw)) | ((unsigned long long)de << mw) | (dm & mmask);
}
static unsigned long long pf_bits64(pf_x87_t v) { return pf_pack(v, 11, 52); }
static unsigned int pf_bits32(pf_x87_t v) { return (unsigned int)pf_pack(v, 8, 23); }
static double pf_to_f64(pf_x87_t v)
{ union { double d; unsigned long long u; } c; c.u = pf_bits64(v); return c.d; }
static float pf_to_f32(pf_x87_t v)
{ union { float f; unsigned int u; } c; c.u = pf_bits32(v); return c.f; }

/* FIST/FISTP in the current RC.  Out of range or NaN -> integer indefinite,
 * which is what the hardware stores with the invalid exception masked. */
static long long pf_to_i64_rc(pf_x87_t v, unsigned int cw, int *bad)
{
    unsigned int s = pf_sgn(v), rc = PF_CW_RC(cw);
    int be = pf_bex(v), E, sh;
    unsigned long long m = v.m, ip, frac;
    *bad = 0;
    if (be == 0x7FFF) { *bad = 1; return 0; }
    if (be == 0) {
        if (m != 0ull) PF_TRAP(0u, "software x87: extended denormal to integer");
        return 0;
    }
    E = be - PF_XBIAS;
    if (E >= 64) { *bad = 1; return 0; }
    if (E < 0) {                                   /* |v| < 1 */
        ip = 0ull;
        if (E == -1) frac = ((m << 1) == 0ull) ? PF_XTOP : (PF_XTOP + 1ull);
        else         frac = 1ull;                  /* 0 < |v| < 1/2 */
    } else {
        sh = 63 - E;                               /* 0 .. 63 */
        ip = m >> sh;
        frac = (sh == 0) ? 0ull : (m << (64 - sh));
    }
    if (rc == 0u) {
        if (frac > PF_XTOP || (frac == PF_XTOP && (ip & 1ull))) ip++;
    } else if (rc == 1u) {
        if (s && frac) ip++;                       /* toward -inf */
    } else if (rc == 2u) {
        if (!s && frac) ip++;                      /* toward +inf */
    }                                              /* rc == 3: toward zero */
    if (ip >= PF_XTOP) { *bad = 1; return 0; }
    return s ? -(long long)ip : (long long)ip;
}
static unsigned int pf_toi32(pf_x87_t v, unsigned int cw)
{
    int bad;
    long long r = pf_to_i64_rc(v, cw, &bad);
    if (bad || r < -2147483648LL || r > 2147483647LL) return 0x80000000u;
    return (unsigned int)(int)r;
}
static unsigned int pf_toi16(pf_x87_t v, unsigned int cw)
{
    int bad;
    long long r = pf_to_i64_rc(v, cw, &bad);
    if (bad || r < -32768LL || r > 32767LL) return 0x8000u;
    return (unsigned int)(int)r & 0xFFFFu;
}

/* ---- arithmetic --------------------------------------------------------- */
static pf_x87_t pf_neg(pf_x87_t v) { v.se = (unsigned short)(v.se ^ 0x8000u); return v; }
static pf_x87_t pf_abs(pf_x87_t v) { v.se = (unsigned short)(v.se & 0x7FFFu); return v; }

static pf_x87_t pf_add(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb;
    int ea, eb, sticky = 0, d;
    unsigned long long ma, mb;
    pf_u128 A, B, S;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    if (pf_is_inf(a)) {
        if (pf_is_inf(b) && pf_sgn(a) != pf_sgn(b)) return pf_indef();
        return a;
    }
    if (pf_is_inf(b)) return b;
    if (pf_is_zero(a) && pf_is_zero(b))
        return pf_zero((pf_sgn(a) && pf_sgn(b)) ? 1u : 0u);
    if (pf_is_zero(a)) return b;
    if (pf_is_zero(b)) return a;
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    if (eb > ea) {
        unsigned int ts = sa; int te = ea; unsigned long long tm = ma;
        sa = sb; ea = eb; ma = mb;
        sb = ts; eb = te; mb = tm;
    }
    A.hi = ma; A.lo = 0ull;
    B.hi = mb; B.lo = 0ull;
    d = ea - eb;
    B = pf_shr(B, d, &sticky);
    if (sa == sb) {
        unsigned long long lo, t, hi;
        int ovf;
        lo = A.lo + B.lo;
        t = B.hi + ((lo < A.lo) ? 1ull : 0ull);
        ovf = (t < B.hi);
        hi = A.hi + t;
        if (hi < A.hi) ovf = 1;
        S.hi = hi; S.lo = lo;
        if (ovf) {
            if (S.lo & 1ull) sticky = 1;
            S.lo = (S.lo >> 1) | (S.hi << 63);
            S.hi = (S.hi >> 1) | PF_XTOP;
            ea++;
        }
        return pf_round128(sa, ea, S, sticky);
    }
    /* opposite signs.  d >= 1 implies |A| > |B| because bit127 of A is set
     * and B has been shifted right; d == 0 implies no sticky bits. */
    if (A.hi > B.hi || (A.hi == B.hi && A.lo >= B.lo)) {
        S.lo = A.lo - B.lo;
        S.hi = A.hi - B.hi - ((A.lo < B.lo) ? 1ull : 0ull);
        if (sticky) {                 /* true B is a hair larger than B */
            if (S.lo == 0ull) { S.hi--; S.lo = ~0ull; } else { S.lo--; }
        }
        if (S.hi == 0ull && S.lo == 0ull) return pf_zero(0u);
        return pf_round128(sa, ea, S, sticky);
    }
    S.lo = B.lo - A.lo;
    S.hi = B.hi - A.hi - ((B.lo < A.lo) ? 1ull : 0ull);
    if (S.hi == 0ull && S.lo == 0ull) return pf_zero(0u);
    return pf_round128(sb, ea, S, 0);
}
static pf_x87_t pf_sub(pf_x87_t a, pf_x87_t b) { return pf_add(a, pf_neg(b)); }

static pf_x87_t pf_mul(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb, s;
    int ea, eb;
    unsigned long long ma, mb;
    pf_u128 p;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    s = pf_sgn(a) ^ pf_sgn(b);
    if (pf_is_inf(a)) return pf_is_zero(b) ? pf_indef() : pf_inf(s);
    if (pf_is_inf(b)) return pf_is_zero(a) ? pf_indef() : pf_inf(s);
    if (pf_is_zero(a) || pf_is_zero(b)) return pf_zero(s);
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    p = pf_mul64(ma, mb);
    return pf_round128(s, ea + eb - PF_XBIAS + 1, p, 0);
}

static pf_x87_t pf_div(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb, s;
    int ea, eb, i, extra = 0, rbit = 0;
    unsigned long long ma, mb, q = 0ull, rem, carry;
    pf_u128 v;
    if (pf_is_nan(a)) return a;
    if (pf_is_nan(b)) return b;
    s = pf_sgn(a) ^ pf_sgn(b);
    if (pf_is_inf(a)) return pf_is_inf(b) ? pf_indef() : pf_inf(s);
    if (pf_is_inf(b)) return pf_zero(s);
    if (pf_is_zero(b)) return pf_is_zero(a) ? pf_indef() : pf_inf(s);  /* #Z masked */
    if (pf_is_zero(a)) return pf_zero(s);
    pf_unpack(a, &sa, &ea, &ma);
    pf_unpack(b, &sb, &eb, &mb);
    /* restoring division: ma/mb is in [1,2) when `extra`, else in [0.5,1).
     * 64 quotient bits are not enough -- pf_round128 needs a round bit BELOW
     * the 64-bit significand, so one extra bit is produced and `rem` supplies
     * the sticky. */
    rem = ma;
    if (rem >= mb) { extra = 1; rem -= mb; }
    for (i = 0; i < 65; i++) {
        if (i == 64) { rbit = 0; }
        carry = rem >> 63;
        rem <<= 1;
        if (i < 64) q <<= 1;
        if (carry || rem >= mb) {
            rem -= mb;
            if (i < 64) q |= 1ull; else rbit = 1;
        }
    }
    if (extra) {
        v.hi = PF_XTOP | (q >> 1);
        v.lo = ((q & 1ull) ? PF_XTOP : 0ull) | (rbit ? (1ull << 62) : 0ull);
        return pf_round128(s, ea - eb + PF_XBIAS, v, rem != 0ull);
    }
    v.hi = q;
    v.lo = rbit ? PF_XTOP : 0ull;
    return pf_round128(s, ea - eb + PF_XBIAS - 1, v, rem != 0ull);
}

static unsigned int pf_fcmp(pf_x87_t a, pf_x87_t b)
{
    unsigned int sa, sb;
    int ea, eb, r;
    if (pf_is_nan(a) || pf_is_nan(b)) return 0x4500u;
    if (pf_is_zero(a) && pf_is_zero(b)) return 0x4000u;
    sa = pf_sgn(a); sb = pf_sgn(b);
    if (sa != sb) return sa ? 0x0100u : 0x0000u;
    ea = pf_bex(a); eb = pf_bex(b);
    if (ea != eb) r = (ea < eb) ? -1 : 1;
    else if (a.m != b.m) r = (a.m < b.m) ? -1 : 1;
    else r = 0;
    if (sa) r = -r;
    if (r > 0) return 0x0000u;
    if (r < 0) return 0x0100u;
    return 0x4000u;
}

#define PF_FI16(x)  pf_from_i16((short)(x))
#define PF_FI32(x)  pf_from_i32((int)(x))
#define PF_FI64(x)  pf_from_i64((long long)(x))
#define PF_FF32(x)  pf_from_f32((float)(x))
#define PF_FF64(x)  pf_from_f64((double)(x))
#define PF_ZERO     pf_zero(0u)
#define PF_ONE      pf_from_i32(1)
#define PF_ADD(a,b) pf_add((a), (b))
#define PF_SUB(a,b) pf_sub((a), (b))
#define PF_MUL(a,b) pf_mul((a), (b))
#define PF_DIV(a,b) pf_div((a), (b))
#define PF_NEG(a)   pf_neg(a)
#define PF_ABS(a)   pf_abs(a)
#define PF_TOF32(v) pf_to_f32(v)
#define PF_TOF64(v) pf_to_f64(v)
#define PF_TOI32(v, cw) pf_toi32((v), (cw))
#define PF_TOI16(v, cw) pf_toi16((v), (cw))

#endif /* PF_X87_SOFT_H */
'''.replace("@GEN@", GEN)



# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------

def load_globals_index(interop_dir):
    idx = []
    p = os.path.join(interop_dir, "interop_index.json")
    if os.path.exists(p):
        d = json.load(open(p))
        for g in d.get("globals", []):
            try:
                idx.append((int(g["va"], 16), g["name"], g.get("type", "")))
            except Exception:
                pass
    idx.sort()
    return idx


def load_iat(pe):
    iat = {}
    try:
        pe.parse_data_directories()
        for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []) or []:
            for imp in e.imports:
                if imp.name:
                    iat[imp.address] = imp.name.decode()
    except Exception:
        pass
    return iat


def main():
    ap = argparse.ArgumentParser(description="per-function x86-32 -> C lifter")
    ap.add_argument("--image", required=True)
    ap.add_argument("--functions", required=True)
    ap.add_argument("--interop", required=True)
    ap.add_argument("--func", required=True,
                    help="function name, or 0xVA")
    ap.add_argument("--out", default="lifted")
    args = ap.parse_args()

    raw = open(args.image, "rb").read()
    sha = hashlib.sha256(raw).hexdigest()
    pe = pefile.PE(args.image, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image(ImageBase=base)

    funcs = json.load(open(args.functions))
    sel = None
    if args.func.lower().startswith("0x"):
        want = int(args.func, 16)
        for f in funcs:
            if int(f["va"], 16) == want:
                sel = f
                break
    else:
        for f in funcs:
            if f.get("name") == args.func:
                sel = f
                break
    if sel is None:
        sys.stderr.write("no such function: %s\n" % args.func)
        return 2
    func = {"name": sel["name"], "va": int(sel["va"], 16), "size": sel["size"]}

    protos = load_protos(args.interop)
    gidx = load_globals_index(args.interop)
    iat = load_iat(pe)

    os.makedirs(args.out, exist_ok=True)
    rt = os.path.join(args.out, "pf_rt.h")
    if not os.path.exists(rt) or open(rt).read() != PF_RT_H:
        open(rt, "w").write(PF_RT_H)
    sf = os.path.join(args.out, "pf_x87_soft.h")
    if not os.path.exists(sf) or open(sf).read() != PF_X87_SOFT_H:
        open(sf, "w").write(PF_X87_SOFT_H)

    lf = Lifter(image, base, func, protos, iat, gidx, sha)
    try:
        text, meta = lf.run()
    except Refusal as r:
        meta = {"generator": GEN, "function": func["name"], "va": "0x%08x" % func["va"],
                "size": func["size"], "image_sha256": sha, "ok": False,
                "refusals": [{"addr": "0x%08x" % r.addr, "insn": r.mnem, "why": r.why}]}
        with open(os.path.join(args.out, "lifted_%s.json" % func["name"]), "w") as fh:
            json.dump(meta, fh, indent=1, sort_keys=True)
        sys.stderr.write("%s\n" % r)
        return 1

    meta["ok"] = True
    cpath = os.path.join(args.out, "lifted_%s.c" % func["name"])
    open(cpath, "w").write(text)
    with open(os.path.join(args.out, "lifted_%s.json" % func["name"]), "w") as fh:
        json.dump(meta, fh, indent=1, sort_keys=True)
    print("lifted %s @0x%08x: %d insns, %d/%d blocks -> %s (%d lines)"
          % (func["name"], func["va"], meta["instructions_lifted"],
             meta["blocks_reachable"], meta["blocks_total"], cpath, meta["generated_lines"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
