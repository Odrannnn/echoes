# match-main-sda-float-constants

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach on the
`CErrorOutputWindow::__vt` link error attempt 2 recorded, the target rose **60 -> 62 / 99**
functions, and `tools/goal_check.sh build/goal/item.json` passed every other check. Diff is
`src/MetroidPrime/main.cpp` only, 77 insertions / 2 deletions, no asm, no deletion of real work.

## The item's premise is false, and finding that out is the main result

The item was filed on the claim that `CMain::CMain` and `TAverage.hpp` read `.sdata2`
`lbl_8041A3D8` / `lbl_8041A3DC` / `lbl_8041A3F0` (0.0f / 1.0f / 0.0) where retail reads `.sdata`
0x80417D98 / 0x80417D9C / 0x80417DB0, derived from `_SDA_BASE_` = 0x8041FD80 plus the
instructions' signed displacements. **That derivation is wrong, and so is every other one made
the same way.** I applied the change, measured it, and it moved `SetMaxSpeed` 99.25% -> 99.25%
and turned a passing relocation into a mismatching one. Reverted.

### There are two small-data bases, and `_SDA_BASE_` is not r2

Retail's `__init_registers` (0x80003464-0x80003470) loads **two** of them, 0x2640 apart:

```
3c 40 80 42   lis  r2,0x8042   /  60 42 23 c0   ori  r2,r2,0x23C0    ->  r2  = 0x804223C0
3d a0 80 41   lis  r13,0x8041  /  61 ad fd 80   ori  r13,r13,0xFD80  ->  r13 = 0x8041FD80
```

- **r2 = 0x804223C0** - the `.sdata2` window.
- **r13 = 0x8041FD80** - the `.sdata` / `.sbss` window. *This* is the value
  `powerpc-eabi-nm build/G2ME01/main.elf` reports for `_SDA_BASE_`, and the one `tools/sda.py`
  uses.

Both are 32-byte aligned, so the ABI allows either and nothing in the disassembly says which is
which. Four independent checks fix the assignment, and all four agree:

| check | r2 = 0x804223C0 | r13 = 0x8041FD80 |
|---|---|---|
| dtk's own naming of retail's relocations in `build/G2ME01/obj/MetroidPrime/main.o`: `lfs f1,-32744(r2)` -> `lbl_8041A3D8`, `lfs f0,-32740(r2)` -> `lbl_8041A3DC`, `lfd f2,-32720(r2)` -> `lbl_8041A3F0`, `lwz r0,-13760(r2)` -> `lbl_8041EE00` | every one lands on its named symbol | every one is out by 0x2640 |
| `dol_read.py 0x8041A3D0 0x60`: 0x8041A3D8 = 0x00000000, 0x8041A3DC = 0x3F800000 = 1.0f, 0x8041A3F0 = 0x0 as a double | the old comment's three values are exactly right | 0x80417D98 = 0x0000003B, 0x80417D9C = 0x00000008, 0x80417DB0 = a denormal |
| the ctor's ten `lwz r0,-32764(r13)` reach 0x80417D84, whose word is 0x000F4240 - the value `rstl::reserved_vector<uint,10>`'s one-argument fill is documented to use | n/a | **confirmed independently of dtk** |
| `dol_read.py 0x8041EE0`: 0x8041EE00 = 0x00008F00, and `fn_80009864` stores `0x8F00 * 28 / 8 * 4` = 0x7D000 = 512512 into the ARAM size, which is what the reader at 0x80007C2C wants | **confirmed independently of dtk** | n/a |

So: **`tools/sda.py` is wrong for every r2-relative displacement**, and it is wrong silently -
it prints `0x8041C7C0 (exact, .sdata2)` for `-13760`, and `.sdata2` 0x8041C7C0 really is
0x3F7D70A4 = 0.99f, so the output looks perfect. `lbl_80417D84` in the tree is *also* an r13
symbol, which is why it is right. `tools/` is the judge's, so nothing is changed here; the
r2/r13 split is now written into `src/MetroidPrime/main.cpp` at `fn_80009864` so the next reader
does not re-derive it.

