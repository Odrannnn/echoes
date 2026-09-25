#!/usr/bin/env python3
"""What the port's game library still needs in order to link.

    tools/link_gap.py [--build DIR] [--rebuild] [--list] [--write-doc]

The port builds its game sources as an OBJECT library today, so nothing ever reports
what is missing: there is no link step to fail. This creates one. It compiles
`mp_game` for the host, reads the undefined symbols out of the objects, subtracts what
the objects define, and classifies what is left:

  c++ runtime    libstdc++/libgcc - satisfied by the C++ runtime at link time
  libc/libm      satisfied by the system
  aurora source  the identifier appears in Aurora's or the platform's sources
  aurora header  it appears only in a header there - a declaration, maybe not a definition
  MISSING        nothing provides it: a function nobody has written, a retail global that
                 is declared `extern` and never defined, or a game global

`MISSING` is the work list for "the port links into a binary". It is compared against
`docs/research/port_link_gap.md`, so a symbol appearing that is not in the document fails
(the gap grew) and a symbol gone from the tree that the document still lists also fails
(the document is stale). That makes the gap a ratchet rather than a number somebody has
to remember to check.

**A measurement of stale objects is worse than no measurement**, and that is not
hypothetical: this check was first wired into `tools/gate.sh` pointing at a build
directory configured against the master tree, so in a lane's collect worktree it reported
the *master's* unresolved symbols and called two freshly-defined ones still missing. The
build directory defaults to `$REPO_ROOT/build-port` so each tree measures its own sources,
and any source newer than the newest object is reported as stale rather than believed.
Pass `--rebuild` to rebuild first; the check is then always current.

The subtlety worth knowing before reading the list: **a retail global declared
`extern "C" T lbl_80419A10;` and then assigned is a declaration, not a definition.** That
is correct for the decompilation - retail's own objects define those symbols and the DOL
links against them - and it is exactly why a standalone PC link cannot resolve them. Every
one of them needs a real definition somewhere, holding the value the retail binary has.
"""
import argparse
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOC = ROOT / "docs" / "research" / "port_link_gap.md"
AURORA_DIRS = ("extern/aurora/include", "extern/aurora/lib", "platform/include", "platform")
SOURCE_DIRS = ("src", "include")
NINJA = pathlib.Path(os.environ.get(
    "MP_TOOLCHAIN_DIR", str(ROOT.parent / "MetroidPrimePort"))
) / "build" / "review-tools" / "bin" / "ninja"
CMAKE = NINJA.with_name("cmake")

# Names the system C library provides. Deliberately a list rather than a pattern: the
# misclassification that matters is calling a game symbol "libc" and hiding it.
LIBC = set("""
memcpy memmove memset memcmp strcmp strncmp strcpy strncpy strlen strchr strrchr strstr
strcat strncat strdup strtol strtoul strtoull atoi atol atof printf fprintf sprintf snprintf
vprintf vfprintf vsprintf vsnprintf puts putchar fputs fopen fclose fread fwrite fseek ftell
feof fflush sscanf abs labs llabs div malloc calloc realloc free abort exit atexit system getenv
qsort bsearch rand srand time clock difftime pow powf sqrt sqrtf exp expf log logf log2 log10
sin sinf cos cosf tan tanf asin asinf acos acosf atan atanf atan2 ceil ceilf floor floorf
fmod fmodf fabs fabsf fmax fmaxf fmin fminf frexp ldexp modf modff scalbn round roundf trunc
fma timegm sincosf sincos
""".split())

# Provided by the C++ runtime, or synthesised by the linker itself.
CXX_RUNTIME = re.compile(r"^(_Z|_Unwind|__cxa|__gxx|std::|typeinfo|vtable for|"
                         r"non-virtual thunk to|guard variable for|"
                         r"operator (new|delete|co_)|_GLOBAL_OFFSET_TABLE_$)")
LINKER_SYNTHESISED = ("_ctors", "_dtors", "_edata", "_end", "__bss_start")


def nm_symbols(objects, flag):
    out = subprocess.run(["nm", flag, "--format=posix"] + objects,
                         capture_output=True, text=True).stdout
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if parts:
            names.add(parts[0].split("@")[0])
    return names


def provider_symbols():
    """Identifiers appearing in Aurora's/ the platform's trees, split by strength.

    A name in a *header* is only a declaration; a name in a *source* file is much more
    likely to be a definition. Both are evidence, not proof - the authoritative answer is
    an actual link - but collapsing the two would hide a symbol behind a header mention.
    """
    ident = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
    headers, sources = set(), set()
    groups = ((("extern/aurora/include", "platform/include"), headers),
              (("extern/aurora/lib", "platform"), sources))
    for rels, acc in groups:
        for rel in rels:
            base = ROOT / rel
            if not base.exists():
                continue
            for path in base.rglob("*"):
                if path.suffix not in (".h", ".hpp", ".c", ".cpp", ".mm") or not path.is_file():
                    continue
                try:
                    acc.update(ident.findall(path.read_text(errors="replace")))
                except OSError:
                    pass
    return headers, sources


