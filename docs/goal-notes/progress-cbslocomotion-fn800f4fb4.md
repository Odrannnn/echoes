# progress-cbslocomotion-fn800f4fb4 — `fn_800F4FB4` is 100%, and the constructor with it

`kind: progress`, target `MetroidPrime/BodyState/CBSLocomotion`. **37 / 49 -> 38 / 49 matched**,
unit fuzzy **79.28% -> 80.52%**, `matched_code` **5436 -> 5504** of 9280 (`58.58% -> 59.31%`).
Tree-wide `All: 31.07% fuzzy, 23.36% matched, 11.78% linked`, matched **10092 -> 10093**, linked
**4918 held**. `tools/goal_check.sh build/goal/item.json` -> `PASS`.

## The item's premise was wrong, and the type was already in the tree

The queue reason (`progress-cbslocomotion-dtors`) said `fn_800F4FB4` (0x800F4FB4, 68 B) is a
relocation-free 8-`double` memberwise copy and that the POD had to be *identified* before it
could be written. It is identified, and it is not a new type at all:

`GetLocoAnimation` (0x800F40DC) reads a cell at `this + 0x10 + row * 0x44 + cell * 8`, and
`CBSBiPedLocomotion`'s constructor walks rows with a 0x44 stride, 15 of them, from `this + 0xc`.
So the element is **0x44 bytes: an `int` at `+0x00` and eight `rstl::pair<int, float>` at
`+0x04`** — which is exactly `rstl::reserved_vector< rstl::pair< int, float >, 8 >`, the type the
header already used for `mAnims`' rows (`int mCount; uchar mData[8 * 8]`). The earlier run's
"no class of size 0x40 made of doubles exists" was the right measurement and the wrong
conclusion: the doubles are not a type of their own, they are a **block copy** of that row.

The caller settles it. Retail's constructor, 0x800F4448..0x800F4468:

```
mr   r3, r26            ; &row
addi r4, r1, 0x58       ; &prototype on the stack
bl fn_800F4FB4          ; copies row+0x00 .. row+0x3f
lwz  r0, 0x98(r1)       ; = prototype+0x40
addi r25, r25, 1
cmpwi r25, 0xf
stw  r0, 0x40(r26)      ; row+0x40 .. row+0x43
addi r26, r26, 0x44
blt .L_800F4448
```

0x44 = 8k + 4, so a block copy splits at the largest 8-byte boundary: the first **0x40** goes out
of line as `fn_800F4FB4` and the **trailing word stays inline in the caller**. That is also why
the same sequence appears at 4/6/8/10 doubles in `CElementGen` / `CDecalManager` /
`CBSLocomotion` / `CMorphBall` (0x24/0x34/0x44/0x64 = 8k + 4 each): one compiler-generated
block-copy routine per unit, not four hand-written types.

## The change

Two files, both a spelling of retail's own code.

* `include/MetroidPrime/BodyState/CBSLocomotion.hpp` — an explicit specialisation of
  `rstl::construct_impl` (the copy helper `reserved_vector` itself uses, and the pattern
  `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` already establishes) for
  `reserved_vector< pair< int, float >, 8 >`: call `fn_800F4FB4` for the 0x40, then move the
  trailing word as bits. The trailing word moves as bits because it is part of the same block
  copy — retail's `lwz`/`stw`, where a `float` assignment would be `lfs`/`stfs`; `mData[60]` is
  `mAnims[7].second`.
* `src/MetroidPrime/BodyState/CBSLocomotion.cpp` — `fn_800F4FB4` itself, `extern "C"`, placed at
  retail's source position (right after `skMaxPitchAngle`, which is retail's `fn_800F4FF8`, so
  the two come out in retail's `.text` order). Its body is a struct assignment of eight doubles,
  which is the only spelling measured to give retail's instruction selection: 16 `lfd`/`stfd`
  then `blr`, byte-for-byte, no relocations.

`fn_800F4FB4` is **called**, not declared: `objdump -r` shows one `R_PPC_REL24 fn_800F4FB4`, from
the out-of-line `construct_impl<reserved_vector<pair<int,float>,8>>` that
`uninitialized_fill_n` drives once per row — the same fifteen copies retail makes, reached the
same way. Nothing was deleted: the row copy still writes all 0x44 bytes, and the generic
`rstl::construct_impl` it replaces wrote all 0x44 bytes too (mCount word, then eight pairs).

## What it bought, measured

| function | before | after |
|---|---|---|
| `fn_800F4FB4` (68 B) | unmatched | **100.00%** |
| `__ct__18CBSBiPedLocomotionFR6CActor` (468 B) | 89.22% | **99.36%** |

