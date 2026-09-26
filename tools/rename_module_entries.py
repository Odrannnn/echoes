#!/usr/bin/env python3
"""Give each compiled-in REL module's entry points a distinct name on the host.

    python3 tools/rename_module_entries.py [--check]

Why. On the cube each REL module is a separate module, and mwldeppc's linker script
builds its prolog and epilog from a `RELMain` and a `RELExit` that the module
defines. Fourteen translation units here define one, which is correct there and
impossible in a flat host link: the first link attempt with all of them compiled
reported 28 duplicate definitions over four distinct symbols - `RELMain`,
`RELExit`, `SetFuncPtrs` and `__ct__10CModelDataFv`.

So each module's entry points get a distinct name **on the host only**:

    #ifdef __MWERKS__
    #define MP_<MOD>_MAIN RELMain
    #define MP_<MOD>_EXIT RELExit
    #else
    #define MP_<MOD>_MAIN mp_relmain_<slug>
    #define MP_<MOD>_EXIT mp_relexit_<slug>
    #endif

MWCC still compiles `RELMain`/`RELExit`, so the `Matching` units are untouched -
which the gate's per-function diff and the DOL sha1 both confirm. A module that
also defines a file-local `SetFuncPtrs` gets the same treatment, because three of
them collide for the same reason.

The transform is mechanical and this script exists so it stays that way: it is
idempotent, it refuses to touch a file it cannot parse, and `--check` fails if any
file is un-renamed. Run it after adding a module to `files.cmake`.

What it deliberately does not do is `-Wl,--allow-multiple-definition`, which would
green the link while running one module's entry point and skipping thirteen. A
green link that lies is worse than a red one.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The files already renamed by hand, with the slug each one uses, so the registry
# in platform/compiled_modules.cpp and the source agree. Keep in step with it.
DONE = {
    "MetroidPrime/Tweaks/Tweaks.cpp": "tweaks",
    "MetroidPrime/ScriptObjects/CScriptCannonBall.cpp": "cannonball",
    "MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp": "forgottenobject",
}


def exit_for(slug: str) -> str:
    """The host exit-point name. Never equal to `slug`.

    The first version did `slug.replace("main", "exit")`, which is a no-op for
    `mp_cswarmbasics` and `mp_swarm` - neither contains "main" - so those two
    modules' init and shutdown became the same function. A registry that lists
    the same pointer twice is a silent bug, so the suffix is now explicit.
    """
    return slug[:-4] + "exit" if slug.endswith("main") else slug + "_exit"


def slug_for(path: Path) -> str:
    """A unique, readable host name from the file's own stem."""
    stem = re.sub(r"^(CScript|CFlyer|)", "", path.stem)
    stem = re.sub(r"(REL|Rel)$", "", stem)
    return "mp_" + re.sub(r"[^a-z0-9]+", "_", stem.lower()).strip("_")


def listed_sources():
    text = (ROOT / "files.cmake").read_text()
    return set(re.findall(r"(src/\S+\.cpp)", text))


