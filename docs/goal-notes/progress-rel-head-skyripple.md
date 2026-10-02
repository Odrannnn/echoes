# progress-rel-head-skyripple

Item `progress-rel-head-skyripple`, target `module:SkyRipple`, lane 3, 2026-09-30.
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-rel-head-skyripple`**
(gate ok, counts 10184 -> 10187, linked 4972 -> 4972, `target rose: module:SkyRipple:
12 -> 15 / 20 functions`, no asm added).

## The measured position before the change

`build/report.json` on the clean tree: the module has two units.

| unit | matched / total | note |
| --- | --- | --- |
| `SkyRipple/MetroidPrime/ScriptObjects/CScriptSkyRipple` | 7 / 15 | `NonMatching`, 34.75% fuzzy |
| `SkyRipple/REL/REL_Setup` | 5 / 5 | complete, 100% |

So the module sat at **12 / 20**. `config/G2ME01/rels/SkyRipple/splits.txt` already splits
the module in two: `CScriptSkyRipple.cpp` claims `.text 0x0..0x8CC` and `REL/REL_Setup.cpp`
claims `0x8CC..0xA70`. The `REL_Setup` half was already done, so the head unit was the whole
of the remaining work, and the `progress` judge sums `matched_functions` over every unit under
`SkyRipple/`.

**The `reason` in `item.json` is wrong about one thing and worth correcting for the next run:**
it says the module has "no own-code unit yet". It has had one since commit `6937ed2`
("decompile: SkyRipple's script-object methods, 7 exact"). The head unit exists, is
`NonMatching`, and 7 of its 15 functions are exact. The remaining 8 are the item's real content.

## What I did

Three of the six unmatched functions in the head unit, all in
`src/MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp`:

| function | retail | before | after |
| --- | --- | --- | --- |
| `fn_70_264` (`CScriptSkyRipple::PreThink`) | `0x264`, 204 bytes | 0.00% | **100.00%** |
| `fn_70_78C` (`ClearFluidList` slot) | `0x78C`, 60 bytes | 0.00% | **100.00%** |
| `fn_70_6F0` (static helper) | `0x6F0`, 156 bytes | 0.00% | **100.00%** |
| `fn_70_658` (`AcceptScriptMsg` slot) | `0x658`, 152 bytes | 0.00% | **99.87%** |

The vtable order (from the `.data` relocations in
`build/G2ME01/SkyRipple/obj/MetroidPrime/ScriptObjects/CScriptSkyRipple.o`) names the slots:
`0x264` is `PreThink`, `0x78C` is the entry between `ClearFluidList` and `AddToRenderer`
(i.e. `PreRender`), `0x658` is `AcceptScriptMsg`. All three are `extern "C"` free functions
taking `self` first, matching how `fn_70_60` was already written in this file, because
MWCC's mangled names for these vtable entries are the unnamed `fn_70_*` forms in
`config/G2ME01/rels/SkyRipple/symbols.txt`.

`fn_70_6F0` is not a vtable entry; it is the static helper `AcceptScriptMsg` calls twice. It
returns the id of the connected `CScriptActor` if that actor is render-only, and
`kInvalidUniqueId` otherwise, and it sets the `mSkipRendering` bit on the way.

## Three spellings worth recording

**1. `TUniqueId` by value needs a stack temporary, and the compiler adds a second store
if you read through the accessor.** `fn_70_78C` and `fn_70_264` both pass `TUniqueId` by
value. Retail has one `lhz` from the member and one `sth` into the outgoing argument slot.
Writing `mgr.fn_80037A04(self->GetUniqueId())` gives *two* `sth` instructions (the return
value is materialised, then copied into the argument slot) and 0x40 bytes instead of 0x3C.
Reading the member through a reference gives the single store:

```cpp
mgr.fn_80037A04(*reinterpret_cast< TUniqueId* >(reinterpret_cast< uint* >(self) + 2));
```

**2. `subis` vs `addis` for a 16-bit-shifted range test.** Retail's `fn_70_658` opens with
`subis r0, r3, 0x5841` / `cmplwi r0, 0x4c44`, i.e. `(msg - 0x58410000) <= 0x4C44` unsigned.
I tried nine spellings (`- 0x5841u`, `- 'XALD'`, `>= && <=`, `(v & 0xffff0000) == ... &&
(v & 0xffff) <= ...`, `static_cast<ushort>`, an explicit `uint` local, the enum type, and
two more) and **every one of them emitted `addis` with a negative immediate, never `subis`**.
That single instruction is the whole 0.13%. It is not a logic error: `addis r0, r3, -22593`
computes the same 32-bit value. The spelling that *does* produce `subis` in this tree is a
plain `==` against a 4-character literal - see `src/MetroidPrime/CMainFlow.cpp:55`
(`GetGameModeType() == 'SNGL'`, Matching, retail `subis r0, r4, 0x534e` / `cmplwi r0, 0x474c`).
MWCC picks `subis` when the comparison is an equality whose constant it can see split into
halves, not for a range. **`fn_70_658` is one instruction short; do not spend another run
re-deriving this.**

**3. `FindConnectedObject` returns through the same `TUniqueId` convention.** In `fn_70_6F0`
the call sequence is `addi r3, r1, 0xc` / `bl FindConnectedObject` / `lhz r0, 0xc(r1)`, so
the header's `TUniqueId FindConnectedObject(const CStateManager&, EScriptObjectState,
EScriptObjectMessage) const` already produces it. The message argument is `kSM_Attach`
(0x41544348), materialised with `lis`/`addi` from the enum, and the state argument comes from
`.data`.

## The two `.data` labels are not free

`lbl_70_data_0` and `lbl_70_data_4` are `EScriptObjectState` values in the module's `.data`
(the four bytes `IS00` and `IS01`), read with `lis`/`lwz` - not immediates. Declaring them
`extern` and reading them costs **two undefined symbols in the port link**, because
`files.cmake:942` compiles this file into the host build:

```
GATE FAIL: probe link-gap
  link_check: STRICT FAIL - regression gate: 252 undefined against a baseline of 250 (GREW)