The constructor was the tell that the row copy was wrong, and it is why this is +2 bytes of
quality and not just a new name: before, the fill called an outlined
`construct_impl` that looped over the eight pairs in `lwz`/`lfs` pairs and the constructor was
**428 bytes against retail's 468**. With the block copy it is **468 bytes, retail's exact size**,
`bytescmp` reports 32 differing instructions of 117 (most of them `bl` relocation fields, which
objdiff ignores), and the shape is retail's: `mr r3,r26; addi r4,..; bl fn_800F4FB4; lwz; addi;
cmpwi; stw; addi; blt`.

`tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `no regression`,
`+1 functions at 100%`, `linked 4918 -> 4918`. `check_decl_order.py --unit` ok.
`check_symbol_names.py` -> 504 units, 0 missing. `unit_fit.sh` -> 16 extras, all the COMDAT
weak destructor/vtable copies the head already had; `fn_800F4FB4` is no longer among them.

## Gates, on the tree as it stands

```
sha1sum build/G2ME01/main.dol                 # via gate.sh: the pinned 6ef9b491... , plus all 86 RELs
./tools/goal_check.sh build/goal/item.json    # PASS  (progress: "target rose: ... 37 -> 38 / 49")
./tools/decomp_build.sh                       # All: 31.07% fuzzy, 23.36% matched, 11.78% linked (10093 / 28465)
```

`git status --short` is `include/MetroidPrime/BodyState/CBSLocomotion.hpp` and
`src/MetroidPrime/BodyState/CBSLocomotion.cpp`. Nothing was committed. (The judge rewrote two
derived numbers in `docs/HANDOFF.md` on its way through; that edit was reverted so the diff is
only the two files above.)

## What the next run should not re-try, with the scores

The four shapes below were all measured **in this run**, and the constructor percentage is the
readable signal for each. All of them are the same idea — make the row copy a flat 0x44-byte
block instead of a loop over pairs — done in the wrong place:

| spelling | `__ct__18CBSBiPedLocomotion` |
|---|---|
| head: rows stay `reserved_vector<pair<int,float>,8>`, generic `construct_impl` | 89.22% (428 B) |
| new POD `SLocomotionAnim` row, `mAnims(DefaultLocomotionAnim())` in the mem-init list | 84.76% |
| same POD, `operator=` in the .cpp calling `fn_800F4FB4`, mem-init list | 71.58% |
| same POD, hand-written copy ctor in the header calling `fn_800F4FB4` | 77.33% |
| same POD, `operator=` **defined in the header** (so it inlines into the loop) | 78.21% |
| same POD, `memcpy(..., 4)` for the trailing word | 70.11% — mwcceppc emits a real `bl memcpy` for a 4-byte `memcpy`, it does not expand it |
| same POD, pointer-walk loop `for (row = data(); row != data()+15; ++row)` | 79.51% |
| **rows unchanged; `rstl::construct_impl` specialised for that one instantiation** | **99.36% (468 B)** |

Two things the dead ends teach, so nobody spends a run on them:

* **Do not change the row type.** Retail's rows are `rstl::reserved_vector< pair< int, float >, 8 >`
  — the head's type was already right, and the destructor behaviour
  (`is_trivially_destructible`, the four 124-byte locomotion destructors) depends on it. Every
  spelling that replaced it with a bespoke POD cost 5-19 points on the constructor and bought
  nothing, because mwceppc inlines a POD row copy as an `lwz`/`lfs` loop instead of calling a
  block-copy helper.
* **The fill must stay where `rstl` puts it.** Retail inlines the 15-iteration fill into the
  constructor; mwceppc 2.7 will not (its `inline_max_size(125)` outlines it), and every attempt
  to write the loop by hand to force the inlining changed register allocation and made the
  constructor worse. Specialising the copy helper moves the bytes that matter and leaves the
  call structure alone.

## Still open in this unit, re-measured

Unchanged by this run, and none of it is a spelling problem:

* `fn_800F4FF8` (12 B) — the static initialiser for `skMaxPitchAngle`, 0.00%. Identified and
  parked by `progress-cbslocomotion-dtors`: it needs `.sdata2 0x8041B8FC` and
  `.sbss 0x80419140` claimed, which is a carve of two `auto_*` data ranges. Left alone.
* `__ct__18CBSBiPedLocomotionFR6CActor` at **99.36%** — 32 differing instructions of 117, now
  468 bytes against retail's 468. The residue is register allocation in the second loop
  (`lwz r23,0x14(r1)` vs `r21`, `cmpwi r23,-1` vs `r23`/`r15`) and `bl` fields.
* `__ct__18CBSFlyerLocomotionFR6CActorb` 99.05% — the three epilogue `lwz` reloads in the wrong
  order; `WALL:` from the earlier run, two spellings, unchanged.
* The four 83.39% destructors — the `.data` vtable `lis` fold; `WALL:` from the earlier run.
* `ApplyLocomotionPhysics__13CBSLocomotion` 86.78% — register allocation, ours 600 B vs 640 B.
* `UpdateLocomotionAnimation__18CBSBiPedLocomotion...` (584 B) and
  `UpdateLocomotionAnimation__20CBSBlendedLocomotion...` (1052 B) at 0.96% / 0.76% — still
  8-byte stubs in our object. 1636 bytes of animation-selection logic, the largest prize here.