def transform(path: Path, slug: str, dry: bool):
    """Return the new text, or None if the file does not need the transform."""
    s = path.read_text()
    if "mp_relmain_" in s:
        return None
    mains = re.findall(r'^(extern "C" )?void RELMain\((?:void)?\)', s, re.M)
    exits = re.findall(r'^(extern "C" )?void RELExit\((?:void)?\)', s, re.M)
    if not mains and not exits:
        return None
    if len(mains) > 1 or len(exits) > 1:
        raise SystemExit(f"{path}: expected at most one RELMain and one RELExit, "
                         f"found {len(mains)} and {len(exits)}")

    up = re.sub(r"[^A-Z0-9]+", "_", slug.upper())
    up = up[3:] if up.startswith("MP_") else up
    guard = f"""// On the cube mwldeppc's linker script calls RELMain/RELExit to build this module's
// prolog and epilog, and every module we have reimplemented defines them, which
// is correct there - they are separate modules - and impossible in a flat host
// link. On the host each takes a distinct name and
// platform/compiled_modules.cpp registers them; see tools/rename_module_entries.py.
// MWCC still compiles RELMain/RELExit, so this Matching unit is unchanged.
#ifdef __MWERKS__
#define MP_{up}_MAIN RELMain
#define MP_{up}_EXIT RELExit
#else
#define MP_{up}_MAIN {slug}
#define MP_{up}_EXIT {exit_for(slug)}
#endif

"""
    # Substituting with a regex rather than string-replacing a concatenation: the
    # first version of this tool built the needle as `prefix + " void RELMain()"`
    # and silently matched nothing whenever the prefix was `extern "C" `, because
    # the file has one space there and the concatenation had two. It reported the
    # files as renamed anyway, which is worse than not matching at all.
    s, n_main = re.subn(r'^(extern "C" )?void RELMain\((?:void)?\)',
                        lambda m: guard + (m.group(1) or "") + f"void MP_{up}_MAIN()",
                        s, count=1, flags=re.M)
    s, n_exit = re.subn(r'^(extern "C" )?void RELExit\((?:void)?\)',
                        lambda m: (m.group(1) or "") + f"void MP_{up}_EXIT()",
                        s, count=1, flags=re.M)
    if n_main != len(mains) or n_exit != len(exits):
        raise SystemExit(f"{path}: substitution did not land "
                         f"(RELMain {n_main} of {len(mains)}, RELExit {n_exit} of {len(exits)})")

    # A module that also defines a file-local SetFuncPtrs collides the same way.
    if re.search(r"^void SetFuncPtrs\(\)", s, re.M):
        s = re.sub(r"^void SetFuncPtrs\(\)",
                   f"#ifdef __MWERKS__\n#define MP_{up}_FUNCPTRS SetFuncPtrs\n#else\n"
                   f"#define MP_{up}_FUNCPTRS {slug}_funcptrs\n#endif\n\n"
                   f"void MP_{up}_FUNCPTRS()", s, count=1)
        s = re.sub(r"^(?<![\w>])SetFuncPtrs\(\)", f"MP_{up}_FUNCPTRS()", s, flags=re.M)
    return s


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if any listed module is un-renamed (no files are written)")
    ap.add_argument("--apply", action="store_true", help="write the changes")
    args = ap.parse_args()
    if not (args.check or args.apply):
        ap.error("pass --check or --apply")

    # Scan every source under src/, not just the ones files.cmake names. A module
    # that is not in the port build yet will be, and the collision only appears when
    # it is - so the rename has to be in place before then, not after.
    listed = sorted(str(q.relative_to(ROOT)) for q in (ROOT / "src").rglob("*.cpp"))
    todo, already = [], 0
    for rel in listed:
        p = ROOT / rel
        if not p.exists():
            continue
        s = p.read_text(errors="replace")
        if not re.search(r"\bvoid RELMain\((?:void)?\)", s) \
                and not re.search(r"\bvoid RELExit\((?:void)?\)", s):
            continue
        if "mp_relmain_" in s:
            already += 1
            continue
        todo.append(p)

    if args.check:
        if todo:
            print("rename_module_entries: these modules still define a bare RELMain "
                  "and would collide in a flat link:", file=sys.stderr)
            for p in todo:
                print(f"  {p.relative_to(ROOT)}", file=sys.stderr)
            return 1
        print(f"rename_module_entries: {already} modules renamed, none outstanding")
        return 0

    if not todo:
        print(f"rename_module_entries: nothing to do ({already} already renamed)")
        return 0
    for p in todo:
        new = transform(p, slug_for(p), dry=False)
        if new is None:
            continue
        p.write_text(new)
        print(f"  renamed {p.relative_to(ROOT)} -> {slug_for(p)}")
    print(f"rename_module_entries: {len(todo)} files written; "
          f"now add each to platform/compiled_modules.cpp's registry")
    return 0


if __name__ == "__main__":
    sys.exit(main())