**This is why the earlier round's `lbl_8041C7C0` for `fn_80009864` is wrong.** That fix was made
because `python3 tools/sda.py -13760` said so and a reviewer accepted it - but the function was
never in the tree afterwards, so nothing shipped. dtk names the site `lbl_8041EE00`.

## What `SetMaxSpeed`'s 99.25% actually was

Not a constant. The prologue and all twenty body instructions were already byte-identical to
retail; the three epilogue reloads were in the other order:

```
retail  80008a04: lwz r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1) ; mtlr r0
ours    00002180: lwz r31,12(r1); lwz r30,8(r1) ; lwz r0,20(r1) ; mtlr r0
```

A void function with no `mr r3,rN` in its epilogue leaves that order free (`~CMain`, which does
have one, puts the lr reload first and we already match it at 100%). **`const` on the parameter
is the whole fix**: `void CMain::SetMaxSpeed(const bool v)`. Twelve spellings measured, all with
the other 88 bytes unchanged - the table is in the source comment. `const bool v` and
`const bool fading = v;` both give 0 diff bytes; the other ten give 8, and two change the size.

`CMain::SetMaxSpeed__5CMainFb` **99.25% -> 100.00%** (96 B). The top-level `const` is not part of
the signature, so `CMain.hpp` is untouched.

## `fn_80009864` written, retail 0x80009864, 0x1C = 28 B (+1)

```
lwz   r0,-13760(r2) ; mulli r0,r0,28 ; srawi r0,r0,3 ; addze r0,r0 ; slwi r0,r0,2 ;
stw   r0,-28384(r13) ; blr
```

`(*(const int*)&lbl_8041EE00 * 28) / 8 * 4` is the only spelling of `v * 14` that emits all five
of those instructions; five measured (table in the source). Our object's two relocations are
`lbl_8041EE00` and `lbl_80418EA0`, which is what dtk names in retail's object. The unit had no
body for this symbol at all - it was one of the 39 unpaired ones - and the earlier acceptance of
`lbl_8041C7C0` never reached the tree, so nothing wrong shipped with it.

Also fixed: the `lbl_80418EA0` comment named `*(u32*)0x80415980`, which is in `.rodata` and is
not what the load reaches. It now names `lbl_8041EE00` and says so.

## What still stops the flip

