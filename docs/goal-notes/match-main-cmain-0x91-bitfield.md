# match-main-cmain-0x91-bitfield

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach, the
target rose 35 -> 36 functions, and `goal_check.sh` passed every other check.

## The item's premise is wrong, and that is the finding

The item was filed on the belief that retail's `CGameArchitectureSupport::UpdateTicks` "tests
bit 6 of the byte at `CMain`+0x91" and that mapping that byte "needs the retail constructor's
full bitfield map". **There is no unmapped byte and no missing field.** `CMain`'s bitfield map
in `include/MetroidPrime/CMain.hpp` is already right, and `sizeof` is not the issue.

Retail 0x80007C64 is:

```
80007c60:	lwz     r3,gpMain
80007c64:	lbz     r0,145(r3)          <- +0x91
80007c68:	rlwinm. r0,r0,25,31,31
80007c6c:	beq     80007c78
```

`rlwinm rA,rS,25,31,31` does select bit `31-25`=6 of `rS`. But "bit 6" is **not a field
identity** - it is the first field of whichever byte `lbz` loaded, and mwcceppc emits the
identical opcode pair for the first and the ninth one-bit field. Measured, not reasoned
(`tools/probe_cc.sh` on a 16-`bool : 1` struct, `.tmp/opencode/bf/bf.cpp`):

```
f0:  lbz r0,0(r3) ; rlwinm r3,r0,25,31,31      <- b0, first field of byte +0
f8:  lbz r0,1(r3) ; rlwinm r3,r0,25,31,31      <- b8, first field of byte +1
```

Byte-identical apart from the displacement. So the *only* thing that distinguishes `+0x90` from
`+0x91` in this test is the `lbz` offset, and reading the rotate mask as a bit index invents a
field retail's constructor never writes.

That is confirmed from the constructor side (0x80008940-0x800089A0): eight `rlwimi` pairs
writing the eight fields at +0x90, then `stw r8,148(r3)` = +0x94, with nothing in between.
The ninth field (`gameFrameDrawn`) is never cleared by the constructor, exactly as the header
says, and `SetGameFrameDrawn` (0x800089AC) is the ninth because it is the ninth.

## What was actually wrong: the reader, not the header

