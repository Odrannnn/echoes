# progress-vt-rel-ingboostballguardian

`kind: progress`, `target: module:IngBoostBallGuardian`. **Module matched_functions 42 -> 54 (of
318)**, six new units all `Matching` at 100.00%, `All:` 13369 -> 13381 matched and 6417 -> 6429
linked, `total_functions` still 28465, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and **86/86
RELs both sha1-equal to `config/G2ME01/config.yml` and `cmp`-equal to
`orig/G2ME01/files/RelProd/`** (measured with the recipe's own check). `./tools/goal_check.sh
build/goal/item.json` -> `PASS`.

**Ten of the twelve `fn_` the item names landed, plus two neighbours the claim forced in.** The two
that did not are characterised below with what stops them.

## What I did

Six new units, each one contiguous `.text` range out of module 30's unclaimed gaps, each a run of
adjacent functions so no claim spans an unclaimed gap:

| range | unit | functions | vtable slots (`tools/rel_class_map.py`) |
|---|---|---|---|
| `0xD230..0xD2E0` | `CIngBoostBallGuardianD2xx.cpp` | `fn_30_D230` (0x20), `fn_30_D250` (0x70), `fn_30_D2C0` (0x20) | vtable 1 (`.data`+0x9C0, base `CScriptDoor`): slot 14 `GetDamageVulnerability`, slot 8 `AddToRenderer`, slot 11 `PreRenderAllViewports` |
| `0x11E44..0x11E4C` | `CIngBoostBallGuardian11E44.cpp` | `fn_30_11E44` (0x8) | vtable 2 (`.data`+0xC44, base `CScriptDock`): slot 29 `GetCollisionPrimitive` |
| `0x13B7C..0x13BA4` | `CIngBoostBallGuardian13B7C.cpp` | `fn_30_13B7C` (0xC), `fn_30_13B88` (0x8), `fn_30_13B90` (0x8), `fn_30_13B98` (0xC) | slots 15 and 14, the two `GetDamageVulnerability` overloads; `13B7C`/`13B98` are not slots |
| `0x13C40..0x13C78` | `CIngBoostBallGuardian13C40.cpp` | `fn_30_13C40` (0x1C), `fn_30_13C5C` (0x1C) | slot 13 `GetHealthInfo`, slot 12 `HealthInfo` |
| `0x13E68..0x13ECC` | `CIngBoostBallGuardian13E68.cpp` | `fn_30_13E68` (0x64) | slot 8 `AddToRenderer` |
| `0x1464C..0x1466C` | `CIngBoostBallGuardian1464C.cpp` | `fn_30_1464C` (0x20) | slot 17 `Touch` |

`fn_30_13B7C` and `fn_30_13B98` are not in any vtable and are in the 0x13B7C run because the claim
has to be contiguous: `13B7C` is the halfword copy at +0x4F6 and `13B98` is one bit of the byte at
+0x5D8. Both are called from `auto_00_000038E0_text.o`, so their inclusion costs nothing.

Each unit is the four-part carve in one change: `configure.py` (one `Object(Matching, ...)` per unit
inside the existing `Rel("IngBoostBallGuardian", ...)` block, with the ranges, slots and dead-strip
measurement in the comment), `config/G2ME01/rels/IngBoostBallGuardian/splits.txt`, `files.cmake`
(empty host branch, the reason `CIngBoostBallGuardianBits.cpp` gives), and the source itself.

## How each was measured before it was wired in

The fast loop from the last run's notes, reused unchanged: compile one candidate source with the
unit's own `cflags` (`build.ninja`, `-lang=c++ -O4,p -inline deferred,noauto -sdata 0 -sdata2 0`,
`mw_version = GC/1.3.2`), `objcopy -O binary --only-section=.text`, `cmp` against retail's bytes
sliced out of `auto_00_0000C6CC_text.o` / `auto_00_00011CD8_text.o`, and only then edit the three
config files. **All six are byte-identical to retail's** (176 + 8 + 40 + 56 + 100 + 32 = 412 bytes),
which is why the module's sha1 never needed a second attempt. `nm -n` on each built object shows its
functions at ascending retail offsets (`fn_30_D230`@0, `fn_30_D250`@0x20, `fn_30_D2C0`@0x90; and so
on), i.e. the descending source order is right, and `size` shows the object is exactly its claimed
range with no `.data`/`.bss` (176/8/40/56/100/32).

Three spellings were load-bearing rather than obvious, and all three are the *branch* context, which
is a different MWCC behaviour from the one `CIngBoostBallGuardianBits.cpp` documents:

1. **A one-bit read in a branch has two shapes and they are not interchangeable.** `(b & M) != 0`
   compiles to `rlwinm. rD, rS, 0, MB, ME` with the mask bits exact (`& 0x20` -> `54 00 06 b5`),
   while retail's `rlwinm. r0, r0, 27, 31, 31` is the *materialised* form, which is what
   `(b >> k) & 1` produces - `k = 5` for `fn_30_13E68`'s flag at +0x5D8. Hoisting the mask into a
   `bool` does **not** give the materialised form: MWCC folds it straight back to the branch shape
   (measured both ways). `fn_30_13B98` is a `bool` *return*, where the mask spelling is the right
   one (`& 4` -> `rlwinm r3, r0, 30, 31, 31`), so a unit can contain both spellings and does.
2. **`fn_30_D250`'s case set is the branch tree, not the source's order**: 2, 4, 6, 7, 8, 9 call
   `CPatterned::AddToRenderer`; 3 and 5 test `float(+0x11BC) < lbl_30_rodata_E0` first and call only
   when it holds; every other value calls nothing. Written that way the seventy bytes match
   including MWCC's binary-search branch tree.
3. **The two `GetDamageVulnerability` overloads are the same eight bytes.** Both slots 14 and 15
   return the address of the record at +0x570, so the module answers the `(CVector3f, CVector3f,
   CDamageInfo)` overload with the plain accessor too.

## The brief says "declare it as a member of this module's own class", and that is a measured wall

I tried it: with the class spelled `class CBoostBallGuardian { public: virtual const void*
GetCollisionPrimitive() const; };`, MWCC emits the member **and** a second
`__vt__18CBoostBallGuardian` object in the unit's own `.data` (measured on a scratch file: `nm`
shows both). The module's vtables are dtk's `auto_04_00000000_data.o` and their slot relocations
name `fn_30_*`; a `__vt__` in our object is a symbol the split does not claim and a duplicate of the
one the module already links. So the twelve keep the module's `fn_30_*` names and the brief's
signatures are used for the *shape* only - which is where they paid: every prologue and every
argument list below is what the brief's slot name says, and the two neighbours above were found by
reading the ranges around the named functions.

**The trial's answer, then: naming helped.** Ten of twelve named virtuals became real functions, and
for each one the name gave the return type and the argument count that the bytes alone do not
("is this 8-byte body a getter or a setter, and does the hidden return pointer come first"). It did
not make any of them free: the `optional_object` slot and `fn_30_D2E0` below stayed blocked with the
name known, so a name is a starting point, not a solution. `fn_30_D230`/`D2C0` are the cheap end
(pure forwarders whose names said which DOL function to call); the 0x9C `GetTouchBounds` is the
expensive end.

## Dead-strip, measured

None of the twelve is in `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list. Eleven
are named as undefined by `auto_04_00000000_data.o` (the module's own two vtables) and `fn_30_13B7C`
/ `fn_30_13B98` by `auto_00_000038E0_text.o`, checked one symbol at a time with
`powerpc-eabi-nm` over every object in `build/G2ME01/IngBoostBallGuardian/obj/`. So dtk's own
objects hold every reference, no `force_active:` entry is needed and `config/G2ME01/config.yml` is
unchanged.

## The two candidates that did not land

- **`fn_30_13BA4` (0x13BA4, 0x9C), slot 16 `GetTouchBounds`.** The body is two virtual dispatches
  and no branch at all: `this->vfn[0x7C]()` and then `X->vfn[3](out, this+0x24)` with the result
  written as `optional_object<CAABox>` (six words plus the flag byte at +0x18 = 1). It is therefore
  *not* the DOL's `CScriptDock::GetTouchBounds` body, which branches on two members and calls
  `GetBoundingBox__13CPhysicsActorCFv` directly. Slot 0x7C of the module's own second vtable is
  `fn_30_142BC`, which `tools/rel_class_map.py` names `CollidedWith` off the DOL vtable, so the
  receiver of the first dispatch is not `this` - it is a class this tree does not model, reached at
  `+0x24`. Not claimed: 0x13BA4 stays retail.
- **`fn_30_D2E0` (0xD2E0, 0xA4), vtable 1 slot 7 `PreRender`.** The switch (same case set as
  `fn_30_D250`) compiles to retail's bytes exactly; the tail does not. Retail's tail is
  `lhz r0,0x102(r31)` / `ori r3,r0,0x80` / **`lwz r0,8(r1)`** / `stw r0,0xfc(r31)` /
  `sth r3,0x102(r31)`, i.e. `SetModelFlags(CModelFlags(GetModelFlags(),
  GetModelFlags().GetOtherFlags() | CModelFlags::kF_Unknown80))` with the `CModelFlags` temporary
  materialised in the frame at r1+8 and its (never written) first word copied into the member - the
  shape `CScriptActor::PreRender` has at `build/G2ME01/asm/MetroidPrime/ScriptObjects/CScriptActor.s`
  1400-1405. **Written with this tree's real `CActor`/`CModelFlags` spelling under this unit's
  `-inline deferred,noauto` flags MWCC does not materialise the temporary**: it keeps it in a
  register, the frame shrinks from 0x20 to 0x10 and the function comes out 168 bytes against
  retail's 164 (measured, both with the `>> 7 & 1` and the `>> 6 & 1` bit spellings, and with the
  receiver spelled `char*` and `CActor*`). The same `lwz r0,8(r1)` / `stw r0,0xfc(r31)` pair is in
  the `Ing`, `SpacePirate`, `Grenchler` and `Parasite` modules (byte scan over all 86 retail
  `.rel`s), so it is a family body and one of those modules' decompiled copies may carry the
  spelling; that is the next thing to try, not another arrangement of the same expression.

## Measured results

`build/report.json` for module 30 after the change (all six units `complete`, 100.0):

```
CIngBoostBallGuardianRel                17/17     CIngBoostBallGuardianD2xx        3/3
CIngBoostBallGuardian194C                1/1      CIngBoostBallGuardian11E44       1/1
CIngBoostBallGuardian2094                1/1      CIngBoostBallGuardian13B7C       4/4
CIngBoostBallGuardian388C                1/1      CIngBoostBallGuardian13C40       2/2
CIngBoostBallGuardianA91C                1/1      CIngBoostBallGuardian13E68       1/1
CIngBoostBallGuardianBits                5/5      CIngBoostBallGuardian1464C       1/1
CIngBoostBallGuardianPredicates          7/7      REL/global_destructor_chain      2/2
CIngBoostBallGuardianC6AC                2/2      REL/REL_Setup                    5/5
(plus main/MetroidPrime/ScriptLoader/IngBoostBallGuardian 1/1, the DOL loader unit)
```

Two of the module's gap splits are not a loss, and the judge's report shows it: the module's function
total is 318 before and 318 after, `auto_00_0000C6CC_text` 52 -> 13 with the three claimed functions
leaving a new `auto_00_0000D2E0_text` of 36, and `auto_00_00011CD8_text` 46 -> 2 with the nine
claimed leaving `auto_00_00011E4C_text` 15, `auto_00_00013BA4_text` 1, `auto_00_00013C78_text` 1,
`auto_00_00013ECC_text` 3 and `auto_00_0001466C_text` 15.

`./tools/goal_check.sh build/goal/item.json` -> `PASS` (gate ok including the DOL sha1, all 86 RELs,
the report diff, module wiring, docs claims, the port probe and the module hash; counts 13369 ->
13381 matched and 6417 -> 6429 linked; `target rose: module:IngBoostBallGuardian: 42 -> 54 / 318`).
`tools/check_raw_offsets.py` -> `ok: 189 raw-offset site(s) in 82 file(s)`, two new sections and the
totals paragraph added to `docs/research/raw_offsets.md`. `tools/check_decl_order.py --unit` reports
"0 unit(s) checked" for every one of the six (it does not see REL units), so the substitute is the
`nm -n` order above. `tools/unit_fit.sh` answers "not declared in any splits.txt" for all six - it
reads the DOL `splits.txt` only, not the module's; the module's sha1 is the acceptance test and it
holds.

NEW: progress-vt-rel-ingboostballguardian-b | progress | module:IngBoostBallGuardian | two named virtuals of the vtable-name trial are left and both are characterised: fn_30_D2E0 (0xD2E0, 0xA4, vtable 1 slot 7 PreRender) matches except its tail, which is CScriptActor::PreRender's `SetModelFlags(CModelFlags(GetModelFlags(), GetModelFlags().GetOtherFlags() | CModelFlags::kF_Unknown80))` shape with the temporary materialised at r1+8 (the same `lwz r0,8(r1)` / `stw r0,0xfc(r31)` pair is in the Ing, SpacePirate, Grenchler and Parasite modules, so a spelling that forces the temporary into memory is what it needs - this tree's real CActor/CModelFlags spelling does not under the REL flags), and fn_30_13BA4 (0x13BA4, 0x9C, vtable 2 slot 16 GetTouchBounds) is two virtual dispatches (slot 0x7C then slot 3 of the result, result written as optional_object<CAABox>) over a receiver class this tree does not model
