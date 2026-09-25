# Lane briefing

The briefing handed to every parallel lane. A lane is a git worktree at a fixed commit plus one
worker; this file is its only instructions. Keep it current: when a lane finds a failure mode,
it belongs here, so the next lane does not rediscover it.

Spawn sequence (from `docs/RUNNING_THE_DECOMP.md`, "Parallel lanes"):

```sh
SRC=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port
N=m1
git -C $SRC worktree add -f /tmp/opencode/$N -b mod-$N HEAD
mkdir -p /tmp/opencode/$N/build /tmp/opencode/$N/orig
ln -sfn $SRC/orig/G2ME01 /tmp/opencode/$N/orig/G2ME01
cp -r $SRC/build/binutils /tmp/opencode/$N/build/binutils
printf '\nbuild-clone/\n' >> /tmp/opencode/$N/.gitignore
cp $SRC/docs/LANE_BRIEFING.md /tmp/opencode/$N/LANE.md
```

---

Work only inside the worktree you were given. Do not commit. Do not touch the master tree.

## Build (your own - do not symlink the master tree's build/)

The toolchain lives in the **port** tree next door (`MetroidPrimePort`, not this `MetroidPrime2Port`
checkout), so set the override once per shell - the default `$REPO_ROOT/../MetroidPrimePort` only
happens to be right when the two trees are siblings:

```sh
# If the sibling tree is not at that path, point MP_TOOLCHAIN_DIR wherever MetroidPrimePort is; every
# tool here defaults to ../MetroidPrimePort relative to this repo and fails loudly if it is wrong.
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 configure.py --version G2ME01 \
  --compilers $MP_TOOLCHAIN_DIR/build/compilers \
  --dtk        $MP_TOOLCHAIN_DIR/build/tools/dtk \
  --wrapper    $MP_TOOLCHAIN_DIR/build/tools/wibo \
  --build-dir  build

./tools/decomp_build.sh                 # ninja + the project total + the unmatched worklist
./tools/decomp_build.sh <unit>          # that one unit, with every unmatched function and its %
./tools/flip_test.sh <unit>.cpp         # THE acceptance test - the trailing .cpp is required
./tools/unit_fit.sh <unit>.cpp          # why a unit will not promote: extra emitted functions, sizes
./tools/compare_unit.sh <unit>          # section-by-section diff against the retail-derived object
python3 tools/find_trivial_functions.py # unmatched functions grouped by machine-code shape
```

`orig/G2ME01` is a symlink to the read-only disc files. `build/binutils/` has `powerpc-eabi-nm`
and `powerpc-eabi-objdump`.

## The one rule

**A function counts only when its unit is `Matching` in `configure.py` and the build still
reproduces retail with our object in the link.** objdiff percentages on a `NonMatching` unit are a
signal, not a result: the unit's own code is not in the binary at all. Functions that reach exactly
100% inside a unit that stays `NonMatching` do count in `build/report.json`'s function total, which
is why partial progress is worth reporting - but a *unit* is only done when `tools/flip_test.sh`
keeps it `Matching` and the DOL sha1 stays `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` with all 86
RELs unchanged.

`87 files OK` is not evidence of anything - it validates the parts of the binary nobody touched.

## For a REL module

`dtk` fills every range **no unit claims** with bytes split out of the retail module. So:

1. Scaffold: `python3 tools/scaffold_rel_module.py <Module> [Class] --write`. Its default claims
   the whole module for one unit - **that will not hold, and you must re-split it.**
2. Write the functions you can, in C++. Rename their `fn_<id>_<offset>` symbols in
   `config/G2ME01/rels/<Module>/symbols.txt` to real mangled names so objdiff can pair them, and
   check the names against the retail object with `build/binutils/powerpc-eabi-nm`.
3. **Re-split so the `Matching` unit claims only the ranges its own object reproduces.** Leave
   everything else unclaimed, or give it to `NonMatching` units whose source path does not exist.
   `configure.py` only requires a source file for `Matching` objects, so that is legal, and it is
   what keeps the module's bytes intact.
