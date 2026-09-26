#!/usr/bin/env python3
"""What the port still needs in order to link.

    tools/link_gap.py [--build DIR] [--rebuild] [--list] [--write-list]

The port builds its game sources as OBJECT libraries, so nothing reports what is missing when
it links: there is no link step to fail. This creates one. It builds `mp_game`, `mp_platform`
and `mp_port_entry` for the host - the three libraries a link would consume - reads the
undefined symbols out of the objects, subtracts what they define, and classifies the rest:

  c++ runtime / linker   libstdc++, libgcc, and what the linker synthesises
  libc/libm              satisfied by the system
  aurora source          the identifier is used in Aurora's or the platform's sources
  aurora header only     it appears only in a header there - a declaration, maybe not a definition
  MISSING                nothing provides it

**`MISSING` is the work list for "the port links into a binary".** It is checked against
`docs/research/port_link_gap_list.md`, which `--write-list` regenerates grouped by kind, so a
newly missing symbol fails and a resolved one also fails until its entry is deleted. The
prose, the method and the category analysis are in `docs/research/port_link_gap.md`.

## Two measurement mistakes this tool has already made, both worth reading before changing it

**Mangled names are not the C++ runtime.** The first version classified `^_Z` as "c++ runtime",
which is true of `std::` instantiations and false of every game function MWCC mangles - they
all start `_Z` too. That hid **700 real game symbols** behind a bucket nobody reads, and
reported a gap of 32 when it was 732. Mangled names are demangled with `c++filt` and classified
on the result. This is the single most important thing in the file.

**A bare word is not evidence that Aurora provides a symbol.** "Allocate" and "Renderer" occur
all over Aurora's trees; matching whole words filed the game's own `AllocateRenderer` and
`CInputGenerator::CInputGenerator` as provided. A symbol now has to be *used* - followed by
`(` or preceded by `::`, `.`, `->`, `&`, `*` - before Aurora's tree is allowed to claim it.

And the standing one: **a measurement of stale objects is worse than no measurement**, so a
source newer than the newest object is reported STALE and exits 3 rather than believed. The
build directory defaults inside this tree, because a directory configured against another tree
measures *that* tree's symbols - which is how two freshly-defined globals looked like they were
still missing.
"""
import argparse
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOC = ROOT / "docs" / "research" / "port_link_gap.md"
LIST = ROOT / "docs" / "research" / "port_link_gap_list.md"
SOURCE_DIRS = ("src", "include")
# The three libraries a link consumes. Measuring mp_game alone hid a whole lane's work:
# mp_port_entry is the only thing that *references* COsContext and CMemorySys, and nothing in
# mp_game calls their out-of-line methods.
LINK_TARGETS = ("mp_game", "mp_platform", "mp_port_entry")
AURORA_GROUPS = ((("extern/aurora/include", "platform/include"), "headers"),
                 (("extern/aurora/lib", "platform"), "sources"))

TOOLCHAIN = pathlib.Path(os.environ.get("MP_TOOLCHAIN_DIR", str(ROOT.parent / "MetroidPrimePort")))
NINJA = TOOLCHAIN / "build" / "review-tools" / "bin" / "ninja"
CMAKE = NINJA.with_name("cmake")

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
USED_SITE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\("
                       r"|(?:::|->|\.|\*|&)\s*([A-Za-z_][A-Za-z0-9_]*)\b")
RUNTIME_DEMANGLED = re.compile(r"^(std::|operator |typeinfo for|vtable for|non-virtual thunk|"
                               r"virtual thunk|guard variable for|construction vtable|"
                               r"\.\.\.|\(__gnu_cxx)")
CXX_RUNTIME = re.compile(r"^(_Unwind|__cxa|__gxx|std::|typeinfo|vtable for|"
                         r"non-virtual thunk to|guard variable for|"
                         r"operator (new|delete|co_)|_GLOBAL_OFFSET_TABLE_$)")
