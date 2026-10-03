# carve-8022a0c0 — `fn_8022A0C0`, the loader that builds `lbl_803B86B8`'s object

`kind: match`, target `MetroidPrime/ScriptLoader/Carve8022A0C0`. Carved, `Matching`,
`flip_test.sh` **PASS**, `tools/goal_check.sh build/goal/item.json` **PASS**. **Done.**

## What the item is

`fn_8022A0C0` at `.text 0x8022A0C0..0x8022A2CC`, 0x20C = 524 bytes, one function, **131
instructions** (`grep -c '^/\* 8022'` on `build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve8022A0C0.s`).
`config/G2ME01/symbols.txt:9840` gives
`fn_8022A0C0 = .text:0x8022A0C0; // type:function size:0x20C align:4`. Before this change it
was the first `.fn` block of dtk's `auto_03_8022A0C0_text` — the run `Carve8022A060.c` (this
lane's previous item) split out of `auto_03_8022A058_text`.

Callees, all named in `symbols.txt`: `__ct__20SLdrEditorPropertiesFv`,
`LoadTypedefEditorProperties__FR20SLdrEditorPropertiesR12CInputStream`,
`ReadFloat__12CInputStreamFv` ×4, `ReadInt32` (inlined),
`ReadBytes__12CInputStreamFPvUl`, `__nw__FUlPCcPCc`,
`AllocateUniqueId__13CStateManagerFv`, `LdrToTransform4f__FRC20SLdrEditorProperties`,
`LdrToEntityInfo__FR11CEntityInfoRC20SLdrEditorProperties`, `fn_8022A408`,
`__dt__20SLdrEditorPropertiesFv`.

## What it builds, from retail's own measurements