4. Verify the module hash against `config/G2ME01/config.yml`, **with your unit `Matching`**:

```sh
python3 - <<'PY'
import re, hashlib
cfg = open('config/G2ME01/config.yml').read()
mod = 'YOUR_MODULE'
for name, expected in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    if name[:-4] != mod: continue
    actual = hashlib.sha1(open(f'build/G2ME01/{mod}/{mod}.rel','rb').read()).hexdigest()
    print('OK' if actual == expected else 'DIFF', actual, expected)
PY
```

Worked examples already in the tree: `ScriptRiftPortal` (three-way prefix/matching/tail split,
only the middle unit ours) and `Metaree` (the ranges unclaimed rather than named). Read their
entries in `configure.py` and their splits before you start.

## Rules that have cost real time

- **No assembly.** A transcribed `.s` unit reproduces the bytes and scores 100% while decompiling
  nothing. Report such a module as blocked instead.
- A range your unit claims but does not reproduce **breaks the module's hash** for every REL.
- A module's `.rodata` may be unsplittable: if the whole pool lives in one FORCEACTIVE
  `auto_*_rodata.s` base object (as in `Tweaks`), a unit that contributes `.rodata` breaks the
  module. Prefer units whose gain is `.text` only.
- A symbol you rename must match what the retail object defines, or the REL link fails with
  "Failed to find symbol". A broken link is what catches a bad rename - `ninja`'s exit status - and a
  lost *pairing* shows up in the per-function report diff. Do not lean on
  `python3 tools/check_symbol_names.py`: it covers only the DOL `.cpp` units and largely compares
  `symbols.txt` against objects that dtk named from it.
- Stale `config/`: your worktree carries `config/` as of its commit. Report config changes as a
  **list of intended changes**, do not assume they will be copied verbatim.
- **`CAi` and `CPatterned` now exist as `Matching` units** (11/11 and 10/10), so a creature class can
  be written. What is still missing is the behaviour inside them: most creature virtuals are unnamed
  and `CPatterned`'s constructor is unwritten. Accessors, predicates, loaders and REL setup are the
  cheap work; say plainly what is blocked rather than guessing a body.
- **Never copy `configure.py` or a `config/` file from another tree or an older commit.** Three
  modules (`Puffer`, `WallCrawler`, `ScriptGui`) lost their `Rel(...)` blocks that way; their sources
  sat in `src/` compiled by nothing and read 0.00% in the report, which looks like "not started".
  Report config changes as a list of intended changes and run
  `python3 tools/check_module_wiring.py` if you touched module wiring.
- **Do not delete an initialisation to gain percent.** A source rearrangement that drops real
  assignments can raise a unit's average score while making the function worse; that is a
  regression, not progress, and it will be rejected. Check per-function scores.
- **Before promising to promote a unit, run `tools/unit_fit.sh <unit>.cpp`.** A unit whose object
  emits functions the retail unit object does not define (usually weak container/destructor
  instantiations) can never be `Matching`, however good its percentages look; the tool lists them.
  Being simply *bigger* than the claimed range is not fatal on its own.
- `CActor::CActor(...)`, `CStateManager` and the `Tweaks` `LoadTypedef*` switches are known-hard:
  expect register allocation and instruction scheduling, not logic.

## Report

State: the unit/module, which functions you wrote and their shape, the exact verification command
you ran and its output, what you reverted, and what blocked you. Negative results are valuable - a
blocker characterised stops the next lane repeating the work. Do not commit.

## DOCUMENTATION

The master tree's `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` and `PORT_NOTES.md` are
load-bearing: a session that trusts a stale one wastes its whole budget. Your lane has its own
copy of them, and edits there will be merged - so:

- In your **report**, state plainly what the docs should now say: any blocked pattern you found,
  any technique that worked, any module you attempted and its outcome, and whether the method
  changed. The orchestrator merges docs by hand; do not assume your file edits will be copied.
- **Measure numbers, never recall them.** `build/report.json` is the source of truth, and
  `python3 tools/check_docs_claims.py` derives the numbers the docs claim from it - yours will be
  checked the same way.