LINKER_SYNTHESISED = ("_ctors", "_dtors", "_edata", "_end", "__bss_start")
LIBC = set("""
memcpy memmove memset memcmp strcmp strncmp strcpy strncpy strlen strchr strrchr strstr
strcat strncat strdup strtol strtoul strtoull atoi atol atof printf fprintf sprintf snprintf
vprintf vfprintf vsprintf vsnprintf puts putchar fputs fopen fclose fread fwrite fseek ftell
feof fflush sscanf abs labs llabs div malloc calloc realloc free abort exit atexit system getenv
qsort bsearch rand srand time clock difftime pow powf sqrt sqrtf exp expf log logf log2 log10
sin sinf cos cosf tan tanf asin asinf acos acosf atan atanf atan2 ceil ceilf floor floorf
fmod fmodf fabs fabsf fmax fmaxf fmin fminf frexp ldexp modf modff scalbn round roundf trunc
fma timegm sincosf sincos sched_yield nanosleep usleep dlopen dlsys gettimeofday localtime
gmtime mktime strftime setenv
isalnum isalpha iscntrl isdigit isgraph islower isprint ispunct isspace isupper isxdigit
tolower toupper
""".split())


# ---------------------------------------------------------------- build and freshness

def run_build(build):
    build = pathlib.Path(build)
    if not (build / "build.ninja").exists():
        print("configuring the port into %s ..." % build)
        r = subprocess.run([str(CMAKE), "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                            "-DCMAKE_MAKE_PROGRAM=%s" % NINJA], capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-2000:], r.stderr[-2000:], file=sys.stderr)
            return False
    # `build.ninja` carries a RERUN_CMAKE edge on CMakeLists.txt and files.cmake, so any
    # edit to either makes ninja re-run cmake - and if the build tree then still looks
    # older than its inputs, ninja retries until it gives up with
    #
    #     manifest 'build.ninja' still dirty after 100 tries, perhaps system time is not set
    #
    # which is not a diagnosis, it is a symptom with a real cause. The cause is the mount:
    # this tree lives on a drive whose writes do not reliably advance the mtime, so a
    # freshly generated `build.ninja` can come out older than the file that triggered it,
    # and no amount of retrying fixes that because the clock, not the content, is wrong.
    #
    # The documented workaround - `touch CMakeLists.txt` before measuring - is what
    # *creates* the condition, because it guarantees the edge is dirty. So: run cmake
    # ourselves whenever the manifest is behind its inputs, and only fall back to stamping
    # the timestamp if cmake ran and the stamp still came out behind.
    #
    # **This ordering was got wrong once and an independent review caught it.** The first
    # version stamped the manifest *without* running cmake, on the reasoning that "content
    # is already correct, only the timestamp is behind, and the timestamp is the whole of
    # the problem". That reasoning is wrong, and demonstrably so:
    #
    #   - `build-port/build.ninja`'s RERUN_CMAKE edge **does** list `files.cmake`
    #     (verified: `ninja -t query build.ninja`). So the manifest being behind means
    #     the *file list* changed, and the fix is to regenerate it, not to declare it
    #     current.
    #   - Reproduced: with `build.ninja` older than `files.cmake`, plain ninja re-ran cmake
    #     and the manifest mtime advanced. After the bare stamp, ninja printed **"no work
    #     to do"** and cmake never ran. A gate step that stamps the manifest it was supposed
    #     to regenerate will confidently report a number for a pre-merge file list - which
    #     is precisely `docs/PROCESS_LESSONS.md` #1, and it is worse than the failure the
    #     stamp was added to fix, because it is invisible.
    #
    # So: regenerate, then stamp only as a post-cmake fallback for the mount's mtime
    # behaviour. The stamp never stands in for a regeneration.
    manifest = build / "build.ninja"
    inputs = [ROOT / "CMakeLists.txt", ROOT / "files.cmake", build / "CMakeCache.txt"]
    newest_input = max((p.stat().st_mtime for p in inputs if p.exists()), default=0.0)
    if not manifest.exists() or manifest.stat().st_mtime < newest_input:
        print("build.ninja is behind its inputs; regenerating the manifest ...")
        r = subprocess.run([str(CMAKE), "-S", str(ROOT), "-B", str(build),
                            "-DCMAKE_MAKE_PROGRAM=%s" % NINJA],
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stdout[-2000:], r.stderr[-2000:], file=sys.stderr)
            return False
        # Post-cmake fallback for this mount, which does not reliably advance mtimes.
        newest_input = max((p.stat().st_mtime for p in inputs if p.exists()), default=0.0)
        if manifest.exists() and manifest.stat().st_mtime < newest_input:
            os.utime(manifest, (newest_input + 1, newest_input + 1))

    r = subprocess.run([str(NINJA), "-C", str(build)] + list(LINK_TARGETS),
                       capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-2000:], file=sys.stderr)
        if "still dirty after" in (r.stdout or "") + (r.stderr or ""):
            print("hint: the build.ninja timestamp fix above did not take on this mount.\n"
                  "      `touch build-port/build.ninja` and re-run; if that is not enough,\n"
                  "      the mount is not storing mtimes and the build tree should live\n"
                  "      on a local filesystem.", file=sys.stderr)
        return False
    return True


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
                mtime = path.stat().st_mtime
            except OSError:
                continue
            if mtime > newest:
                newest, which = mtime, path
    return newest, which


# ---------------------------------------------------------------- symbol plumbing

def nm_symbols(objects, flag):
    out = subprocess.run(["nm", flag, "--format=posix"] + objects,
                         capture_output=True, text=True).stdout
    return {line.split()[0].split("@")[0] for line in out.splitlines() if line.split()}


def demangle(names):
    todo = [n for n in names if n.startswith("_Z")]
    if not todo:
        return {}
    out = subprocess.run(["c++filt"], input="\n".join(todo), capture_output=True, text=True)
    lines = out.stdout.splitlines()
    return dict(zip(todo, lines)) if len(lines) == len(todo) else {}


def provider_symbols():
    """{headers, sources}: identifiers Aurora or the platform actually *uses*."""
    out = {"headers": set(), "sources": set()}
    for rels, key in AURORA_GROUPS:
        for rel in rels:
            base = ROOT / rel
            if not base.exists():
                continue
            for path in base.rglob("*"):
                if path.suffix not in (".h", ".hpp", ".c", ".cpp", ".mm") or not path.is_file():
                    continue
                try:
                    text = path.read_text(errors="replace")
                except OSError:
                    continue
                out[key].update(IDENT.findall(text))
                for m in USED_SITE.finditer(text):
                    out[key].add(m.group(1) or m.group(2))
    return out["headers"], out["sources"]


def categorise(sym, dem):
    """Which kind of work closes this symbol. The bulk categories are the useful part: they
    are where one generator closes two hundred of them."""
    d = dem or sym
    if re.match(r"^(Load\w+\(|SetLoader_|REL_loader_)", d):
        return "REL module loaders"
    if re.match(r"^(SLdr\w+::|SScript\w*_FuncPtrs)", d):
        return "SLdr* script-loader struct constructors"
    if "::TypesMatch" in d:
        return "TypesMatch overrides"
    if d.startswith("rstl::"):
        return "rstl templates"
    if re.search(r"::(k[A-Z]\w*|m[A-Z]\w*|sk[A-Z]\w*|mNull|sNull)$", d):
        return "static data members"
    if sym.startswith("_Z"):
        return "other game methods"
    return "unmangled: fn_*, lbl_*, globals"


def documented_symbols():
    if not LIST.exists():
        return set()
    return {m.group(1) for m in
            (re.match(r"^- `(.+)`", line) for line in LIST.read_text().splitlines()) if m}


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--build", default=os.environ.get("MP_PORT_BUILD", str(ROOT / "build-port")))
    ap.add_argument("--rebuild", action="store_true", help="build before measuring")
    ap.add_argument("--list", action="store_true", help="print the classification and exit 0")
    ap.add_argument("--write-list", action="store_true", help="regenerate %s" % LIST.name)
    args = ap.parse_args()

    build = pathlib.Path(args.build)
    if args.rebuild and not run_build(build):
        print("error: could not build %s in %s" % (", ".join(LINK_TARGETS), build), file=sys.stderr)
        return 2

    # Only objects whose source is still in files.cmake. CMake never removes the
    # object of a source dropped from the source list, and a bare rglob counts it,
    # so a removed unit keeps contributing the symbols it defined. That is not
    # hypothetical: 16 module objects outlived their removal from files.cmake and
    # were being counted, which is part of why this tool and the real linker
    # disagreed. The linker does not have the problem - it links CMake's list, not a
    # directory - so the divergence was this tool's alone.
    live = set(re.findall(r"(src/\S+\.c(?:pp)?)", (ROOT / "files.cmake").read_text()))
    markers = ("mp_game.dir/", "mp_platform.dir/", "mp_port_entry.dir/", "mp_port_audio.dir/")
    objects = []
    for obj in build.rglob("*.o"):
        s_obj = str(obj)
        if not any(t in s_obj for t in LINK_TARGETS):
            continue
        for marker in markers:
            if marker in s_obj:
                rel = s_obj.split(marker, 1)[1][:-2]
                break
        else:
            continue
        if live and rel not in live:
            continue
        objects.append(s_obj)
    objects = sorted(objects)
    if not objects:
        print("error: no objects for %s under %s; run with --rebuild"
              % (", ".join(LINK_TARGETS), build), file=sys.stderr)
        return 2

    if not args.rebuild:
        src_time, src_path = newest_source()
        if src_time > max(os.stat(o).st_mtime for o in objects):
            print("STALE: %s is newer than the newest object; this measures an old build.\n"
                  "       re-run with --rebuild, or the result is not about this tree." % src_path,
                  file=sys.stderr)
            return 3

    missing = sorted(nm_symbols(objects, "--undefined-only") - nm_symbols(objects, "--defined-only"))
    demangled = demangle(missing)
    headers, sources = provider_symbols()

    buckets = {"c++ runtime / linker": [], "libc/libm": [], "aurora source": [],
               "aurora header only": [], "MISSING": []}
    for sym in missing:
        if (RUNTIME_DEMANGLED.match(demangled.get(sym, "")) or CXX_RUNTIME.match(sym)
                or sym in LINKER_SYNTHESISED):
            buckets["c++ runtime / linker"].append(sym)
        elif sym in LIBC or sym.startswith("__"):
            buckets["libc/libm"].append(sym)
        elif sym in sources:
            buckets["aurora source"].append(sym)
        elif sym in headers:
            buckets["aurora header only"].append(sym)
        else:
            buckets["MISSING"].append(sym)

    order = ("c++ runtime / linker", "libc/libm", "aurora source", "aurora header only", "MISSING")
    if args.list:
        for name in order:
            print("%5d  %s" % (len(buckets[name]), name))
        print()
        for sym in buckets["MISSING"]:
            print("   ", sym)
        return 0

    if args.write_list:
        groups = {}
        for sym in buckets["MISSING"]:
            groups.setdefault(categorise(sym, demangled.get(sym)), []).append(sym)
        out = ["# The port's link gap, as a list", "",
               "Generated by `python3 tools/link_gap.py --write-list`. `tools/gate.sh` checks this",
               "file against the tree: a symbol that is missing but not listed here fails, and a",
               "symbol listed here that is no longer missing fails until its entry is deleted.",
               "", "See `port_link_gap.md` for what the groups mean.", ""]
        for name in sorted(groups, key=lambda k: -len(groups[k])):
            out.append("## %s (%d)" % (name, len(groups[name])))
            out.append("")
            out += ["- `%s`" % x for x in sorted(groups[name])]
            out.append("")
        LIST.write_text("\n".join(out))
        print("wrote %d entries in %d groups to %s"
              % (len(buckets["MISSING"]), len(groups), LIST.name))
        return 0

    documented = documented_symbols()
    problems = ["gap grew: %s is not in %s" % (s, LIST.name)
                for s in buckets["MISSING"] if s not in documented]
    problems += ["stale: %s is listed but no longer missing - delete the entry" % s
                 for s in sorted(documented - set(buckets["MISSING"]))]

    print("port link gap, measured over %d object(s) in %s%s"
          % (len(objects), build, "" if args.rebuild else " (not rebuilt)"))
    for name in order:
        print("  %5d  %s" % (len(buckets[name]), name))
    if problems:
        print("\nlink gap not accounted for:")
        for p in problems[:20]:
            print("  " + p)
        if len(problems) > 20:
            print("  ... and %d more" % (len(problems) - 20))
        print("\nRun --write-list and describe what provides each new symbol in %s." % DOC.name)
        return 1
    print("ok: %d MISSING symbol(s), all accounted for in %s" % (len(buckets["MISSING"]), LIST.name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
