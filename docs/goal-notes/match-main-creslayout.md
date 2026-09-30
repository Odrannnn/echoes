# match-main-creslayout (lane 1, 2026-09-30) - `fn_80006724` closed; the unit is 95/99

Verdict **PARTIAL** (`tools/goal_check.sh build/goal/item.json`, exit 3): gate ok, counts ok,
names ok, **target rose 94 -> 95 / 99**, no asm. The flip still fails on the pre-existing
`CErrorOutputWindow::__vt` link error that
`docs/goal-notes/match-main-cmain-0x91-bitfield.md` records for attempts 2-7 of the previous
item. Diff is **one file, `src/MetroidPrime/main.cpp`**, one function body plus its comment.

## The item's own function is already matched - measured before acting

`build/report.json` on the clean tree of this worktree:

```
main/MetroidPrime/main   94 / 99 functions matched, 68.55% fuzzy
fn_80009008   NOT in the <100% list  -> already 100.00%
```

So the reason this item was filed on (`fn_80009008`, 80 B, `rc_ptr<CRelayTracker>::ReleaseData`)
is **closed** - a previous run landed it, and `src/MetroidPrime/main.cpp:1773` now spells it with
retail's own name against `fn_800B8CA0`. This is **not** written as `STALE:`, because the item's
*target* is the unit `MetroidPrime/main` and the unit is not done: five functions are still
unmatched. The work below raises the target's count, which is what the judge measures.

The five that are left, at the base measurement (94/99):

| function | size | % |
|---|---|---|
| `InitializeSubsystems__5CMainFv` | 348 | 12.44 |
| `AddPaksAndFactories__18CGameGlobalObjectsFv` | 1936 | 0.21 |
| `CheckReset__5CMainFv` | 1180 | 0.34 |
| `fn_80006724` | 132 | 78.21 |
| `RsMain__5CMainFiPCPCc` | 2148 | 2.38 |

## `fn_80006724` 78.21% -> **100.00%** - 33 of 33 instructions, byte-identical

**The recipe was already in this file, 60 lines above the function, and three previous runs had
not read it across.** `fn_800068F4` (100%, in the same unit) is *the same shape*: the same four
frame slots in the same order, the same two dead stores, the same double read of the member.
Copying its body verbatim - four locals with `volatile` on the 2nd and 4th, a separate `end`
temporary that both `last` locals are assigned from, and `self->mUnkC` read in **two separate
source expressions** for the two `first` locals - is 100% on the first try, no iteration:

```c
extern "C" void* fn_80006724(CInGameTweakManager* self, short flag) {
  if (self) {
    STweakValue* first;
    STweakValue* volatile firstCopy;
    STweakValue* last;
    STweakValue* volatile lastCopy;
    STweakValue* end = reinterpret_cast< STweakValue* >(self->mUnkC) + self->mUnk4;
    last = end;
    lastCopy = end;
    firstCopy = reinterpret_cast< STweakValue* >(self->mUnkC);
    first = reinterpret_cast< STweakValue* >(self->mUnkC);
    fn_800067A8(&first, &last);
    CMemory::Free(reinterpret_cast< void* >(self->mUnkC));
    if (flag > 0) { CMemory::Free(self); }
  }
  return self;
}
```

Three things in it are load-bearing, and each is what a previous run was one step short of:

1. **`volatile` on the 2nd and 4th locals.** Without it mwcceppc proves the two copies redundant
   and drops their stores: 30 instructions, only the two `stw`s the two-local spelling has.
   (The note in `match-main-ciengametweakmanager-dtor.md` claiming "a `volatile` local whose
   address is never taken is deleted" is **wrong for this spelling** - it is deleted when it is
   declared and never assigned again, i.e. when the compiler can fold it into the original.)
2. **The separate `end` temporary, and both `last` locals assigned from it.** Assigning `last`
   from `data + count` puts the multiply's sum in r0 (`add r0,r5,r0`) where retail has
   `add r5,r5,r0`. This is the same "the base register is the accumulator" effect
   `fn_800068F4`'s comment already documents for its `end`.
3. **Two source expressions for `self->mUnkC`.** That is what forces retail's second
   `lwz r0,12(r30)`, positioned between the two `last` stores. One `data` local CSEs the two
   reads and loses it. (This is also the aliasing-barrier reading in
   `match-main-fn-800067e0-chain.md` - here it needs no aggregate, only two source reads.)

