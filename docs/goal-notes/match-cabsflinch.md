# match-cabsflinch - DONE (unit flipped: `flip_test.sh` PASS, all 6 functions linked)

## What the item asked, and what the tree already had

The item's reason said 5 of 6 functions were at 100% and only
`Start__10CABSFlinchFR15CBodyControllerR13CStateManager` was at 60.96%. **That was already
fixed before I started** - commit `183e94f` ("progress: match-cabsflinch", 2026-09-30 03:47)
hoisted `bc.GetPASDatabase()` into a `const CPASDatabase& pas` local, and the judge committed it
as a **partial**, because `flip_test.sh` still could not keep the unit. Re-measured on this
branch head: `build/report.json` had **6/6 at 100.00%** for
`main/MetroidPrime/BodyState/CABSFlinch`, so **the function work the item asked for was
already done**. The item had been requeued with nothing left to do except the flip itself.

So this run was the second half: `Start` was at 100% and the unit still would not flip.

## What actually blocked the flip

`./tools/flip_test.sh MetroidPrime/BodyState/CABSFlinch.cpp` failed on the **link**, not on the
code:

```
FAILED: [code=1] build/G2ME01/main.elf
### mwldeppc.exe Linker Error:
#   undefined: 'CPASAnimParmData::CPASAnimParmData(pas::EAnimationState,const
#   CPASAnimParm&,const CPASAnimParm&,const CPASAnimParm&,const
#   CPASAnimParm&,const CPASAnimParm&,const CPASAnimParm&,const
#   CPASAnimParm&,const CPASAnimParm&)'
#   Referenced from 'CABSFlinch::Start(CBodyController&,CStateManager&)' in
#   CABSFlinch.o
```

`Start` constructs a `CPASAnimParmData` to pass to `CPASDatabase::FindBestAnimation`, and our
object emitted an **undefined reference to the mangled constructor**. Nothing in the link
defines it: `powerpc-eabi-nm` over all 1760 `build/G2ME01/obj/**/*.o` finds no `T`/`D` for it
anywhere, and `build/binutils/powerpc-eabi-nm build/G2ME01/main.dol | grep CPASAnimParmData`
is empty.

Retail's own object calls it as **`fn_80079A60`** - a dtk name, 0x184 bytes, at `.text:0x80079A60`
(`config/G2ME01/symbols.txt:2235`). `0x80079A60` falls inside `CPatterned.cpp`'s claimed range
(`0x80073938..0x8007b0dc`), and `CPatterned.o` defines it there. **Retail's constructor is a
TU-local emission that dtk could not name**, so retail's other units call it by the placeholder
name and the tree has no way to spell it.

This is the trap `docs/RUNNING_THE_DECOMP.md:195-213` documents: *"dtk cannot name a TU-local weak
template instantiation ... **Rename the retail symbol instead** - `config/G2ME01/symbols.txt` *is*
the rename mechanism"*, with the two measured traps right below it (a rename must **replace** its
`fn_` line, never be inserted beside it; dtk rewrites `symbols.txt` and **drops a duplicate
address**, so a stale `fn_` line silently leaves the address unnamed). `tools/apply_rename.py`
is the tool for it.

## The fix

One rename, name read out of **our own object** with `powerpc-eabi-nm --undefined-only` (never
guessed, per the doc), applied with the repo's own tool:

```
$ build/binutils/powerpc-eabi-nm --undefined-only \
    build/G2ME01/src/MetroidPrime/BodyState/CABSFlinch.o | grep CPASAnimParmData | sed 's/^ *U //'
__ct__16CPASAnimParmDataFQ23pas15EAnimationStateRC12CPASAnimParmRC12CPASAnimParmRC12CPASAnimParmRC12CPASAnimParmRC12CPASAnimParmRC12CPASAnimParmRC12CPASAnimParm
$ printf 'fn_80079A60 = <that name>\n' | python3 tools/apply_rename.py
renamed 1/1
```

plus `configure.py:594` `NonMatching` -> `Matching`. That is the whole diff, two files, two
lines - **no `src/` change at all**, because the source was already byte-exact. The rename makes
dtk name the base object symbol with the name our object emits, so the reference resolves to the
*same bytes* at the *same address* that retail has.