```

`goal_check` fails the item on that even with every count green. **A REL unit's `.data`
constant that the source reads needs a real definition in the source**, with the right value,
and it is not `const` (retail puts it in `.data`, not `.rodata`):

```cpp
extern "C" EScriptObjectState lbl_70_data_0 = static_cast< EScriptObjectState >(0x49533030);
extern "C" EScriptObjectState lbl_70_data_4 = static_cast< EScriptObjectState >(0x49533031);
```

After that, `link_check: unique undefined symbols 250` / `unchanged from baseline`. This is a
general trap for REL-module work, not specific to this module: a `.data` read in a REL source
is a new port symbol unless it is defined.

## What is still unmatched, measured

`./tools/decomp_build.sh CScriptSkyRipple` on the final tree:

```
SkyRipple/MetroidPrime/ScriptObjects/CScriptSkyRipple: 60.15% fuzzy, 35.17% matched (10 / 15 functions)
   __ct__16CScriptSkyRippleF9TUniqueIdRC11CEntityInfoRC20SLdrEditorProperties  72.54%  228 bytes
   fn_70_658                                             99.87%  152 bytes
   fn_70_4D0                                              0.00%  392 bytes
   fn_70_330                                              0.00%  416 bytes
   REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo  90.18%  272 bytes