Unchanged and not close - 37 of the 99 functions still have no body and `.text` is **SHORT by
7852** of the 17608 claimed (`tools/unit_fit.sh MetroidPrime/main.cpp`; 16 unclaimed extras, all
COMDAT, are pre-existing). `flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

the same `splits.txt`/`.data` question attempt 2 filed, and
`python3 tools/check_decl_order.py --unit MetroidPrime/main` still reports the pre-existing
**would break on a flip** (our `.text` is in descending retail order). None of the three is this
item's to fix.

## Two traps in this tree that cost time here, for the next run

1. **`build/G2ME01/obj/MetroidPrime/main.o` is a stale copy of dtk's *retail* object, and
   `build.ninja`'s link rule feeds the DOL from `build/G2ME01/obj/*.o`.** So `./tools/decomp_build.sh`
   does not relink for a `NonMatching` unit, `sha1sum build/G2ME01/main.dol` is retail's
   whatever we write, and a green DOL hash says nothing about this unit. objdiff, by contrast,
   reads `build/G2ME01/src/MetroidPrime/main.o` (`objdiff.json`'s `base_path`), so the report is
   the real measurement - and it is up to date after a `./tools/decomp_build.sh main`.
2. **A scratch harness that rewrites `src/` and restores it from a backup taken before later
   edits will silently delete those later edits.** Mine did, once, and cost a rebuild cycle.
   Take the backup, or the restore, last.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.07% fuzzy, 23.36% matched, 11.78% linked (10094 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
                                       "matched 10092 -> 10094, linked 4918 -> 4918"
                                       "target rose: main/MetroidPrime/main: 60 -> 62 / 99 functions"
unit: main/MetroidPrime/main          60 -> 62 / 99, fuzzy 47.05% -> 47.22%, matched_code 7364 -> 7488
  SetMaxSpeed__5CMainFb               99.25% -> 100.00% (96 B)
  fn_80009864                         absent  -> 100.00% (28 B)
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (see trap 1)
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp  .text SHORT by 7852; 16 extras, all COMDAT (pre-existing)
git status                            src/MetroidPrime/main.cpp only
```

No function anywhere got worse - the judge's report diff is `+2 functions at 100%` and nothing
fell. `docs/HANDOFF.md` is reverted after the judge; `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## For whoever picks up the r2/r13 split

`build/G2ME01/obj/*.o` is dtk's *retail* object for each unit and carries the correctly-resolved
relocation names, so it is the authority on which small-data address an instruction reaches -
**read it before `tools/sda.py`, not after.** A one-line fix to `tools/sda.py` (take the base
from the register named in the instruction) would retire this whole class of wrong constant; it
is the judge's file, so it is a `NEW:` for whoever owns tooling rather than for a lane.

NEW: match-main-tooling-sda-r2-base | match | MetroidPrime/main | `tools/sda.py` resolves r2-relative
displacements against r13's base 0x8041FD80 instead of r2's 0x804223C0 (retail `__init_registers`
0x80003464), so every `.sdata2` address it reports for an `lfs`/`lfd`/`lwz r2` is out by 0x2640 -
`fn_80009864`'s `-13760` is 0x8041EE00, not the 0x8041C7C0 an earlier review accepted; fixing the
tool and re-checking the tree's other r2-relative constants can change several units' meaning
without changing a single byte of output

NEW: match-main-getaveragevalue-f | match | MetroidPrime/main | `GetAverageValue<f>__FPCfi`
(0x80008B60, 200 B) is still unpaired and is real work, not transcription: 8x-unrolled `fadds`
sum, then `xoris r3,r4,32768` / `lis r0,17200` / two `stw` / `lfd f0,8(r1)` (the 2^52 double
trick) / `fsubs` / `fdivs` / `fmuls`, i.e. `sum / (count - c)` with `lfs f2,-32740(r2)` =
`lbl_8041A3DC` and `lfd f1,-32664(r2)` = `.sdata` 0x80417DE8; `include/Kyoto/TAverage.hpp`'s
`sum * (1.f / count)` is the wrong shape and it is a shared header, so it needs its own change

---

# Run 2 (2026-09-30, lane 3)

Re-measured on this tree first: `main/MetroidPrime/main` was **63 / 99** (the previous run's 62
plus one from `ffedecc progress: match-main-fn-80009274`), `.text` SHORT by 7852, 30 of the 99
functions with no body. Verdict **PARTIAL**, target **63 -> 66**, gate clean, no regression
anywhere. Diff is `src/MetroidPrime/main.cpp` only, +60 lines, no asm.

**The previous run's `NEW:` for `GetAverageValue<f>__FPCfi` is DONE, and its reasoning about
`TAverage.hpp` was wrong.** It is not a wrong shape that needs a shared-header change; the shape
in `include/Kyoto/TAverage.hpp` is **exactly right** and produces retail's 200 bytes verbatim
(0 diff bytes with relocations masked). What was missing was only that nothing in the tree
*instantiated* it - the function was unpaired because no TU reached it, not because it was
spelled wrongly. Fixed by giving `main.cpp` the two callers retail has.

## What landed: the `TReservedAverage<float, 4>` pair, 596 bytes, 3 functions

`CMain::RsMain` (0x80005C6C, 2.38% matched here) calls both, and `build/G2ME01/obj/MetroidPrime/
main.o` carries the six `R_PPC_REL24` records - `fn_800069AC` at 0x80005D0C, 0x80005D18, 0x80006108,
0x80006228 and `fn_80006954` at 0x80006114, 0x80006234. `dtk`'s map has no name for either
(`config/G2ME01/symbols.txt:133-134`), the same situation as `fn_80007040`/`fn_800070A4` which this
file already handles, so they take the `fn_<address>` spelling and objdiff pairs on it.

Written into `src/MetroidPrime/main.cpp` after `fn_80007040` (descending-order neighbours, so
`check_decl_order`'s pre-existing verdict is unchanged):

| symbol | retail | bytes | diff |
|---|---|---|---|
| `fn_800069AC` = `TReservedAverage<float,4>::AddValue(const float&)` | 0x800069AC | 308 | **0** |
| `fn_80006954` = `TReservedAverage<float,4>::GetAverage() const` | 0x80006954 | 88 | **0** |
| `GetAverageValue<f>__FPCfi` (reached by the second) | 0x80008B60 | 200 | **0** |

The bodies are `include/Kyoto/TReservedAverage.hpp`'s `AddValue` and `GetAverage` **verbatim** -
that header needed no change at all, which is the correction to run 1's `NEW:`. `GetAverage` is
*declared* in that header and never defined anywhere in the tree, so `fn_80006954` is now its only
definition. The class parameter is `<float, 4>`, read off `cmpwi r0,4` in `fn_800069AC`; the map's
named `TReservedAverage<f, 8>` members (`AddValue__21TReservedAverage<f,8>FRCf`, 0x800D3D10, 0x134
= the same 308 bytes) are a second instantiation elsewhere in the DOL.

Two spellings were needed to get the *sizes* right, and the reason is worth keeping:
`AddValue` is 308 and `GetAverage` is 88, both over this unit's `-pragma "inline_max_size(125)"`.
Writing them as calls to the class members leaves an extra out-of-line instantiation in the
object and the bodies are not emitted at all; the bodies written out by hand are what make
retail's two symbols appear. Same argument as `TOneStatic.hpp`'s existing note.

## The `xoris r3,r4,32768` in `GetAverageValue` is real, and it is *not* a bug

Run 1 read `sum * (1.f / count)` and flagged the `xoris r3,r4,32768` / `lis r0,17200` /
`lfd f0,8(r1)` / `fsubs` / `fdivs` / `fmuls` tail as the wrong shape. It is retail's own
int-to-float-by-the-2^52-double trick for the divisor, and `sum * (1.f / count)` is what makes
mwcceppc emit it. Checked the two pool entries our object ends up referencing rather than trusting
the 0-diff-byte score, which cannot see a constant's *value*:

```
ours  @2192 (.sdata2 +0x28) = 3f 80 00 00                1.0f
ours  @2195 (.sdata2 +0x30) = 43 30 00 00 80 00 00 00   the 2^52 double
retail 0x8041A3DC (dol_read) = 3f 80 00 00               MATCH
retail 0x8041A428 (dol_read) = 43 30 00 00 80 00 00 00  MATCH
```

Both bit-identical. (The reloc *names* differ - ours are mwcceppc's `@2192`/`@2195`, retail's are
`lbl_8041A3DC`/`lbl_8041A428` - which is the usual "literal vs named symbol" situation and is why
the file already declares `extern const float lbl_8041A3DC;` for `CMain::CMain`. Not changed here:
the values agree, and a flip is not in reach.)

## Tried and did not work, so the next run does not have to

**`CGameGlobalObjects::~CGameGlobalObjects() {}` (retail 0x80006518, 264 B) - reverted, does not
match.** This was the biggest single prize on the board: one `{}` body would have emitted the whole
out-of-line destructor chain - 0x80006518, 0x80006678, 0x800066D0, 0x80006724, 0x800067A8,
0x800067E0, 0x80006830, 0x80006850, 0x80006874 and 0x80008B04, nine functions and ~1000 bytes,
because those member types' destructors are declared-not-defined so each stays a `bl`. Measured:
we emit 252 bytes, not 264, and 150 of the 264 differ. **The blocker is the class layout, not the
body.** Retail's first teardown is at **+0x150** (`bl fn_801F097C`) and its second is `+0x14C`
(`bl __dt__80006678`); ours starts at +0x14C and 0x80006678 is `rstl::single_ptr<X>::~single_ptr`
whose body calls `__dt__CGameGlobalObjects` itself - i.e. **retail's +0x14C holds a
`single_ptr<CGameGlobalObjects>`, a self-pointer, not the `single_ptr<CInGameTweakManager>` the
header has there** (`include/MetroidPrime/CGameGlobalObjects.hpp:124`). Ours also calls
`CMemory::Free` where retail calls `TOneStatic::operator delete`, and inlines
`single_ptr<IRenderer>` where retail makes the vtable call itself. Fixing this is a
`CGameGlobalObjects` layout change in a shared header, which is a different item.

**`fn_80008B04` is `TOneStatic<CGameGlobalObjects>::operator delete`** and the tree already
matches its twin `__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (0x80008A78, 44 B) at 100%, so
the body is known-correct - but nothing in `main.cpp` *uses* `operator delete` on a
`CGameGlobalObjects`, so the weak instantiation is not emitted. Its only retail caller is
0x80006600, inside the destructor above, so it is downstream of the same blocker. `dtk` has no
mangled name for it (`symbols.txt:174` is `fn_80008B04`) while it does for the
`CGameArchitectureSupport` one, so objdiff could not pair our
`__dl__32TOneStatic<18CGameGlobalObjects>FPv` even once it is emitted.

**`fn_800068F4` / `fn_800069AC`'s neighbours 0x80006830-0x80006874** are the 0x80006518 chain
above, not standalone functions.

**`reserve__Q24rstl55vector<pair<Ui,Ui>,rmemory_allocator>Fi` (0x80008DE8, 172 B) and
`fn_80008E94` (0x80008E94, 172 B) are byte-for-byte the same function** - one is
`CGameState::xf4_`'s `reserve`, the other an unnamed twin, and neither has a caller in main.o, so
both are COMDAT copies the linker discards. Writing `rstl::vector::reserve` out of line does not
put them in the object without a caller, and inventing a caller would be transcription.

**`fn_80008C28` / `fn_80008CE0` / `fn_80008D68`** (0x80008C28, 184 B; 0x80008CE0, 136 B; 0x80008D68,
128 B) are a mutually recursive 44-byte-node tree with an `rstl::basic_string` at +0x10. Only
0x80008C28's two self-calls exist in main.o; nothing reaches the group from the unit, so it is the
same COMDAT problem.

**The four 80-byte `ReleaseData` bodies** - `rc_ptr<CMapWorldInfo>` (0x80009058),
`fn_80009224` (`rc_ptr<CWorldLayerState>`), `rc_ptr<CPlayerState>` (0x8000934C), `fn_800095E4`
(`rc_ptr<CWorldTransManager>`) - are `include/rstl/rc_ptr.hpp`'s existing `ReleaseData()` and the
two other instantiations of it in this unit already match at 100%. They have **no caller in
main.o** either (the only `R_PPC_REL24` in range is 0x80009254 -> `__dt__16CWorldLayerStateFv`,
which is a callee, not a caller). Emitting them needs a `CWorldState`/`CGameState` teardown in
this TU, and those classes' destructors belong to `CGameState.cpp`.

## A fast per-function diff harness, since 0.4 s per compile makes guessing cheap

`build/G2ME01/obj/MetroidPrime/main.o` is dtk's **retail** object and its `.text` starts at the
unit's retail vaddr (**0x800053B8**, from the report's `sections[].metadata.virtual_address` -
not 0x80003858 as the offsets suggest), so retail bytes for a function are
`obj/MetroidPrime/main.o`'s `.text` at `addr - 0x800053B8`, ours are
`src/MetroidPrime/main.o`'s at the symbol's nm offset, and the relocation fields are masked in
both so a differently-*named* call still compares equal. Cross-checked against
`tools/dis.sh` on `fn_80009864` (0 diff, 28 B) and `__dl__38TOneStatic<24CGameArchitectureSupport>FPv`
(0 diff, 44 B), both of which the report already scores 100%. It also prints the two relocation
lists side by side, which is how the `lbl_8041A428` / `@2195` difference above was found.
`./tools/decomp_build.sh <unit>` alone re-runs ninja + the whole report; for iterating,
`$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja build/G2ME01/src/MetroidPrime/main.o` is **0.4 s**.

Two things this harness showed that objdiff's percentage does not: `fn_800070A4` is at 86% and
**19 of its 80 diff bytes are pure register allocation** (same instructions, r3/r4/r5/r8/r9/r10
renumbered) - that is a wall and was left alone; and every matched function here really is
0-diff-bytes, so the score is not hiding a wrong constant.

## What still stops the flip

Unchanged and not close. `.text` is **SHORT by 7084** of the 17608 claimed (was 7852; this run
closed 768 of it), 30 of the 99 functions still have no body. `flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

the same `splits.txt`/`.data` question run 1 filed, and
`python3 tools/check_decl_order.py --unit MetroidPrime/main` still reports the pre-existing
**would break on a flip** with the same first-8 comparison (the new pair is in the right place
relative to its neighbours and did not change the verdict). None of the three is this item's.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.08% fuzzy, 23.37% matched, 11.78% linked (10099 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
                                       "matched 10096 -> 10099, linked 4918 -> 4918"
                                       "target rose: main/MetroidPrime/main: 63 -> 66 / 99 functions"
tools/report_diff.py <base> build/report.json
                                       "+3 functions at 100%, 0 units newly linked" / "no regression"
unit: main/MetroidPrime/main          63 -> 66 / 99, fuzzy 48.13% -> 51.51%, matched_code 7588 -> 8000
  fn_800069AC                         absent  -> 100.00% (308 B)
  fn_80006954                         absent  -> 100.00% (88 B)
  GetAverageValue<f>__FPCfi           absent  -> 100.00% (200 B)
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh              750 files, 0 failed; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp  .text SHORT by 7084; 16 extras, all COMDAT (pre-existing)
git status                            src/MetroidPrime/main.cpp only
```

`docs/HANDOFF.md` is reverted after every build; `gate.sh` rewrites it and the driver owns it.

## For the next run on this unit

The remaining 30 unpaired functions split cleanly into two groups, and the split is the useful
part:

* **COMDAT weak copies with no caller in `main.o`** (0x80008B04, the four 80-byte `ReleaseData`s,
  0x80008DE8/0x80008E94, 0x80008C28/0x80008CE0/0x80008D68, 0x80008E94). objdiff scores them 0%
  because the *retail object* has them, but they are unreachable from this unit, so no amount of
  source in `main.cpp` emits them. Each needs either a real caller in a TU that claims that caller
  (a `CWorldState`/`CGameState` teardown for the `ReleaseData`s, `CGameState.cpp` territory) or
  accepting that `main.cpp` is the wrong home for it.
* **Genuinely reachable, large, still open**: `__dt__CGameGlobalObjects_80006518` and its
  nine-function chain (blocked on the +0x14C layout, see above), `AddPaksAndFactories` (0.21%,
  1936 B), `CheckReset` (0.34%, 1180 B), `StreamNewGameState` (18.68%, 532 B),
  `InitializeSubsystems` (12.44%, 348 B), `RsMain` (2.38%, 2148 B). The four big ones are real
  work, not transcription, and `RsMain` is what calls all three functions landed here.

NEW: match-main-cgameglobalobjs-14c | match | MetroidPrime/main | retail's `+0x14C` member of
`CGameGlobalObjects` is a `single_ptr<CGameGlobalObjects>` (a self-pointer whose destructor
0x80006678 calls `__dt__CGameGlobalObjects` itself), not the `single_ptr<CInGameTweakManager>` the
header declares there, so `CGameGlobalObjects::~CGameGlobalObjects(){}` emits 252 bytes instead of
retail's 264 and 150 differ - correcting the +0x14C member (and the +0x148 `single_ptr<IRenderer>`
vtable call and the `TOneStatic::operator delete` tail in place of `CMemory::Free`) is a
`CGameGlobalObjects.hpp` layout change that unlocks nine functions and ~1000 bytes at once
