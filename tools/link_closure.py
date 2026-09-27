#!/usr/bin/env python3
"""The port's link closure: the complete set of symbols the link is missing, in ONE pass, with
no link.

    tools/link_closure.py [--build DIR] [--report] [--json OUT] [--verify-ld LOG] [--selftest]

Why this exists. `tools/link_check.sh` measures the port's undefined count by *running the
linker* and scraping its diagnostics. That is the ground truth, and it is also a ~100 s build plus
a link every time, and - the part that actually cost six waves of work - it only ever tells you
about the tree as it is *now*. Landing a key function emits a vtable, the vtable's 83 relocations
become 83 new undefined symbols, and the only way to learn the new set is to fix some of the old
ones, relink, read the log, fix more, relink. Each wave is a full port build.

The set is a pure function of the object files, so it is knowable in one pass. This reads the
link line out of `build.ninja`, walks it the way `ld` does (objects, then archives with their
real "pull a member in only if it defines something still undefined" rule, then shared objects),
and reports the residue - without running the linker at all.

## Measured versus derived, so the two are not confused

* **Measured**: the set of symbols nothing in the link defines but something in the link
  references. That is exactly what `ld` reports, and `--verify-ld` diffs the two sets. On the
  tree as of 2026-09-27 the diff is empty.
* **Derived**: the grouping (by requiring object), the vtable-vs-ordinary split, and the
  data-vs-function classification. These are attributions on top of the measured set and are
  labelled as such. The data/function split is not a guess: `config/G2ME01/symbols.txt` carries
  retail's own `type:function` / `type:object` and `size:`, and that is the authority - a data
  symbol stubbed as a function has cost this project four times.

## The two traps this file is built around, both of which are false as usually stated

**Trap 1: "the vtable slots are invisible to `nm`, because they are relocations and not
symbols."** Not true for a relocatable `.o`, and the exact reason is worth writing down. A vtable
slot naming a function that is not yet defined is present in the object's *symbol table* as
`STB_GLOBAL` / `shndx = SHN_UNDEF`, so `nm -u` prints it. Measured on the real object:

    $ nm -u .../MetaRender/Carve80270848.cpp.o | wc -l
    93
    $ readelf -rW .../MetaRender/Carve80270848.cpp.o | \
        awk '/rela.data.rel.ro._ZTV13CCubeRenderer/{f=1;next} f&&/^$/{exit} f{print $5}' | sort -u | wc -l
    84
    # 82 of the 93 are the vtable's slots, and all 82 are in the `nm -u` output.

What `nm` genuinely cannot tell you is *where* the reference came from: it does not know that 82 of
those came out of `.rela.data.rel.ro._ZTV13CCubeRenderer`, so it cannot tell a vtable slot from a
call. That is the only reason this tool reads relocation sections, and it is a reason for the
*attribution*, not for the *count*. `ld` is not fooled either: it prints the slot as

    ld.bfd: vt.o:(.data.rel.ro._ZTV7Derived[_ZTV7Derived]+0x28): undefined reference to `Derived::B()'

with the section name in its own message. So the vtable-vs-ordinary split below is a grouping of
something `ld` already tells you, not a recovery of something it hid.

**Trap 2: "`ld` truncates, so its set is not the whole set."** Also not true of `ld.bfd`, and
measured rather than assumed, because "it might stop at a limit" is exactly the belief that makes
a tool report a subset and call it an answer:

* `ld.bfd` printed **2000 of 2000** undefined references from one link of a 2000-symbol object,
  with no truncation (self-test case "ld.bfd does NOT truncate").
* The real port link printed **507 reference lines / 322 unique symbols** in one run and exited
  through `collect2: error: ld returned 1 exit status`. There is no partial-failure mode here.

So the six waves were not the linker hiding things. They were fixing things and relinking. The
count was knowable in one pass the whole time; this tool's job is to *group and attribute* the
set, not to discover it.

## The parts that are easy to get wrong, and what is done about each

* **`nm` output has three shapes** - `ADDR T name`, `         U name`, and local symbols. This
  file does not parse `nm` at all: it reads the ELF symbol table structurally, so the three
  shapes cannot be mis-columned. `nm` appears only in the self-test, as an independent
  cross-check of the claim in Trap 1.
* **Relocations in non-allocated sections.** `.rela.debug_*` names only section symbols, but
  `.rela.eh_frame` *is* `SHF_ALLOC` and its `__gxx_personality_v0` reference is one `ld` really
  does report. Only allocated sections are considered, which is also exactly what `ld` will
  complain about.
* **A symbol both defined and undefined in one object.** The definition wins; the object
  contributes no reference. Definitions are taken before the object's own references.
* **Weak undefined symbols.** `ld` does not report them - and this is the one place where `nm -u`
  and `ld` genuinely differ - so neither does this. A symbol counts as reportable only if at
  least one of its referencing sites is non-weak, because a weak reference in one object and a
  strong one in another *is* an error.
* **Archive members.** A `.a` member is pulled in only if it defines something still undefined,
  and pulling it in can create more undefined symbols, which can need more members. Implemented
  as a real fixpoint over the member list, in member order, with `--start-group`/`--end-group`
  iterated as a unit. The archives here are 318 MB - Dawn is most of it - so *member selection*
  uses the archive's own symbol index, the same index `ld` uses, and only selected members are
  parsed.
* **The compiler's implicit start files.** `crt1.o`, `crti.o`, `crtbeginS.o`, `crtendS.o`,
  `crtn.o` are added by the `c++` driver and appear on no link line, so they are asked for by
  name with `-print-file-name` and included. `ld`'s own synthesised symbols (`__bss_start`,
  `_edata`, `_end`, `__TMC_END__`, ...) are added as a set.
* **`-lfoo`** is resolved through the same driver, so `-lrt`/`-ldl`/`-lm` are the files the link
  uses and not whatever happens to be first in a search path.
* **COMDAT groups.** `ld` discards a duplicate group and its relocations with it. Modelled here: a
  group's first definition in link order wins, and a later object's group sections - and the
  relocations into them - are dropped. **Measured effect on this tree: none on the set, and it
  could not be otherwise** - two copies of a COMDAT group have the same relocations, so a
  discarded copy's references are the kept copy's references, and the count is unchanged. It
  changes the *attribution* for 2 of the 322: without it, `CActor::Think(float, CStateManager&)`
  is credited to 4 requiring objects instead of 2, because `CAi.cpp.o` and `CScriptSkyRipple.cpp.o`
  each hold a discarded copy. Since the whole point of grouping is telling a lane *which file to
  edit`, an extra two wrong files per vtable symbol is the defect worth paying for, and 68
  requiring objects instead of 67 is how it shows up.
* **Staleness.** An object older than its source is a measurement of the wrong tree, exactly as
  `tools/link_gap.py` records. A stale tree is refused, not reported.
* **An unreadable link input is an error, not a warning.** Every symbol such an input would have
  defined is then counted as missing, so the number is an over-report - and an over-report is the
  one failure nobody notices, because a lane closing symbols finds some of them already resolved
  and shrugs. Exit 4, with `--allow-missing-inputs` to downgrade it deliberately.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import struct
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_BUILD = pathlib.Path(os.environ.get(
    "MP_PORT_BUILD", os.environ.get("MP_LINK_BUILD", str(ROOT / "build-port-link"))))
TARGET = "metroid_prime2_port"

# ---------------------------------------------------------------- ELF constants

SHN_UNDEF = 0
SHT_SYMTAB, SHT_RELA, SHT_GROUP, SHT_DYNSYM = 2, 4, 17, 11
SHF_ALLOC, SHF_EXECINSTR = 0x2, 0x4
GRP_COMDAT = 0x1
STB_LOCAL, STB_GLOBAL, STB_WEAK, STB_GNU_UNIQUE = 0, 1, 2, 10
STT_NOTYPE, STT_OBJECT, STT_FUNC, STT_SECTION = 0, 1, 2, 3
SHT_REL = 9

# The Itanium ABI's table specialisations. GCC names a COMDAT section after the symbol it holds,
# so the *section* name is how a relocation gets attributed to a vtable. Attribution only: these
# names never enter the measured set.
VT_SECTION = re.compile(r"_ZTV|_ZTI|_ZTS|_ZTT|_ZTC")
VT_SYMBOL = re.compile(r"^(_ZTV|_ZTI|_ZTS|_ZTT|_ZTC|_ZGTt|_ZGTn)")
THUNK = re.compile(r"^(_ZT[hvp]|__cxa_|_ZTh|_ZTv)")

LD_SYNTHESISED = {
    "__bss_start", "_bss_start", "__bss_start__", "_edata", "edata", "__edata", "_end", "end",
    "__end__", "etext", "_etext", "__etext__", "__TMC_END__", "_GLOBAL_OFFSET_TABLE_",
    "__ehdr_start", "__executable_start", "__dso_handle", "_init", "_fini", "_IO_stdin_used",
    "__GNU_EH_FRAME_HDR", "__init_array_start", "__init_array_end",
    "__fini_array_start", "__fini_array_end",
}
CRT_NAMES = ("crt1.o", "crti.o", "crtn.o", "crtbegin.o", "crtbeginS.o",
             "crtend.o", "crtendS.o")


# ---------------------------------------------------------------- ELF reading

class Sym:
    __slots__ = ("name", "shndx", "bind", "type")

    def __init__(self, name, shndx, bind, type_):
        self.name, self.shndx, self.bind, self.type = name, shndx, bind, type_

    @property
    def defined(self):
        return self.shndx != SHN_UNDEF

    @property
    def globalish(self):
        return self.bind in (STB_GLOBAL, STB_WEAK, STB_GNU_UNIQUE)

    @property
    def weak(self):
        return self.bind == STB_WEAK


class Section:
    __slots__ = ("name", "type", "flags", "off", "size", "link", "info", "entsize")

    def __init__(self, name, type_, flags, off, size, link, info, entsize):
        self.name, self.type, self.flags = name, type_, flags
        self.off, self.size, self.link, self.info, self.entsize = off, size, link, info, entsize

    @property
    def alloc(self):
        return bool(self.flags & SHF_ALLOC)

    @property
    def exec(self):
        return bool(self.flags & SHF_EXECINSTR)


class BadElf(ValueError):
    """A file this tool was told to read is not a readable ELF. Never swallowed."""


class _Window:
    """A seekable, readable view of a byte range inside a larger file.

    An archive member is not a file, and copying it out to a temporary one to parse would put
    318 MB of Dawn through the filesystem for no reason. The window keeps the member's offset
    inside the archive and presents the same `seek`/`read` the parser already uses.
    """
    __slots__ = ("fh", "base", "size", "name")

    def __init__(self, fh, base, size, name):
        self.fh, self.base, self.size, self.name = fh, base, size, name

    def seek(self, off, whence=0):
        if whence == 1:
            off += self.fh.tell() - self.base
        elif whence == 2:
            off += self.size
        self.fh.seek(self.base + max(off, 0))
        return self.fh.tell() - self.base

    def tell(self):
        return self.fh.tell() - self.base

    def read(self, n=-1):
        if n is None or n < 0:
            n = self.size - self.tell()
        return self.fh.read(min(n, self.size - self.tell()))


class Elf:
    """Enough ELF for symbols and relocations, and nothing else.

    Deliberately not `nm`/`readelf` output parsing. The two shapes this project has already got
    wrong by hand are an `awk` that stopped at the first blank line and an address-column parse,
    and both are parse-the-text failures; reading the tables removes the class of bug.
    """
    __slots__ = ("name", "win", "sections", "symbols", "groups", "_relocs", "_defined")

    def __init__(self, win, name):
        self.win, self.name = win, name
        self.sections, self.symbols, self.groups = [], [], {}
        self._relocs = None
        self._defined = None
        self._parse()

    @classmethod
    def from_path(cls, path):
        fh = open(path, "rb")
        try:
            size = os.fstat(fh.fileno()).st_size
        except OSError:
            size = 0
        try:
            return cls(_Window(fh, 0, size, str(path)), str(path))
        except Exception:
            fh.close()
            raise

    def close(self):
        try:
            self.win.fh.close()
        except OSError:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def _parse(self):
        w = self.win
        head = w.seek(0) or w.read(64)
        if len(head) < 64 or head[:4] != b"\x7fELF":
            raise BadElf(f"{self.name}: not an ELF file")
        if head[4] != 2 or head[5] != 1:
            raise BadElf(f"{self.name}: not a little-endian 64-bit ELF "
                         f"(EI_CLASS={head[4]} EI_DATA={head[5]})")
        e_shoff, = struct.unpack_from("<Q", head, 0x28)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", head, 0x3A)
        if not e_shoff or not e_shnum:
            raise BadElf(f"{self.name}: no section header table")
        w.seek(e_shoff)
        raw = w.read(e_shentsize * e_shnum)
        if len(raw) < e_shentsize * e_shnum:
            raise BadElf(f"{self.name}: section header table is truncated "
                         f"({len(raw)} of {e_shentsize * e_shnum} bytes)")
        # sh_offset is at 0x18 and sh_size at 0x20, both 8 bytes wide. Reading them as two 4-byte
        # ints takes sh_offset's high half as sh_size - which for any table below 4 GB is 0, so
        # every section comes out nameless and the tool sees no vtable section at all.
        shstr_off, shstr_size = struct.unpack_from("<QQ", raw, e_shstrndx * e_shentsize + 0x18)
        w.seek(shstr_off)
        shstr = w.read(shstr_size)
        for i in range(e_shnum):
            (nameoff, stype, flags, _addr, off, size, link, info, _align,
             entsize) = struct.unpack_from("<IIQQQQIIQQ", raw, i * e_shentsize)
            end = shstr.find(b"\0", nameoff) if nameoff < len(shstr) else -1
            name = shstr[nameoff:end].decode("utf-8", "replace") if end >= 0 else ""
            self.sections.append(Section(name, stype, flags, off, size, link, info, entsize))
        self._read_groups()
        self._read_symbols(SHT_SYMTAB, SHT_DYNSYM)

    def _read_groups(self):
        """{section index: (signature symbol, is_comdat)} - needed to model `ld`'s group
        discarding, which is the one rule that can make a relocation-derived set a superset."""
        for idx, sec in enumerate(self.sections):
            if sec.type != SHT_GROUP or not sec.size:
                continue
            w = self.win
            w.seek(sec.off)
            data = w.read(sec.size)
            flags, = struct.unpack_from("<I", data, 0)
            rest = data[4:]
            for k in range(0, len(rest) - 3, 4):
                (member,) = struct.unpack_from("<I", rest, k)
                self.groups[member] = (rest[0:4] if k == 0 else None, bool(flags & GRP_COMDAT))

    def _read_symbols(self, want_symtab, want_dynsym):
        w = self.win
        for wanted in (want_symtab, want_dynsym):
            for sec in self.sections:
                if sec.type != wanted or not sec.size:
                    continue
                if sec.link >= len(self.sections):
                    continue
                strtab = self.sections[sec.link]
                w.seek(strtab.off)
                strs = w.read(strtab.size)
                entsize = sec.entsize or 24
                count = sec.size // entsize
                w.seek(sec.off)
                data = w.read(sec.size)
                syms = []
                for i in range(count):
                    b = i * entsize
                    nameoff, info, _other, shndx = struct.unpack_from("<IBBH", data, b)
                    end = strs.find(b"\0", nameoff) if nameoff < len(strs) else -1
                    name = strs[nameoff:end].decode("utf-8", "replace") if end >= 0 else ""
                    syms.append(Sym(name, shndx, info >> 4, info & 0xF))
                if wanted == want_symtab:
                    self.symbols = syms
                    return
                if not self.symbols:
                    self.symbols = syms

    def relocs(self, keep=None):
        """[(section_name, section_is_exec, reloc_type, symbol_name)] for allocated sections.

        `keep(section_index)` may veto a section, which is how a COMDAT group that `ld` would
        discard is excluded.
        """
        if self._relocs is not None:
            return self._relocs
        out = []
        w = self.win
        for sec in self.sections:
            if sec.type not in (SHT_RELA, SHT_REL) or not sec.size:
                continue
            if keep is not None and not keep(sec.info):
                continue
            # The allocated test belongs on the section being relocated, not on the relocation
            # section: in a relocatable object `.rela.*` is NOT SHF_ALLOC (readelf prints its
            # flags as `IG`, with no `A`), so filtering on the relocation section itself drops
            # every relocation in the whole port and reports a vacuous zero. `.rela.debug_*`
            # is still excluded, because the section it points at is not allocated - and
            # `.rela.eh_frame` is still included, because `.eh_frame` is.
            target = self.sections[sec.info] if sec.info < len(self.sections) else None
            if target is None or not target.alloc:
                continue
            fname, fexec = target.name, target.exec
            entsize = sec.entsize or (24 if sec.type == SHT_RELA else 16)
            step = 24 if sec.type == SHT_RELA else 16
            count = sec.size // entsize
            w.seek(sec.off)
            data = w.read(sec.size)
            for i in range(count):
                rinfo = struct.unpack_from("<Q", data, i * entsize + 8)[0]
                symidx, rtype = rinfo >> 32, rinfo & 0xFFFFFFFF
                if symidx == 0 or symidx >= len(self.symbols):
                    continue
                sym = self.symbols[symidx]
                if not sym.name or sym.type == STT_SECTION:
                    continue
                out.append((fname, fexec, rtype, sym.name, sym))
        self._relocs = out
        return out

    def defined_global(self):
        if self._defined is None:
            self._defined = {s.name for s in self.symbols
                             if s.defined and s.globalish and s.name}
        return self._defined

    def comdat_signatures(self):
        """{signature symbol name} for the COMDAT groups this object defines."""
        names = set()
        for member, (sig, is_comdat) in self.groups.items():
            if not is_comdat or not sig:
                continue
            (idx,) = struct.unpack("<I", sig)
            if 0 < idx < len(self.symbols) and self.symbols[idx].name:
                names.add(self.symbols[idx].name)
        return names


# ---------------------------------------------------------------- archives

def archive_members(path):
    """[(header_offset, data_offset, size, member_name)] for every member.

    The header offset is kept because the symbol index refers to *headers*, not to member data;
    keying the members by data offset while the index says header offset matches nothing, and
    the archive then silently contributes no symbols at all.
    """
    out = []
    with open(path, "rb") as fh:
        if fh.read(8) != b"!<arch>\n":
            raise BadElf(f"{path}: not an archive")
        pos = 8
        while True:
            # Seek, never read sequentially: after a 60-byte header the file position is the
            # start of the member's *data*, not the next header, so a sequential walk lands
            # 26 bytes into the archive and then reads a member header out of the symbol index.
            fh.seek(pos)
            header = fh.read(60)
            if len(header) < 60:
                break
            raw = header[:16].decode("ascii", "replace")
            name = raw.rstrip("/").strip() or "//"
            try:
                size = int(header[48:58].decode("ascii", "replace").strip() or "0")
            except ValueError:
                raise BadElf(f"{path}: bad member header at offset {pos}")
            out.append((pos, pos + 60, size, name))
            pos += 60 + size + (size & 1)
    return out


def archive_index(path):
    """{symbol name: (member offset, member name)} from the archive's own symbol index.

    The link's archives are 318 MB, almost all Dawn and abseil. Parsing every member's ELF to
    find out what it defines would take minutes and answer the same question; the index at the
    head of the `.a` is exactly the list of global symbols per member, and it is what `ld` uses.
    """
    with open(path, "rb") as fh:
        fh.read(8)
        header = fh.read(60)
        if len(header) < 60:
            return {}
        name = header[:16].decode("ascii", "replace").rstrip("/ ").strip()
        try:
            size = int(header[48:58].decode("ascii", "replace").strip() or "0")
        except ValueError:
            return {}
        body = fh.read(size) if name in ("", "/", "/SYM64/", "__.SYMDEF", "__.SYMDEF SORTED") \
            else b""
        if not body or len(body) < 4:
            return {}
        (count,) = struct.unpack_from(">I", body, 0)
        if not count or len(body) < 4 + count * 4:
            return {}
        offs = struct.unpack_from(f">{count}I", body, 4)
        if name.startswith("__.SYMDEF"):
            # Portable format: offsets are from the start of the member list, and each name is a
            # NUL-terminated string at that offset.
            strs = body[4 + count * 4:]
            out = {}
            for off in offs:
                end = strs.find(b"/\n", off)
                if end < 0:
                    end = strs.find(b"\0", off)
                sym = strs[off:end].decode("utf-8", "replace")
                out[sym] = off
            return out
        pos = 4 + count * 4
        out = {}
        for off in offs:
            end = body.find(b"\0", pos)
            if end < 0:
                end = len(body)
            out[body[pos:end].decode("utf-8", "replace")] = off
            pos = end + 1
        return out


# ---------------------------------------------------------------- the link line

class LinkLineError(Exception):
    pass


def _ninja_unescape(value):
    out, i = [], 0
    while i < len(value):
        c = value[i]
        if c == "$" and i + 1 < len(value):
            nxt, i = value[i + 1], i + 2
            out.append(" " if nxt == " " else ("$" if nxt == ":" else nxt))
            continue
        out.append(c)
        i += 1
    return "".join(out)


def read_link_line(build):
    """The link command's inputs, in order, read from `build.ninja`.

    Parsed from the manifest rather than from `ninja -t commands`, so the tool needs nothing but
    the build tree it is pointed at: no ninja, no cmake, no compiler on PATH.
    """
    manifest = build / "build.ninja"
    if not manifest.exists():
        raise LinkLineError(f"no build.ninja at {manifest}; the port is not configured there")
    text = manifest.read_text(errors="replace")
    pos = text.find(f"build {TARGET}:")
    if pos < 0:
        raise LinkLineError(f"{manifest} has no `build {TARGET}:` edge")
    end = text.find("\n\n", pos)
    statement = text[pos:end if end > 0 else len(text)]
    lines = statement.split("\n")

    def abspath(p):
        return p if os.path.isabs(p) else os.path.normpath(os.path.join(str(build), p))

    toks = lines[0].split(":", 1)[1].split()
    if toks and toks[0].startswith(("CXX_", "C_", "EXE_")):
        toks = toks[1:]
    # `build <out>: <rule> <inputs...>`. The rule name is one token in CMake's output, but it is
    # identified here by *what it is* rather than by counting, because a token left in place
    # becomes a phantom link input: `RelWithDebInfo` is not a file, so it is either dropped as
    # noise or reported as a missing input, and both are wrong.
    rules = set()
    rules_ninja = build / "CMakeFiles" / "rules.ninja"
    if rules_ninja.exists():
        rules = {m.group(1) for m in
                 (re.match(r"^rule (\S+)$", ln) for ln in rules_ninja.read_text(
                     errors="replace").split("\n")) if m}
    while toks and (toks[0] in rules or not os.path.exists(abspath(toks[0]))):
        if os.path.exists(abspath(toks[0])):
            break
        toks = toks[1:]
    # Order-only (`||`) and implicit (`|`) dependency lists are not link inputs. A single `|`
    # is the one that bites: this manifest uses it, the tail is ~70
    # `cmake_object_order_depends_target_*` names, and leaving them in makes each one a
    # "missing input" while the real libraries go unread.
    for stop in ("||", "|"):
        if stop in toks:
            toks = toks[:toks.index(stop)]
    if not toks:
        raise LinkLineError(f"{manifest}: the {TARGET} edge names no input files")
    variables = {}
    for ln in lines[1:]:
        # `= ?(.*)$` rather than `= (.*)$`, because a `LINK_LIBRARIES =` with nothing after it is
        # a real edge (no libraries) and must not read as a manifest this tool cannot parse.
        m = re.match(r"^\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s?(.*)$", ln.rstrip())
        if m:
            variables[m.group(1)] = _ninja_unescape(m.group(2))
    if "LINK_LIBRARIES" not in variables:
        raise LinkLineError(f"{manifest}: the {TARGET} edge has no LINK_LIBRARIES, so this CMake "
                            f"does not emit a link edge this tool can read")
    # Both lists are relative to the build directory. Leaving LINK_LIBRARIES relative is the
    # subtle one: a relative `extern/aurora/libaurora_core.a` resolves against the *current*
    # directory, does not exist there, and is then reported as a missing input - which reads as
    # a broken build tree rather than as a path this tool got wrong.
    libs = variables["LINK_LIBRARIES"].split()
    return [abspath(p) for p in toks], [abspath(p) if not p.startswith("-") else p for p in libs], \
        variables


def link_driver(build):
    """The driver named in the link rule, so `-print-file-name` asks the same compiler."""
    rules = build / "CMakeFiles" / "rules.ninja"
    if not rules.exists():
        return None
    for ln in rules.read_text(errors="replace").split("\n"):
        s = ln.strip()
        if s.startswith("command =") and "-o $TARGET_FILE" in s:
            toks = s.split()
            for i, t in enumerate(toks):
                if t.endswith("c++") or t.endswith("g++") or t.endswith("clang++"):
                    return t
    return None


def _print_file_name(driver, name):
    if not driver:
        return None
    try:
        out = subprocess.run([driver, "-print-file-name=" + name],
                             capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return None
    return out if out and out != name and os.path.exists(out) else None


def resolve_lib(driver, name, search):
    """`-lfoo` -> the file the driver would pick.

    `-print-file-name` takes a *file name*, not a `-l` flag: `-print-file-name=lstdc++` prints
    `lstdc++` back and this returns None, so every implicit library of the driver is silently
    absent and every libc and libstdc++ symbol comes out undefined. Ask for `libfoo.so` first,
    which is what the driver itself is doing.
    """
    for cand in (f"lib{name}.so", f"lib{name}.a"):
        p = _print_file_name(driver, cand)
        if p:
            return p
    for d in search:
        for cand in (f"lib{name}.so", f"lib{name}.a"):
            p = os.path.join(d, cand)
            if os.path.exists(p):
                return p
    return None


COMMENT = re.compile(r"/\*.*?\*/", re.S)
GROUP_HEAD = re.compile(r"\b(GROUP|INPUT)\s*\(", re.I)


def _balanced_group(text, open_at):
    """The contents of the parenthesis that starts at `open_at`, honouring nesting.

    `AS_NEEDED ( ... )` is nested inside `GROUP ( ... )` on this platform's `libc.so`, so
    splitting on the first `)` truncates the group and drops the shared library out of it - and a
    dropped `libc.so.6` looks exactly like a complete measurement that happens to be wrong.
    """
    depth, out = 0, []
    for i in range(open_at, len(text)):
        c = text[i]
        if c == "(":
            depth += 1
            if depth == 1:
                continue
        elif c == ")":
            depth -= 1
            if depth == 0:
                return "".join(out)
        if depth >= 1 and c not in "()":
            out.append(c)
    return "".join(out)


def expand_input(path, driver, search, depth=0):
    """A `.so` that is really a linker script, expanded to the files it names.

    `libc.so`, `libm.so` and `libgcc_s.so` on this platform are text `GROUP ( ... )` scripts, not
    ELF, so reading "the shared objects on the link line" as ELF hits three non-ELF files and
    then answers every libc symbol as undefined - 664 spurious symbols, which is what this
    replaced. The comment header is stripped first, or the anchor never matches it.
    """
    if depth > 4:
        return [path]
    try:
        with open(path, "rb") as fh:
            head = fh.read(8192)
    except OSError:
        return [path]
    if head[:4] == b"\x7fELF":
        return [path]
    try:
        text = COMMENT.sub(" ", head.decode("utf-8"))
    except UnicodeDecodeError:
        return [path]
    m = GROUP_HEAD.search(text)
    if not m:
        return [path]
    body = _balanced_group(text, m.end() - 1)
    out = []
    for tok in body.split():
        if tok in ("AS_NEEDED", "-z", "AS_NEEDED("):
            continue
        if tok.startswith("-l"):
            p = resolve_lib(driver, tok[2:], search)
            if p:
                out.extend(expand_input(p, driver, search, depth + 1))
            continue
        if not os.path.isabs(tok):
            found = None
            for d in search:
                if os.path.exists(os.path.join(d, tok)):
                    found = os.path.join(d, tok)
                    break
            if found is None:
                p = _print_file_name(driver, tok)
                found = p
            tok = found
        if tok and os.path.exists(tok):
            out.extend(expand_input(tok, driver, search, depth + 1))
    return out or [path]


def driver_implicit(driver):
    """The start files and libraries the `c++` driver adds that appear on no link line.

    `Scrt1.o`, `crti.o`, `crtbeginS.o`, `-lstdc++ -lm -lgcc_s -lgcc -lc`, `crtendS.o`, `crtn.o`.
    They come from `<driver> -###`, which prints the `collect2` invocation without running it -
    a real measurement of what the driver will do, and the only way to know which libc and which
    libstdc++ this link uses. Returns `([pre_objects], [post_tokens])`; the split is at the
    driver's own temporary object, which is exactly where the user's objects go.
    """
    if not driver:
        return None
    try:
        proc = subprocess.run([driver, "-###", "-o", os.devnull, "-x", "c++", "-"],
                              input="int main(){}\n", capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    line = next((ln for ln in proc.stderr.split("\n") if "collect2" in ln), None)
    if not line:
        return None
    toks = line.split()
    try:
        cut = toks.index("-o") + 2      # skip `-o /dev/null`
    except ValueError:
        cut = len(toks)
    tail = [t.strip('"') for t in toks[cut:]]
    tmp_obj = next((t for t in tail if t.endswith(".o") and os.path.sep in t
                    and t.startswith("/tmp")), None)
    if tmp_obj is None:
        return [], [t for t in tail if t.endswith((".o", ".so", ".a"))
                    or t.startswith(("-l", "-L")) or os.path.basename(t).startswith("lib")]
    split = tail.index(tmp_obj)
    keep = lambda ts: [t for t in ts
                       if t.endswith((".o", ".so", ".a")) or t.startswith(("-l", "-L"))
                       or os.path.basename(t).startswith("lib")]
    return keep(tail[:split]), keep(tail[split + 1:])


# ---------------------------------------------------------------- the closure

class Closure:
    def __init__(self):
        self.defined = set()
        self.comdat_owner = {}          # COMDAT signature symbol -> label that claimed it
        self.refs = {}                  # name -> {"sites": [...], "weak": n, "total": n}
        self.pulled = 0
        self.members_total = 0
        self.missing_inputs = []
        self.notes = []
        # A census of what the ABI tables in this link actually reference, whether or not the
        # reference is currently satisfied. Without it, "2 from vtable relocations" reads as
        # "the tool barely looked at vtables", when the truth is that the tree's vtables are
        # closed - and a reader cannot tell those two apart.
        self.vt_sections = set()
        self.vt_symbols = set()

    def _keep(self, elf, label):
        """Veto sections in COMDAT groups another input already claimed, the way `ld` does."""
        claims = elf.comdat_signatures()
        fresh = {s for s in claims if s not in self.comdat_owner}
        if not fresh and not claims:
            return None
        for s in fresh:
            self.comdat_owner[s] = label
        group_of = {}
        for member, (_sig, is_comdat) in elf.groups.items():
            if not is_comdat:
                continue
            sig = _sig
            idx = struct.unpack("<I", sig)[0] if sig else None
            name = elf.symbols[idx].name if (idx and 0 < idx < len(elf.symbols)) else None
            group_of[member] = self.comdat_owner.get(name, label) if name else label
        return lambda idx: idx not in group_of or group_of[idx] == label

    def add_object(self, elf, label, is_archive=False):
        if is_archive:
            self.pulled += 1
        self.defined |= elf.defined_global()
        keep = self._keep(elf, label)
        for fname, fexec, rtype, symname, sym in elf.relocs(keep=keep):
            if is_vtable_section(fname):
                self.vt_sections.add((label, fname))
                self.vt_symbols.add(symname)
            if not sym.defined:
                # A reference *to* a symbol this object also defines is not a reference at all.
                if symname in self.defined:
                    continue
                entry = self.refs.setdefault(symname, {"sites": [], "weak": 0, "total": 0})
                entry["total"] += 1
                if sym.weak:
                    entry["weak"] += 1
                entry["sites"].append((label, fname, fexec, rtype))

    def add_shared(self, elf, label):
        self.defined |= elf.defined_global()

    def unresolved(self):
        """Symbols referenced and not defined, and with at least one non-weak site.

        `ld` stays silent about a symbol that is only ever referenced weakly, and a weak
        reference in one object plus a strong one in another *is* an error, which is why the test
        is "not all sites weak" rather than "any site weak".
        """
        return {n: i for n, i in self.refs.items()
                if n not in self.defined and i["weak"] < i["total"]}


def scan_archives(clo, archives, build, driver):
    """The archive rule, implemented as a fixpoint rather than a single pass.

    A member is needed only if it defines something still undefined; pulling it in adds its own
    undefined symbols, which can need later members - and with `--start-group` the whole group
    repeats until nothing new is pulled in. Convergence is asserted, not assumed.
    """
    for archive in archives:
        try:
            members = archive_members(archive)
            index = archive_index(archive)
        except (OSError, BadElf) as exc:
            clo.notes.append(f"{os.path.basename(archive)}: {exc}")
            continue
        byoff = {hdr: (data, size, name) for hdr, data, size, name in members}
        clo.members_total += len(members)
        want = {off for off in index.values() if off in byoff}
        base = os.path.basename(archive)
        fh = open(archive, "rb")
        try:
            rounds = 0
            pulled_this = 0
            while True:
                progressed = False
                rounds += 1
                if rounds > len(index) + 2:
                    clo.notes.append(f"{base}: archive fixpoint did not converge after "
                                     f"{rounds} rounds ({pulled_this} members)")
                    break
                for off in sorted(want):
                    if off not in byoff:
                        continue
                    data, size, name = byoff[off]
                    win = _Window(fh, data, size, f"{base}({name})")
                    try:
                        elf = Elf(win, f"{base}({name})")
                    except (BadElf, OSError):
                        continue
                    exported = elf.defined_global()
                    needed = exported & set(clo.unresolved())
                    if not needed and not (exported & set(clo.refs) - clo.defined):
                        continue
                    clo.add_object(elf, label=f"{base}({name})", is_archive=True)
                    pulled_this += 1
                    progressed = True
                if not progressed:
                    break
        finally:
            fh.close()


def run_closure(inputs, libtokens, build, driver):
    """Walk the link line the way `ld` does and return a `Closure`."""
    clo = Closure()
    search = [str(build), "/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/lib/x86_64-linux-gnu",
              "/usr/local/lib", "/usr/lib", "/lib"]
    for p in re.findall(r"-L(\S+)", " ".join(libtokens)):
        search.insert(0, p)
    clo.defined |= LD_SYNTHESISED
    pending: list = []

    def do_object(tok):
        try:
            with Elf.from_path(tok) as e:
                clo.add_object(e, label=short_label(tok, build))
        except (BadElf, OSError) as exc:
            clo.notes.append(str(exc))

    def ingest(tok):
        """One link input, whatever kind of file it turns out to be.

        Dispatch is on the file, not on the spelling: `libgcc_s.so`, `libc.so` and `libm.so` are
        text GROUP() scripts, `librt.a` is an archive, and the driver's tail mixes all three with
        the start files. A tool that dispatches on the suffix opens the linker script as an ELF
        and answers every libc symbol as undefined - 663 spurious symbols, which is what this
        replaced.
        """
        if tok.startswith("-l"):
            p = resolve_lib(driver, tok[2:], search)
            if p is None:
                clo.missing_inputs.append(tok)
            else:
                ingest(p)
            return
        if tok.startswith("-"):
            return
        if not os.path.exists(tok):
            clo.missing_inputs.append(tok)
            return
        if tok.endswith(".a"):
            pending.append(tok)
            return
        if tok.endswith(".o"):
            do_object(tok)
            return
        expanded = expand_input(tok, driver, search)
        if expanded == [tok]:
            try:
                with Elf.from_path(tok) as e:
                    clo.add_shared(e, label=os.path.basename(tok))
            except (BadElf, OSError) as exc:
                clo.notes.append(str(exc))
            return
        for x in expanded:
            ingest(x)

    implicit = driver_implicit(driver)
    if implicit:
        pre, post = implicit
        search = [t[2:] for t in post if t.startswith("-L")] + search
    else:
        # No driver to ask. Fall back to the conventional start files and SAY so, because a
        # silently incomplete fallback reports every libc symbol as missing and reads as a
        # catastrophic finding rather than as a missing compiler.
        clo.notes.append("no usable C++ driver found: the compiler's implicit start files and "
                         "libraries (libstdc++, libc, libgcc) are NOT part of this measurement")
        pre, post = [c for c in CRT_NAMES if _print_file_name(driver, c)], []

    def flush_group():
        if pending:
            scan_archives(clo, list(pending), build, driver)
            pending.clear()

    # The link command is `$in -o $OUT $LINK_LIBRARIES`, and the driver appends its own tail
    # after that. Walking only `LINK_LIBRARIES` - the one variable whose name looks like "the
    # link" - reads no objects at all and reports a vacuous zero, which in a tool whose whole
    # claim is that it cannot report one is the worst bug available.
    for tok in list(pre) + list(inputs) + list(libtokens) + list(post):
        if tok in ("-Wl,--start-group", "--start-group"):
            flush_group()
            continue
        if tok in ("-Wl,--end-group", "--end-group"):
            flush_group()
            continue
        if tok.startswith("-Wl,-l"):
            for m in re.finditer(r"-l(\S+?)(?:,|$)", tok[4:]):
                ingest("-l" + m.group(1))
            continue
        if tok.startswith("-Wl,-L"):
            search.insert(0, tok[6:])
            continue
        if tok.startswith("-") and not tok.startswith(("-l", "-Wl,")):
            # Every other flag. A bare "-" in this tuple's prefix list would swallow `-lfoo` and
            # every implicit library of the driver with it, which is a silent zero rather than
            # an error, and it is invisible: the tool still prints a number.
            continue
        ingest(tok)
    flush_group()
    return clo



def short_label(path, build):
    try:
        rel = os.path.relpath(path, build)
    except ValueError:
        return path
    # `CMakeFiles/mp_game.dir/src/a/b.cpp.o` -> `src/a/b.cpp.o`: the object dir is noise.
    m = re.match(r"^CMakeFiles/[^/]+\.dir/(.*)$", rel)
    return m.group(1) if m else rel


# ---------------------------------------------------------------- classification

def is_vtable_section(name):
    return bool(VT_SECTION.search(name))


def demangle(names):
    todo = sorted({n for n in names if n.startswith("_Z")})
    if not todo:
        return {}
    try:
        out = subprocess.run(["c++filt"], input="\n".join(todo) + "\n",
                             capture_output=True, text=True, timeout=180).stdout
    except (OSError, subprocess.SubprocessError):
        return {}
    lines = out.splitlines()
    return dict(zip(todo, lines)) if len(lines) == len(todo) else {}


SYM_RE = re.compile(r"^(\S+) = [^;]*;\s*//\s*type:(\w+)(?:\s+size:(0x[0-9A-Fa-f]+|\d+))?")
# MWCC: `<base>__<len><Name>(<len><Name>)*<parameter encoding>`. The length-prefixed run stops
# where the parameters start because a parameter encoding begins with a type code, not a digit
# (`fn_801D8EC0__10CGunWeaponFR13CStateManager` -> base `fn_801D8EC0`, class `CGunWeapon`,
# parameters `FR13CStateManager`), and every other encoding starts with a letter too.
MWCC_RE = re.compile(r"^(?P<base>[A-Za-z_~][A-Za-z0-9_~]*)__(?P<rest>.+)$")


def mwcc_class(rest):
    """The class part of a MWCC qualified name: a run of `<length><name>` pairs, read minimally.

    The run has to be read as *exactly* `<len>` characters, not as a greedy alphanumeric blob.
    `Unk9__10CGunWeaponFR13CStateManager` has a class part of `10` + `CGunWeapon` and then a
    parameter encoding that also starts with a length prefix (`FR13CStateManager`); a greedy read
    swallows the parameters, the length check then fails, and **every function in the file is
    silently dropped from the index** - leaving only the objects, so the join answers "data" for
    the whole tree and the tool is confidently wrong about all 322.
    """
    names, pos = [], 0
    while pos < len(rest):
        j = pos
        while j < len(rest) and rest[j].isdigit():
            j += 1
        if j == pos:
            break
        n = int(rest[pos:j])
        name = rest[j:j + n]
        if n == 0 or len(name) != n or not re.match(r"^[A-Za-z_]", name):
            return "::".join(names) if names else None
        names.append(name)
        pos = j + n
    return "::".join(names) if names else None


class RetailTypes:
    """`config/G2ME01/symbols.txt`, joined to the Itanium names the port's objects carry.

    The join is by `(class, member name)`, not by string equality: the port is linked with the
    host compiler, so a missing `CGraphics::mViewport` is `_ZN9CGraphics10mViewportE`, while
    retail spells it `mViewport__9CGraphics`. Retail's spelling carries the class and the member
    name followed by a parameter encoding; the Itanium one carries the same two facts in a
    different order. Parsing both and meeting in the middle is the whole join, and it needs no
    demangler of MWCC's grammar and no parameter-type parser.

    A `(class, member)` with **more than one** retail entry is an overload set and is refused:
    an ambiguous answer to "is this a function or 24 bytes of data" is worse than no answer.
    """
    def __init__(self, path=None):
        self.exact = {}
        self.by_member = {}
        self.ambiguous = 0
        path = pathlib.Path(path or ROOT / "config" / "G2ME01" / "symbols.txt")
        if not path.exists():
            return
        for line in path.read_text(errors="replace").splitlines():
            m = SYM_RE.match(line.strip())
            if not m:
                continue
            name, kind, size = m.group(1), m.group(2), m.group(3)
            if kind not in ("function", "object"):
                continue
            self.exact[name] = (kind, size)
            mm = MWCC_RE.match(name)
            if not mm:
                continue
            cls = mwcc_class(mm.group("rest"))
            if cls is None:
                continue
            self.by_member.setdefault((cls, mm.group("base")), []).append((name, kind, size))

    def lookup(self, dem, mangled=None):
        """(type, size, retail_name) or None."""
        if "::" not in dem:
            got = self.exact.get(dem)
            return (got[0], got[1], dem) if got else None
        scope, member = dem.rsplit("::", 1)
        cls = scope.split("::")[-1].split("<")[0]
        base = member.split("(")[0]
        bases = {base, base.lstrip("~")}
        # MWCC's constructor/destructor prefix is `__ct`/`__dt` and the *separator* is the `__`
        # that follows it, so `__dt__10CGunWeaponFv` parses as base `__dt`, not `__dt__`. The
        # regex eats the separator; writing `__dt__` here silently finds nothing and every
        # constructor and destructor in the tree falls back to a guess.
        if base == cls:
            bases |= {"__ct", "__vt"}
        if base.startswith("~"):
            bases |= {"__dt"}
        hits = []
        for b in sorted(bases):
            hits.extend(self.by_member.get((cls, b), []))
        if hits:
            # 55 (class, member) pairs in this file are overload sets - and an overload set is
            # **not** ambiguous for the only question being asked. A name cannot be both a
            # function and an object in one class, so agreement on the type is the whole
            # criterion. Disagreement is refused, because that would be a real ambiguity and a
            # guess here is a wrongly sized global.
            types = {h[1] for h in hits}
            if len(types) == 1:
                first = hits[0]
                return (first[1], first[2], first[0])
            self.ambiguous += 1
            return None
        # Retail's `fn_*` and `lbl_*` names are unique in the whole binary, so a verbatim lookup
        # is unambiguous whatever class the port chose to put them in. This is not a rare case:
        # `symbols.txt` has `fn_801D8EC0` as a free function, and `CPlayerGun.cpp` declares it
        # `CGunWeapon::fn_801D8EC0()`, so the (class, member) join finds nothing for it.
        if re.match(r"^(fn_|lbl_)[0-9A-Fa-f]+$", base):
            got = self.exact.get(base)
            if got:
                return (got[0], got[1], base)
        return None

    def __bool__(self):
        return bool(self.exact)


def load_retail_types(path=None):
    return RetailTypes(path)


RETAIL_LABEL = re.compile(r"^lbl_[0-9A-Fa-f]+$")


def classify(sites, dem, mangled, retail):
    """`data` or `function`, with the evidence named.

    Order matters and is the point: a *declaration of type* is a fact, a call site is a fact, and
    a name is a guess - so retail's own `type:` is consulted first, then the ABI's table prefixes,
    then retail's `lbl_` convention, then the call site, and the name shape is never enough on
    its own.

    A `CGraphics::mViewport` read through a pointer in `.data` has no call site and no Itanium
    hint; retail's `type:object size:0x18` is what says it is 24 bytes of data, and this project
    has stubbed such a symbol as a function four times. Measured here: retail's 7,780 `lbl_*`
    entries are **all** `type:object` and its 13,449 `fn_*` entries are **all** `type:function`,
    so the `lbl_` rule below is a convention with no counterexample in the file, not a heuristic.

    The last rule biases towards `function` **on purpose**. A data-only reference is more often a
    function-pointer table than a variable - `platform/compiled_modules.cpp`'s module entry table
    is exactly that, and calling it `data` would send whoever closes it looking for a variable.
    The expensive mistake in this project has been the other way round four times, and a symbol
    misfiled as a function costs a lane one look at `symbols.txt`; a data object stubbed as a
    function costs a wrong-sized global and a silent memory bug.
    """
    if THUNK.match(mangled):
        return "function", "Itanium thunk or ABI-runtime name"
    got = retail.lookup(dem, mangled) if retail else None
    if got:
        return ("function" if got[0] == "function" else "data",
                f"retail symbols.txt `{got[2]}` type:{got[0]}" + (f" size:{got[1]}" if got[1] else ""))
    if VT_SYMBOL.match(mangled):
        return "data", "Itanium ABI table symbol (_ZTV/_ZTI/_ZTS/_ZTT/_ZTC)"
    if RETAIL_LABEL.match(mangled) and retail and retail.exact.get(mangled, ("", ""))[0] == "object":
        return "data", "retail `lbl_` label, type:object in symbols.txt"
    if any(s[2] for s in sites):
        return "function", "referenced from an executable section"
    if not any(s[2] for s in sites):
        return "function", ("no call site, but a data-only reference is usually a function-pointer "
                            "table; nothing declares this an object, so it is filed as a function "
                            "rather than guessed the other way")
    return "function", "default: a function"


LD_UNDEF = re.compile(r"undefined reference to `(.+?)'")


def ld_set(log):
    return set(LD_UNDEF.findall(pathlib.Path(log).read_text(errors="replace")))


# ---------------------------------------------------------------- freshness

def check_fresh(build):
    """Refuse a tree whose objects are older than the sources they were built from.

    `tools/link_gap.py` records the same trap: a build directory configured against another tree
    measures that tree's symbols, and a stale object measures an older source. Both read as a
    confident wrong answer.
    """
    newest_src, which = 0.0, None
    for sub in ("src", "include", "platform"):
        base = ROOT / sub
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if p.suffix in (".h", ".hpp", ".c", ".cpp", ".cp", ".cxx") and p.is_file():
                try:
                    m = p.stat().st_mtime
                except OSError:
                    continue
                if m > newest_src:
                    newest_src, which = m, p
    if not newest_src:
        return None
    newest_obj = 0.0
    for sub in ("CMakeFiles/mp_game.dir", "CMakeFiles/mp_platform.dir",
                "CMakeFiles/mp_port_entry.dir", "CMakeFiles/mp_port_audio.dir"):
        base = build / sub
        if base.exists():
            for p in base.rglob("*.o"):
                try:
                    newest_obj = max(newest_obj, p.stat().st_mtime)
                except OSError:
                    pass
    if newest_obj and newest_obj < newest_src:
        return (f"build tree {build} is STALE: its newest object predates {which}. "
                f"Refusing to report a measurement of the wrong tree.")
    return None


# ---------------------------------------------------------------- report

def main(argv=None):
    ap = argparse.ArgumentParser(add_help=True, description=__doc__.split("\n")[0])
    ap.add_argument("--build", default=str(DEFAULT_BUILD))
    ap.add_argument("--report", action="store_true", help="print the full grouped report")
    ap.add_argument("--json", help="write the machine-readable result here")
    ap.add_argument("--top", type=int, default=10)
    ap.add_argument("--verify-ld", metavar="LOG",
                    help="diff this tool's set against a real ld log; must be empty")
    ap.add_argument("--allow-stale", action="store_true")
    ap.add_argument("--allow-missing-inputs", action="store_true",
                    help="downgrade an unresolvable link input from an error to a warning. It is "
                         "an error by default because an input this tool could not read makes the "
                         "count a silent over-report, and an over-report that looks like an "
                         "under-report is the failure this whole file exists to prevent")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    build = pathlib.Path(args.build)
    t0 = time.time()
    if not args.allow_stale:
        stale = check_fresh(build)
        if stale:
            print(f"link_closure: {stale}", file=sys.stderr)
            return 3
    try:
        inputs, libtokens, variables = read_link_line(build)
    except LinkLineError as exc:
        print(f"link_closure: {exc}", file=sys.stderr)
        return 2
    driver = link_driver(build)
    clo = run_closure(inputs, libtokens, build, driver)
    elapsed = time.time() - t0

    missing = clo.unresolved()
    dem = demangle(missing.keys())
    retail = load_retail_types()
    info = {}
    for name, raw in missing.items():
        d = dem.get(name, name)
        kind, why = classify(raw["sites"], d, name, retail)
        info[name] = {
            "demangled": d,
            "kind": kind,
            "why": why,
            "vtable": any(is_vtable_section(s[1]) for s in raw["sites"]),
            "objects": sorted({s[0] for s in raw["sites"]}),
            "sections": sorted({s[1] for s in raw["sites"]}),
        }
    nvtable = sum(1 for i in info.values() if i["vtable"])
    ndata = sum(1 for i in info.values() if i["kind"] == "data")
    by_obj = {}
    for name, i in info.items():
        for o in i["objects"]:
            by_obj.setdefault(o, []).append(name)
    unresolved_names = set(missing)

    nld = {info[n]["demangled"] for n in missing}
    print(f"link_closure: {len(missing)} undefined symbol(s) the link is missing - one pass, "
          f"no link")
    print(f"link_closure:   {nvtable} from vtable relocations, "
          f"{len(missing) - nvtable} from ordinary references")
    print(f"link_closure:   {ndata} data, {len(missing) - ndata} function "
          f"({sum(1 for i in info.values() if 'symbols.txt' in i['why'])} of {len(missing)} from "
          f"retail's own type declaration, {retail.ambiguous} join(s) refused as ambiguous)")
    print(f"link_closure:   vtable census: {len(clo.vt_sections)} vtable/ABI-table section(s) in "
          f"the link reference {len(clo.vt_symbols)} distinct symbol(s), of which "
          f"{len(clo.vt_symbols & unresolved_names)} are unresolved")
    print(f"link_closure:   {len(by_obj)} requiring object(s); {clo.pulled} of "
          f"{clo.members_total} archive members pulled in")
    print(f"link_closure:   measured in {elapsed:.2f}s over {len(inputs)} link-line object(s)")
    for n in clo.notes:
        print(f"link_closure: note: {n}", file=sys.stderr)

    status = 0
    if clo.missing_inputs:
        uniq = sorted(set(clo.missing_inputs))
        if args.allow_missing_inputs:
            print(f"link_closure: WARNING unresolvable link inputs: {' '.join(uniq)}",
                  file=sys.stderr)
        else:
            # Hard failure, not a warning. Every symbol those inputs would have defined is
            # counted as missing, so the number is an over-report - and an over-report is the one
            # failure mode nobody notices, because a lane closing symbols will simply find some
            # of them already resolved and shrug.
            print(f"link_closure: FAILED - {len(uniq)} link input(s) could not be resolved, so "
                  f"this is an over-report and not a measurement:", file=sys.stderr)
            for u in uniq[:20]:
                print(f"  missing input: {u}", file=sys.stderr)
            if args.json:
                write_json(args.json, info, by_obj, nvtable, ndata, build, elapsed, equal=None,
                           unresolvable_inputs=uniq)
            return 4
    if args.verify_ld:
        lds = ld_set(args.verify_ld)
        only_tool = sorted(nld - lds)
        only_ld = sorted(lds - nld)
        print(f"link_closure: verify against {args.verify_ld}")
        print(f"  ld reports        {len(lds)}")
        print(f"  link_closure says {len(nld)}")
        if not only_tool and not only_ld:
            print("  diff: EMPTY - the two sets are equal")
        else:
            status = 1
            print(f"  only link_closure ({len(only_tool)}):")
            for s in only_tool[:60]:
                print(f"    + {s}")
            if len(only_tool) > 60:
                print(f"    ... and {len(only_tool) - 60} more")
            print(f"  only ld ({len(only_ld)}):")
            for s in only_ld[:60]:
                print(f"    - {s}")
            if len(only_ld) > 60:
                print(f"    ... and {len(only_ld) - 60} more")

    if args.report:
        print(f"\n--- requiring object, top {args.top} of {len(by_obj)} ---")
        for obj, syms in sorted(by_obj.items(), key=lambda kv: (-len(kv[1]), kv[0]))[:args.top]:
            vt = sum(1 for s in syms if info[s]["vtable"])
            print(f"  {len(syms):4d}  {obj}    ({vt} vtable, {len(syms) - vt} ordinary)")
        print("\n--- every symbol, by requiring object ---")
        for obj, syms in sorted(by_obj.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            print(f"  {obj}  [{len(syms)}]")
            for s in sorted(syms):
                i = info[s]
                tag = "VT " if i["vtable"] else "   "
                print(f"    {tag}{i['kind'][:4]}  {s}\n           {i['demangled']}  "
                      f"[{i['why']}]  from {', '.join(i['sections'][:2])}")
        print("\n--- data symbols, named ---")
        for name in sorted(n for n, i in info.items() if i["kind"] == "data"):
            i = info[name]
            print(f"  {name}\n      {i['demangled']}  [{i['why']}]  from {', '.join(i['objects'][:3])}")
        print("\n--- from vtable relocations ---")
        for name in sorted(n for n, i in info.items() if i["vtable"]):
            secs = sorted({s for s in info[name]["sections"] if is_vtable_section(s)})
            print(f"  {name}\n      {info[name]['demangled']}  in {', '.join(secs[:2])}")

    if args.json:
        write_json(args.json, info, by_obj, nvtable, ndata, build, elapsed,
                   equal=(True if status == 0 and args.verify_ld else
                          (False if args.verify_ld else None)),
                   vtable_sections=len(clo.vt_sections),
                   vtable_symbols=len(clo.vt_symbols),
                   vtable_unresolved=len(clo.vt_symbols & unresolved_names),
                   only_tool=sorted(nld - ld_set(args.verify_ld)) if args.verify_ld else None,
                   only_ld=sorted(ld_set(args.verify_ld) - nld) if args.verify_ld else None)
    return status


def write_json(path, info, by_obj, nvtable, ndata, build, elapsed, equal, **extra):
    # `by_object` is a list of pairs, not a dict: `sort_keys` would reorder it alphabetically
    # and the one ordering a reader wants is "most symbols first".
    data = {
        "build": str(build),
        "measured_seconds": round(elapsed, 3),
        "total": len(info),
        "from_vtable_relocations": nvtable,
        "from_ordinary_references": len(info) - nvtable,
        "data": ndata,
        "function": len(info) - ndata,
        "requiring_objects": len(by_obj),
        "equals_ld": equal,
        "by_object": [[k, len(v)] for k, v in
                      sorted(by_obj.items(), key=lambda kv: (-len(kv[1]), kv[0]))],
        "symbols": {n: {k: i[k] for k in ("demangled", "kind", "why", "vtable", "objects",
                                          "sections")}
                    for n, i in sorted(info.items())},
    }
    data.update(extra)
    pathlib.Path(path).write_text(json.dumps(data, indent=1) + "\n")
    print(f"link_closure: wrote {path}")


# ---------------------------------------------------------------- self test

def selftest():
    """Assertions, each of which can fail. A self-test that cannot fail is not a test; see
    `docs/PROCESS_LESSONS.md` #1. Every broken-input case below must *raise*, because a tool
    that answers a plausible number about a file it could not read is worse than one that stops.
    """
    import shutil
    import tempfile
    failures = []

    def check(name, cond, detail=""):
        print(f"  {'ok  ' if cond else 'FAIL'}  {name}{'' if cond else ': ' + detail}")
        if not cond:
            failures.append(name)

    tmp = pathlib.Path(tempfile.mkdtemp(prefix="linkclosure-", dir="/tmp/opencode"))
    cxx = shutil.which("c++") or shutil.which("g++")
    if not cxx or not shutil.which("ar"):
        print("link_closure: needs a C++ compiler and ar to self-test")
        return 1
    try:
        # 1. A vtable whose slots are undefined: `ld` reports them, and so must this tool -
        #    including which of them came from a vtable section.
        src = tmp / "vt.cpp"
        src.write_text(
            "struct Base { virtual ~Base(); virtual int A(); virtual int B(); };\n"
            "Base::~Base() {}\n"
            "struct Derived : Base { int A() override; int B() override; int extra(); };\n"
            "int Derived::A() { return 1; }\n"
            "int Derived::extra() { return 2; }\n"
            "extern int gMissing;\n"
            "int use() { return gMissing; }\n"
            "Derived* mk() { return new Derived(); }\n")
        main_cpp = tmp / "main.cpp"
        main_cpp.write_text("int main(){return 0;}\n")
        obj = tmp / "vt.o"
        r = subprocess.run([cxx, "-c", "-O0", "-g0", "-fdata-sections", "-o", str(obj), str(src)],
                           capture_output=True, text=True)
        check("compiles the vtable probe", r.returncode == 0, r.stderr[-300:])
        prog = tmp / "prog"
        r = subprocess.run([cxx, "-O0", "-g0", "-o", str(prog), str(main_cpp), str(obj)],
                           capture_output=True, text=True)
        check("the probe link fails, as it must", r.returncode != 0)
        lds = set(LD_UNDEF.findall(r.stderr))
        check("ld reports the unresolved vtable slots", len(lds) >= 4, str(sorted(lds)))
        check("ld names the vtable SECTION for a data relocation",
              any(".data.rel.ro._ZTV" in ln for ln in r.stderr.split("\n")),
              r.stderr[-300:])
        with Elf.from_path(str(obj)) as e:
            relocs = e.relocs()
            undef_names = {s.name for s in e.symbols if not s.defined and s.name}
            vt = {n for (_f, _x, _t, n, _s) in relocs if is_vtable_section(_f)}
        check("the tool reads the vtable section's relocations", len(vt) >= 2, str(sorted(vt)))
        # The honest form of Trap 1: the *undefined* slots are in the symbol table too, so
        # `nm -u` is not blind to them. Slots naming a function this object also defines are not
        # in that set, and must not be expected to be.
        vt_undef = vt & undef_names
        check("the undefined vtable slots are symbol-table entries, not relocations alone",
              len(vt_undef) >= 2, str(sorted(vt - undef_names)))
        nmu = subprocess.run(["nm", "-u", str(obj)], capture_output=True, text=True).stdout
        check("`nm -u` really does print the vtable slots (Trap 1 in the header is false)",
              all(n in nmu for n in vt_undef),
              str(sorted(n for n in vt_undef if n not in nmu)))

        # 2. The important negative: does `ld` truncate? 2000 undefined, one link.
        many = tmp / "many.cpp"
        many.write_text("\n".join(
            f"extern int s{i}(void);\nint u{i}(void){{return s{i}();}}" for i in range(2000)))
        mo = tmp / "many.o"
        subprocess.run([cxx, "-c", "-O0", "-g0", "-o", str(mo), str(many)], check=True,
                       capture_output=True)
        p2 = tmp / "prog2"
        r2 = subprocess.run([cxx, "-O0", "-g0", "-o", str(p2), str(main_cpp), str(mo)],
                            capture_output=True, text=True)
        got = set(LD_UNDEF.findall(r2.stderr))
        check("ld.bfd does NOT truncate: 2000 of 2000 reported", len(got) == 2000,
              f"ld reported {len(got)}")

        # 3. Broken inputs must raise, not answer.
        broken = tmp / "broken.o"
        broken.write_bytes(b"\x7fELF" + b"\x00" * 200)
        try:
            with Elf.from_path(str(broken)) as e:
                e.relocs()
            check("a truncated ELF raises rather than reading as empty", False, "no error")
        except BadElf:
            check("a truncated ELF raises rather than reading as empty", True)
        notelf = tmp / "notelf.o"
        notelf.write_bytes(b"this is not an object file at all")
        try:
            with Elf.from_path(str(notelf)) as e:
                e.relocs()
            check("a non-ELF input raises", False, "no error")
        except BadElf:
            check("a non-ELF input raises", True)
        try:
            read_link_line(tmp)
            check("a build tree with no build.ninja raises", False, "no error")
        except LinkLineError:
            check("a build tree with no build.ninja raises", True)
        try:
            read_link_line(build_stub := (tmp / "stub"))
            check("a build.ninja with no LINK_LIBRARIES raises", False, "no error")
        except LinkLineError:
            check("a build.ninja with no LINK_LIBRARIES raises", True)

        # 4. The archive rule, both ways.
        fb = tmp / "fakebuild"
        fb.mkdir()
        lc = tmp / "libc_probe.c"
        lc.write_text('extern "C" int needed_by_nobody(void) { return 1; }\n')
        lco = tmp / "libc_probe.o"
        subprocess.run([cxx, "-c", "-O0", "-g0", "-o", str(lco), str(lc)], check=True)
        arpath = tmp / "libprobe.a"
        subprocess.run(["ar", "rcs", str(arpath), str(lco)], check=True)
        idx = archive_index(str(arpath))
        check("the archive index lists the member's global",
              "needed_by_nobody" in idx, str(sorted(idx)))
        (fb / "build.ninja").write_text(
            f"build {TARGET}: CXX_EXECUTABLE_LINKER__x {obj}\n"
            f"  LINK_LIBRARIES = {arpath}\n  LINK_FLAGS = \n")
        inputs, libs, _v = read_link_line(fb)
        clo = run_closure(inputs, libs, fb, cxx)
        check("an archive member nothing references is NOT pulled in",
              clo.pulled == 0, f"pulled {clo.pulled}")
        user = tmp / "user.cpp"
        # `extern "C"` on BOTH sides, deliberately: a C++-mangled declaration on one side and a
        # C symbol on the other is two different symbols, and the test would then be measuring a
        # genuinely missing definition rather than the archive rule it claims to test.
        user.write_text('extern "C" int needed_by_nobody(void);\n'
                        'extern "C" int go(void){return needed_by_nobody();}\n')
        uo = tmp / "user.o"
        subprocess.run([cxx, "-c", "-O0", "-g0", "-o", str(uo), str(user)], check=True)
        (fb / "build.ninja").write_text(
            f"build {TARGET}: CXX_EXECUTABLE_LINKER__x {uo}\n"
            f"  LINK_LIBRARIES = {arpath}\n  LINK_FLAGS = \n")
        inputs, libs, _v = read_link_line(fb)
        clo = run_closure(inputs, libs, fb, cxx)
        check("an archive member something references IS pulled in",
              clo.pulled == 1, f"pulled {clo.pulled}")
        check("...and then the symbol is defined, not missing",
              "needed_by_nobody" not in clo.unresolved(), str(sorted(clo.unresolved())[:5]))

        # 5. A weak undefined symbol is not reported, because ld does not report it.
        wsrc = tmp / "weak.cpp"
        wsrc.write_text("extern \"C\" __attribute__((weak)) int maybe_missing(void);\n"
                        "int wuser(){return maybe_missing ? maybe_missing() : 0;}\n")
        wo = tmp / "weak.o"
        subprocess.run([cxx, "-c", "-O0", "-g0", "-o", str(wo), str(wsrc)], check=True)
        p3 = tmp / "prog3"
        r3 = subprocess.run([cxx, "-O0", "-g0", "-o", str(p3), str(main_cpp), str(wo)],
                            capture_output=True, text=True)
        check("ld does not complain about an undefined WEAK symbol", r3.returncode == 0,
              r3.stderr[-300:])
        (fb / "build.ninja").write_text(
            f"build {TARGET}: CXX_EXECUTABLE_LINKER__x {wo}\n"
            f"  LINK_LIBRARIES = \n  LINK_FLAGS = \n")
        inputs, libs, _v = read_link_line(fb)
        clo = run_closure(inputs, libs, fb, cxx)
        check("and neither does this tool", "maybe_missing" not in clo.unresolved(),
              str(sorted(clo.unresolved())))

        # 5b. An input that cannot be read must be an ERROR, because every symbol it would have
        #     defined is then counted as missing. A warning there produces an over-report that
        #     reads like an answer, which is the one failure this tool must not have.
        (fb / "build.ninja").write_text(
            f"build {TARGET}: CXX_EXECUTABLE_LINKER__x {wo}\n"
            f"  LINK_LIBRARIES = {tmp/'libnothere.a'} {tmp/'libgoneso.so'} -lnosuchlib\n"
            f"  LINK_FLAGS = \n")
        inputs, libs, _v = read_link_line(fb)
        clo = run_closure(inputs, libs, fb, cxx)
        check("an unresolvable link input is recorded, not ignored",
              len(clo.missing_inputs) >= 2, str(clo.missing_inputs))
        rc = main(["--build", str(fb), "--allow-stale", "--json", str(tmp / "out.json")])
        check("...and the tool exits 4 rather than printing a number", rc == 4, f"rc={rc}")
        rc = main(["--build", str(fb), "--allow-stale", "--allow-missing-inputs"])
        check("...unless explicitly downgraded", rc == 0, f"rc={rc}")

        # 6. data vs function. `CGraphics::mViewport` is the symbol this project has stubbed as a
        #    function four times; retail says it is a 0x18-byte object, and that has to win over
        #    "it is only ever referenced from .data", which is also true and is not evidence.
        retail = load_retail_types()
        if not retail:
            check("config/G2ME01/symbols.txt is readable", False, "no types loaded")
        else:
            got = retail.lookup("CGraphics::mViewport", "_ZN9CGraphics10mViewportE")
            check("retail declares CGraphics::mViewport an object of 0x18 bytes",
                  got == ("object", "0x18", "mViewport__9CGraphics"), str(got))
            data_site = [("x.o", ".data.rel.local._ZZN6CActor4InfoE", False, 1)]
            kind, why = classify(data_site, "CGraphics::mViewport", "_ZN9CGraphics10mViewportE",
                                 retail)
            check("CGraphics::mViewport is classified DATA even with no call site",
                  kind == "data", f"{kind}: {why}")
            check("...and the reason names retail's declaration",
                  "type:object" in why and "0x18" in why, why)
            # The converse must hold too, or the rule is not a rule.
            kind2, _ = classify([("x.o", ".text", True, 4)], "CGraphics::GetWidth()",
                                "_ZN9CGraphics8GetWidthEv", retail)
            check("CGraphics::GetWidth() is classified FUNCTION from its call site",
                  kind2 == "function", kind2)
            # A method *with parameters* is the case a string-equality join could not do.
            got3 = retail.lookup("CGunWeapon::Unk9(CStateManager&)", "_ZN10CGunWeapon4Unk9ER13CStateManager")
            check("a member with parameters is joined on (class, member), not on the full name",
                  got3 is not None and got3[0] == "function" and got3[2].startswith("Unk9__"),
                  str(got3))
            # retail has fn_801D8EC0 as a free function; the port declares it as a CGunWeapon
            # member. Retail's fn_/lbl_ names are unique in the binary, so the bare name answers.
            got4 = retail.lookup("CGunWeapon::fn_801D8EC0()", "_ZN10CGunWeapon11fn_801D8EC0Ev")
            check("a port-placed retail `fn_` name is still found by its unique bare name",
                  got4 is not None and got4[0] == "function" and got4[2] == "fn_801D8EC0",
                  str(got4))
            # An unresolved join must return nothing rather than a guess, and an overload set
            # must NOT be treated as ambiguous: a name is either a function or an object, never
            # both, so 55 overload sets in this file are all still answerable.
            check("an unresolved join returns nothing rather than a guess",
                  retail.lookup("Foo::Bar()") is None, str(retail.lookup("Foo::Bar()")))
            ovl = retail.by_member.get(("CStateManager", "DeliverScriptMsg"), [])
            check("an overload set exists in symbols.txt to exercise that path", len(ovl) > 1,
                  str([o[0] for o in ovl]))
            got5 = retail.lookup("CStateManager::DeliverScriptMsg(TUniqueId, TUniqueId, "
                                 "EScriptObjectMessage, TUniqueId)",
                                 "_ZN13CStateManager17DeliverScriptMsgE9TUniqueId9TUniqueId"
                                 "20EScriptObjectMessage9TUniqueId")
            check("...and an overload set still answers, because they cannot disagree on the type",
                  got5 is not None and got5[0] == "function", str(got5))
            # A data-only reference with nothing declaring it an object is a function-pointer
            # table far more often than a variable, and the expensive error is the other one.
            kind3, why3 = classify([("p.o", ".data.rel.ro._ZTS11Compiled", False, 1)],
                                   "mp_cswarmbasics", "mp_cswarmbasics", retail)
            check("a data-only reference with no object declaration is filed as a function",
                  kind3 == "function", f"{kind3}: {why3}")
            # An overload set must be refused, not guessed.
            kinds = {k for (c, b), v in retail.by_member.items() for _n, k, _s in v}
            check("symbols.txt really does declare both functions and objects",
                  kinds == {"function", "object"}, str(sorted(kinds)))

        # 7. The class's own entries must not answer for an unrelated member. An earlier version
        #    of this join emitted `__dt__10CGunWeaponFv` as a candidate for **every**
        #    `CGunWeapon::` member, and it answered with the destructor's size: a confident fact
        #    about the wrong symbol, which is worse than no answer at all.
        retail = load_retail_types()
        check("an unrelated member of a class does not get the class's ctor/dtor entry",
              retail.lookup("CGunWeapon::NoSuchMember()",
                            "_ZN10CGunWeapon12NoSuchMemberEv") is None,
              str(retail.lookup("CGunWeapon::NoSuchMember()")))
        dtor = retail.lookup("CGunWeapon::~CGunWeapon()", "_ZN10CGunWeaponD1Ev")
        check("...while the destructor does get its own entry",
              dtor is not None and dtor[2] == "__dt__10CGunWeaponFv", str(dtor))
        ctor = retail.lookup("CGunWeapon::CGunWeapon()", "_ZN10CGunWeaponC1Ev")
        check("...and so does the constructor",
              ctor is not None and ctor[2].startswith("__ct__10CGunWeapon"), str(ctor))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print(f"link_closure: selftest {'PASS' if not failures else 'FAIL: ' + ' '.join(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