```

Three left in the head unit, and they are the next item's work:

- **`fn_70_4D0` (392 bytes, the `Render` slot) and `fn_70_330` (416 bytes, its static
  helper)** - the `Render` pair. `fn_70_4D0` is nearly all `CGraphics` calls
  (`DisableAllLights`, `White`, `gpRender->vtable+0xC8`, `GXSetColorUpdate`, `GetFog`,
  `SetFog`, `SetDepthRange`, `SetDepthWriteMode`) and `fn_70_330` reads a `CColor` at
  `lbl_70_rodata_0` and calls `CModelData::Render`. Two `CColor` stack temporaries are built
  field-by-field in both, which is where the bytes will go.
- **`__ct__` (72.54%)** - the constructor, and the documented technique for these
  (mem-init list vs body) is in `docs/RUNNING_THE_DECOMP.md` under "A technique that works on
  the generated loader structs". It also calls `fn_70_8AC`, the REL-local forwarder to
  `CModelData`'s DOL default constructor, which is already written.
- **`REL_LoadSkyRipple` (90.18%)** - the loader, already 90%; a small rearrangement.

## Gates, all re-measured on the final tree

```
sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs, 0 mismatched (sha1 vs config.yml)
python3 tools/check_symbol_names.py           checked 505 units; 0 declared names are missing
./tools/goal_check.sh build/goal/item.json    PASS progress-rel-head-skyripple
link_check: unique undefined symbols 250, 0 duplicates, unchanged from baseline
```

The module's own sha1 is unaffected by construction: `CScriptSkyRipple` is `NonMatching`, so
`build.ninja` links the dtk split object `build/G2ME01/SkyRipple/obj/.../CScriptSkyRipple.o`,
not our compiled one. The head unit is **not** promotable yet - it still has 4 functions
under 100% and 2 of them at 0% - and per the one rule I did not try to flip it. A flip would
put our object in the module link and the 0% functions would remove bytes the hash needs.

## Note for the next run

`python3 tools/check_decl_order.py --unit CScriptSkyRipple` reports the unit **permuted** -
functions are declared ascending by retail offset, and MWCC emits in reverse source order.
This is pre-existing (`docs/research/decl_order.md` already lists `SkyRipple` among the 18),
and it is harmless while the unit is `NonMatching` and the dtk split object is what links.
**It becomes the blocker the moment the unit is promotable**, so whoever finishes the last
three functions must reorder the file descending by retail offset before running
`flip_test.sh`; objdiff will sit at 100% and the module hash will break on a few bytes with
nothing else reporting it.

## Review rejected run 10 (2026-09-30 09:53:41Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`fn_70_658` (src/MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp:133) does not mean what retail does. Retail's `subis r0,r3,0x5841` / `cmplwi r0,0x4c44` / `bne` is an equality test, so the body runs only for `msg.GetMessage() == kSM_XALD` (0x58414C44); the source's unsigned `<= 0x4c44u` emits `bgt` (bytes 0x68c/0x68d, `41 81` vs `40 82` — the only real difference in the function) and runs it for the whole 0x58410000..0x58414C44 range, i.e. 0x4C45 messages instead of one. That is a semantic change hiding behind a 99.87% objdiff score, and the note's claim that the residual difference is the `subis`/`addis` mnemonic is wrong (identical encoding), so it would mislead the next attempt. An acceptable change writes the comparison as the equality retail encodes — `if (msg.GetMessage() == kSM_XALD)` — which is also the spelling the note itself says produces MWCC's `subis`, and keeps the two `.data` state arguments as they are; the other three functions in this diff are sound and can stay.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-rel-head-skyripple-L3-10.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-rel-head-skyripple-L3-10-review1-20260930T095012.jsonl

## Fix round 1 (2026-09-30, worker)

One hunk changed, exactly as the reviewer asked. In
`src/MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp` the body of `fn_70_658` was gated on

```cpp
if (static_cast< uint >(msg.GetMessage()) - 0x58410000u <= 0x4c44u) {
```

which is a **range** test: it admitted 0x4C45 messages sharing kSM_XALD's top half, and
MWCC emitted `bgt` (`41 81`) where retail has `bne` (`40 82`). It is now the equality
retail encodes, and the comment above it says so:

```cpp
if (msg.GetMessage() == kSM_XALD) {
```

`kSM_XALD` is `0x58414c44` (`include/MetroidPrime/CEntityInfo.hpp:212`), so the test is the
single message retail selects. The two `.data` connection states (`lbl_70_data_0`,
`lbl_70_data_4`) and the three other functions in the diff are untouched.

**The fix also closed the function**, which the note above had written off:

- Before: `fn_70_658` 99.87% (152 bytes), unit 10 / 15, module 12 / 20, total 10187.
- After: `fn_70_658` **100.00%** (152 bytes), unit 11 / 15, module **16 / 20**, total **10188**.

Verified: the emitted compare is now `addis r0, r3, -22593` / `cmplwi r0, 19524` /
`bne 0x268` against retail's `3C 03 A7 BF` / `28 00 4C 44` / `40 82 00 48` - identical
bytes, including the branch.

**Correction to the note's "Three spellings" section 2.** Its conclusion was right but its
diagnosis was wrong. MWCC does *not* refuse `subis` on a range test; the nine spellings all
failed because they asked for the wrong test. `subis`/`addis` are the same four bytes here
(`3c 03 a7 bf` - GNU objdump calls it `addis`, the dtk dump calls it `subis`), and the
branch encoding is the only thing that differed. Written as an equality against the named
enum constant, the compiler reproduces both instructions exactly. So the earlier wall line
"`fn_70_658` is one instruction short; do not spend another run re-deriving this" is
withdrawn - it is not short, it was wrong.

Gates on the corrected tree:

```
./tools/goal_check.sh build/goal/item.json    PASS progress-rel-head-skyripple
  counts: matched 10184 -> 10188, linked 4972 -> 4972
  target rose: module:SkyRipple: 12 -> 16 / 20 functions, no asm added
python3 tools/check_raw_offsets.py             ok: 153 raw-offset site(s) in 62 file(s)
```

The unit is still not promotable - `__ct__` 72.54%, `REL_LoadSkyRipple` 90.18%, and
`fn_70_330` / `fn_70_4D0` at 0% - and the decl-order warning in "Note for the next run" above
still applies before any `flip_test.sh`.
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-rel-head-skyripple-L3-10-review1-20260930T095012.jsonl

## Run L9 (2026-10-02) - goal_check PASS, module 16 -> 17 / 20

Changed `src/MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp` (+ docs/research/port_link_gap.md, port_link_gap_list.md regenerated).
- `fn_70_4D0` (retail 0x4D0) now 100%, matched on first write: DisableAllLights, gpRender->SetAmbientColor(CColor::White()),
  GXSetColorUpdate(false), CGX::GetFog/SetFog(GX_FOG_NONE..), SetDepthRange(0.99999988f twice), view-matrix translation,
  two `fn_70_330` calls on copies of x158_/x15a_ (TUniqueId copied to a local; passed by reference), SetDepthWriteMode(true,LEqual,true),
  SetDepthRange(0.125f,1.f), restore fog, SetModelMatrix(Identity).
- `REL_LoadSkyRipple` 90.18% -> 99.71%: `new CScriptSkyRipple(mgr.AllocateUniqueId(), LdrToEntityInfo(info, props), props)` (global CMemory operator new,
  one null check) and `input.Get<uint>()` for the property id. Side effect: `lbl_70_rodata_C` is no longer referenced, link-gap entry removed (279).
- `fn_70_330` written, 90.02%: structure right, residual is a third CModelFlags temp at r1+8 in retail (else branch reads its uninitialised x0 from 12(r1)), frame 128 vs 112,
  and retail reloads `lfs f4` from the pool address (we fold to fmr). Tried: named const local, helper returning by value, ctor-direct, pointer indirection (all <= 90.02%).
- Ctor 73.89% unchanged (analysed only).

WALL: REL_LoadSkyRipple 99.71% - only diff is the new() result in r28 (retail) vs r29 (ours); measured against count-in-block, result local, id/entityinfo locals,
  int/uint/u16/short count, while-loop, down-counting loop (worse, 95%). None moved it.
WALL: fn_70_330 90.02% - third CModelFlags temp / f4 reload, see above.
NOTE: source order is ascending by retail offset like the rest of the file; reverse before any flip_test.