`lbl_803B86B8` (`.data:0x803B86B8`, `size:0x80`, `symbols.txt:18370`) is a 0x80 = 32-word
table whose entry 0x8 is `fn_8022A060` (this lane's `Carve8022A060.c`) and whose other entries
m2c names `TypesMatch__13CScriptAIHintCFi` (0xC), `PreThink__7CEntityFfR13CStateManager`
(0x10), `GetValueParm__13CScriptAIHintCFv`, `SetInUse__13CScriptAIHintFb`,
`GetInUse__13CScriptAIHintCF9TUniqueId`, `GetValueParm2__13CScriptAIHintCFv` — so the class is
the `CScriptAIHint` branch of `CActor`, and the four float property cases below are its
`radius` / `valueParm` / `valueParm2` / `valueParm3`
(`include/MetroidPrime/ScriptLoader/SLdrAIHint.hpp:13-17`).

`fn_8022A408` (`.text:0x8022A408`, `size:0x13C`, `symbols.txt:9851`) is the constructor retail
calls. Read off its own body: it runs
`__ct__6CActorF9TUniqueIdRCQ24rstl66basic_string...RC11CEntityInfoUiRC12CTransform4fRC10CModelDataRC13CMaterialListRC16CActorParameters9TUniqueId`,
then stores the vptr `lbl_803B86B8` at +0, `+0x158` from argument r8, `+0x15C`..`+0x168` from
f1..f4, a clear byte at `+0x16C`, `kInvalidUniqueId` at `+0x16E` and `lbl_8041DB08` at `+0x170`.
**The class is unnamed in retail** — constructor, destructor and vtable all carry `fn_`/`lbl_`
placeholders and no `__ct__13CScriptAIHint...` or `__dt__13CScriptAIHint...` exists anywhere in
`symbols.txt` — so this file calls `fn_8022A408` as the free function it is. The allocation
size `0x178` is that constructor's own: `+0x170` is the last member and `0x178` is its
8-byte-rounded end.

## The carve, four files

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8022A0C0.cpp` | new, 264 lines, 104 of them header comment |
| `config/G2ME01/splits.txt` | `MetroidPrime/ScriptLoader/Carve8022A0C0.cpp:` `.text 0x8022A0C0..0x8022A2CC`, between `Carve8022A060.c` and `Carve8022A3F4.c` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A0C0.cpp"),` on one line, between the same two |
| `files.cmake` | one line + a 5-line comment in the ScriptLoader carve block, immediately after `Carve8022A060.c` |

Plus `docs/research/port_link_gap.md` and `port_link_gap_list.md`, which the gate derives its
own counts from (below). `docs/HANDOFF.md` shows modified because `tools/check_docs_claims.py`
rewrites the state block from the tree.

## The three spellings that were load-bearing (all measured, none guessed)

**1. The property block must be an aggregate, not five locals.** Written as five separate
locals it compiles to **0x248 bytes in a 0xE0 frame** against retail's 0x20C in a 0xB0 frame:
mwcceppc register-allocates the four floats into non-volatile f28..f31 across the property loop
and emits eight `stfd`/`psq_st` pairs in the prologue and eight `psq_l`/`lfd` pairs in the
epilogue. As members of an aggregate (`SAIHintBlock`, an unnamed-namespace struct in the file)
they are addressed off `r1` and retail's frame comes out exactly. This is the whole reason the
struct exists.

**2. `SLdrAIHint` itself is not usable, for two independent reasons.**
`include/MetroidPrime/ScriptLoader/SLdrAIHint.hpp` has the right layout and its `.inc` the right
six case values (measured equal: `0x255a4580`, `0xb3127b71`, `0x78c507eb`, `0x19028099`,
`0x2c93aaf5`, `0xe7cf7950`, and mwcceppc builds the same six-comparison binary tree from them in
the same shape as retail's). But:
  - its destructor is **declared and defined empty** (line 28), so a `SLdrAIHint` local is torn
    down with *no call at all*, where retail's tail is
    `addi r3,r1,0x40 / li r4,-1 / bl __dt__20SLdrEditorPropertiesFv`. Writing out a destructor
    that calls `editorProperties.~SLdrEditorProperties()` is also wrong — mwcceppc then emits
    that call **twice**. The implicit destructor is the only spelling that emits it once;
  - its constructor writes the four floats as `0.0f` **literals**, which would put 4 bytes of
    `.sdata2` in an object of ours that owns none and shift every section after it. Retail
    loads them from its own `lbl_8041DB08` and so does this.

Fixing the generated header would fix both, but it is shared with the port and is generated by
`scripts/generate_script_loaders.py`, so the aggregate is restated locally and the item is one
file. **A future run that owns `SLdrAIHint.hpp` can fold this in** — see the `NEW:` line.

**3. `mgr.AllocateUniqueId()` and `LdrToTransform4f(...)` must stay in the argument list.**
Retail's order is `0x8022A254 AllocateUniqueId`, `0x8022A268 LdrToTransform4f`,
`0x8022A274 LdrToEntityInfo` — i.e. the transform (fifth argument) is called *before*
`LdrToEntityInfo` (fourth). Both other spellings are wrong, and measured:
  - `const TUniqueId uid = mgr.AllocateUniqueId();` as a statement gets the call order right but
    costs a **third** `TUniqueId` slot (r1+0x08, +0x0C *and* +0x10), pushing the transform to
    r1+0x14 and the block to r1+0x44 — 24 bytes over the claim (0x224 function, 0x224 vs 0x20C);
  - `const CTransform4f& transform = LdrToTransform4f(...)` as a statement calls
    `LdrToTransform4f` *before* `AllocateUniqueId`, the reverse of retail's.

### A claim I removed rather than shipped

I had written that `static_cast< u16 >` on the property count is load-bearing here (by analogy
with `CUnknown90.cpp:65-73`). **It is not, for this unit.** Removing it leaves the object's
`.text` byte-identical — same sha1, still 0x20C, `fn_8022A0C0` still in r27. The cast is kept
because every sibling loader carries it and it is harmless, but the header now says it was
*measured to be neutral here* rather than claiming it decides a register. Worth knowing before
anyone copies that node into another loader on the strength of `CUnknown90`'s comment.

### Two port-link failures on the way, both fixed properly

  - **`::operator new(0x178, str, nullptr)` does not compile off-mwcceppc.**
    `include/Kyoto/Alloc/CMemory.hpp:42` includes `<new>` on the host and declares no such
    overload, so `tools/link_check.sh` failed with *no matching function for call to
    `operator new(unsigned long, const char*, const char*)`*. Spelled
    `extern "C" void* __nw__FUlPCcPCc(uint, const char*, const char*)` and called directly
    instead — mwcceppc's own mangling of that operator, defined host-side at
    `src/Kyoto/Alloc/PortMwccNew.cpp:27`, and the same spelling as
    `src/MetroidPrime/PathFinding/CPathFindArea.cpp:40`.
  - **Three new undefined symbols** then showed up in the port link. Two are data and are now
    *defined* host-side under `#ifndef __MWERKS__`, the way `CUnknown90.cpp:44-49` defines
    `lbl_8041D648` and `Carve8022A060.c:174-183` defines `lbl_803B86B8`:
    `lbl_8041DB08` (`.sdata2`, 0.0f, read out of `main.elf` at `.sdata2` file offset `0x3c3c20`
    + `0x8041DB08 - 0x8041a3c0`) and `lbl_803AD338` (`.rodata`, `"??(??)"` + terminator, retail
    `size:0x7`, `symbols.txt:17570`, read at `.rodata` file offset `0x3a5038`). The third is
    `fn_8022A408` — real retail code in dtk's `auto_03_8022A3FC_text` with no source in this
    tree — so it joined `docs/research/port_link_gap_list.md` in the **unmangled** group beside
    `fn_80270A64` and `lbl_8041A3C0`, and that file's per-group count and the table in
    `port_link_gap.md:111` moved 53 → 54 with a dated entry. Defining `fn_8022A408` in a stub
    would have been the wrong answer: it is 0x13C bytes of real retail constructor.

## What the split did to the auto unit (measured, not recalled)

Before: `main/auto_03_8022A0C0_text` held `# 0x8022A0C0..0x8022A3F4 | size: 0x334` and **8**
functions. A claim at its head renames it: after the build the listing reads
`# 0x8022A2CC..0x8022A3F4 | size: 0x128` (**7** functions) as
`main/auto_03_8022A2CC_text`. `report_diff.py` reports it as
`SPLIT main/auto_03_8022A0C0_text: 8 function(s) accounted for across 2 new unit(s) in main
(exact count match - a split, not a loss)` — the verdict a rename would *not* get. Eight in,
eight out, and `total_functions` is still **28465**, the check that matters after a
`splits.txt` edit. The three cases are worth keeping apart: a claim at an auto unit's end only
shortens it (`docs/goal-notes/carve-80229eac.md`), a claim in the middle splits it
(`carve-8022a060`), and a claim over its first bytes renames it (this one).

## Verification

* `./tools/goal_check.sh build/goal/item.json` -> **`PASS carve-8022a0c0`**, every line `ok`:
  no judge-owned path touched; `gate.sh` (DOL sha1, 86 RELs vs config.yml, per-function diff,
  module wiring, docs claims, decl order, files.cmake, module order, port probe, port link gap
  and dups); `counts: matched 13606 -> 13607 linked 6654 -> 6655`; `check_symbol_names.py`;
  `flip_test`.
* `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022A0C0.cpp` -> `PASS -> kept as
  Matching`, `kept: 1 / 1  failed: 0  skipped: 0`. This is the real byte check: it flips
  `configure.py` to `NonMatching`, rebuilds and confirms the DOL still hashes to retail.
* `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022A0C0.cpp` -> `.text claimed 524  ours
  608  retail 524  over by 84`, and `extra: + 84
  __dt__Q227@unnamed@Carve8022A0C0_cpp@12SAIHintBlockFv` — the weak COMDAT destructor MW emits
  for the aggregate's scope exit, which mwldeppc drops and which is not in the DOL. Exactly
  `CUnknown90.cpp`'s 84 bytes of `__dt__16SLdrTimeKeyframeFv` over its claim. `flip_test`
  decides, and it passed.
* `./build/tools/objdiff-cli report generate` -> unit
  `main/MetroidPrime/ScriptLoader/Carve8022A0C0`: `fn_8022A0C0` **100.0%**, 524/524,
  `complete_units 1`.
* `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
* `./tools/probe_sources.sh` -> `probe: 1007 files, 0 failed, 0 errors; link: LINKED (287
  undefined, 0 duplicates)` — the baseline of 287, unchanged.
* `python3 tools/check_symbol_names.py` -> `checked 611 units; 0 declared names are missing`.
* `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve8022A0C0.cpp` ->
  `ok` (one function, so the descending-order rule cannot be violated).
* Comment-only edits after the final flip were confirmed not to change codegen: the object's
  `.text` sha1 is `e2250a3695a0d3bc10873818471e13871aaaed1b` both before and after.

## Left in the same unclaimed run (0x8022A2CC..0x8022A3F4, dtk's `auto_03_8022A2CC_text`, 7 functions)

`GetValueParm2__13CScriptAIHintCFv` (0x8022A2CC, 8 B), `GetValueParm__13CScriptAIHintCFv`
(0x8022A2D4, 8 B), `SetInUse__13CScriptAIHintFb` (0x8022A2DC, 36 B), `fn_8022A300` (0x8022A300,
56 B), `GetInUse__13CScriptAIHintCF9TUniqueId` (0x8022A358, 92 B), `fn_8022A394` (0x8022A394,
44 B, a clamp on +0x170) and `fn_8022A3C0` (0x8022A3C0, 52 B, an `AcceptScriptMsg__6CActor`
forwarder). All candidates; **filing one, not seven** — the 2026-09-29 batch of 49
lessons-and-walls is what happens when a lane files one per neighbour. `fn_8022A300` and
`fn_8022A394` look the most tractable, being leaf arithmetic on a member.

NEW: match | MetroidPrime/ScriptLoader/SLdrAIHint.hpp | SLdrAIHint's ctor writes the four floats as 0.0f literals (puts 4 bytes of .sdata2 in an object that owns none and shifts every later section; carve-8022a0c0 measured) and its ~SLdrAIHint() is declared empty, so a SLdrAIHint local emits no __dt__20SLdrEditorPropertiesFv where retail has one; fixing the generated header would let every AIHint loader use it