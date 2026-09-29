#!/usr/bin/env python3
"""Which GameCube mwcceppc version, on the REAL CErrorOutputWindowCtor.cpp source, gets
closest to retail's object - and does any version match it byte-for-byte?

`probe_cntlzw_versions.py` answered this for an isolated `!bool` and found 3.0a* emits a bare
`cntlzw`. That is necessary but **not sufficient**: 3.0a* also rewrites retail's four separate
`lbz`/`rlwimi`/`stb` read-modify-write pairs on the `bool : 1` group into one
`lbz`/`ori`/`rlwimi`/`stb`. So the honest question is per-version and per-object, which is what
this measures: rank every compiler on this tree by differing instructions against
`build/G2ME01/obj/MetroidPrime/CErrorOutputWindowCtor.o`.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TC = pathlib.Path("/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort")
OBJDUMP = ROOT / "build/binutils/powerpc-eabi-objdump"
SRC = "src/MetroidPrime/CErrorOutputWindowCtor.cpp"
RETAIL = ROOT / "build/G2ME01/obj/MetroidPrime/CErrorOutputWindowCtor.o"
FUNC = "__ct__18CErrorOutputWindowFb"


def cflags(major: int) -> str:
    """configure.py's cflags_retro, with its >= 3.0 encoding split, in configure.py's order."""
    enc = "-enc SJIS" if major >= 3 else "-multibyte"
    return (
        '-nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off '
        '-O4,p -inline auto -pragma "cats off" -pragma "warn_notinlined off" -maxerrors 1 '
        '-nosyspath -RTTI off -fp_contract on -str reuse -i include -i libc '
        '-i build/G2ME01/include -DBUILD_VERSION=0 -DVERSION=0 '
        f'{enc} -DNDEBUG=1 -use_lmw_stmw on -str reuse,pool,readonly -gccinc '
        '-inline deferred,noauto -common on -lang=c++'
    )


def body(obj: pathlib.Path) -> list:
    d = subprocess.run([str(OBJDUMP), "-d", str(obj)], capture_output=True, text=True).stdout
    m = re.search(rf"<{FUNC}>:\n(.*?)(?=\n[0-9a-f]+ <|\n\n|\Z)", d, re.S)
    if not m:
        return []
    out = []
    for ln in m.group(1).splitlines():
        # keep the mnemonic + operands; the address column and the raw bytes are noise
        parts = ln.split("\t")
        if len(parts) >= 3:
            out.append(" ".join(parts[2].split()))
    return out


def main() -> int:
    retail = body(RETAIL)
    if not retail:
        print(f"no retail disassembly for {FUNC} in {RETAIL} - this is not a result")
        return 2
    tmp = pathlib.Path(tempfile.mkdtemp())
    vers = sorted((p.name for p in (TC / "build/compilers/GC").iterdir()
                   if (p / "mwcceppc.exe").exists()),
                  key=lambda v: [int(x) for x in re.findall(r"\d+", v)])
    print(f"retail: {len(retail)} instructions")
    print(f"{'version':10s} {'instrs':>6s} {'diff':>5s}  cntlzw  first difference")
    rows = []
    for v in vers:
        out = tmp / f"{v}.o"
        major = int(re.match(r"(\d+)", v).group(1))
        cmd = (f"{TC / 'build/tools/wibo'} build/tools/sjiswrap.exe "
               f"{TC}/build/compilers/GC/{v}/mwcceppc.exe {cflags(major)} "
               f"-c {SRC} -o {out}")
        r = subprocess.run(["bash", "-c", cmd], cwd=ROOT, capture_output=True, text=True)
        if r.returncode or not out.exists():
            print(f"{v:10s} COMPILE FAILED: {(r.stderr or r.stdout).strip()[:60]}")
            continue
        ours = body(out)
        n = max(len(ours), len(retail))
        diff = sum(1 for i in range(n)
                   if (ours[i] if i < len(ours) else "<end>") != (retail[i] if i < len(retail) else "<end>"))
        first = next((i for i in range(n)
                      if (ours[i] if i < len(ours) else "<end>") != (retail[i] if i < len(retail) else "<end>")), None)
        fd = f"{first:3d} ours={ours[first] if first < len(ours) else '<end>'} / retail={retail[first] if first < len(retail) else '<end>'}" if first is not None else "-"
        has_c = "yes" if any(x.startswith("cntlzw") for x in ours) else "no"
        print(f"{v:10s} {len(ours):6d} {diff:5d}  {has_c:5s}  {fd}")
        rows.append((diff, v, len(ours)))
    if not rows:
        print(f"NO VERSIONS COMPILED ({len(vers)} attempted) - this is not a result.")
        return 2
    rows.sort()
    print()
    print(f"best: {rows[0][1]} with {rows[0][0]} differing instructions ({rows[0][2]} instructions vs retail's {len(retail)})")
    exact = [v for d, v, _ in rows if d == 0]
    print("byte-exact versions: " + (", ".join(exact) if exact else "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