## The spelling that does NOT work (measured, so nobody repeats it)

Giving the constructor an asm label in `include/Kyoto/Animation/CPASAnimParmData.hpp` -
`CPASAnimParmData(...) asm("fn_80079A60")` - is the obvious first move and **mwcceppc rejects
it**. Measured:

```
#      In: include\Kyoto\Animation\CPASAnimParmData.hpp
#    From: src\MetroidPrime\BodyState\CABSFlinch.cpp
#      19: const CPASAnimParm& parm8 = CPASAnimParm::NoParameter()) asm("fn_80079A60");
#   Error:                        ^
#   type cannot be made into a global register variable;
#   only scalers, doubles, floats and vectors are supported.
```

mwcceppc parses `asm("...")` as its local-register syntax. The same trap is recorded at
`src/MetroidPrime/CMainResetGameState.cpp:124` ("an `extern "C"` asm label - mwcceppc rejects
it"). **The rename in `symbols.txt` is the only route**; there is no source spelling.

## Verification

| Check | Result |
| --- | --- |
| `./tools/flip_test.sh MetroidPrime/BodyState/CABSFlinch.cpp` | **`PASS -> kept as Matching`**, kept 1/1 failed 0 skipped 0 |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (expected) |
| all 86 RELs vs `config/G2ME01/config.yml` | ok (gate step `hashes vs config.yml`) |
| `./tools/gate.sh build/goal/judge/report.base.json` | **GATE PASS** (with `MP_GATE_DOCS_WRITE=1`) |
| `tools/check_symbol_names.py` | `checked 503 units; 0 declared names are missing` |
| `tools/probe_sources.sh` (gate `port probe`) | ok, port link gap ok |
| `tools/check_decl_order.py --unit MetroidPrime/BodyState/CABSFlinch` | ok, emits in retail order |
| `total_functions` | still **28465** (no `splits.txt` edit was needed) |

`build/gate-diff.log`:

```
matched  10032 -> 10032   linked 4896 -> 4902   (+0 functions at 100%, 1 units newly linked)
  LINKED   main/MetroidPrime/BodyState/CABSFlinch
  RENAMED  main/MetroidPrime/Enemies/CPatterned :: fn_80079A60 -> __ct__16CPASAnimParmDataF...
no regression
```

**`linked 4896 -> 4902`, +6, no function worse, no `asm` added, no `src/` line touched.** The
`RENAMED` line is `CPatterned`'s 0.00% -> 0.00% placeholder changing name; the gate's `no
regression` line is the statement that nothing got worse.

Note for the judge: `gate.sh` run **without** `MP_GATE_DOCS_WRITE=1` fails one step,
`docs claims`, complaining only that HANDOFF's `linked 4902` line is stale. That is the derived
count the judge rewrites itself, so I reverted the `docs/HANDOFF.md` edit the write mode made
rather than committing a doc I was told not to touch.

## What this teaches the next lane (a lesson, not a NEW: item)

**A unit at 100% on every function that still will not flip is usually a naming problem, not a
code problem, and `symbols.txt` is the fix.** The loop's own wording for this state - *"the rest
is link-level: tools/compare_unit.sh"* - points at bytes, and `compare_unit.sh` would not have
found this: it compares sections and hex, and our object matched. The failure was in
`flip_test.sh`'s link step, whose `undefined:` line names the symbol. **Read the flip's link
error before reaching for `compare_unit.sh`.**

Second, narrower: **`CPASAnimParmData`'s constructor and `~CPASAnimParmData` are retail code we
cannot name and have never had.** `fn_80079A60` is now named, so it is a 0x184-byte function
inside `CPatterned`'s 30,628-byte range that a `CPatterned` session could now pair. Six other
units (`CGSFidget`, `CGunMotion`, `CGSComboFire`, `CGunController`, `CGSFreeLook`, `CGunWeapon`,
`CScriptDoor`, `CPatterned`, `CBodyController`) all call this same constructor and are all
`NonMatching`; **any of them will hit this identical wall and this identical one-line fix**, so
the rename should make several of them flippable at once. That is a consequence of this change,
not extra work, and the driver will see those units fail their own flips and find the tree
already renamed.