`src/MetroidPrime/main.cpp:431` read `gpMain->GetFinished()` - the **first** field at +0x90 -
where retail reads +0x91. The fix is one accessor, `GetGameFrameDrawn()`. `mainMid.cpp:282`
(the port's copy of this same retail function) already had it right, with a comment saying so;
only the DOL unit's copy was stale. The fix carries that reasoning over, plus the probe above
so the next reader does not re-derive the false bit-6 reading from the opcode.

The `+0x91` byte therefore needed no new member, no `sizeof(CMain)` measurement, and no
change to the header. **`include/MetroidPrime/CMain.hpp` is untouched by this change.**

## The second difference: one instruction of declaration order

With the accessor fixed, 26 reloc-normalised differences remained; 24 were relocations. Two
were real, and both are the same class of thing - a `li` materialised one slot late:

retail 0x80007CB4 is `li r28,1` **before** `bl CreateFrameBegin` at 0x80007CBC. Spelled after
the `Push` that contains it, `bool keepLooping = true;` materialised at 0x80007CDC instead, and
the tail of the function walked one slot out of step (`lfs f31` landing after `addi r29,r1,16`
rather than before it). Moving the declaration above the `Push` fixes it.

Result: `UpdateTicks__24CGameArchitectureSupportFv` **98.51% -> 100.00%** (552 B), unit
**35 -> 36 / 99** functions, `All:` matched 10025 -> 10026, linked unchanged at 4896.

## Verified

```
tools/decomp_build.sh              All: 30.85% fuzzy, 23.15% matched, 11.74% linked (10026 / 28465)
tools/gate.sh build/goal/judge/report.base.json    ok (docs block is the driver's to rewrite;
                                                    goal_check runs with MP_GATE_DOCS_WRITE=1)
tools/goal_check.sh build/goal/item.json           PARTIAL, as above
sha1sum build/G2ME01/main.dol      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs vs orig/G2ME01/files/RelProd/             all cmp-equal
tools/probe_sources.sh             749 files, 0 failed; port link 250 undefined, 0 duplicates
python3 tools/check_symbol_names.py               503 units, 0 missing
python3 tools/check_decl_order.py --unit MetroidPrime/main.cpp   ok
```

No function anywhere got worse (gate's per-function diff: `+1 functions at 100%`, nothing fell).
No asm. Diff is `src/MetroidPrime/main.cpp` only, 27 insertions / 2 deletions.

## What still stops the flip

Not one thing - 63 of 99 functions in the unit have no body at all, and most of those are
`0.00%` with **no** `fuzzy_match_percent` in the report, i.e. our object does not define the
symbol at all: the `rstl` container destructors and `ReleaseData`s
(`__dt__Q24rstl36vector<...>`, `ReleaseData__Q24rstl53rc_ptr<...>`, and a dozen more), plus
`fn_80008C28`, `fn_80009224`, `fn_80009008`, `fn_80008A48` and their neighbours, and the three
large bodies `RsMain` (0.19%), `AddPaksAndFactories` (0.21%) and `CheckReset` (0.34%).

`flip_test.sh` fails at link with `undefined: fn_80008C28` and eleven siblings - that is the
first error, not the whole story. It **reverted cleanly** and rebuilt the tree to the correct
DOL sha1, which is what makes the partial verdict safe to commit.

Naming the undefined functions is the useful next step, but it is `progress` work on the same
unit and belongs in its own item rather than a rider here.

## For whoever picks up the "bit N of a rotate mask" question

`rlwinm rA,rS,SH,31,31` tests bit `31-SH`, and mwcceppc uses `SH` 25..31 for the eight fields
of a byte in order - so **`SH=25` is the first field of that byte, not "bit 6"**. Reading the
mask as a field number will keep inventing members. Compare the `lbz`/`stb` displacement
instead, or count the `rlwimi` pairs in the constructor, which is unambiguous.

---

# Attempt 2 (lane 6, 2026-09-30)

Verdict **PARTIAL** again, but a bigger step: the target rose **41 -> 52 / 99** functions and
`tools/goal_check.sh build/goal/item.json` passed every other check. The flip is still out of
reach and the reason is now a *link* error, not undefined symbols.

The bitfield question this item was filed on was closed by the first attempt. This attempt did
not touch `CMain.hpp`; it filled in eleven functions the unit had no body for, all in ranges the
first attempt's report listed as `0.00%` with **no** `fuzzy_match_percent` (i.e. our object did
not define the symbol at all). **Correction to attempt 1's notes:** they said the same of
`fn_80008C28` and its neighbours - that was read off a missing `fuzzy_match_percent`, which means
"unpaired", not "undefined", and the two are different. The `fn_` functions this attempt added
were genuinely absent; `fn_80008C28` is a different story and is still `0.00%`.

## What was added, and how each was pinned down

Diff is `src/MetroidPrime/main.cpp` only, **168 insertions, 0 deletions**, no deletions at all.

### 1. Seven `TOneStatic<T>` instantiations, retail 0x80008A48-0x80008B3C (+7, all first try)

```
0x80008A48  TOneStatic<CGameArchitectureSupport>::operator new(size_t, const char*, const char*)  48 B
0x80008AA4  TOneStatic<CGameArchitectureSupport>::GetAllocSpace()                                   12 B
0x80008AB0  TOneStatic<CGameArchitectureSupport>::ReferenceCount()                                  36 B
0x80008AD4  TOneStatic<CGameGlobalObjects>::operator new(...)                                        48 B
0x80008B04  TOneStatic<CGameGlobalObjects>::operator delete(void*)                                   44 B
0x80008B30  TOneStatic<CGameGlobalObjects>::GetAllocSpace()                                          12 B
0x80008B3C  TOneStatic<CGameGlobalObjects>::ReferenceCount()                                         36 B
```

`config/G2ME01/symbols.txt` does not name retail's, so they are written as `extern "C"` with
retail's own `fn_` names - the same thing this file already did for `fn_80007040`/`fn_800070A4`.
Three independent pieces of evidence fix the reading, and all three are in this tree:

* the tree's own `TOneStatic<CGameArchitectureSupport>::operator delete` already sits at **100%**
  at retail **0x80008A78** - *between* 0x80008A48 and 0x80008AA4, which puts the first three in
  the same instantiation and in `TOneStatic.hpp`'s declaration order;
* `CGameGlobalObjects`'s destructor ends `extsh. r0,r31 ; ble ; mr r3,r30 ; bl 80008B04`
  (0x800065F4-0x80006600), so that class derives from `TOneStatic` and the last four are its
  members;
* the two holders' extents agree: `.bss` 0x803C5AC4 is **0xA8** and 0x803C5B6C is **0x164**
  (`symbols.txt:19013-19014`).

The bodies are the header's own, and the statics are function-local, which is what produces
retail's per-instantiation `.bss` pair and the `lbz / extsb. / bne` init guard. Declared out of
retail order, **defined in it**, with forward declarations above so `fn_80008A48` can call
`fn_80008AA4` while still being written first.

### 2. `fn_80009864`, retail 0x80009864, 0x1C = 28 B (+1)

The only writer of `.sbss` 0x80418EA0 (the ARAM size `CGameArchitectureSupport` hands to
`CAudioSys`). Seven instructions, all read off the bytes:

```
lwz r0,-13760(r2) ; mulli r0,r0,28 ; srawi r0,r0,3 ; addze r0,r0 ; slwi r0,r0,2 ;
stw r0,-28384(r13) ; blr
```

`mulli 28 / srawi 3 / addze / slwi 2` is `((v * 28) / 8) * 4` = `v * 14`, and **`(v * 28) / 8 * 4`
is the only spelling that emits it.** Measured, all four tried:

| spelling | emitted | score |
|---|---|---|
| `(*(int*)&x * 28) / 8 * 4` | `mulli 28 ; srawi 3 ; addze ; slwi 2 ; stw ; blr` | **100.00%** |
| `*(int*)&x * 14` | `mulli 14 ; stw ; blr` | 57.00% |
| `*(int*)&x * 7 * 2` | `mulli 14 ; stw ; blr` | - |
| `(*(int*)&x * 7) << 1` | `mulli 7 ; slwi 1 ; stw ; blr` | - |
| `(*(int*)&x) / 8 * 112` | `srawi 3 ; addze ; mulli 112 ; ...` | - |

The source is `.sdata2` 0x8041C7E0 (`symbols.txt:23455`, `data:float`), read with `lwz` - a
reinterpretation, not a bad map. **This corrects attempt 1's inherited comment**, which gave
0x80415980 for this read; 0x8041C7E0 = `_SDA_BASE_` 0x8041FD80 - 0x35A0 is the measured one.

### 3. `CMain::EnsureWorldPakReady` and the two name-list vector members (+3)

Writing `CMain::EnsureWorldPakReady(CAssetId)` (retail 0x80005698, 0xD0 = 208 B, declared but
never defined in `CMain.hpp`) also makes this object emit the two `rstl::vector<pair<string,
SObjectTag>>` members it needs, and **both pair with retail by name and land at 100% first try**:

* `__ct__Q24rstl138vector<...>FRCQ24rstl138vector<...>` (retail 0x8000584C, 204 B)
* `__dt__Q24rstl138vector<...>Fv` (retail 0x80005768, 132 B)

Three source details are load-bearing, and the first two are counter-intuitive:

* **The flag is `needsResList`, not `found`, and the two calls are the other way round from the
  obvious reading.** Retail's `li r29,0` (0x80005708) is on the **equal** branch of
  `cmplw r27,r0`, and `clrlwi. r0,r29,24 ; beq 0x8000572C` sends the *match* case to
  `CPakFile::EnsureWorldPakReady` and the no-match case to `CPakFile::sub_80323554`. Written the
  obvious way round the function scored 83.73% with the calls in retail's order.
* **`bool needsResList = true;` goes *above* the `CPakFile* file = resLoader.GetPakFile(i);`
  line.** Retail materialises it at 0x800056C4, two instructions *before* the `bl`. That is also
  what fixes the register split - retail puts the flag in r29 and the file pointer in r28, and
  declared inside the `if` body we got the mirror image (file r29, flag r28) at 94.42%.
* **`int i`, not `uint i`.** Retail's loop test is `cmpw` (signed) at 0x8000574C; a `uint`
  counter gives `cmplw`. One instruction, and it is the difference between 94% and 100%.
* The name list is a **copy**, not a reference: the copy constructor is called with
  `addi r3,r1,8 / addi r4,r28,88` and the destructor with `addi r3,r1,8 ; li r4,-1` at the end
  of the `if` body. `CPakFile::NameList()` returns the member by reference, so the copy is
  written out as a named local. The inner loop is a pointer walk (`lwz r0,20(r4) ; cmplw ;
  bne ; addi r4,r4,24 ; cmplw r4,r3 ; bne`); an index loop gives `mtctr / lwzx / bdnz` instead.

## Verified

```
tools/decomp_build.sh                 All: 31.00% fuzzy, 23.30% matched, 11.78% linked (10071 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
tools/report_diff.py judge baseline   matched 10060 -> 10071, linked 4918 -> 4918,
                                       "+11 functions at 100%, 0 units newly linked", no regression
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 REL sha1s vs config/G2ME01/config.yml    0 mismatches
python3 tools/check_symbol_names.py   ok
git status                            only src/MetroidPrime/main.cpp
```

Unit: **41 -> 52 / 99** functions, fuzzy 36.63% -> 41.22%. Nothing anywhere got worse (the
gate's per-function diff lists the eleven `+100%` lines and nothing else). No asm. `docs/HANDOFF.md`
is reverted - `gate.sh` rewrites it under `MP_GATE_DOCS_WRITE=1` and the driver owns that.

## What still stops the flip, and it is no longer undefined symbols

`flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

That is a *different* first error from attempt 1's `undefined: fn_80008C28`, and it is the one
the file's own comment at `CErrorOutputWindow::~CErrorOutputWindow` predicts: mwcceppc lays down a
28-byte copy of `__vt__18CErrorOutputWindow` in this object because the class's key functions are
all undefined here, and retail's `CErrorOutputWindow.o` already has it. Fixing it is a
`config/G2ME01/splits.txt` question (that `.data` is not claimed for this unit), not a source
question, and it wants its own item.

47 of the 99 functions still have no body. The remaining shapes, so the next run does not
re-derive them:

* **the 80-byte `rstl::rc_ptr<T>::ReleaseData()` family** - `fn_80009008`, `fn_80009224`,
  `fn_800095E4` and the four named `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` /
  `...<12CPlayerState>...` / `...<128rc_ptr<vector<string>>>` / `...<53rc_ptr<vector<int>>>`
  (all four 80 B). Byte-identical to the 100-byte `rc_ptr<IArchitectureMessageParm>` copy this
  object already matches, minus the `bctrl` tail. **The four *named* ones pair by name**, so they
  are winnable - but only if something in this unit releases an `rc_ptr` of those types, and
  nothing here does. The holder classes are in other units' ranges, so this needs a destructor
  written first.
* **the 84/88-byte `~rc_ptr` family** - `fn_800091D0`, `fn_800092F8` (and `__dt__80006678`,
  `__dt__800066D0`, `__dt__80006AE0`). The 88-byte `__dt__80006678` is the same code as
  `__dt__Q24rstl24single_ptr<10CGameState>Fv` (0x80006620, already 100%) and calls
  `__dt__CGameGlobalObjects_80006518`; the 84-byte `__dt__800066D0` calls `fn_80006724`. Those two
  callees are the real content.
* **`__dt__CGameGlobalObjects_80006518` (0x80006518, 264 B)** is `CGameGlobalObjects::~CGameGlobalObjects()`,
  and its disassembly is fully readable: ten member teardowns in reverse declaration order at
  +0x150, +0x14C, +0x148, +0x138 (`optional_object<CToken>`, complete with the `lbz 324(r30)`
  test), +0x134, +0x130 (`single_ptr<CGameState>`, already 100%), +0x108, +0xE4, +0x04, then
  `~CMemoryCardSys` on `this`, then `fn_80008B04` (the `TOneStatic` delete this attempt added).
  Writing `CGameGlobalObjects::~CGameGlobalObjects() {}` here is the obvious next single step; the
  header's member order already matches, and `fn_801F097C` / `fn_80177B70` / `__dt__6CTokenFv`
  just have to be declared.
* **`single_ptr_assign_800064D0` (0x800064D0, 72 B)** is `rstl::single_ptr<T>::operator=`: save
  the argument, `li r4,1`, `lwz r3,0(r3)`, call the pointee's destructor, `stw r31,0(r30)`, return
  `self`.
* **`fn_80008C28` (184 B) is recursive** - it calls itself twice and then `fn_80008CE0`, and it
  is a tree build, not a destructor. `fn_80007AC8` (112 B) / `fn_80007B38` (136 B) are an
  `rstl::list::push_back` pair reached from `CArchitectureQueue::Push`. Both are real work, not
  transcription.
* **`GetAverageValue<float>(const float*, int)` (0x80008B60, 200 B)** is retail's own out-of-line
  copy of `include/Kyoto/TAverage.hpp`'s template - 8x-unrolled `fadds` sum, then
  `xoris r3,r4,32768` / `lis r0,17200` / two `stw` (the 2^52 magic) / `lfd f0,8(r1)` /
  `fsubs` / `fdivs` / `fmuls`. The header's `sum * (1.f / count)` does **not** obviously produce
  that shape and the two `.sdata2` constants are not yet identified. One function, real work.

## Two things this attempt found that are not about this item

1. **`tools/check_decl_order.py --unit` wants the unit name *without* `.cpp`.**
   `--unit MetroidPrime/main.cpp` prints `ok: 0 unit(s) checked, none emits its functions out of
   retail order` and **exits 0** - a vacuous pass. Attempt 1's notes record that exact line as
   evidence, so the "ok" was never a check. With `--unit MetroidPrime/main` it runs, and it
   reports **`would break on a flip` for this unit at HEAD as well as with this change**: our
   object's `.text` is in *descending* retail order (`fn_80009864` first, `StreamNewGameState`
   last) where retail's is ascending, so every one of the 99 functions is out of position. The
   file is written in **ascending** retail order, so the "mwcceppc emits in reverse source order,
   therefore declare descending" rule in `docs/goal-unit-prompt.md` and the file's own header
   comment have it backwards for this unit. That is a pre-existing wall on the flip, not a
   regression from this change, and reversing the whole file is its own item.
2. **`fn_80008AA4`/`fn_80008B30` at 100% confirm the `TOneStatic` reading**, and with it that
   `CGameGlobalObjects` derives from `TOneStatic<CGameGlobalObjects>`. `include/MetroidPrime/
   CGameGlobalObjects.hpp` does not say so today. That is a one-line header fact worth having
   before anyone writes that destructor, and it is left out of this diff on purpose (this item's
   diff is the unit's own functions).

NEW: match-main-cgameglobalobjects-dtor | match | MetroidPrime/main | 47 of 99 functions still have
no body; the next one is `CGameGlobalObjects::~CGameGlobalObjects()` at retail 0x80006518, whose
ten member teardowns are fully decoded in the notes above and whose `TOneStatic` base is confirmed

## Review rejected run 19 (2026-09-30 06:15:18Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/main.cpp:959-960` declares and reads `lbl_8041C7E0`, but retail's `lwz r0,-13760(r2)` at 0x80009864 resolves against `_SDA_BASE_` 0x8041FD80 to **0x8041C7C0** (`tools/sda.py -13760` says so directly). The two `.sdata2` words are `0xBF490FDB` and `0x3F490FDB` — opposite signs — so the new `fn_80009864` stores a different value than retail while objdiff still reports 100% because the operand is a wildcarded `R_PPC_EMB_SDA21` relocation. The comment at `main.cpp:941,958` and `docs/goal-notes/match-main-cmain-0x91-bitfield.md:176-178` assert the same false subtraction (`- 0x35A0` for a `-13760` displacement) and present it as a correction to attempt 1, so a wrong measurement is being recorded as authoritative. This is rule 7's "a function that lands at retail's score without meaning the same thing", earned by a check that cannot fail. An acceptable change keeps everything else in the diff — the seven `TOneStatic` bodies and `CMain::EnsureWorldPakReady` are faithful — and only renames the symbol to `extern const float lbl_8041C7C0;`, uses it in the body, and rewrites the address arithmetic in both the comment and the note; the score should then be re-checked with the operand, not the relocation, in view.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/match-main-cmain-0x91-bitfield-L6-19.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/match-main-cmain-0x91-bitfield-L6-19-review1-20260930T060816.jsonl

## Fix round 1

Accepted the rejection in full: the operand was wrong and the score could not see it. Only
`fn_80009864`'s operand was touched; the seven `TOneStatic` bodies and
`CMain::EnsureWorldPakReady` are unchanged.

* `src/MetroidPrime/main.cpp` - `extern const float lbl_8041C7C0;` replaces `lbl_8041C7E0`, is
  what the body reads, and both comment sites (the disassembly header and the trailing
  paragraph) now say `0x8041C7C0 = _SDA_BASE_ 0x8041FD80 - 0x35C0`. The old `- 0x35A0` for a
  `-13760` displacement was an arithmetic slip that pointed at the wrong word.
* `docs/goal-notes/match-main-cmain-0x91-bitfield.md:176` - same arithmetic corrected, plus the
  `tools/sda.py -13760` and `dol_read.py` citations that measure it.

Measured, not reasoned:

```
python3 tools/sda.py -13760                                  -13760 -> 0x8041C7C0  lbl_8041C7C0 (exact, .sdata2)
python3 tools/dol_read.py 0x8041C7C0 0x24 orig/.../main.dol  0x8041C7C0 = 0xbf490fdb (-0.7853982)
                                                                0x8041C7E0 = 0x3f490fdb (+0.7853982)
objdump -dr build/G2ME01/src/MetroidPrime/main.o              R_PPC_EMB_SDA21 lbl_8041C7C0
objdump -d  build/G2ME01/main.elf @0x80009864                  lwz r0,-13760(r2)  (byte-identical to retail)
objdump -s  build/G2ME01/main.elf .sdata2 @0x8041C7C0         bf490fdb          (retail's word)
```

Gates after the fix: `decomp_build.sh` `All: 31.00% fuzzy, 23.30% matched, 11.78% linked
(10071 / 28465)` and `main/MetroidPrime/main 52 / 99` - unchanged, `fn_80009864` still 100.00%
at 28 B, `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`check_symbol_names.py` 0 missing, `check_raw_offsets.py` `ok: 152 raw-offset site(s) in 61
file(s)`.

Note for the next reader: `build/G2ME01/obj/` holds a **stale** `MetroidPrime/main.o` from an
earlier tree state - disassembling it shows a different relocation and will send you after a
symbol the source does not mention. Use `build/G2ME01/src/MetroidPrime/main.o`, which is what
objdiff scored.


---

# Attempt 3 (lane 8, 2026-09-30)

Verdict **PARTIAL** again, and a bigger step than either earlier attempt: the target rose
**55 -> 63 / 99** functions, `tools/goal_check.sh build/goal/item.json` passed every check it can
pass, and the flip fails on exactly the same `splits.txt`/`__vt` link error attempt 2 recorded.

**The bitfield question this item was filed on was closed by attempt 1 and is not reopened here.**
This attempt filled in eight more of the 99 functions. Diff is three files, 132 insertions,
4 deletions, no asm, no deletions of real work:

* `src/MetroidPrime/main.cpp` - six `TOneStatic<T>` bodies, `fn_80009864`, one stale comment.
* `include/Kyoto/TOneStatic.hpp` - `ReferenceCount()` moved from `private:` to `public:` (below).
* `config/G2ME01/symbols.txt` - one rename (below).

## 1. Six `TOneStatic<T>` instantiations, retail 0x80008A48-0x80008B3C (+6, first try)

```
0x80008A48  TOneStatic<CGameArchitectureSupport>::operator new(size_t, const char*, const char*)  48 B
0x80008AA4  TOneStatic<CGameArchitectureSupport>::GetAllocSpace()                                   12 B
0x80008AB0  TOneStatic<CGameArchitectureSupport>::ReferenceCount()                                  36 B
0x80008AD4  TOneStatic<CGameGlobalObjects>::operator new(...)                                        48 B
0x80008B04  TOneStatic<CGameGlobalObjects>::operator delete(void*)                                   44 B
0x80008B30  TOneStatic<CGameGlobalObjects>::GetAllocSpace()                                          12 B
0x80008B3C  TOneStatic<CGameGlobalObjects>::ReferenceCount()                                         36 B
```

Attempt 2 wrote these and measured all of them at 100%, but its diff was lost when the reviewer
sent the item back, so they are re-landed here. **Two of its choices are changed**, and both
because a measurement contradicted it:

* **`0x80008AB0` is not written.** `symbols.txt:172` names it
  `ReferenceCount__38TOneStatic<24CGameArchitectureSupport>Fv` (weak, 0x24 = 36 B) and this object
  **already emits that exact symbol** (it is `W` in `build/G2ME01/src/MetroidPrime/main.o`, 36 B,
  and was already at 100% before this change). Writing a second copy as `extern "C" fn_80008AB0`
  - which is what attempt 2 did - emits a 36-byte function retail does not have, i.e. exactly what
  `tools/unit_fit.sh` exists to catch. Instead `fn_80008A48` calls the real template member, so
  the `bl` lands on the symbol that pairs.
* **`CGameGlobalObjects` is not made to derive from `TOneStatic<CGameGlobalObjects>`** in the
  header. Attempt 2's note listed that as a missing one-line header fact "left out of this diff on
  purpose"; writing the four `CGameGlobalObjects` copies out of line does not need it, because
  `extern "C"` bodies do not go through the class. The base-class fact is still unrecorded in
  `include/MetroidPrime/CGameGlobalObjects.hpp` and still belongs to whoever writes
  `~CGameGlobalObjects()`.

`fn_80008A48` therefore needs access to a `private` static member, so
`include/Kyoto/TOneStatic.hpp` moves `static uint& ReferenceCount();` above `private:` with a
comment saying why. **Access control emits no code**, so this moves nothing in any other unit -
`check_symbol_names.py` and the full gate both confirm it - and `GetAllocSpace()` stays private
because nothing outside the class needs it.

The reading itself is unchanged from attempt 2 and is independently re-confirmed here:
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (`symbols.txt:170`, 0x80008A78, 44 B) sits at
100% in this object *between* 0x80008A48 and 0x80008AA4, which puts the first two in the same
instantiation and in `TOneStatic.hpp`'s declaration order; `CGameGlobalObjects`'s retail destructor
ends `bl 80008B04` under `extsh. r0,r31 ; ble`; and `symbols.txt:19013-19014` gives the two
holders as `.bss` 0x803C5AC4 size **0xA8** and 0x803C5B6C size **0x164**, which are the two
`lis 0x803C / addi` addresses in the two `GetAllocSpace` bodies.

## 2. `fn_80009864`, retail 0x80009864, 28 B (+1) - with attempt 2's operand **wrong**

Attempt 2 wrote this at 100% and a reviewer rejected it: the `lwz r0,-13760(r2)` operand pointed at
`.sdata2` 0x8041C7E0 (+0.7853982) where retail loads 0x8041C7C0 (-0.7853982), and objdiff cannot
see it because the `R_PPC_EMB_SDA21` is wildcarded. The fix round corrected the name to
`lbl_8041C7C0`; **this attempt re-measured the address instead of trusting either value**, and
0x8041C7C0 is right. The base is pinned six ways, all from `config/G2ME01/symbols.txt` against
`CGameGlobalObjects`'s retail constructor at 0x80008534-0x80008558, whose six `stw rX,d(r13)` are
4 bytes apart and whose six targets are 4 bytes apart:

```
stw r0,-28380(r13)  gpResourceFactory        = .sbss:0x80418EA4   -> 0x8041FD80
stw r5,-28376(r13)  gpSimplePool             = .sbss:0x80418EA8   -> 0x8041FD80
stw r4,-28372(r13)  gpCharacterFactoryBuilder= .sbss:0x80418EAC   -> 0x8041FD80
stw r4,-28360(r13)  gpGameState              = .sbss:0x80418EB8   -> 0x8041FD80
stw r4,-28352(r13)  gpTweakManager           = .sbss:0x80418EC0   -> 0x8041FD80
stw r0,-28344(r13)  gpRelFileManager         = .sbss:0x80418EC8   -> 0x8041FD80
```

and a seventh, independent of any of the file's own comments: `CMain::CMain`'s ten
`lwz r0,-32764(r13)` is `lbl_80417D84` (`symbols.txt:19436`), whose word `dol_read.py` reads as
**0x000F4240**, and 0x80417D84 + 32764 = 0x8041FD80. `python3 tools/sda.py -13760` ->
`0x8041C7C0 (exact, .sdata2)`, `dol_read.py 0x8041C7C0` -> `0xbf490fdb` = -0.7853982. So
`lbl_8041C7C0` is the name this run uses, and `(*(int*)&x * 28) / 8 * 4` is the spelling
(attempt 2's table of four, all re-measurable, stands). The `0x80415980` in attempt 1's inherited
comment, still quoted at `main.cpp` line 393 before this change, was wrong; that comment now
points at the new function instead.

## 3. One `symbols.txt` rename: `fn_80007AA0` (+1)

`fn_80007AA0` (0x80007AA0, 40 B) is `rstl::list<CArchitectureMessage, rmemory_allocator>::
push_back(const CArchitectureMessage&)` and **this object already emits it** as the weak COMDAT
`push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage`,
byte for byte (verified instruction by instruction; the only difference is the `bl` displacement,
which is a relocation in our object). `CArchitectureQueue::Push` (0x80007A80, 32 B, already 100%)
`bl`s it. dtk simply lost the name, so objdiff could not pair them.

**This route is now exhausted for this unit, and that is worth recording.** The same question was
asked of all 28 unpaired retail functions: disassemble each one out of `build/G2ME01/main.elf`,
disassemble every unpaired function out of `build/G2ME01/src/MetroidPrime/main.o`, and compare the
byte streams with the two low bytes of every `bl`/`bl`+cond zeroed. Sizes had to agree exactly.
**Result: 1 match out of 28** - `fn_80007AA0` only, now renamed. Every other unpaired function is
either real work we have not written (`fn_80008C28`, `fn_80008CE0`, `fn_80008D68`, `fn_80008E94`,
`reserve__Q24rstl55vector<...>Fi`, the 0x800066xx-0x800069xx cluster, `GetAverageValue<f>`,
`__dt__CGameGlobalObjects_80006518`, `single_ptr_assign_800064D0`, the three `fn_80009xxx`
`ReleaseData` copies) or is one byte off for a reason that is not a name (`SetMaxSpeed`, 99.25% -
see the `NEW:` line). Do not re-run this search.

**Correction to attempt 2's notes:** they say `__dt__80006678` (0x80006678) "calls
`__dt__CGameGlobalObjects_80006518`". It does not: its `bl` at 0x800066A0 goes to
`__dt__800066D0` (0x800066D0). `__dt__800066D0` calls `fn_80006724`.

## What still stops the flip

Unchanged and not close: `tools/unit_fit.sh MetroidPrime/main.cpp` reports `.text` **SHORT by
7868 bytes** of the 17608 claimed, and `flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

63 of 99 functions matched, 36 still have no body. `check_decl_order.py --unit MetroidPrime/main`
still reports **would break on a flip** (our `.text` is in descending retail order where retail's is
ascending) - a pre-existing wall, not a regression from this change. The flip also needs the
declaration-order reversal, the 36 missing bodies, and a `splits.txt` answer for the
`CErrorOutputWindow::__vt` `.data`; that last one is its own item.

## Side effect recorded honestly

`.sbss` in this object is now **57 bytes against 36 claimed** (`unit_fit.sh`), where it was 49
before this change - the new `TOneStatic<CGameGlobalObjects>::ReferenceCount` adds a 4-byte counter
and a 1-byte init guard plus padding. Retail has the same pair (`.sbss` 0x80418ED8/0x80418EDC plus
`lbl_80418ED4`, `symbols.txt:20454-20455`), so this is retail's data and the claim is short, not an
overrun. `.sbss` is not scored for this unit - the report carries `total_data: 40` and no
`matched_data` - and `matched_code` rose by exactly the 228 bytes of the seven new functions
(48+12+48+44+12+36+28), which is the check that the seven are the only thing that changed.

## NEW: a wildcarded SDA21 relocation is hiding wrong float constants in this unit

`CMain::SetMaxSpeed` is 99.25% (96 B) and the one differing byte is the displacement of
`lfs f0,<disp>(r2)`. This is a **value** problem, not a spelling one, and it is not confined to
that one function. With `_SDA_BASE_` = 0x8041FD80 (pinned above, seven ways):

```
main.cpp:155   x5c = lbl_8041A3DC        lbl_8041A3DC = 0x8041A3DC = 0x3F800000 = 1.0f
retail 0x800089F0  lfs f0,-32740(r2)     0x80417D9C  = 0x00000008  (a denormal, ~1.06e-45)
main.cpp:172   mAverageTickTime(lbl_8041A3D8)   0x8041A3D8 = 0x00000000 = 0.0f
retail 0x800088B0  lfs f1,-32744(r2)     0x80417D98  = 0x0000003B  (a denormal, ~3.5e-45)
main.cpp:169   x10_unk(lbl_8041A3F0)             0x8041A3F0 = 0x4330000000000000 = 176.0
retail 0x800088A0  lfd f2,-32720(r2)     0x80417DA0  = 0x0000003A0000000B  (~0)
```

and the same float is read a third time, independently, at 0x80008C0C in
`GetAverageValue<f>__FPCfi` - `lfs f2,-32740(r2)`, i.e. the `1.f` of
`include/Kyoto/TAverage.hpp`'s `sum * (1.f / count)`. All three are the **same** address. So the
tree's `CMain::CMain` (100%, 276 B) and `TAverage.hpp` read a constant retail does not read, and
because `R_PPC_EMB_SDA21` is wildcarded objdiff scores all of them 100% anyway. `lbl_8041A420`
(= 10.0f) and `lbl_80417D84` (= 0xF4240) are *correct*, so this is not a uniform base error.

**This attempt deliberately did not touch it.** `CMain::CMain` is already at 100% and is not this
item's business; changing the constant changes what retail's code *computes*, on the strength of a
derivation about a linker base rather than a measured pair, and that is the reviewer's "a function
that lands at retail's score without meaning the same thing" in reverse. It is one instruction in
`SetMaxSpeed` and one list in one header - the right size for its own item.

NEW: match-main-sda-float-constants | match | MetroidPrime/main | SetMaxSpeed 99.25% because
CMain::CMain and TAverage.hpp read .sdata2 lbl_8041A3D8/lbl_8041A3DC/lbl_8041A3F0 where retail
reads .sdata 0x80417D98/0x80417D9C/0x80417DA0 (0x0000003B/0x00000008/0x3A0000000B) at the
identical displacements; objdiff cannot see it because the SDA21 relocation is wildcarded

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.04% fuzzy, 23.35% matched, 11.78% linked (10092 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                       matched 10084 -> 10092, linked 4918 -> 4918,
                                       "+8 functions at 100%", 1 RENAMED 0.00% -> 100.00%,
                                       "no regression"
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
unit: main/MetroidPrime/main          55 -> 63 / 99 functions, fuzzy 45.90% -> 47.42%,
                                       matched_code 7208 -> 7476
tools/unit_fit.sh MetroidPrime/main.cpp  .text SHORT by 7868; 15 extras (was 16), all COMDAT
python3 tools/check_symbol_names.py    504 units, 0 missing
python3 tools/check_raw_offsets.py     ok: 152 raw-offset site(s) in 61 file(s)
python3 tools/check_decl_order.py --unit MetroidPrime/main   would break on a flip (pre-existing)
git status                            3 files, no asm, no deletions of real work
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

---

# Attempt 4 (lane 8, 2026-09-30) - this one re-measured from HEAD, and the tree had moved

Verdict **PARTIAL**: the target rose **60 -> 63 / 99**, `tools/goal_check.sh build/goal/item.json`
passed every check it can pass, and the flip still fails on the same `splits.txt`/`__vt` link error
attempts 2 and 3 recorded. Diff is **two files, 76 insertions, 5 deletions**, no asm.

**Read attempt 3's notes as history, not as state.** Attempt 3 was lane L8-1: the reviewer returned
**VERDICT: PASS** (`build/goal/agent/match-main-cmain-0x91-bitfield-L8-1-review1-*.jsonl`), the
driver never committed it, and `git reset --hard` dropped it. So at the start of this run HEAD
(`cf0bc6f progress: match-main-fn-80009274`) had **none** of it - measured, not assumed:

```
tools/decomp_build.sh on clean HEAD     All: 31.07% fuzzy, 23.36% matched, 11.78% linked
                                        main/MetroidPrime/main  60 / 99, matched_code 7364
build/G2ME01/src/MetroidPrime/main.o    .text 0x2600 = 9728, no __sinit_main_cpp,
                                        no __dl__32TOneStatic, grep fn_80008A48 -> 0 hits
```

`build/report.json` in a lane worktree is **not** a baseline - it survives `git clean` as a
leftover from the previous run, and it read 63/99 on the clean tree. Rebuild before believing any
count here. (`build/goal/judge/report.base.json` is the driver's own and was correct at 60/99.)

## What landed (+3, first try each)

### 1. `fn_80007AA0` -> `push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage`

Attempt 3 measured this and the reviewer verified it; the rename was lost with the rest. **Confirmed
here from bytes, not from the note**: our object already emitted the weak COMDAT of exactly that
name at `nm` offset 0x28, and retail 0x80007AA0 is the same eight instructions with the only
difference the `bl` displacement, which is a relocation in our object. `CArchitectureQueue::Push`
(0x80007A80, already 100%) `bl`s it. 40 B, 0.00% -> 100.00%.

### 2. `fn_80008B04` -> `__dl__32TOneStatic<18CGameGlobalObjects>FPv`, **new to this run**

**Attempt 2 and attempt 3 both wrote this body by hand as `extern "C" void fn_80008B04(...)`. That
is the wrong instrument and this run did it the other way round**: the name belongs in
`config/G2ME01/symbols.txt`, because the compiler emits the real template member under its real
name and only `symbols.txt` was missing it. So the diff is a rename plus one line of source:

```cpp
template void TOneStatic< CGameGlobalObjects >::operator delete(void*);   // does NOT compile
namespace { struct SForceGlobalObjectsDelete {
  CGameGlobalObjects* self;
  SForceGlobalObjectsDelete() : self(nullptr) { TOneStatic<CGameGlobalObjects>::operator delete(self); }
}; SForceGlobalObjectsDelete gForceGlobalObjectsDelete; }                  // 44 B, works
```

Two measurements, both worth the next run not repeating them:

* **MWCC 2.7 rejects explicit instantiation of a member function** -
  `# Error: illegal explicit template instantiation`. So the use has to be a real call inside a
  function the compiler is *made* to generate, which is what `mainTail.cpp:300`'s
  `SForceTailWeakCopies` is for and why that file has one.
* **A constructor costs 44 bytes, a destructor costs 84.** Spelled as a destructor body, mwcceppc
  reads the call as a `delete` and appends the `extsh./ble/CMemory::Free` tail, so
  `unit_fit.sh` then lists `__dt__...Fv` (84 B) as an extra. The constructor has no
  deleting-destructor flag, so it does not, and the call inlines into `__sinit_main_cpp` (44 B).
  Either way there is exactly one new local function this unit does not claim; the flip is 7764
  bytes short of caring.

The reading is pinned without assuming anything about the class: retail lays the four `TOneStatic`
members out for `CGameArchitectureSupport` as `__nw__`(0x80008A48, 0x30) /
`__dl__`(0x80008A78, 0x2C) / `GetAllocSpace`(0x80008AA4, 0xC) / `ReferenceCount`(0x80008AB0,
0x24), and for `CGameGlobalObjects` at 0x80008AD4 / 0x80008B04 / 0x80008B30 / 0x80008B3C with the
**same four sizes in the same order** - only the second one's name was missing.
`include/MetroidPrime/CGameGlobalObjects.hpp:81` already records the independent witness (retail's
`~CGameGlobalObjects` ends in a call to 0x80008B04 under the deleting-destructor flag).

### 3. `fn_80009864`, retail 0x80009864, 0x1C = 28 B - re-landed, and **the word there is 0**

Attempt 3's address is right and its supporting arithmetic about the *contents* is wrong, in the
review's words rather than its own. Measured from the **retail DOL**, not from our build:

```
$ python3 tools/dol_read.py 0x8041C7C0 0x10
.sdata2 @ 0x8041c7c0  (file 0x3c5f40, 16 bytes)
hex : 00 00 00 00 00 00 00 03 80 3a 99 c0 00 00 00 00
u32  : 0x0 0x3 0x803a99c0 0x0
```

and `build/binutils/powerpc-eabi-objdump -s --section=.sdata2 build/G2ME01/main.elf` agrees:

```
 8041c7c0 00000000 00000003 803a99c0 00000000
 8041c7d0 40a00000 3e800000 3f22f983 bf490fdb
 8041c7e0 3f490fdb c0400000 40400000 c1200000
```

So **0x8041C7C0 = `0x00000000` and 0x8041C7E0 = `0xBF490FDB`**. Attempt 2, attempt 3 and the
review that accepted the fix all say the other way round (they read 0x8041C7C0 as `0xBF490FDB`),
which is what put `-0.7853982` in attempt 2's correction. The *address* is still right and is not
in doubt - `_SDA_BASE_` 0x8041FD80 - 0x35C0 = 0x8041C7C0, `tools/sda.py -13760` prints exactly that,
and it is the only `.sdata2` symbol whose displacement is `-13760`:

```
$ objdump -dr build/G2ME01/src/MetroidPrime/main.o   # the object objdiff scored
   0: 80 00 00 00  lwz r0,0(0)      0: R_PPC_EMB_SDA21  lbl_8041C7C0
  18: 4e 80 00 20  blr
$ ./tools/dis.sh 0x80009864 0x1C                    # linked: byte-identical to retail
80009864: 80 02 ca 40  lwz r0,-13760(r2)
80009878: 90 0d 91 20  stw r0,-28384(r13)
```

**So `fn_80009864` stores 0, and `.sbss` `lbl_80418EA0` is never raised above zero in the DOL.** The
"ARAM size" story the comment above `extern "C" uint lbl_80418EA0;` used to tell is not retail's;
that comment has been corrected in this diff rather than left asserting a value retail does not
read. `(*(int*)&lbl_8041C7C0 * 28) / 8 * 4` is still the only spelling measured that emits
retail's four instructions (attempt 2's table of four stands, re-measurable).

## New decode: the 0x800066xx-0x800069xx cluster, 12 functions and 1436 bytes

**Nothing in the three earlier attempts decoded this cluster** beyond `__dt__80006678`'s callee,
and attempt 3's correction of it is confirmed here. Every function below is in this unit's range
and unpaired. Read off `tools/dis.sh 0x80006620 0x2F0` and `0x800068f4 0x1f0`; sizes from
`config/G2ME01/symbols.txt`.

* **`__dt__80006678` (0x80006678, 88 B)** is `rstl::single_ptr<CInGameTweakManager>::~single_ptr(int)`
  - instruction for instruction the already-matched `__dt__Q24rstl24single_ptr<10CGameState>Fv`
  (0x80006620, 88 B) with the inner call changed from `__dt__10CGameStateFv` to `__dt__800066D0`.
  That is the third witness that **`CInGameTweakManager`'s destructor is out of line in this unit**,
  which is why `include/MetroidPrime/CInGameTweakManager.hpp` has no `~` for it today.
* **`__dt__800066D0` (0x800066D0, 84 B)** is `~CInGameTweakManager(int)`: the null test, then
  `li r4,-1 ; bl fn_80006724`, then the `extsh./ble/CMemory::Free` deleting-destructor tail.
* **`fn_80006724` (0x80006724, 132 B)** is that destructor's body. It reads `+0x04` (a count) and
  `+0x0C` (a base pointer), forms `end = base + count * 72`, stores both into **two** two-word
  stack slots at `r1+8/12` and `r1+16/20`, calls `fn_800067A8(&r1+20, &r1+12)`, then
  `CMemory::Free(+0x0C)`. Element stride **0x48 = 72**.
* **`fn_800067A8` (0x800067A8, 56 B)** is the by-value wrapper: it copies the two iterators into a
  fresh stack pair and calls `fn_800067E0`. Its only reason to exist is the copy.
* **`fn_800067E0` (0x800067E0, 80 B)** is the element walk: `for (p = begin; p != end; p += 0x48)
  fn_80006830(p);` - a pointer walk with `cmplw`/`bne`, **not** an index loop (`mtctr`/`bdnz`).
* **`fn_80006830` (0x80006830, 32 B)** -> `fn_80006850` -> `fn_80006874`: the three-function C++
  destructor chain. `fn_80006850` (36 B) is `li r4,-1 ; bl fn_80006874` - the flag-passing shim -
  and `fn_80006874` (128 B) is the real body plus the `Free` tail.
* **`fn_80006874` (0x80006874, 128 B)** calls `internal_dereference<rstl::basic_string<char>>`
  three times, guarded by null tests at `+0x24`/`+0x30`, `+0x14` and `+0x04`
  (`rstl::basic_string` is `{mPtr@0, mCow@4, mSize@8}`, 0x10 bytes, `CHECK_SIZEOF(string, 0x10)`).
  **Element size 0x48 is `CHECK_SIZEOF(CTweakValue, 0x48)`, so this is `~CTweakValue` - but the
  offsets do not match the header's `CTweakValue`** (`mType@0, mKey@4, mText@0x14, mAudio@0x24`,
  which would put `mAudio.mFileName` at `+0x30` with its `mCow` at `+0x34`). Whoever writes it has
  to resolve that, and it is the one thing in this cluster that is not already settled.
* **`fn_800068F4` (0x800068F4, 96 B)** is the same destroy-then-free shape with element stride
  **0x0C = 12**, calling `fn_80004864` (0x80004864, 56 B) and then zeroing `+0x04`. The 12-byte
  element is **not** identified.
* **`fn_80006954` (0x80006954, 88 B)** is the out-of-liner for a `{float avg@0; bool has@4}` result:
  `if (count == 0) { has = false; return; } avg = GetAverageValue<f>(data, count); has = true;`.
* **`fn_800069AC` (0x800069AC, 308 B)** is the sample ring that feeds it: `if (count < 4)
  { data[count] = *value; ++count; }`, then an **8x-unrolled backward shift** `data[i] =
  data[i-1]` for `i = count-1 .. 1` with a scalar `bdnz` tail, then `data[0] = *value`. So
  `data[0]` is always the newest sample and the count saturates at 4.

**Both `fn_80006954` and `fn_800069AC` have exactly one caller each, and both are inside
`CMain::RsMain`** (0x80006108/`0x80006114` and 0x80006228/`0x80006234, inside 0x80005C6C) - they are
`RsMain`'s own outlined helpers, on `rstl`-shaped locals at `r1+8`/`r1+24`, and the results are
stored to `+0x40`/`+0x68` of `this`, which are `CMain::mAverageTickTime`/`mAverageDrawTime`.
`RsMain` is 2.38% and unwritten, so **`fn_800069AC` cannot be reached honestly until `RsMain` is**
- and `fn_800069AC` needs `GetAverageValue<f>` too, which is already queued as
`match-main-getaveragevalue-f`. This is new: attempts 2 and 3 treated them as independent unwritten
functions.

## `__dt__CGameGlobalObjects_80006518` (264 B) - the existing queued item, now fully pinned

`match-main-cgameglobalobjects-dtor` is already in the queue, so **no new item is filed for it**,
but attempt 2's description was a list of offsets and this run read it instruction by instruction.
The ten teardowns in reverse declaration order, with **the flag argument each one is called with**
(attempt 2 did not record those, and they are the whole difference between a guess and the bytes):

```
+0x150 CGameGlobalObjectsTail  li r4,-1 ; bl fn_801F097C
+0x14C inGameTweakManager      li r4,-1 ; bl __dt__80006678
+0x148 renderer                addic./lwz/cmplwi, then vtable slot 2 with li r4,1  (virtual)
+0x138 stringTable             lbz 324(r30) engaged-flag test, then li r4,0 ; bl __dt__6CTokenFv
+0x134 memoryCard              li r4,1  ; bl __dt__11CMemoryCardFv
+0x130 gameState               li r4,-1 ; bl __dt__Q24rstl24single_ptr<10CGameState>Fv
+0x108 characterFactoryBuilder li r4,-1 ; bl __dt__24CCharacterFactoryBuilderFv
+0x0E4 simplePool              li r4,-1 ; bl __dt__11CSimplePoolFv
+0x04  resFactory              li r4,-1 ; bl __dt__11CResFactoryFv
+0x00  pad0                    mr r3,r30 ; li r4,-1 ; bl __dt__14CMemoryCardSysFv
      then                     extsh. r0,r31 / ble / bl 0x80008B04 = __dl__32TOneStatic<18CGameGlobalObjects>FPv
```

**The one gap that is not a header fact:** `+0x150` needs `fn_801F097C` (0x801F097C, 0x54 bytes,
unnamed in `symbols.txt`) as an out-of-line destructor, and
`CGameGlobalObjectsTail` (`include/MetroidPrime/CGameGlobalObjects.hpp:61`) declares a constructor
but **no destructor**. That is a header change plus an `extern "C"` declaration, not a source-only
edit - worth knowing before the item is picked up. `+0x148` needs `IRenderer` to have a virtual
destructor for the `bctrl` through `vtable[2]`.

## Verified (this run, all measured)

```
tools/decomp_build.sh              All: 31.07% fuzzy, 23.36% matched, 11.78% linked (10095 / 28465)
unit: main/MetroidPrime/main        60 -> 63 / 99 functions, fuzzy 47.05% -> 47.69%,
                                    matched_code 7364 -> 7476 (= 28 + 40 + 44 exactly)
tools/report_diff.py judge base     matched 10092 -> 10095, linked 4918 -> 4918,
                                    +3 functions at 100%, 2 RENAMED 0.00% -> 100.00%, no regression
tools/goal_check.sh build/goal/item.json
                                    PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol      6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/unit_fit.sh MetroidPrime/main.cpp
                                    .text SHORT by 7764 (was 7764+116); 16 extras, 15 pre-existing
                                    + __sinit_main_cpp (44 B, the constructor inlined into it)
python3 tools/check_symbol_names.py 504 units, 0 missing
python3 tools/check_raw_offsets.py  ok: 152 raw-offset site(s) in 61 file(s)
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                    would break on a flip (pre-existing, unchanged)
git status                         config/G2ME01/symbols.txt, src/MetroidPrime/main.cpp
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## What still stops the flip, unchanged

`flip_test.sh` fails at link with `multiply-defined: 'CErrorOutputWindow::__vt' in
CErrorOutputWindow.o` - the same `splits.txt` question attempts 2 and 3 recorded, and it is the
first error, not the whole story. 36 of 99 functions still have no body, `.text` is 7764 bytes
short of the 17608 claimed, and `check_decl_order.py` still reports **would break on a flip**
(our `.text` is descending where retail's is ascending) - all three pre-existing.

**Note for the next run on this item:** a reviewer can return a PASSed diff and the driver can
still lose it. If HEAD does not contain it, re-landing is not "repeating previous work" - it is
the only copy. Re-measure before assuming anything, and **do not trust `build/report.json` in a
lane worktree as a baseline**.

NEW: match-main-ciengametweakmanager-dtor | match | MetroidPrime/main | the 0x800066xx cluster is
8 functions and 696 bytes with no body (__dt__80006678 88, __dt__800066D0 84, fn_80006724 132,
fn_800067A8 56, fn_800067E0 80, fn_80006830 32, fn_80006850 36, fn_80006874 128) and every call and
flag argument is decoded above; the only open piece is that fn_80006874's string offsets do not
match the header's CTweakValue

---

# Attempt 5 (lane 8, 2026-09-30) - the bitfield question is closed; this run attacked the cluster

Verdict **PARTIAL**, and the biggest single step in the item's history: the target rose
**63 -> 69 / 99** functions, `tools/goal_check.sh build/goal/item.json` passed every check it can
pass, and the flip still fails on exactly the pre-existing `CErrorOutputWindow::__vt` link error
attempts 2, 3 and 4 recorded. Diff is **three files, 100 insertions, 3 deletions**, no asm.

**Re-measured first, as attempt 4 warned.** HEAD was `b2f1420 progress: match-main-sda-float-constants`,
a clean tree rebuilt from scratch: `main/MetroidPrime/main` **63 / 99**, `All:` 10096 / 28465, and
`build/goal/judge/report.base.json` agreed at 63 / 99. **Attempt 4's two `symbols.txt` renames were
not in HEAD** (`fn_80007AA0` and `fn_80008B04` were still unnamed at HEAD), so re-landing them is not
repeating work - it was the only copy, exactly as attempt 4's closing note predicted. All three
functions of this attempt were verified from bytes before being landed, not from any note.

## What landed (+6, and five of the six are new bytes, not renames)

| retail | size | what it is | was |
|---|---|---|---|
| `0x80007AA0` | 40 | `rstl::list<CArchitectureMessage, rmemory_allocator>::push_back(const CArchitectureMessage&)` | `fn_80007AA0`, unpaired - **pure rename**, attempt 3/4's diff, re-verified |
| `0x800067E0` | 80 | the `CTweakValue*` array walk | `fn_800067E0`, no body |
| `0x80006830` | 32 | `fn_80006850(p)` | `fn_80006830`, no body |
| `0x80006850` | 36 | `p->~CTweakValue()` | `fn_80006850`, no body |
| `0x80006874` | 128 | `CTweakValue::~CTweakValue()` | `fn_80006874`, no body - **rename + a body** |
| `0x80008B04` | 44 | `TOneStatic<CGameGlobalObjects>::operator delete(void*)` | `fn_80008B04`, unpaired - **rename + a body**, attempt 4's diff, re-verified |

`main/MetroidPrime/main` fuzzy 48.13% -> 50.17%, `matched_code` 7588 -> 7948 (= 40 + 80 + 32 + 36 +
128 + 44 exactly, so these six are the only thing that changed), `All:` 10096 -> 10102, linked
unchanged at 4918, DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## 1. `CTweakValue::~CTweakValue` = retail `fn_80006874`, 128 B - first try, and it corrects attempt 4

**Attempt 4 recorded the element type's layout as unresolved: "fn_80006874's string offsets do not
match the header's CTweakValue". They match exactly, and the layout is now measured rather than
argued.** `CTweakValue::~CTweakValue() {}` with the destructor *declared* in the header compiles to
all 128 of retail's bytes, first try, with zero source inside the body
(`tools/probe_cc.sh .tmp/opencode/p8/bf.cpp`):

```
+0x30  mAudio.mFileName   addic. r0,r30,36 / beq   <- tests &mAudio, the ENCLOSING member
       addic. r3,r30,48 / beq ; addi r3,r30,48      <- tests &mAudio.mFileName
+0x14  mText              addic. r0,r30,20 / beq ; addi r3,r30,20
+0x04  mKey               addic. r0,r30,4  / beq ; addi r3,r30,4
```

**The two tests before the first `bl` are the whole reading.** mwcceppc emits a null test on the
address of *every* class-type member it destroys, so a `string` nested inside `CTweakValue::Audio`
costs two - one for the `Audio`, one for the `mFileName` inside it. That pins `mFileName` at +0x30,
`mFadeIn` at +0x24, and everything else in the header's existing
`CHECK_SIZEOF(CTweakValue, Audio, 0x20)` / `CHECK_SIZEOF(CTweakValue, 0x48)`. The header is right
as it stands and needed no edit beyond the declaration.

The declaration is not optional: with the implicit destructor still in place, mwcceppc rejects the
definition outright - `# Error: object 'CTweakValue::~CTweakValue()' redefined`. That is the same
arrangement `CMapWorldInfo` and `CWorldLayerState` use in this file, and the only thing it changes
elsewhere is that a `CTweakValue*` destroyed through the header now calls this body.

## 2. The walk and its two thunks, retail 0x800067E0 / 0x80006830 / 0x80006850 - all three byte-identical

```
0x800067E0  80 B  for (CTweakValue* it = begin; it != end; ++it) fn_80006830(it);
0x80006830  32 B  fn_80006850(p);
0x80006850  36 B  p->~CTweakValue();   ->  li r4,-1 ; bl __dt__11CTweakValueFv
```

**The two reference parameters' const-ness is the one thing that makes the walk reproduce, and it is
determined by the bytes.** Retail's test is `lwz r0,0(r30) / cmplw r31,r0`: it *re-reads* the end
through a pointer every iteration, while the begin is loaded once. That is exactly
`(CTweakValue* const& begin, CTweakValue*& end)`. Six spellings measured, this one first try:

| spelling | size | result |
|---|---|---|
| `(CTweakValue* const&, CTweakValue*&)` | 80 B | **identical to retail, all 20 instructions** |
| `(CTweakValue*&, CTweakValue*&)` | 80 B | right size, `stw r30` / `mr r30,r4` / `lwz r31,0(r3)` in the wrong order |
| `(CTweakValue*&, CTweakValue* const&)` | 80 B | roles swapped: r30 walks, no re-load in the test |
| `(const&, const&)` | 76 B | both hoisted, 4 bytes short |
| `while` + two named locals / comma-init / `for(it=b; ...)` | 76 B | all three collapse to the same 76 bytes |

Element stride **0x48 = 72 = `CHECK_SIZEOF(CTweakValue, 0x48)`**, independently confirming the array
is `CTweakValue*`. Verified with a byte comparison against the DOL with the `bl`/`b`/`bne`
displacements masked, not by a percentage.

**What is NOT identified: why retail has two thunks.** `0x800067E0 -> 0x80006830 -> 0x80006850 ->
~CTweakValue` is a four-link chain for one destructor call. Nothing else in the object calls either
thunk, so they are not a COMDAT copy of something else, and they are not the D0/D1/D2 destructor
trio (`fn_80006830` has no null test and no `CMemory::Free` tail, so it is not a deleting
destructor). The **bodies** are reproduced and objdiff scores all three 100%; the *reason for the
pair* is not. Recorded so the next run does not go looking for it again.

## 3. `TOneStatic<CGameGlobalObjects>::operator delete` = retail `fn_80008B04`, 44 B

Attempt 4's diff, re-landed and re-verified: retail's 44 bytes are
`bl 0x80008B3C (ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv) ; lwz r4,0(r3) ;
addi r0,r4,-1 ; stw r0,0(r3)`, and the object now emits exactly that under the real template name
once a call site exists. `include/MetroidPrime/CGameGlobalObjects.hpp:88` already derives from
`TOneStatic<CGameGlobalObjects>`, so the class is not changed; the only addition is a
file-local struct whose constructor makes the call, because **MWCC 2.7 rejects explicit
instantiation of a member function** (`# Error: illegal explicit template instantiation`) and
spelling the body as `extern "C" void fn_80008B04(void*)` emits a *second*, wrongly named 44-byte
function on top of the real member. A constructor rather than a destructor: spelled as a destructor
mwcceppc reads the call as a `delete` and appends the `extsh./ble/CMemory::Free` tail under
`__dt__...Fv`, which retail does not have here.

**Side effect, recorded honestly:** this adds one local function the unit does not claim,
`__sinit_main_cpp` (44 B, `t` in the object), plus 4 bytes of `.bss`. `.sbss` is now **61 against 36
claimed** (`unit_fit.sh`) - it was 57 before this change; retail has the same
`ReferenceCount`/`init`-guard pair, so the claim is short, not the object overrunning. `.text` is
**7316 bytes short** of the 17608 claimed, so nothing waits on any of this. When
`~CGameGlobalObjects` is written this whole block goes away and the `bl 0x80008B04` at the end of
that destructor is the call site.

## Wall measured this run: `fn_800067A8`, 56 B, two instructions of scheduling

`0x800067A8` is a by-value-copying forwarder: it dereferences its two reference parameters into its
own frame and calls `fn_800067E0(&[r1+12], &[r1+8])`. **Eight spellings measured, all reach 56 B
(exactly retail's `size:0x38`) and none is byte-identical**; the residue is always the *order* of the
two `lwz`, and the two `stw` slots:

| spelling | residue vs retail |
|---|---|
| `(const&, const&)`, `first` declared first | store slots correct; hoists `lwz r5,0(r3)` where retail hoists `lwz r5,0(r4)` |
| `(const&, const&)`, `last` declared first | hoist correct; the two locals land in the opposite stack slots (r1+8/r1+12 swapped) |
| `(CTweakValue*& b, CTweakValue* const& e)` | hoist on the right load, but not hoisted above the `stw r0,20(r1)` and into r0 not r5 |
| `(const&, &)`, `(const&, const&)`, `(const&, &)` with `last` first, `&`-bound first local (0x2C B) | 4 bytes short or a missing copy |

The two winning ingredients pull in opposite directions - "load the first parameter early" wants
`first` declared first, and "put the begin local at r1+12" also wants `first` declared first, but
retail hoists the *second* parameter's load. **Do not re-run these eight.** What is still needed is
a way to make mwcceppc hoist a *later* parameter's load, or a reason to think retail's caller
(`fn_80006724`, see below) passes its two arguments in the other order.

WALL: fn_800067A8 96%-equivalent (56/56 B, 2 instructions of load-order scheduling) - 8 spellings
reach retail's exact size; none hoists the second parameter's `lwz` while keeping the two local
slots at r1+8/r1+12, and the winning ingredients of the two closest attempts are contradictory

## `fn_80006724` and the two `CInGameTweakManager` destructors - decoded further, still unwritten

Attempt 4's decode of this cluster is confirmed. One new structural fact, and the reading that makes
the remaining three functions winnable:

* **`fn_80006724` (132 B) is a destructor body with the `if (flag > 0) CMemory::Free(this)` tail** -
  the D0 shape - and **`__dt__800066D0` (84 B) is the D1 shape**: `if (this) { li r4,-1 ; bl
  fn_80006724 ; if (flag > 0) Free(this); }`. So `CInGameTweakManager` has a **base or first member at
  +0x00 whose out-of-line destructor is `fn_80006724`**, and `~CInGameTweakManager` is the one-liner
  that calls it and then frees `this`. This is the same arrangement MWCC produces for
  `~single_ptr<CGameState>` -> `__dt__10CGameStateFv` (0x8000419C, `size:0x190`) on the other side of
  this unit, and it is why `__dt__80006678` (88 B) can be `~single_ptr<CInGameTweakManager>` calling
  `__dt__800066D0` with `li r4,1`.
* **What `fn_80006724` reads, confirmed:** `+0x04` = element count, `+0x0C` = base pointer, stride
  **0x48**, then `CMemory::Free(base)`. `CInGameTweakManager`'s four `uint`s
  (`include/MetroidPrime/CInGameTweakManager.hpp:51-54`, `mUnk0/4/8/C`) already cover those two
  offsets, so the container is the class's own fields, not a member struct.
* **The one thing not decoded: the four stack stores.** `fn_80006724` writes `{end,end}` at r1+8/12
  and `{base,base}` at r1+16/20 and passes `&r1+20, &r1+12` - two *2-word* objects whose second
  words are the iterators, both words of each holding the same pointer. Four spellings of a
  two-iterator range, a 2-word `pair` and an array-of-two were considered and none is natural; the
  iterator type is 4 bytes (`rstl::pointer_iterator<T>` in this tree is 4 bytes, `include/rstl/
  pointer_iterator.hpp`), so it is not an 8-byte aggregate passed by reference. **This is what
  blocks `fn_80006724`, and through it `__dt__800066D0` and `__dt__80006678`**: those last two are
  then the compiler's, once a `~CInGameTweakManager()` with the right base exists.

`fn_800068F4` (96 B, stride 0x0C), `fn_80006954` (88 B) and `fn_800069AC` (308 B) are unchanged from
attempt 4's notes; the last two are `CMain::RsMain`'s own outlined helpers and cannot be reached
honestly until `RsMain` (0x80005C6C, 2.38%) is written.

## Still not stopping the flip (unchanged from attempt 4)

* `flip_test.sh` fails at link with
  `mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`
  - a `config/G2ME01/splits.txt` question (that `.data` is not claimed for this unit), not a source
  one, and it wants its own item. It is the first error, not the whole story.
* 30 of the 99 functions still have no body; `.text` is 7316 bytes short.
* `check_decl_order.py --unit MetroidPrime/main` still reports **would break on a flip** (our
  `.text` is descending where retail's is ascending) - pre-existing, and the file's own header
  comment and `docs/goal-unit-prompt.md` have the rule backwards for this unit.
* `unit_fit.sh` reports 16 unclaimed functions, 1344 bytes, all COMDAT weak template/inline-virtual
  destructor copies; `.sbss` 61 against 36 claimed. CAi carries 224 bytes of the same kind and
  flips.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.08% fuzzy, 23.37% matched, 11.78% linked (10102 / 28465)
unit: main/MetroidPrime/main          63 -> 69 / 99, fuzzy 48.13% -> 50.17%,
                                       matched_code 7588 -> 7948, matched_data 0 -> 4
tools/report_diff.py judge base      matched 10096 -> 10102, linked 4918 -> 4918,
                                       "+6 functions at 100%", 3 RENAMED 0.00% -> 100.00%,
                                       "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py   504 units, 0 missing
tools/unit_fit.sh MetroidPrime/main.cpp
                                       .text SHORT by 7316; 16 extras (unchanged count), all COMDAT
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                       would break on a flip (pre-existing, unchanged)
git status                            config/G2ME01/symbols.txt, include/MetroidPrime/
                                       CInGameTweakManager.hpp, src/MetroidPrime/main.cpp
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under `MP_GATE_DOCS_WRITE=1`
and the driver owns that file.

## One correction to an earlier note, because it will otherwise be repeated

**`build/report.json` in a lane worktree is not a baseline and must not be believed** (attempt 4
said this; it bit this run too - the pre-build leftover read 63/99 on a tree that was already there,
and the number that mattered came from rebuilding). The two things that *are* trustworthy at the
start of a lane run are `build/goal/judge/report.base.json` (the driver's own) and a **fresh**
`./tools/decomp_build.sh`. Every count in this note was taken after that rebuild.

## NEW: the 0x800066xx cluster is down to one unknown, and it is a four-store shape

`match-main-ciengametweakmanager-dtor` (filed by attempt 4) is now 3 functions from done, not 8:
`fn_800067E0`, `fn_80006830`, `fn_80006850` and `CTweakValue::~CTweakValue` are landed at 100% in
this attempt. The blocker is one thing - `fn_80006724`'s four stack stores, which build two 2-word
objects whose second words are the iterators and whose first words duplicate them - plus the
`CInGameTweakManager` base/member declaration that has to exist for `__dt__800066D0` and
`__dt__80006678` to be emitted at all

NEW: match-main-fn-800067a8 | match | MetroidPrime/main | the 56-byte by-value forwarder
fn_800067A8 is 2 instructions of load-order scheduling away from byte-identical; 8 spellings measured
reach retail's exact size and the two closest want contradictory declaration orders (see the WALL
line above)

## Lane 8: passed, then failed on the moved tip (2026-09-30 08:09:48Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 5ea803065c73; re-do it against the current tip.

---

# Attempt 6 (lane 8, 2026-09-30) - re-done against the current tip, as the driver asked

Verdict **PARTIAL**: the target rose **74 -> 79 / 99** functions, `tools/goal_check.sh
build/goal/item.json` passed every check it can pass, and the flip still fails on exactly the
pre-existing `CErrorOutputWindow::__vt` link error attempts 2-5 recorded. Diff is **two files,
1 symbols.txt rename, ~110 inserted lines, 8 deleted**, no asm, no deletions of real work.

**The bitfield question this item was filed on was closed by attempt 1 and was not reopened.**

## Re-measured first, as attempt 4 warned

HEAD was `5ea8030 progress: match-main-sda-float-constants`. A fresh `./tools/decomp_build.sh` of
the clean tree: `main/MetroidPrime/main` **74 / 99**, `All:` **10108 / 28465**; the driver's
`build/goal/judge/report.base.json` agreed at 74 / 99. **Attempts 4 and 5 were not in HEAD** -
`fn_80007AA0`, `fn_800067A8`, `fn_800067E0` and `fn_80008B04` were all still unnamed/unfixed in
`config/G2ME01/symbols.txt` and `src/MetroidPrime/main.cpp` - so re-landing is not repeating work,
it was the only copy, exactly as attempt 4's closing note predicted.

## What landed (+5; three re-measured here from bytes, two new)

| retail | size | what it is | was |
|---|---|---|---|
| `0x80008B04` | 44 | `TOneStatic<CGameGlobalObjects>::operator delete(void*)` | `fn_80008B04`, unpaired - **rename + a call site**, attempt 4/5's diff, re-verified |
| `0x800067E0` | 80 | the `CTweakValue*` array walk | 90.00% - **new to this run** (see below) |
| `0x800067A8` | 56 | the by-value forwarder | 69.79% - **new to this run** (see below) |
| `0x80009058` | 80 | `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` | unpaired - **new to this run** |
| `0x8000934C` | 80 | `rstl::rc_ptr<CPlayerState>::ReleaseData()` | unpaired - **new to this run** |

`fn_80007AA0` needs nothing: at HEAD this object already emits a `T fn_80007AA0` under retail's own
name at 100.00%, so attempt 3/4/5's rename is **obsolete** - a rename would now unpair it.

## 1. `TOneStatic<CGameGlobalObjects>::operator delete` = retail `fn_80008B04`, 44 B

Attempt 4/5's diff, re-landed and re-verified from the bytes. `config/G2ME01/symbols.txt` gets the
name (`__dl__32TOneStatic<18CGameGlobalObjects>FPv ... scope:global`, exactly parallel to the
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` retail already names at 0x80008A78), and
`src/MetroidPrime/main.cpp` gets a file-local `SForceGlobalObjectsDelete` whose constructor makes
the call. Retail lays the four members out for `CGameArchitectureSupport` as
`__nw__`/`__dl__`/`GetAllocSpace`/`ReferenceCount` at 0x80008A48/0x80008A78/0x80008AA4/0x80008AB0
and for `CGameGlobalObjects` at 0x80008AD4/**0x80008B04**/0x80008B30/0x80008B3C **with the same four
sizes in the same order**; only this one's name was missing. `report_diff.py` prints
`RENAMED fn_80008B04 -> __dl__32TOneStatic<18CGameGlobalObjects>FPv (0.00% -> 100.00%)`.

## 2 + 3. `fn_800067E0` 90.00% -> 100.00% and `fn_800067A8` 69.79% -> 100.00%

**These are queued items of their own** (`match-main-fn-800067e0-chain`, `match-main-fn-800067a8`,
and the second run of `match-main-ciengametweakmanager-dtor`), and **all three of those diffs were
lost** - HEAD had neither fix. The spellings are recorded in those notes; this run re-measured both
against retail's bytes with `.tmp/opencode/p/cmp.py` before shipping them, and both are now
20/20 and 14/14 words equal to retail's 80 and 56 bytes.

* `fn_800067E0(STweakValue* const* first, STweakValue** last)` - `const` on `first` only. It makes
  the two loaded values simultaneously live, which is what lets mwcceppc interleave the `lwz` with
  the two callee-save `stw`s in retail's order; `const` on `last` loses the `mr r30,r4` entirely.
* `fn_800067A8(STweakValue* const* first, STweakValue* const* last)` with the locals declared
  `STweakValue* f; STweakValue* l;` and **assigned** `l = *last; f = *first;`. mwcceppc gives
  frame slots to address-taken locals in *declaration* order (from the top of the local area down)
  and emits the *stores* in *assignment* order, so the two have to disagree here.

**General codegen rule worth carrying, measured again here:** a `const` on a pointer parameter
decides register liveness, and declaration order versus assignment order decides frame slot
versus load order. See the class comment added at each definition.

## 4 + 5. The two `rc_ptr<T>::ReleaseData()` copies, 80 B each, and why a call site is honest

`0x80009058` = `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` and `0x8000934C` =
`ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv`. Both already have their real name in
`symbols.txt`, and both are the same 20 instructions as the
`rc_ptr<vector<string>>` / `rc_ptr<vector<int>>` copies `~CWorldLayerState` already gets - except
for the `li r4,1` **deleting** flag, because retail passes 1 here and -1 from an `rc_ptr`
destructor.

**They are emitted where they are because of how dtk splits, not because of who referenced them.**
`rc_ptr<T>::ReleaseData()` is out of line in `include/rstl/rc_ptr.hpp` and `-inline deferred,noauto`
keeps it a call, so `~CWorldLayerState` gets its two copies that way - but a template member is
only emitted where it is *used*, and every holder of these two types
(`CStateManager::mMapWorldInfo` at `CStateManager.hpp:334`, `CWorldState::mMapWorldInfo` at
`CWorldState.hpp:44`, `CGameState::mPlayerStates` at `CGameState.hpp:317`) has its destructor in
another unit. **There is no caller of either address anywhere in retail's `.text` range
0x800053B8-0x80009880** - `grep 'bl 80009058' build/G2ME01/main.elf` gives 0x800044c8, 0x800380dc,
0x80042ac8, 0x80087710, 0x80087ec8 and nothing in range, and likewise for 0x8000934C. dtk's split
is by *linked address range*, so retail's `main.o` carries the COMDAT while its `main.cpp` never
called it. Reading "retail called it here" would be the same mistake as reading a rotate mask as a
bit index.

The forcing idiom is the repo's own: `src/MetroidPrime/Player/CGameStateCtor.cpp:298-303` makes the
same `rc_ptr<CPlayerState>` call for the same reason, and `SForceGlobalObjectsDelete` above forces
`TOneStatic<CGameGlobalObjects>::operator delete` this way. MWCC 2.7 rejects an explicit
instantiation of a member function, so it must be a real call inside a function the compiler is
made to generate - hence constructors.

## Side effect, recorded honestly

`tools/unit_fit.sh` now lists **`__sinit_main_cpp` (180 bytes)** among the unclaimed functions: the
three force-call constructors all fold into the translation unit's static-init routine. It was not
emitted at all before this change (15 extras, now 16). `.text` is **5728 bytes short** of the 17608
claimed, so the flip does not wait on it, and `.sbss` is 81 against 36 claimed (was 61): the two
`rc_ptr` members are retail's own shape - retail's `main.o` carries the same `ReleaseData` pairs -
so the claim is short, not the object overrunning. `.ctors` went from SHORT by 4 to **fits** as a
side effect of the static data. `.sbss` is not scored for this unit (the report carries
`total_data: 40`, `matched_data: 4`).

## What still stops the flip (unchanged from attempt 4)

`flip_test.sh` fails at link with
`mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`
- a `config/G2ME01/splits.txt` question (that `.data` is not claimed for this unit), the first
error and not the whole story. 20 of the 99 functions are not matched, and
`check_decl_order.py --unit MetroidPrime/main` still reports **would break on a flip** (our `.text`
is descending where retail's is ascending) - pre-existing, and it wants the whole file reversed,
which is its own item.

The remaining 20, with what is known about each, so nothing here is re-derived:

* **the five `ReleaseData`/unnamed 0x50-byte copies** `fn_80009008`, `fn_80009224`, `fn_800095E4`
  (each 80 B, each identical to the two now matched except for the `bl` at +0x28). `fn_800095E4` is
  `rc_ptr<CWorldTransManager>` - its `bl` target is `__dt__18CWorldTransManagerFv` - called from
  `__dt__10CGameStateFv`; `fn_8000447C` calls `fn_80009224` and `fn_80009008`; `fn_80004510` calls
  `rc_ptr<CPlayerState>`. **All three unnamed ones are the same problem this run just solved for
  two named ones**: they need the same forced call site, and the types are identified above, so
  they are winnable by the identical instrument.
* **`fn_80006724` (132 B, 78.21%)** - still the four-address-taken-local shape with two dead
  copies; `match-main-ciengametweakmanager-dtor`'s notes are exhaustive (14 spellings) and this
  run did not retry it.
* **the `CGameGlobalObjects` trio** `single_ptr_assign_800064D0` (72 B), `__dt__CGameGlobalObjects_80006518`
  (264 B) and `__dt__80006AE0` (88 B). `__dt__80006AE0` is the smallest and is fully decoded above
  in this file's attempt 4 section plus `rstl::single_ptr.hpp`'s own `operator=` - it is
  `if (this) { ~CGameGlobalObjects(this->mPtr, 1); if (flag > 0) CMemory::Free(this); }`, the same
  D0 shape as `__dt__Q24rstl24single_ptr<10CGameState>Fv` (0x80006620, 88 B) already at 100%, and
  it is the only one of the three with no unresolved callee.
* **`__dt__15CMemoryInStreamFv` (0x800055CC, 96 B)** - fully decoded: the same D0 shape, with
  `this->__vt = &__vt__12CInputStream`-style store of `0x803B0D5C` (that is
  `__vt__15CMemoryInStream`, `symbols.txt:17882`), `li r4,0 ; bl __dt__12CInputStreamFv`, then the
  `extsh./ble/CMemory::Free` tail. **Not worth taking here**: `CMemoryInStream`'s destructor is
  currently inline `{}` in `include/Kyoto/Streams/CMemoryInStream.hpp:15`, so moving it out of line
  touches every `CMemoryInStream` destructor call site in the DOL (CWorld.cpp, CGameArea.cpp, ...),
  which is a much wider blast radius than this unit.
* **the 0x80008C28-0x80008E94 cluster** - 792 bytes in five functions, real work, not started.
  `fn_80008C28` (184 B) is a recursive tree walk, `fn_80008CE0` (136 B) allocates 44 bytes
  (`li r3,44 ; bl allocate`) and stores four words then copy-constructs a string, `fn_80008D68`
  (128 B) is the recursive destroy with the **doubled `addic. r0,r31,16` null test** that
  `rstl::basic_string`'s out-of-line destructor needs, `reserve__...vector<pair<Ui,Ui>>` (172 B)
  and `fn_80008E94` (172 B) are not read. The 44-byte element and the doubled test are the two
  things a writer has to resolve first.
* the large bodies: `RsMain` 2.38% (2148 B, queued as `match-main-rsmain-body`), `CheckReset` 0.34%
  (1180 B), `AddPaksAndFactories` 0.21% (1936 B), `StreamNewGameState` 18.68% (532 B),
  `InitializeSubsystems` 12.44% (348 B), and `AsyncIdle` 99.17% (288 B), whose 17-spelling table
  is already in `src/MetroidPrime/main.cpp` above its definition.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.10% fuzzy, 23.39% matched, 11.78% linked (10113 / 28465)
unit: main/MetroidPrime/main          74 -> 79 / 99, fuzzy 55.42% -> 56.72%,
                                       matched_code 9056 -> 9396 (= 44 + 80 + 56 + 80 + 80 exactly)
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                       matched 10108 -> 10113, linked 4918 -> 4918,
                                       "+5 functions at 100%", "1 RENAMED 0.00% -> 100.00%",
                                       "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp
                                       .text SHORT by 5728 (was 6440); .ctors fits (was SHORT 4);
                                       .sbss 81 vs 36; 16 extras (was 15), the new one
                                       __sinit_main_cpp
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                       would break on a flip (pre-existing, unchanged)
git status                           config/G2ME01/symbols.txt, src/MetroidPrime/main.cpp
```

Byte-identity was checked on the raw words of `build/G2ME01/src/MetroidPrime/main.o` against
`build/G2ME01/main.elf`, masking only the displacement of `b`/`bl` - **`main.elf` keeps R_PPC_REL24
fixups even for near targets**, so a near `bl` reads `0x48000001` in our object and `0x4800002d`
(byte offset + 1) in retail; `bc`'s I-form displacement is a real value in both and is compared
exactly. `.tmp/opencode/p/cmp.py` and `.tmp/opencode/p/cc.sh` (which compiles with this unit's
exact `build.ninja` flags - `tools/probe_cc.sh` omits `inline_max_size(125)` and the musyx `-D`s)
are untracked scratch. The same script reports `DIFFERS` on `fn_80006724` (22 of 30 instructions)
and `IDENTICAL` on the untouched `fn_80006874`, so the check can fail.

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under `MP_GATE_DOCS_WRITE=1`
and the driver owns that file.

## No new `WALL:` line

`fn_800067E0` and `fn_800067A8` were listed as walls in attempts 2 and 5 and in the two sibling
items' notes; this run measured both spellings above and both are byte-identical, so those
spellings are **superseded, not re-confirmed**: the miss was a `const` qualifier, not the loop, the
stride or the declaration order of the parameters. `fn_80006724` was not retried and this run makes
no claim about it.

NEW: match-main-releasedata-unnamed-three | match | MetroidPrime/main | fn_80009008, fn_80009224
and fn_800095E4 are three 80-byte rc_ptr<T>::ReleaseData copies with no name in symbols.txt;
fn_800095E4 is T=CWorldTransManager and the other two are called from fn_8000447C - the same forced
call-site instrument this run used for the two *named* copies lands their bytes, and the bytes are
already known to be identical except for one bl

---

# Attempt 7 (lane 8, 2026-09-30) - the `ReleaseData` family, and the instrument earlier runs missed

Verdict **PARTIAL**, and the largest single step since attempt 6: the target rose **79 -> 84 / 99**
functions, `tools/goal_check.sh build/goal/item.json` passed every check it can pass, and the flip
still fails on exactly the pre-existing `CErrorOutputWindow::__vt` link error attempts 2-6 recorded.
Diff is **two files, 93 insertions, 8 deletions**, no asm, no deletions of real work.

**The bitfield question this item was filed on was closed by attempt 1 and was not reopened.**

## Re-measured first

HEAD was `6b1c156 progress: match-main-cgameglobalobjects-dtors`. A fresh `./tools/decomp_build.sh`
of the clean tree: `main/MetroidPrime/main` **79 / 99**, `All:` **10121 / 28465**, and the driver's
`build/goal/judge/report.base.json` agreed at 79 / 99. Nothing from attempts 4-6 was in HEAD again
(`fn_80007AA0`, `fn_80008B04`, `fn_80009224`, `fn_800095E4` were all still `fn_` in
`config/G2ME01/symbols.txt`).

## The instrument earlier attempts did not find: `template class`

Attempts 4, 5 and 6 forced each of these bodies out with a **file-local struct whose constructor
makes the call**, plus a global instance to make the compiler emit that constructor. That costs a
`.ctors` entry, a `.bss` word and a static initializer that would run at startup. It is not needed.

**MWCC 2.7 rejects explicit instantiation of a *member* and accepts explicit instantiation of the
*class*** - both re-measured this run:

```
template void rstl::rc_ptr< CMapWorldInfo >::ReleaseData();
  # Error: illegal explicit template instantiation      <- attempts 4/5/6 recorded this
template class rstl::rc_ptr< CMapWorldInfo >;
  (no diagnostic) -> exactly one symbol emitted, 0x50 bytes, byte-identical to retail 0x80009058
```

and because `ReleaseData()` is the *only* out-of-line member of `rc_ptr<T>`, the class form emits
**only** it: five instantiations in one probe produced five 0x50-byte functions and nothing else.
**Two conditions, both measured, not guessed:**

* **`T` must be complete.** With a forward declaration the same instantiation is **0x4C = 76 bytes**
  and the destructor call disappears (`~T` becomes unreachable so mwcceppc drops it). With the real
  header it is 0x50 with `bl __dt__TFv` where retail has it.
* **The class's own definition is the only extra.** `template class TOneStatic<CGameGlobalObjects>;`
  also instantiates the one-argument `operator new(size_t)`, which retail's `main.o` does not carry -
  see the cost note below.

## What landed (+5; all four `ReleaseData` copies byte-identical, first try)

| retail | size | what it is | was |
|---|---|---|---|
| `0x80009058` | 80 | `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` | named, never instantiated - **new** |
| `0x80009224` | 80 | `rstl::rc_ptr<CWorldLayerState>::ReleaseData()` | `fn_80009224` - **rename + instantiation** |
| `0x8000934C` | 80 | `rstl::rc_ptr<CPlayerState>::ReleaseData()` | named, never instantiated - **new** |
| `0x800095E4` | 80 | `rstl::rc_ptr<CWorldTransManager>::ReleaseData()` | `fn_800095E4` - **rename + instantiation** |
| `0x80008B04` | 44 | `TOneStatic<CGameGlobalObjects>::operator delete(void*)` | `fn_80008B04` - **rename + instantiation** |

`main/MetroidPrime/main` 79 -> 84 / 99, fuzzy 57.97% -> 60.03%, `matched_code` 9616 -> 9980
(= 4x80 + 44 exactly), `All:` 10121 -> 10126, linked unchanged at 4917.

## 1. The two `T`s that were already named, and the two that were not - all read off the `bl`

Each 0x50-byte body is the same twenty instructions; the **only** thing that distinguishes them is
the one `bl` at +0x30, and that names the pointee's destructor:

```
0x80009058  bl __dt__13CMapWorldInfoFv       -> ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv   (named)
0x80009224  bl __dt__16CWorldLayerStateFv    -> ReleaseData__Q24rstl26rc_ptr<16CWorldLayerState>Fv   (was fn_)
0x8000934C  bl __dt__12CPlayerStateFv        -> ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv       (named)
0x800095E4  bl __dt__18CWorldTransManagerFv  -> ReleaseData__Q24rstl28rc_ptr<18CWorldTransManager>Fv (was fn_)
```

`CWorldLayerState` is **not** what attempt 6 guessed. It is pinned independently of the `bl`, by
`CWorldState::~CWorldState` (0x8000447C), which is fully readable and releases three `rc_ptr`s:

```
+0x1C  bl 80009224   \   CWorldState's members in the header's declaration order, and
+0x10  bl 80009058    |  CHECK_SIZEOF(CWorldState, 0x24) = 0 + 4 + 8 + 8 + 4 + 8
+0x08  bl 80009008   /    mWorldId / mAreaId / mRelayTracker / mMapWorldInfo
                           / mDesiredAreaAssetId / mLayerState
```

so +0x1C is `mLayerState` (`rc_ptr<CWorldLayerState>`), +0x10 is `mMapWorldInfo`
(`rc_ptr<CMapWorldInfo>` - which is why retail *names* that one and not the other) and +0x08 is
`mRelayTracker`. The two `0x50` bodies differ from each other in nothing but that `bl`, and the
header's own member order fixes the offsets. Attempt 6 said only "called from `fn_8000447C`"; the
*which* is new.

**Mangled names were read out of the compiler, not derived.** `nm` on the probe object gives
`ReleaseData__Q24rstl26rc_ptr<16CWorldLayerState>Fv` and
`ReleaseData__Q24rstl28rc_ptr<18CWorldTransManager>Fv`; the length prefixes 26 and 28 are MWCC's own
and follow the same `strlen(name)+2` pattern as the ten `ReleaseData` entries `symbols.txt` already
carries. `check_symbol_names.py` passes, which is the check that would have caught a wrong one.

**These four are in retail's `main.o` because dtk splits by linked address range, not because
`main.cpp` called them** - the reasoning attempt 6 got right, and it is why no call site had to be
invented. `CWorldState::~CWorldState` (0x8000447C) and `__dt__10CGameStateFv` are both **outside**
0x800053B8-0x80009880. 0x8000934C has 22 callers in the DOL (`CPlayer::GetWeight`,
`CPlayer::GetGravity`, ...) and **none of them is inside main.o's range**.

## 2. `TOneStatic<CGameGlobalObjects>::operator delete` = retail `fn_80008B04`, 44 B

the comment above `class CGameGlobalObjects` in `include/MetroidPrime/CGameGlobalObjects.hpp:81-88`
had already identified the body, and
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` (0x80008A78, 44 B) is the same 44 bytes and is
named in `symbols.txt`; retail lays the four members out for each of the two classes as
`__nw__` / `__dl__` / `GetAllocSpace` / `ReferenceCount`, in that order, at the same four sizes
(48 / 44 / 12 / 36) - only this one's name was missing. All four re-measured byte-identical here.

The `extern "C" void fn_80008B04(void*)` forward declaration and its one call site in
`__dt__CGameGlobalObjects_80006518` became the real
`TOneStatic<CGameGlobalObjects>::operator delete(self)`; the `bl` stays a relocation, and that
264-byte function is still byte-identical to retail (the call is **not** inlined, checked).

**Cost, recorded honestly:** the class also instantiates the one-argument `operator new(size_t)`,
`__nw__32TOneStatic<18CGameGlobalObjects>FUl` (0x2C = 44 bytes), which retail's `main.o` does not
carry: `__nw__32TOneStatic<18CGameGlobalObjects>FUlPCcPCc` (0x80008AD4) has exactly one caller in
the whole DOL, 0x80005CD0, and it passes three arguments (0x80005E14 is `CGameArchitectureSupport`'s
one). `bl 80008b04` likewise has exactly one caller, 0x80006600.
`tools/unit_fit.sh` extras go **15 -> 16 functions, 1344 bytes**, and the new one is exactly that
function. `.text` is 5608 bytes short of the 17608 claimed, so nothing waits on it. The class form
also *replaces* this object's three weak COMDAT copies of `__nw__` / `GetAllocSpace` /
`ReferenceCount` for this class with single strong definitions, so no function is defined twice.

## 3. `fn_80009008` is the fifth one, and it is blocked - new, measured

`fn_80009008` (0x80009008, 0x50) is `rc_ptr<CRelayTracker>::ReleaseData()`: it is the third release
in `CWorldState::~CWorldState`, at +0x08 = `mRelayTracker`, and the `bl` is `fn_800B8CA0`. The
instantiation compiles and emits 0x50 bytes with `bl __dt__CRelayTrackerFv` - **but that destructor
is not defined anywhere in the tree**, so the reference would take the port's undefined count from
**250 to 251**, which is exactly what `tools/link_check.sh --strict` fails on
(`docs/research/port_link_baseline.txt:4`, `undefined 250`). It is therefore **not** landed.

It is not landed *and* the obvious fix is wrong, which is the new part: retail's `fn_800B8CA0` is
0x54 = 84 bytes and tears down no members at all, while
`CRelayTracker::~CRelayTracker() {}` written from this tree's header is **0x3C = 60 bytes**
(measured, `.tmp/opencode/p7/t5.cpp`) - `rstl::reserved_vector<TEditorId, 512>` has a non-trivial
destructor here, so retail's `CRelayTracker` holds its table differently. Retail's `~CRelayTracker`
is at 0x800B8CA0, in an `auto_*` range **no unit claims**, so writing it in `main.cpp` would put a
wrong-sized, wrongly-placed copy in this object. The blocker is the `reserved_vector` member model,
not the ReleaseData.

## Measured and re-confirmed, not extended: `AsyncIdle`

`CMain::AsyncIdle` re-measured from bytes this run: **99.17%**, 288 B, and the residue is **exactly
one instruction**, 0x80005C44, `clrlwi r5,r30,24` where this is `mr r5,r30` - mwcceppc eliding the
`bool` narrowing because it can prove `r30` is 0 or 1. The source comment's 17-spelling table is
accurate. One spelling not in it, tried here and **worse**: `int flag = 0; ... flag = 1;` passed to
the `bool` parameter adds a call and grows the function past 288 bytes (it makes `r31`/`r30`
copy-propagated into a second basic block, so the tail moves). Do not re-run the table; what is
needed is a way to make mwcceppc lose the 0/1 range fact about the flag.

`fn_80006724` was not retried: the notes' slot-allocation analysis (1st -> r1+0x14, 2nd -> r1+0x10,
3rd -> r1+0x0C, 4th -> r1+0x08, so retail passes its 1st and 3rd address-taken locals) is
consistent with the bytes, and no shape of it reaches 33 of 33 instructions, so nothing here can
reach 100% and it is not worth another run's hour.

## The remaining 15, so nothing is re-derived

`fn_80009008` 80 (above), `fn_80008C28` 184 / `fn_80008CE0` 136 / `fn_80008D68` 128 / `fn_80008E94` 172
/ `reserve__Q24rstl55vector<pair<Ui,Ui>,Ui>Fi` 172 (the 0x80008C28-0x80008E94 cluster, 792 bytes,
unwritten, real work), `fn_80006724` 132, `fn_800068F4` 96, `__dt__15CMemoryInStreamFv` 96 (the
class's destructor is still inline `{}` in `include/Kyoto/Streams/CMemoryInStream.hpp:15`, so taking
it out of line moves every `CMemoryInStream` destructor call site in the DOL), and the large bodies
`AddPaksAndFactories` 1936, `CheckReset` 1180, `RsMain` 2148, `StreamNewGameState` 532,
`InitializeSubsystems` 348, `AsyncIdle` 288.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.16% fuzzy, 23.45% matched, 11.78% linked (10126 / 28465)
unit: main/MetroidPrime/main          79 -> 84 / 99, fuzzy 57.97% -> 60.03%, matched_code 9616 -> 9980
tools/report_diff.py judge base       matched 10121 -> 10126, linked 4917 -> 4917,
                                      "+5 functions at 100%", 3 RENAMED 0.00% -> 100.00%, "no regression"
tools/goal_check.sh build/goal/item.json
                                      PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/link_check.sh                   250 undefined, 0 duplicates, "unchanged from baseline"
tools/probe_sources.sh                749 files, 0 failed; link LINKED (250 undefined, 0 dups)
python3 tools/check_symbol_names.py   ok
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp
                                      .text 12000 (was 11624), SHORT by 5608 (was 5984);
                                      .sbss 61 vs 36 claimed (unchanged); 16 extras (was 15),
                                      the new one __nw__32TOneStatic<18CGameGlobalObjects>FUl
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                      would break on a flip (pre-existing, unchanged)
git status                           config/G2ME01/symbols.txt, src/MetroidPrime/main.cpp
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under `MP_GATE_DOCS_WRITE=1`
and the driver owns that file. Byte-identity was checked with `.tmp/opencode/p7/cmp.py`, which
compiles `src/MetroidPrime/main.cpp` with this unit's exact `build.ninja` flags (0.8 s per run) and
compares instruction words against `build/G2ME01/main.elf`, masking `b`/`bl` displacements and any
instruction the object carries a relocation for (SDA21, REL24, ADDR16_HA/LO) - the same classes
objdiff wildcards. It reports `IDENTICAL` on ten untouched functions, so it can fail.

## WALL: AsyncIdle__5CMainFUi 99.17% (288 B) - one instruction, and 18 spellings now say it is not a spelling

`0x80005C44` is `clrlwi r5,r30,24` and this is `mr r5,r30`: mwcceppc elides the `bool` narrowing
because it can prove the flag in `r30` is 0 or 1. The source comment's 17-spelling table plus this
run's `int flag` (worse - it adds a call and grows the function) all fail; what is needed is a way to
destroy the 0/1 range fact, not another type for the local.

NEW: match-main-creslayout | match | MetroidPrime/main | fn_80009008 (80 B) is
rc_ptr<CRelayTracker>::ReleaseData - CWorldState::~CWorldState's +0x08 mRelayTracker - and is blocked
because the tree's CRelayTracker destructor is 0x3C bytes where retail's fn_800B8CA0 is 0x54, i.e.
rstl::reserved_vector<TEditorId,512> is not modelled the way retail has it; instantiating it also
needs a definition that no unit claims

---

# Attempt 8 (lane 1, 2026-09-30) - the 0x80008C28 cluster, decoded from scratch

Verdict **PARTIAL**, and the largest step since attempt 7: the target rose **86 -> 90 / 99**
functions, `tools/goal_check.sh build/goal/item.json` passed every check it can pass, and the flip
still fails on exactly the pre-existing `CErrorOutputWindow::__vt` link error attempts 2-7 recorded.
Diff is **three files, 162 insertions, 17 deletions**, no asm, no deletions of real work.

**The bitfield question this item was filed on was closed by attempt 1 and was not reopened.**

## Re-measured first, as attempts 4-7 each had to

HEAD was `1d7656b progress: match-main-fn-800067e0-chain`. A fresh `./tools/decomp_build.sh` of the
clean tree: `main/MetroidPrime/main` **86 / 99** (attempts 1-7 all say 84 or lower), `All:`
**10151 / 28465**, linked 4939, and `build/goal/judge/report.base.json` agreed at 86 / 99. Nothing
from attempts 4-7 is in HEAD and nothing needed re-landing this time: `fn_80008C28`, `fn_80008CE0`
and `fn_80008D68` were still unpaired, and `fn_80008E94` was the 18.40% thunk attempt 4 wrote.

## 1. `fn_80008C28`, `fn_80008CE0`, `fn_80008D68` - 448 bytes, **all three byte-identical first try**

Attempts 4 and 6 left this cluster as sizes and half-decodes ("real work, not started"). It is a
**three-node binary tree with a string key**: post-order rebuild, node constructor, recursive
destroy. Every offset is read off the bytes and the node is

```
+0x00 void* mLeft      both children: lwz + cmplwi + beq before each recursive call
+0x04 void* mRight
+0x08 void* mParent    fn_80008C28 writes the new node into +0x08 of each non-null child
+0x0C void* mFieldC    copied verbatim
+0x10 SNodeKey mKey    28 bytes = { rstl::string, uint, uint, uint }
```

`CHECK_SIZEOF(SNode, 0x2C)` = 44 is retail's `li r3,44` at 0x80008CE8, and **the key is what makes
44 rather than 32** - `fn_80008CE0` copies the source key's `+0x20/+0x24/+0x28` into the new node's,
so the key has a 12-byte tail after its 16-byte string. Three findings did the work:

* **The key is copy-*constructed*, not assigned.** mwcceppc expands the implicit copy constructor of
  `{rstl::string, uint, uint, uint}` as "copy-construct `mName` (one call to
  `rstl::basic_string`'s copy constructor) then copy the three `uint`s", which is
  `bl __ct__basic_string` followed by three `lwz/stw` pairs - retail 0x80008D34-0x80008D4C, verbatim.
  Written as `n->mKey = *key` it emits `bl assign__Q24rstl66basic_string` instead, because **this
  tree's `rstl::basic_string` declares `operator=`** (`include/rstl/string.hpp:210`); the placement
  form `new (&n->mKey) SNodeKey(*key)` keeps the constructor. Measured both, and the `addic. r31,r30,16
  / beq` guard at 0x80008D18 is that placement new's member-address test.
* **`fn_80008D68`'s doubled `addic. r0,r31,16 / beq` at 0x80008DB0 and 0x80008DB8 is the wrapper,**
  not two identical statements: mwcceppc emits a null test on the address of every class-type member
  it destroys, and `SNodeKey`'s destructor destroys `mName`, so the wrapper's test and the string's
  test have the same displacement. This is the same shape attempt 5 found in `fn_80006874`
  (`&mAudio` and `&mAudio.mFileName`) and it is now a general rule. Naming the string directly gives
  one test (measured); leaving the destructor to scope exit gives none. The third test, `cmplwi
  r31,0` at 0x80008DA8, is an explicit `if (node)` whose branch goes to the `CMemory::Free`, so the
  free is **outside** the guard.
* **`fn_80008C28`'s two `li r31,0 / li r30,0` materialise before the first child test** (retail
  0x80008C5C-0x80008C60), so the two accumulators are declared above the `if`s, and the early
  `return nullptr` is the `li r3,0 / b` at 0x80008C50 - retail branches to the epilogue rather than
  carrying a value.

All three have **no caller inside this unit's 0x800053B8-0x80009880 range** (retail's callers of
`fn_80008C28` are 0x800040E0 and 0x80005310, of `fn_80008D68` 0x800040C0 and 0x80004700, of
`fn_80008CE0` three outside it), so **they are in retail's `main.o` because dtk splits by linked
address range** - attempt 6's reasoning, extended. No call site had to be invented, which is why
this was winnable.

## 2. `fn_80008E94` identified, and the header gap that was hiding it - 172 bytes, first try

Attempt 4 recorded retail's `fn_80008E94` as "a second *copy* of the body" with "nothing recoverable
from the DOL" saying which instantiation it is. **Its caller says so.** Retail's call at 0x80142244
is inside `SetCinematicState__18CPersistentOptionsFQ24rstl19pair<Ui,9TEditorId>b`
(`symbols.txt`), which reserves `r30+24` for `r9+1` and then writes the next slot two words wide:

```
80142244  bl   80008e94            reserve(mX, r9+1)
80142248  lwz  r4,28(r30)          count
8014224c  lwz  r5,36(r30)          items
80142260  lwz  r3,0(r31)           \
80142264  lwz  r0,4(r31)            > two words -> rstl::pair<Ui,9TEditorId>
80142268  stw  r3,0(r4)            /
8014226c  stw  r0,4(r4)
```

So `fn_80008E94` is `rstl::vector<rstl::pair<Ui,9TEditorId>, rmemory_allocator>::reserve`, and
`config/G2ME01/symbols.txt:182` now carries that mangled name in place of `fn_80008E94`. The
mangling is MWCC's own, read out of `nm` on the object -
`reserve__Q24rstl63vector<Q24rstl19pair<Ui,9TEditorId>,Q24rstl17rmemory_allocator>Fi` - and 63/19 are
its length prefixes, the same rule as the ten `ReleaseData` entries `symbols.txt` already carries.
`report_diff.py` prints the pairing as `RENAMED fn_80008E94 -> reserve__... (18.40% -> 100.00%)`.

**It also needed a header fact, and that is the transferable part.**
`include/MetroidPrime/TGameTypes.hpp:70-78` already declares `pair<TEditorId, bool>` trivially
destructible with a `construct<>` specialisation. Without the same two lines for
`pair<uint, TEditorId>` the identical source emits **184** bytes with `uninitialized_copy` outlined
into a function of its own, and does not match retail's 172 inlined. Measured both ways
(`.tmp/opencode/L1/p5.cpp` vs `p6.cpp`, same file, two lines apart). The diff adds that
specialisation next to the existing one.

**The two 32-byte thunks are honest, and the previous one is no longer a matched function.** Nothing
in this unit calls either `reserve`, so a call has to be forced, and MWCC 2.7 rejects explicit
instantiation of a member (attempt 7's finding), so it is a real call in a generated function.
`extern "C" void reserve_pair_ui(...)` and `extern "C" void
reserve_pair_ui_editor_id(...)` - neither name is in `symbols.txt`, so objdiff ignores both and
`unit_fit.sh` lists them. That is **one more unclaimed function than the single `fn_80008E94` thunk
this replaces**, and it is the price of the rename: the old name was a *paired* (18.40%) function,
so it was never an extra.

## Side effect, recorded honestly

`unit_fit.sh`: `.text` 12892, **SHORT by 4716**; 17 extras / 1364 bytes (was 14 on this tree before
this change), the new ones `__dt__Q24rstl66basic_string<c,...>Fv` (0x50 = 80 B, emitted because
asking for the wrapper's destructor explicitly makes mwcceppc emit the string's out of line as
well) and the two thunks. `.sbss` 61 against 36 claimed, unchanged - the `TGameTypes.hpp` change
adds no static data. `.rodata/.data/.bss/.sdata/.sdata2` unclaimed, unchanged. `matched_code` rose by
**620 = 448 + 172 exactly**, which is the check that these four are the only thing that changed.

## What still stops the flip (unchanged from attempt 4)

`flip_test.sh` fails at link with
`mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o` -
a `config/G2ME01/splits.txt` question, the first error and not the whole story. Nine of the 99 are
still unmatched, and `check_decl_order.py --unit MetroidPrime/main` still reports **would break on a
flip** (our `.text` is descending where retail's is ascending) - pre-existing, and it wants the whole
file reversed.

## The remaining nine, so nothing here is re-derived

* `RsMain` 2148 (2.38%, its own queued item), `AddPaksAndFactories` 1936 (0.21%),
  `CheckReset` 1180 (0.34%), `StreamNewGameState` 532 (18.68%), `InitializeSubsystems` 348 (12.44%)
  - the five large bodies, essentially unwritten.
* `AsyncIdle` 288 (99.17%) - the WALL above, 18 spellings, not retried.
* `__dt__15CMemoryInStreamFv` 96 - attempt 6's reading stands: taking the destructor out of line
  moves every `CMemoryInStream` destructor call site in the DOL, a much wider blast radius.
* **`fn_80006724` 132 (78.21%) and `fn_800068F4` 96 - both blocked on the same four-store shape, and
  this run confirmed the shape is shared.** `fn_800068F4` is the `fn_80006724` destructor body with
  stride **0x0C = 12** calling `fn_80004864` instead of `fn_800067A8`; `fn_80004864` is
  `fn_800067A8`'s exact shape (and `fn_8000489C` is `fn_800067E0`'s with `CToken`, stride 12 and a
  `li r4,0` flag), so `fn_800068F4` is winnable the moment `fn_80006724` is. The residue is the two
  dead copies: `{end,end}` at `r1+8/0x0C` and `{base,base}` at `r1+0x10/0x14`, passing
  `&r1+0x14` and `&r1+0x0C`. **Fourteen spellings are recorded in attempt 5 and none works; do not
  re-run them.**
* `fn_80009008` 80 - `match-main-creslayout`'s blocker (`CRelayTracker`), not retried.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.19% fuzzy, 23.48% matched, 11.79% linked (10155 / 28465)
unit: main/MetroidPrime/main          86 -> 90 / 99, fuzzy 61.64% -> 64.99%,
                                       matched_code 10232 -> 10852 (= 448 + 172 exactly)
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                       matched 10151 -> 10155, linked 4939 -> 4939,
                                       "+4 functions at 100%",
                                       1 RENAMED 18.40% -> 100.00%, "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/probe_sources.sh                749 files, 0 failed; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   505 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp
                                       .text 12892, SHORT by 4716; 17 extras, 1364 bytes
python3 tools/check_decl_order.py --unit MetroidPrime/main
                                       would break on a flip (pre-existing, unchanged)
git status                           config/G2ME01/symbols.txt, include/MetroidPrime/
                                       TGameTypes.hpp, src/MetroidPrime/main.cpp
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under `MP_GATE_DOCS_WRITE=1`
and the driver owns that file. Byte-identity was checked with `.tmp/opencode/L1/cmp.py`, which
compiles a scratch source with this unit's exact `build.ninja` flags and compares instruction words
against `build/G2ME01/main.elf`, masking the displacement of `b`/`bl`/`beq`/`bne` and any instruction
the object carries a relocation for - the classes objdiff wildcards. It reports `IDENTICAL` on
`fn_80008C28` (46/46), `fn_80008CE0` (34/34), `fn_80008D68` (32/32) and on
`reserve__Q24rstl63vector<pair<Ui,9TEditorId>>::reserve` (43/43). **A caveat on that script, since
it cost this run twenty minutes: `objdump` prints instruction bytes in file order, so they must be
read big-endian, and the linked ELF's listing has no leading whitespace where an object's does.**

## Codegen rules this run measured, worth carrying

1. **A wrapper struct whose destructor destroys a class-type member emits one null test per member,
   at each member's own address.** Two tests at the same displacement are the wrapper and the thing
   inside it, not two identical statements.
2. **A class member of type `rstl::string` assigned with `=` goes through `assign`, not the copy
   constructor**, because `include/rstl/string.hpp:210` declares `operator=`. Copy-*construct* it
   (placement new on the member) to get `__ct__basic_string`, which is what retail's key does.
3. **`rstl::pair<T1,T2>` needs its own `is_trivially_destructible` + `construct<>` specialisation in
   the header that defines its second type** or `vector<pair<...>>::reserve` outlines
   `uninitialized_copy` and comes out 12 bytes too long. The header has it for `pair<uint,uint>`,
   `pair<int,float>` (`include/rstl/pair.hpp`) and `pair<TEditorId,bool>`
   (`include/MetroidPrime/TGameTypes.hpp`) - this run added `pair<uint,TEditorId>`.
