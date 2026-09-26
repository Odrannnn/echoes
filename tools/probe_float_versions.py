#!/usr/bin/env python3
"""Would a GC/3.0a*-built object fix the units that fail on *register allocation* rather
than on logic?

`probe_cntlzw_versions.py` established the bool-mask axis, and the mixed-build experiment on
`CErrorOutputWindowCtor` established that 3.0a* gets that axis right and a *different* axis
wrong (it coalesces retail's four separate `lbz`/`rlwimi`/`stb` read-modify-write pairs into
one). Float register allocation is a third axis again, so "3.0 fixes the bool" says nothing
about it and has to be measured per object.

For each unit named on the command line (default: the three register-allocation candidates),
compile the source with GC/2.7 and with GC/3.0a3 under that library's own cflags, pair our
functions against the retail object by name, and report the differing-instruction count and
the instruction count for the function that is not 100% in the current report.

The metric is **differing instructions**, not objdiff's percentage, because a percentage is
size-dominated and these units are all register-allocation questions where the instruction
count is identical and only the register *number* differs.
"""
import json
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TC = pathlib.Path("/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort")
OBJDUMP = ROOT / "build/binutils/powerpc-eabi-objdump"

# (unit name under main/, the retail object under build/G2ME01/obj/, the function, our source)
# All three are built by the same `cflags_retro` as CErrorOutputWindowCtor (read out of
# build.ninja), so the only variable between the two compiles is the compiler binary.
TARGETS = [
    ("Kyoto/Math/CFrustumPlanes", "Kyoto/Math/CFrustumPlanes",
     "__ct__14CFrustumPlanesFRC12CTransform4ffffbf", "src/Kyoto/Math/CFrustumPlanes.cpp"),
    ("Kyoto/Math/Carve8001994C", "Kyoto/Math/Carve8001994C",
     "Cross__9CVector3fFRC9CVector3fRC9CVector3f", "src/Kyoto/Math/Carve8001994C.cpp"),
    ("Kyoto/Graphics/CGX", "Kyoto/Graphics/CGX",
     "SetVtxDescv_Compressed__3CGXFUi", "src/Kyoto/Graphics/CGX.cpp"),
]

CFLAGS_27 = (
    '-nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off '
    '-O4,p -inline auto -pragma "cats off" -pragma "warn_notinlined off" -maxerrors 1 '
    '-nosyspath -RTTI off -fp_contract on -str reuse -i include -i libc '
    '-i build/G2ME01/include -DBUILD_VERSION=0 -DVERSION_G2ME01 '
    '-multibyte -DNDEBUG=1 -use_lmw_stmw on -str reuse,pool,readonly -gccinc '
    '-inline deferred,noauto -common on -lang=c++'
)
# The one token GC/3.0 needs instead: configure.py:226-230.
CFLAGS_30 = CFLAGS_27.replace("-multibyte", "-enc SJIS")


def functions(obj: pathlib.Path) -> dict:
    d = subprocess.run([str(OBJDUMP), "-d", str(obj)], capture_output=True, text=True).stdout
    out = {}
    for m in re.finditer(r"^([0-9a-f]+) <([^>]+)>:\n(.*?)(?=\n[0-9a-f]+ <|\Z)", d, re.S | re.M):
        ins = []
        for ln in m.group(3).splitlines():
            p = ln.split("\t")
            if len(p) >= 3:
                ins.append(" ".join(p[2].split()))
        out[m.group(2)] = ins
    return out


def compile_with(ver: str, cflags: str, src: str, out: pathlib.Path) -> bool:
    major = int(re.match(r"(\d+)", ver).group(1))
    cmd = (f"{TC / 'build/tools/wibo'} build/tools/sjiswrap.exe "
           f"{TC}/build/compilers/GC/{ver}/mwcceppc.exe {cflags} -c {src} -o {out}")
    if major < 3:
        cmd = cmd.replace("-enc SJIS", "-multibyte")
    r = subprocess.run(["bash", "-c", cmd], cwd=ROOT, capture_output=True, text=True)
    return r.returncode == 0 and out.exists()


def main() -> int:
    tmp = pathlib.Path(tempfile.mkdtemp())
    print(f"{'function':52s} {'ver':7s} {'ours':>5s} {'retail':>6s} {'diff':>5s}")
    for _unit, retail_name, fn, src in TARGETS:
        retail = functions(ROOT / "build" / "G2ME01" / "obj" / f"{retail_name}.o").get(fn)
        if retail is None:
            print(f"{fn:52s} NOT FOUND in retail object {retail_name}.o")
            continue
        for ver, cflags in (("2.7", CFLAGS_27), ("3.0a3", CFLAGS_30)):
            o = tmp / f"{ver}.o"
            if not compile_with(ver, cflags, src, o):
                print(f"{fn:52s} {ver:7s} COMPILE FAILED")
                continue
            ours = functions(o).get(fn)
            if ours is None:
                print(f"{fn:52s} {ver:7s} function absent from our object")
                continue
            n = max(len(ours), len(retail))
            diff = sum(1 for i in range(n)
                       if (ours[i] if i < len(ours) else "<end>")
                       != (retail[i] if i < len(retail) else "<end>"))
            print(f"{fn[:52]:52s} {ver:7s} {len(ours):5d} {len(retail):6d} {diff:5d}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
