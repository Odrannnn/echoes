#!/usr/bin/env python3
"""Which GameCube mwcceppc version emits a bare `cntlzw` for `!x` on a `bool`?

The question, because it decides whether `CErrorOutputWindow` (retail 0x8018169C) is
reachable at all. Retail wants:

    cntlzw r0,r31

and mwcceppc 2.7 emits:

    clrlwi r0,r31,24 ; cntlzw r0,r0 ; srwi r4,r0,5

A lane established this is a *version* difference rather than a source one, because
retail's own binary contains **both** forms 22 KB apart - masked at
0x8015DDD8 (`IsOneShot__20CScriptStreamedMusicFb`) and unmasked at 0x8018169C. Both are
retail. So MP2's build used more than one compiler, and the question is *which*.

**Twenty GameCube compilers are already on this machine** (`GC/1.0` .. `GC/3.0a5.2`), so
this is a sweep, not a purchase.

Two earlier attempts at this probe were wrong and the reasons are worth keeping:

1. A reduced flag set made *every* version look unmasked. It had dropped `-inline auto`,
   and "register-resident" is exactly the condition under which the mask appears. **A probe
   that does not reproduce the failure cannot rank anything.**
2. Adding flags back introduced `-str reuse` twice plus `-str reuse,pool,readonly`, and
   `-multibyte` on GC 3.0, where `configure.py` requires `-enc SJIS` instead. Every
   version failed to compile.

So: the flag list is read out of `configure.py` rather than retyped, and the
`-enc SJIS` / `-multibyte` split is honoured.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent  # the repo root, not tools/
TC = pathlib.Path("/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort")
OBJDUMP = ROOT / "build/binutils/powerpc-eabi-objdump"  # this repo, not the sibling port

SRC = r"""
extern "C" unsigned sink(unsigned);
extern "C" int leaf_bool(bool x) { return !x; }
extern "C" int after_call_bool(bool x) { sink((unsigned)x); return !x; }
extern "C" int store_field(bool x, bool* out) { *out = x; return !*out; }
extern "C" int local_const(bool x) { const bool b = x; return !b; }
"""

SHAPES = ["leaf_bool", "after_call_bool", "store_field", "local_const"]


def flag_string(major: float) -> str:
    """configure.py's cflags_base, with its >= 3.0 encoding split, as a SHELL string.

    A string, not an argv list, and that is not a style choice. Building the list in
    Python and passing it to subprocess reproduces **none** of the working invocations:
    `-RTTI off` and friends get split, `-multibyte` lands after `-lang=c++`, and every
    version aborts with "Specified file 'off' not found". The gate's own command line is a
    shell string, so this is one too - the flags and their order are then exactly the ones
    the decompilation build uses, which is the only comparison worth making.
    """
    enc = "-enc SJIS" if major >= 3 else "-multibyte"
    return (
        '-nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off '
        '-O4,p -inline auto -pragma "cats off" -pragma "warn_notinlined off" -maxerrors 1 '
        '-nosyspath -RTTI off -fp_contract on -str reuse -i include -i libc '
        '-i build/G2ME01/include -DBUILD_VERSION=0 -DVERSION_G2ME01 '
        f'{enc} -DNDEBUG=1 -use_lmw_stmw on -str reuse,pool,readonly -gccinc '
        '-inline deferred,noauto -common on -sdata 0 -sdata2 0 -lang=c++'
    )


def masked(disasm: str, sym: str) -> bool:
    """True if `clrlwi` appears in the first instructions of `sym`."""
    m = re.search(rf"<{sym}>:\n(.*?)(?=\n\n|\Z)", disasm, re.S)
    return bool(m and "clrlwi" in m.group(1))


def main() -> int:
    src = pathlib.Path(tempfile.mkdtemp()) / "probe.cpp"
    src.write_text(SRC, encoding="utf-8")
    vers = sorted((p.name for p in (TC / "build/compilers/GC").iterdir()
                   if (p / "mwcceppc.exe").exists()),
                  key=lambda v: [int(x) for x in re.findall(r"\d+", v)])

    print(f"{'version':10s} " + " ".join(f"{s[:12]:>13s}" for s in SHAPES))
    winners = []
    compiled = []
    for v in vers:
        c = TC / f"build/compilers/GC/{v}/mwcceppc.exe"
        out = src.with_suffix(f".{v}.o")
        cmd = (f"{TC / 'build/tools/wibo'} build/tools/sjiswrap.exe {c} "
               f"{flag_string(float(re.match(r'(\d+)', v).group(1)))} "
               f"-c {src} -o {out}")
        r = subprocess.run(["bash", "-c", cmd], cwd=ROOT, capture_output=True, text=True)
        if r.returncode or not out.exists():
            print(f"{v:10s} COMPILE FAILED: {(r.stderr or r.stdout).strip()[:70]}")
            continue
        d = subprocess.run([str(OBJDUMP), "-d", str(out)], capture_output=True,
                           text=True).stdout
        compiled.append(v)
        row = ["MASK" if masked(d, s) else "bare" for s in SHAPES]
        print(f"{v:10s} " + " ".join(f"{x:>13s}" for x in row))
        # retail's shape: unmasked for a bool, i.e. the *leaf* case must be bare
        if row[0] == "bare":
            winners.append(v)

    print()
    if not compiled:
        # Say this, rather than the conclusion below it. The first run of this tool did
        # exactly that - every compile failed because wibo could not resolve the guest
        # program, and it printed "no version emits a bare cntlzw" as though that were a
        # measurement. **A tool that reports a finding when it measured nothing is worse
        # than one that fails**: it is indistinguishable from a real negative.
        print(f"NO VERSIONS COMPILED ({len(vers)} attempted) - this is not a result.")
        return 2
    if winners:
        print(f"compiled {len(compiled)}/{len(vers)}; versions emitting a BARE cntlzw for "
              f"`!x` on a bool: {', '.join(winners)}")
    else:
        print(f"compiled {len(compiled)}/{len(vers)}; **no version emits a bare cntlzw for a "
              f"bool - the mask is universal across every GameCube compiler here**, so this is "
              f"not a version difference after all.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