def newest_source():
    newest, which = 0, None
    for rel in SOURCE_DIRS:
        base = ROOT / rel
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.suffix not in (".h", ".hpp", ".c", ".cpp", ".cp") or not path.is_file():
                continue
            try:
                m = path.stat().st_mtime
            except OSError:
                continue
            if m > newest:
                newest, which = m, path
    return newest, which


def newest_object(objects):
    newest = 0
    for obj in objects:
        try:
            newest = max(newest, os.stat(obj).st_mtime)
        except OSError:
            pass
    return newest


def run_build(build):
    """Configure and build `mp_game` in `build`. Returns True on success."""
    build = pathlib.Path(build)
    if not (build / "build.ninja").exists():
        print("configuring the port into %s ..." % build)
        r = subprocess.run([str(CMAKE), "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                            "-DCMAKE_MAKE_PROGRAM=%s" % NINJA],
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-2000:], r.stderr[-2000:], file=sys.stderr)
            return False
    r = subprocess.run([str(NINJA), "-C", str(build), "mp_game"], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-2000:], file=sys.stderr)
        return False
    return True


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--build", default=os.environ.get("MP_PORT_BUILD", str(ROOT / "build-port")),
                    help="cmake build dir for the port (headers-only config). Defaults inside "
                         "this tree, so a lane's worktree measures its own sources.")
    ap.add_argument("--rebuild", action="store_true",
                    help="rebuild mp_game before measuring, so the result is never stale")
    ap.add_argument("--list", action="store_true", help="print the classification and exit 0")
    ap.add_argument("--write-doc", action="store_true", help="rewrite the document's list")
    args = ap.parse_args()

    build = pathlib.Path(args.build)
    if args.rebuild and not run_build(build):
        print("error: could not build mp_game in %s" % build, file=sys.stderr)
        return 2

    objects = sorted(str(p) for p in build.rglob("*.o") if "mp_game" in str(p))
    if not objects:
        print(f"error: no mp_game objects under {build}. Build the port first:\n"
              f"  python3 tools/link_gap.py --rebuild", file=sys.stderr)
        return 2

    src_time, src_path = newest_source()
    obj_time = newest_object(objects)
    stale = src_time > obj_time
    if stale and not args.rebuild:
        print("STALE: %s is newer than the newest mp_game object; this measures an old build.\n"
              "       re-run with --rebuild, or the result below is not about this tree."
              % src_path, file=sys.stderr)
        return 3

    undefined = nm_symbols(objects, "--undefined-only")
    defined = nm_symbols(objects, "--defined-only")
    missing = sorted(undefined - defined)
    header_names, source_names = provider_symbols()

    buckets = {"c++ runtime / linker": [], "libc/libm": [], "aurora source": [],
               "aurora header only": [], "MISSING": []}
    for sym in missing:
        if CXX_RUNTIME.match(sym) or sym in LINKER_SYNTHESISED:
            buckets["c++ runtime / linker"].append(sym)
        elif sym in LIBC or sym.startswith("__"):
            buckets["libc/libm"].append(sym)
        elif sym in source_names:
            buckets["aurora source"].append(sym)
        elif sym in header_names:
            buckets["aurora header only"].append(sym)
        else:
            buckets["MISSING"].append(sym)

    order = ("c++ runtime / linker", "libc/libm", "aurora source", "aurora header only", "MISSING")
    if args.list:
        for name in order:
            print(f"{len(buckets[name]):5d}  {name}")
        print()
        for sym in buckets["MISSING"]:
            print("   ", sym)
        return 0

    if args.write_doc:
        documented = set()
        if DOC.exists():
            for line in DOC.read_text().splitlines():
                m = re.match(r"^-\s+`([\w$.]+)`", line)
                if m:
                    documented.add(m.group(1))
        keep = [l for l in DOC.read_text().splitlines() if not re.match(r"^-\s+`[\w$.]+`", l)]
        new = ["- `%s`" % s for s in sorted(set(buckets["MISSING"]) - documented)]
        if new:
            keep += ["", "## MISSING, measured", ""] + new
        DOC.write_text("\n".join(keep).rstrip() + "\n")
        print(f"wrote {len(new)} new entr(ies) to {DOC.name}")
        return 0

    documented = set()
    if DOC.exists():
        for line in DOC.read_text().splitlines():
            m = re.match(r"^-\s+`([\w$.]+)`", line)
            if m:
                documented.add(m.group(1))
    problems = []
    for sym in buckets["MISSING"]:
        if sym not in documented:
            problems.append("gap grew: %s is not in %s" % (sym, DOC.name))
    for sym in sorted(documented - set(buckets["MISSING"])):
        problems.append("stale: %s is listed but no longer missing - delete the entry" % sym)

    print("port link gap, measured over %d object(s) in %s%s"
          % (len(objects), build, "" if args.rebuild else " (not rebuilt)"))
    for name in order:
        print("  %5d  %s" % (len(buckets[name]), name))
    if problems:
        print("\nlink gap not accounted for:")
        for p in problems:
            print("  " + p)
        print("\nIf a symbol is newly missing, add it to %s with what provides it."
              % DOC.relative_to(ROOT))
        print("If one is gone, delete its entry - that is progress and should be visible.")
        return 1
    print("ok: %d MISSING symbol(s), all accounted for in %s"
          % (len(buckets["MISSING"]), DOC.name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