Retail's 33 instructions and ours, in order: `lwz r0,4(r30) / addi r3,r1,20 / lwz r5,12(r30) /
addi r4,r1,12 / mulli r0,r0,72 / add r5,r5,r0 / stw r5,12(r1) / lwz r0,12(r30) / stw r5,8(r1) /
stw r0,16(r1) / stw r0,20(r1) / bl`. The four slots are the four declared locals in declaration
order from the top of the local area down (1st -> `r1+0x14`, 2nd -> `r1+0x10`, 3rd -> `r1+0x0C`,
4th -> `r1+0x08`), and the call takes the 1st and the 3rd - retail's own arrangement.

**Byte-identity was checked on the object, not on a percentage.** A scratch compile of a copy of
`main.cpp` with this unit's exact `build.ninja` flags (`-inline deferred,noauto`,
`-pragma "inline_max_size(125)"`, ...) and a per-function instruction diff that masks branch
displacements and relocated fields: `1 function(s) changed of 117`, and that one is
`fn_80006724: 30 -> 33 insns`. No other function in the object moved, and no symbol was added or
removed. The three `stw`s that are new are the *same code* written more than once - the
instruction count rises 30 -> 33 to 0x84 = 132 bytes, which is retail's claim, and no
initialisation is dropped.

## `InitializeSubsystems` - re-measured, still one register transposition (99.08% shape)

Not this item's blocker and not changed, but measured so the next run does not re-derive it.
`src/MetroidPrime/CMainInitializeSubsystems.cpp` (not claimed by `configure.py`, so absent from
`main/MetroidPrime/main`) already holds the full body at 99.08% in its own unit; the version in
`main.cpp` is still the two-line `ARInit` TODO at 12.44%. Spliced into `main.cpp` it is **87
instructions against retail's 87**, and the whole residue is one transposition - retail has the
loop bound in **r4** (`addi r4,r29,-8192`, `cmplw r6,r4`) and the guard word in **r5**
(`lis r5,29496`, nine `stw r5`), ours has the bound in r5 and the word in r4. 15 instructions,
all of them the same two register numbers.

Three new spellings measured this run, all against the same 87-instruction baseline, none
different from the 27 already in `src/MetroidPrime/CMainInitializeSubsystems.cpp`'s header:

| spelling | result |
|---|---|
| baseline (the file's current body) | 15 differing instructions |
| `uint* dest = guardEnd + 0x400/4;` **plus** an explicit `if (dest < fillStart)` around the fill | 39 differing, 89 instructions - the `if` loses the eight-wide unroll |
| `uint* const limit = fillStart;` and `p < limit` | identical to baseline, 15 |
| `const uint guardWord` local declared *after* `fillStart` | identical to baseline, 15 |
| naming `dest` with **no** `if` | 39 differing - again no unroll |
| `uint* fillStart` assigned from a separate `uint* base8` temporary (the `fn_800068F4` trick) | identical to baseline, 15 |

So the transposition is not reachable by giving the bound more local-level names, and the
`fn_800068F4` "accumulator" trick does not transfer: here it is the *bound*, not a product, that
is competing for a register. The header's own conclusion stands - the lever that flips the
pairing (`uint bytes = ...` named before the loop) costs four bytes of size and is **worse**
(96.38%, and a 352-byte object that no longer fits its 348-byte claim). Recorded here as a
measurement, not as a `WALL:` line: this item's function is not this one and the item is not
blocked by it.

## Verified (this run, all measured, this worktree)

```
./tools/decomp_build.sh                 All: 31.47% fuzzy, 23.89% matched, 11.83% linked
                                        (10357 / 28465 functions)
main/MetroidPrime/main                  94 -> 95 / 99, 68.55% -> 68.71% fuzzy
fn_80006724                             78.21% -> 100.00%  (132 B, 33/33 instructions)
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                        matched 10356 -> 10357, linked 5048 -> 5048,
                                        "+100%  main/MetroidPrime/main :: fn_80006724",
                                        "no regression"
./tools/goal_check.sh build/goal/item.json
                                        PARTIAL (exit 3): gate ok, counts ok, names ok,
                                        "target rose: main/MetroidPrime/main: 94 -> 95 / 99",
                                        "no asm added", flip FAIL (CErrorOutputWindow::__vt)
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/gate.sh                         GATE PASS  f267ec72+2 changed (all 16 sub-checks ok,
                                        incl. docs claims, per-function diff, 86 RELs)
./tools/probe_sources.sh                752 files, 0 failed; link LINKED (250 undefined, 0 dups)
python3 tools/check_symbol_names.py     checked 505 units; 0 declared names are missing
./tools/unit_fit.sh MetroidPrime/main.cpp
                                        .text 13508, SHORT by 4100 (was 13496 / 4112);
                                        18 extras, 1396 bytes - the same set as the base
                                        (the object diff shows no symbol added or removed)
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                        "would break on a flip" (pre-existing, unchanged)
git status                              src/MetroidPrime/main.cpp only
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## What is still in the way, and what is worth the next hour

* The unit cannot flip: `.text` is 4100 bytes short of its claim, `check_decl_order.py` says the
  source order is permuted (pre-existing, wants the whole file reversed), and the DOL link fails
  on `multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`. All three are
  recorded in `docs/goal-notes/match-main-cmain-0x91-bitfield.md`.
* The four large bodies (`RsMain` 2148 B, `AddPaksAndFactories` 1936 B, `CheckReset` 1180 B) are
  essentially unwritten and are each more than an item's worth of disassembly.
* The lesson worth keeping, because it cost four runs: **when a run writes one member of a family
  and stops, read the sibling that already reads 100% in the same file before trying new
  spellings.** `fn_800068F4` was the whole answer for `fn_80006724` and sat 60 lines above it.
