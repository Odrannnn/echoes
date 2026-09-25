# The 23 unidentified symbols, one row each

Measured 2026-09-26 by lane `e6` at commit `755a0ca`, from `build/G2ME01/main.elf` (a linked ELF
of the retail DOL), `config/G2ME01/symbols.txt`, `powerpc-eabi-objdump -d`, `powerpc-eabi-nm -n`,
and `dtk rel info` for the one REL module involved.

## What this file is, and why it is the deliverable

`docs/research/port_link_gap.md` has a row reading

> | unmangled: `fn_*`, `lbl_*`, globals | 23 | 3 unwritten functions and 20 `fn_*`/`lbl_*`
> nobody has identified |

and the count itself was wrong for a while: the row used to say 31, which included 8 game globals
and 3 unwritten functions the first measurement had already closed, so the prose and the generated
list quoted different numbers without anyone noticing. **A count of unidentified symbols is not
work.** An unidentified symbol cannot be planned for, and a lane pointed at one spends its budget
re-deriving what a caller already says. A row per symbol, with the evidence, turns the number into
a work list - and in this case 21 of the 23 turned out to be identifiable in a single pass, mostly
because the port's own call sites already carry the signature.

**Sizes are hexadecimal.** `config/G2ME01/symbols.txt` writes them as `size:0x…`, and a `B` in
that field is a hex digit: `fn_800CB764` is `size:0x90` = **144** bytes, not 90. The decimal
column below is computed, not transcribed.

**The method that worked here is the one in `docs/research/rel_loaders.md`**, applied to a much
easier target. For each symbol: take the address out of the gap list, take the range and the
claiming unit out of `tools/range_owner.py`, then (a) count the call sites in the whole DOL and
name what they are, (b) read the body, and (c) check whether the *caller* in `src/` already knows
the class. A static initialiser, a vtable, a caller's argument list and a mangled name are where an
`fn_*`'s name lives, and **two of the 23 here were not unidentified at all** - they are
symbols retail names, which the port was calling under invented names (rows 8 and 15).

## The table

| # | as the link reports it | demangled / real name | addr | size (hex / dec) | unit that claims it | what it is | evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `fn_80038624` | `CStateManager::…` - a dead debug one-shot render | `0x80038624` | `0x25C` / 604 | `MetroidPrime/CStateManager.cpp` (NonMatching) | Frames one actor: bounding-box centre, a `LookAt` transform, `CGraphics::SetViewPointMatrix`, `GetCurrentCamera`, `GetProjectionState`, one draw, then restores both. | **`docs/research/CStateManager_80038624.txt` already has a full region map**, a byte accounting and three independent proofs it is dead: its only caller is `fn_800388EC` (0x800388EC, itself uncalled), that caller does not set the `r4` the function dereferences, and its central lookup resolves into the middle of `CStateManager::m_scriptMsgs` using an index read from padding. **Not matchable** until `CStateManager`'s layout is repaired - the doc says so. |
| 2 | `fn_8004F770` | a `CWorld` method: touch the world's two resident models | `0x8004F770` | `0x58` / 88 | *unclaimed* | `if (this->x90) this->x8c->Touch(0); if (this->xA0) this->x9c->Touch(0);` and nothing else - 24 bytes of prologue and epilogue around two guarded `CModel::Touch(int)` calls with the literal `0`. | Two callers, both in the `CStateManager` unit and both passing `m_world`: `CStateManager::TouchSky` (0x80038570) and `fn_80039FBC` (0x80039FE8). `CModel::Touch(int)` is a named DOL symbol, so the callee is certain; the two `CModel*` and two flag bytes are not named in `include/MetroidPrime/CWorld.hpp`, which stops at `x4c_chainHead`. The port calls it from `CStateManager::TouchSky` (`src/MetroidPrime/CStateManager.cpp:762`). |
| 3 | `fn_800CB764` | **`CMorphBall::Get_800cb764()`** | `0x800CB764` | `0x90` / 144 | *unclaimed* | `CTransform4f::Translate(v) * CTransform4f(model->xf).GetRotation()`, where `v = (model->x54 + 1.5f, model->x58 + 1.5f, model->x5C + ball->x0C)` and `model = *(void**)ball`. | **The port already names it**: `include/MetroidPrime/Player/CMorphBall.hpp:8` declares `CTransform4f Get_800cb764() const;` and nothing defines it, so the port calls the address instead. The constant is `1.5f` read from `.sdata2:0x8041B308` (`3fc00000`), added to two of the three components. 13 call sites, all in the `MetroidPrime` player/script range (`fn_80011200`, `fn_800C406C`, `fn_800C50B4`, `fn_800C57E4`, `fn_800C6B78`, `fn_800C93F8`, `fn_800CA808`, `fn_800CAC74`, `fn_800CC258`) - the morph-ball transform is needed by everything that has to place Samus. |
| 4 | `fn_800E5C78` | `CGunEffect` - "touch every part of all three beams" | `0x800E5C78` | `0xA8` / 168 | `MetroidPrime/Player/CGunEffectTouchAll.cpp` (NonMatching) | Gate on bit 5 of the byte at `+0x14`; then either three calls to `fn_800E4E50` + `fn_80027AE8` (when `+0x10` is non-null) or three calls to `fn_800E4E9C` followed by a `CModel::Touch(part)` loop over `model->x1c` parts. | **Already written** - `src/MetroidPrime/Player/CGunEffectTouchAll.cpp` holds the body with a 12-variant account of the register allocation that blocks it. The gap exists only because that file is not in `files.cmake`, and adding it alone is **net +1**: it defines `fn_800E5C78` and references `fn_800E4E50`, `fn_800E4E9C` and `fn_80027AE8`, which nothing currently references. See "What closing these would cost" below. |
| 5 | `fn_800E5D80` | `CGunEffect` - "touch one part of all three beams" | `0x800E5D80` | `0x68` / 104 | `MetroidPrime/Player/CGunEffectTouch.cpp` (**Matching**) | The same bit-5 gate, then a three-iteration loop over `fn_800E5D20(self, i, part)`. `r4` (the `CStateManager&`) is never read. | **Already written and byte-exact**: `src/MetroidPrime/Player/CGunEffectTouch.cpp` is a `Matching` unit that claims `0x800E5D20..0x800E5DE8` and defines both `fn_800E5D20` and `fn_800E5D80`. The gap exists only because the file is not in `files.cmake`; the third parameter is documented as dead because `fn_800C122C` and `fn_801C5990` both pass the state manager and `0x800E5D80` never reads `r4`. Same net-+1 problem as row 4. |
| 6 | `fn_800E6AD0` | **`CModelData::CModelData()`** | `0x800E6AD0` | `0x98` / 152 | `MetroidPrime/CModelDataDefaultCtor.cpp` (NonMatching, 97.11%) | `CModelData`'s default constructor: scale `(1,1,1)` from a pooled `.sdata2` word, `xc_animData` cleared, four bit-fields (bits 25 and 26 set, 24 and 27 clear), `x18_ambientColor` = `CColor::White()`, and the valid flags of the three `rstl::optional_object` members cleared. | **Already identified and written**: `src/MetroidPrime/CModelDataDefaultCtor.cpp` holds the body and names the blocker - a `Matching` REL unit (`CScriptScriptStreamedMovie.cpp`) *defines* `__ct__10CModelDataFv` while *importing* `fn_800E6AD0`, so the DOL symbol cannot be renamed. **Closed anyway on the port side** (see below): the unit was not in `files.cmake`. |
| 7 | `fn_801C5990` | a `CGrappleArm` method - the grapple's "make the beam models resident" pass | `0x801C5990` | `0x80` / 128 | *unclaimed* | `if (!x298_flags) return;` then `fn_800E5C78(this + 0x2C)`, then, if `this->x8c` or `this->xa4`, `fn_800E5D80(this + 0x7C, mgr, 0)`. The second guard is MWCC's `x8c or xa4` reduced to a `bool` in `r3` and then tested with `clrlwi. r0,r3,24; bne`, which is why the disassembly reads as a `!= 0` on a value that can only be 0 or 1.` | `+0x298` is `CGrappleArm::x298_flags` in `include/MetroidPrime/Player/CGrappleArm.hpp:29`, and the whole body is the same two functions rows 4 and 5 are built from, on the sub-object the grapple keeps at `+0x2C` - the same `+0x2C` the `CGunWeapon` sibling uses. Two callers, both in `fn_801D18D0` (0x801D1914, 0x801D1988), the function the port's own comments identify as `CPlayerGun`'s teardown. |
| 8 | `fn_801CA0F8__10CPlayerGunFv` | a `CPlayerGun` member (the port called it `fn_801CA0F8`) | `0x801CA0F8` | `0x2D0` / 720 | `MetroidPrime/Player/CPlayerGun.cpp` (NonMatching) | `CPlayerGun::GetPlayer(mgr, x)`, a float compare on `player->x1294`, a `CToken` built on the stack, a 10-argument factory (`fn_80100928`, which is retail's 10-argument `CToken`/particle emitter), then `fn_80041E60` and a 16-bit store through the first argument. | **The name says the class**: `config/G2ME01/symbols.txt:7448` is `fn_801CA0F8__10CPlayerGunFv`, and `__10CPlayerGunFv` is dtk's demangling of a CodeWarrior symbol whose parameter list the map did not record - so `Fv` does **not** mean "no arguments"; retail passes `r4` and `r5` and the body reads both. Two callers: `fn_801C97DC__10CPlayerGunFv` (0x801C97F0) and `fn_801CA3C8` (0x801CA70C). |
| 9 | `fn_801D9F90` | **`CGunWeapon::Touch(const CStateManager&)`** | `0x801D9F90` | `0x88` / 136 | *unclaimed* | `if (mgr.fn_80036F10()) return;` then, if `this->x78`, and if bit 5 of the byte at `+0x271` is set, `fn_800E5D80(this + 0x2C, mgr, x250)`; then, if `this->x118`, `fn_800E5D80(this + 0xCC, mgr, x11C)`. | The port's `include/MetroidPrime/Weapons/CGunWeapon.hpp:126-127` already declares the pair `Touch(const CStateManager&)` / `TouchHolo(const CStateManager&)`, and `src/MetroidPrime/Weapons/CGunWeaponTouch.cpp` implements retail `0x801D9F5C` as `TouchHolo`. `0x801D9F90` is the other one, and the sibling's comment (`src/MetroidPrime/Weapons/CGunWeaponTouch.cpp:14-26`) already measured the shared shape: three sub-objects of one type at `+0x2C` / `+0x7C` / `+0xCC`, each 0x50 apart, each with its "is it loaded" flag at `+0x4C`. `x250` is `CGunWeapon::x250_shaderIdx`. Four callers, three in `fn_801D18D0`, one in `fn_801CE0DC__10CPlayerGunFR13CStateManager`. |
| 10 | `fn_801EBBC8` | **not identified** - a list walk calling vtable slot 4 | `0x801EBBC8` | `0x5C` / 92 | *unclaimed* | `for (node = r4; node; node = *(void**)((char*)node + 0x60)) node->vtable[0x14/4](r3);` - a singly-linked list whose nodes carry a vtable, one argument per call, the link at node `+0x60`. | **Exactly one caller in the whole DOL**: `fn_80039B1C` (0x80039B1C, 32 bytes, `stwu/mflr/stw/bl/lw…/blr`, no register setup), which is called once, from `fn_80039248` at 0x800399C8 with `r3 = ` its first argument and `r4 = ` a word in its own frame that `fn_80039E54` produced. **Ruled out:** it is not a `CEntity` walk (`CStateManager::m_graveyard` is `rstl::list<reserved_vector<CEntity*,32>>` and its link is an `rstl::list` node, not a `+0x60` self-pointer); it is not `CFrustumPlanes`/`CPlane` (`CPlane` is `CHECK_SIZEOF(…, 0x10)` and `CFrustumPlanes` is `0x64` of six `CPlane`, no vtable). **Still open:** the node type, and therefore what slot 4 is. |
| 11 | `fn_802275B8` | one `CGameOptions::unk2` element, **written** | `0x802275B8` | `0x6C` / 108 | *unclaimed* | `out.WriteBits(p->first != 0, 1); out.WriteBits(p->second != 0, 1);` | Two `WriteBits__16CBitStreamWriterFUiUi` calls, one per byte, each preceded by MWCC's `neg/or/srwi` triple for `x != 0`; no branch. One caller: `CGameOptions::PutTo`, in the four-element loop at `src/MetroidPrime/Player/CGameOptions.cpp:146`. `unk2` is a `reserved_vector<rstl::pair<bool,bool>,4>`. **Closed** (below). |
| 12 | `fn_80227624` | one `CGameOptions::unk2` element, **read** | `0x80227624` | `0x70` / 112 | *unclaimed* | `p->first = in.ReadBits(1) != 0; p->second = in.ReadBits(1) != 0;` | Retail returns it through `r3` (a two-byte `rstl::pair<bool,bool>`, sret), and the second `ReadBits`' result is stored to `r3+1` before the epilogue with `r3` reloaded - i.e. by value. One caller: `CGameOptions::CGameOptions(CBitStreamReader&)` at `src/MetroidPrime/Player/CGameOptions.cpp:119`, the mirror of row 11. **Closed** (below). |
| 13 | `fn_8029AF00` | a 10-entry global `{int key; uchar value;}` table: set by key, allocate if absent | `0x8029AF00` | `0x80` / 128 | *unclaimed* | Two loops over the table at `.bss:0x80415288`: the first finds `key == r3` and stores `r4` at `+4`; if it falls off the end, the second finds the first record whose key is `-1` and claims it. | The count word is `0x80415288` and the records start at `0x8041528C`, 8 bytes each - which the table's own constructor settles: `fn_8029FA60` (0x8029FB7C) does `li r0,10; stwu r0,21128(r3)` with `r3 = 0x80410000`, then fills ten records at 8-byte stride with one repeated `{key, value}` pair. Four call sites in three callers, passing either `127` (`0x7F`, the max 7-bit volume) or a volume: `CGameOptions::SetSfxVolume` (0x8016108C), `fn_8029B81C` (0x8029B8A4), `fn_8029C378` (0x8029C468, 0x8029C6AC). The last two also call `fn_8034066C(0x804152DC, key)`, a *different* table 0x54 bytes further on, so the pair is "look up, then set" over two tables. |
| 14 | `fn_802CB608` | a **cubic root finder** (`CMayaSpline`'s only user) | `0x802CB608` | `0x310` / 784 | `Kyoto/Math/RMathUtils.cpp` (NonMatching) | Solves `c0 x³ + c1 x² + c2 x + c3 = 0` in doubles with a tolerance and a root buffer; the prologue is a cascade of `fabs` / `fcmpo` against the tolerance for the three degenerate cases, and the epilogue writes the count of real roots. | **One caller in the DOL**: `CMayaSpline::FindSegmentIntersections` (0x80328C04) passes `coefs[3], coefs[2], coefs[1], coefs[0], FLT_EPSILON, roots` - leading coefficient first, which fixes the argument order the port already uses at `src/Kyoto/Math/CMayaSpline.cpp:579`. `r3` is the leading coefficient (saved to `r31` at 0x802CB63C), and `f5` is the tolerance, both confirmed at 0x802CB638-0x802CB664. The port's own comment, "Polynomial solvers; original GameCube names have not been established", is still true of this one. |
| 15 | `fn_802CC064` | **`CMath::SolveQuadratic(float, float, float, float&, float&)`** | `0x802CC064` | `0xBC` / 188 | `Kyoto/Math/RMathUtils.cpp` (NonMatching) | Retail already names it. | `config/G2ME01/symbols.txt:12947` reads, in full, `SolveQuadratic__5CMathFfffRfRf = .text:0x802CC064; // type:function size:0xBC`. The CodeWarrior mangling spells the two out-params `Rf` (reference-to-float), which is what makes the identity certain rather than plausible. **The port was calling it under a name that exists in no binary** while `CMath::SolveQuadratic` sat defined in `src/Kyoto/Math/RMathUtils.cpp:173`, a file already in `files.cmake`. **Closed** (below). |
| 16 | `fn_803111A4` | a three-way rotate of a pending-work list | `0x803111A4` | `0xAC` / 172 | *unclaimed* | `++*(0x80419BE8);` then, unless the byte at `0x80418C20` is set, walk the list at `0x80419BE4` calling either `fn_802BB5C8()` or `fn_80311F48(node)` per node (chosen by comparing `node->x14->x28->x1c` against the node itself) and clearing `+0x18`/`+0x1C`; then shift three `.sbss` words up one slot - `lwz r4,-24992(r13); lwz r3,-24996(r13); stw r4,-24988(r13); stw r3,-24992(r13); stw r0,-24996(r13)` - i.e. `0x80419BDC <- 0`, `0x80419BE0 <- old 0x80419BDC`, `0x80419BE4 <- old 0x80419BE0`, a three-slot ring. | One caller: `fn_8003EC0C__13CStateManagerFv` (0x8003EC18), which the port has as `CStateManager::fn_8003EC0C` and which does exactly `fn_803111A4(); gpSimplePool->Flush();` (`src/MetroidPrime/CStateManager.cpp:711-714`). The second caller is `src/MetroidPrime/main.cpp:238`. The `+0x60`-less node has a vtable at `+0x14->+0x28`, so it is a two-level indirection, not a flat node. The four `.sbss` words are at `0x80419BDC`, `0x80419BE0`, `0x80419BE4` and `0x80419BE8` - all four are `-24996`, `-24992`, `-24988` and `-24984` off `_SDA_BASE_` - and none of them is named in `symbols.txt`; `0x80418C20` is `lbl_80418C20`. |
| 17 | `fn_8033CEE8` | a private initialiser of `CGameArchitectureSupport`'s constructor | `0x8033CEE8` | `0x148` / 328 | *unclaimed* | Builds a 4-entry × 32-byte table in `.bss:0x80417CE0` (`lbl_80417CE0`, 0x84 bytes - and 4 + 4×32 = 0x84 exactly), freeing whatever was there; then allocates **two 0x11E00-byte blocks** with `CCallStack` breadcrumbs and installs them in 32-byte slots; then calls `CMemory::OffsetFakeStatics(0x23C00)`, and `0x23C00 == 2 × 0x11E00`. | **One caller in the DOL**: `CGameArchitectureSupport::CGameArchitectureSupport(COsContext*)` at 0x80007FA8, i.e. it is constructor tail work and not reachable any other way. The table's shape is settled by its own two private helpers, which nothing else calls: `fn_8033D0CC` (0x8033D0CC) default-constructs one 32-byte entry into the caller's frame, and `fn_8033D030` (0x8033D030) - the only caller of both - does `container + container->count*32 + 4`, copy-constructs through `fn_8033D078`, then `++container->count`. The two allocation sizes are literals in the body (`lis r30,1; addi r3,r30,7680` = 0x11E00) and the `OffsetFakeStatics` argument is `2 × 0x11E00`, so the two blocks are one arena pair. The `CCallStack` arguments are code addresses (`0x80230228`, `0x8022EAB8`), which is what `CCallStack`'s `lineStr`/`type` pair is for. |
| 18 | `fn_8033D2EC` | **the `std::exception` RTTI accessor** - returns `&__RTTI__Q23std9exception` | `0x8033D2EC` | `0x8` / 8 | *unclaimed* | Two instructions: `lwz r3,-28920(r13); blr`. | `-28920` off `r13` is `_SDA_BASE_ = 0x8041FD80`, giving `0x80418C98`, and `config/G2ME01/symbols.txt:20243` reads `__RTTI__Q23std9exception = .sdata:0x80418C98; // type:object size:0x8`. Three callers, all using the value as a plain pointer: `CMain::RsMain` (0x80005CAC, tests bit 29 and calls `LCEnable` if clear), `fn_802C11C0` (0x802C11D4, stores it and four offsets from it - 964, 1928, 3856, 5784 - into five consecutive `.sbss` words at `0x80419938`), and `ShouldEnableLockedCache__Fv` (0x80319FE8, `(r3 - 0xE0000000 | 0xE0000000 - r3) >> 31`). **Closed** (below). |

### The five `lbl_57_rodata_*` - not functions, and already half-named

All five are `.rodata` of **`ScriptCannonBall.rel`**, module 57, and the port *already references
four of them by name* from `src/MetroidPrime/ScriptObjects/CScriptCannonBall.cpp:16-21` and
`:61-68`. The gap is that nothing defines them. `dtk rel info
orig/G2ME01/files/RelProd/ScriptCannonBall.rel` puts `.rodata` at file offset **0x14E0**, size
0xB4, so the objects are the first 0x20 bytes of that section and the bytes are already in
IEEE-754 order:

| as the link reports it | addr in the module | size | value | what the port uses it for |
| --- | --- | --- | --- | --- |
| `lbl_57_rodata_0` | `.rodata:0x0` | `0x4` | `3f800000` = **1.0f** | the reset value of `CScriptCannonBall::m_f`, and the "1.0" first argument of `CScriptEffect` |
| `lbl_57_rodata_4` | `.rodata:0x4` | `0x4` | `00000000` = **0.0f** | the `m_f > 0` / `m_f < 0` clamp in `Think` |
| `lbl_57_rodata_8` | `.rodata:0x8` | `0x4` | `3e800000` = **0.25f** | the `dt / 0.25f` decay divisor in `Think` |
| `lbl_57_rodata_C` | `.rodata:0xC` | `0x4` | `437f0000` = **255.0f** | declared at `CScriptCannonBall.cpp:19` and never referenced, so it is *not* on the gap list |
| `lbl_57_rodata_10` | `.rodata:0x10` | `0x4` | `40000000` = **2.0f** | the second `CScriptEffect` scale argument |
| `lbl_57_rodata_14` | `.rodata:0x14` | `0x1C` | `3f3f283f` then `"CannonBall Effect"` | `rstl::string_l(lbl_57_rodata_14 + 7)` at `CScriptCannonBall.cpp:64` |

`lbl_57_rodata_14` is the interesting one and the check that makes the row evidence rather than a
guess: dtk gives it `size:0x1C` and no type, because a float and a string literal sit back to back
with no padding between them, so the object runs from `+0x14` to the next symbol. **"CannonBall
Effect" begins at `+0x14 + 7`**, which is exactly the `+ 7` the port already writes - five
independent uses of the same `+7` and the same string, from the port and the module agreeing.

**All five are defined on the port side**, in `src/MetroidPrime/PortGlobals.cpp`. A REL module's
`.rodata` cannot be claimed by a `Matching` unit without also reproducing the module's hash, and
no unit claims it.

## Summary: 22 of 23 identified, 10 of them closed

| | count |
| --- | --- |
| identified with evidence | **22** |
| of those, **closed** (a real definition now exists) | **10** |
| identified, closure needs decompilation or a missing helper | 12 |
| **not identified** | **1** (`fn_801EBBC8`, with what I ruled out above) |

22 + 1 = 23. The one that stayed unidentified is named, sized, given its single caller and its
exact loop shape, so the next lane starts from that rather than from the address.

## What was closed, and how

Ten symbols went from MISSING to defined. Every one of them is a **port-side** definition - none
adds a definition to a `configure.py` unit, so none can shift a unit's SDA offsets or move a
`NonMatching` object's `.text`, which is the failure mode `PortGlobals.cpp`'s own header and
`LANE.md` both warn about.

| symbol | addr | size | what it turned out to be | evidence (one line) | where the definition went |
| --- | --- | --- | --- | --- | --- |
| `lbl_57_rodata_0` | `.rodata+0x0` (file 0x14E0) | 4 | `1.0f` | `3f800000` at file 0x14E0 | `src/MetroidPrime/PortGlobals.cpp` |
| `lbl_57_rodata_4` | `.rodata+0x4` | 4 | `0.0f` | `00000000` | same |
| `lbl_57_rodata_8` | `.rodata+0x8` | 4 | `0.25f` | `3e800000` | same |
| `lbl_57_rodata_10` | `.rodata+0x10` | 4 | `2.0f` | `40000000` | same |
| `lbl_57_rodata_14` | `.rodata+0x14` | `0x1C` | a float then `"CannonBall Effect"` | the string starts at `+7`, the offset the port already used | same |
| `fn_800E6AD0` | `0x800E6AD0` | 152 | `CModelData::CModelData()` | already written at 97.11% in `CModelDataDefaultCtor.cpp`; the file was simply not in `files.cmake` | added `src/MetroidPrime/CModelDataDefaultCtor.cpp` to `files.cmake` |
| `fn_802275B8` | `0x802275B8` | 108 | one `CGameOptions::unk2` element, written | two `WriteBits` calls and MWCC's `neg/or/srwi` for `!= 0` | `src/MetroidPrime/PortGlobals.cpp` |
| `fn_80227624` | `0x80227624` | 112 | one `CGameOptions::unk2` element, read | two `ReadBits` calls, result stored as bytes, returned by value through `r3` | same |
| `fn_802CC064` | `0x802CC064` | 188 | `CMath::SolveQuadratic` - **retail names it** | `symbols.txt:12947`; the port was calling a non-existent `fn_802CC064` while `CMath::SolveQuadratic` was defined in a file already in `files.cmake` | the call in `src/Kyoto/Math/CMayaSpline.cpp` now uses `CMath::SolveQuadratic`; the bogus `extern "C"` is gone |
| `fn_8033D2EC` | `0x8033D2EC` | 8 | returns `&__RTTI__Q23std9exception` | one `lwz` off `_SDA_BASE_` = `0x80418C98` = that symbol | `src/MetroidPrime/PortGlobals.cpp` |

One more change is **not** a closure but fixes a real defect: `fn_801CA0F8` was the port's own name
for `fn_801CA0F8__10CPlayerGunFv`, a symbol that exists in retail's map but in no binary. The
declaration and the one call site in `src/MetroidPrime/Player/CPlayerGun.cpp` now use retail's
name. The gap count is unchanged; what changes is that the list now names a symbol a lane can
grep for in `symbols.txt` and in the DOL disassembly.

Measured: `python3 tools/link_gap.py --rebuild`, **559 MISSING -> 549 MISSING**, and every entry it
reported as stale or new is accounted for above. `docs/research/port_link_gap_list.md` was
regenerated with `--write-list`, not hand-edited.

## What closing these would cost - the trap, measured

Three of the remaining 13 are **already written in the tree** and are missing only because their
files are not in `files.cmake`. Wiring them in does not move the gap the way it looks like it
should:

| file | status | defines | references | net |
| --- | --- | --- | --- | --- |
| `src/MetroidPrime/Player/CGunEffectTouch.cpp` | **Matching** | `fn_800E5D80`, `fn_800E5D20` | `fn_800E4E50`, `fn_800E4E9C`, `fn_80027B44` | **+2** |
| `src/MetroidPrime/Player/CGunEffectTouchAll.cpp` | NonMatching | `fn_800E5C78` | `fn_800E4E50`, `fn_800E4E9C`, `fn_80027AE8` | **+1** |
| both | | 3 | 4 | **+1** |

`fn_800E4E50`, `fn_800E4E9C`, `fn_80027B44` and `fn_80027AE8` are **not on the gap list today**
because nothing in the port build references them. They are all `UNCLAIMED` in
`config/G2ME01/splits.txt`, and they are four small functions: two beam-holder selectors and two
"touch part N of a model" loops. Writing those four is what turns rows 4, 5, 7 and 9 from a net
loss into a net win of four - and it is the single cheapest remaining move in this group, because
the two consumers are already byte-exact or written.

This is the same shape as the point `docs/research/port_link_gap.md` makes under "A gap list is not
a closed set of work": **the list is what is reachable, not what is left.** Three of the 23 were
files that exist and compile nowhere.

## The other negative results, so the next lane does not repeat them

- **`fn_80038624` is not a 9,675-byte mystery.** `docs/research/CStateManager_80038624.txt` measured
  it: `size:0x25C` = 604 bytes, and it is dead code that cannot be matched until
  `CStateManager`'s layout is repaired. Two earlier documents quoted 9,675, which is
  `0x8003ABF0 - 0x80038624 - 1` - a gap-to-next-symbol computation applied to one symbol instead
  of 39.
- **`objdump -b binary` mis-disassembles this toolchain's PowerPC.** `build/binutils/
  powerpc-eabi-objdump -d -b binary -m powerpc` decoded `94 21 ff e0` (`stwu r1,-32(r1)`) as
  `psq_l f7,404(r31)`. Every address in this file comes from
  `powerpc-eabi-objdump -d` on `build/G2ME01/main.elf`, where the section and symbol table make the
  instruction stream alignable. Do not disassemble raw slices with `-b binary`.
- **`r13` and `r2` are the two small-data bases and they are different.** `r13 = _SDA_BASE_ =
  0x8041FD80` and `r2 = _SDA2_BASE_ = 0x804223C0`. Row 18's whole identification is one `lwz` off
  `r13`; reading it against `r2` gives `0x80417458`, a plausible address in the wrong section that
  is not the RTTI.
- **`config/G2ME01/symbols.txt` sizes are hexadecimal and `B` is a digit.** `size:0xBC` is 188.
  Rows 3, 15 and 18 would all be misreported by a decimal reading.
- **`dtk rel info` is how a REL module's section offsets are found.** The `.rodata` of
  `ScriptCannonBall.rel` is at file offset 0x14E0; `config/G2ME01/rels/ScriptCannonBall/symbols.txt`
  addresses are *section-relative*, so adding them to 0x14E0 is what produces the values in the
  table above. Getting this wrong puts every float one or two words out and yields denormals.
- **Two of the 23 were never unidentified at all** - they are symbols retail names
  (`SolveQuadratic__5CMathFfffRfRf`, `fn_801CA0F8__10CPlayerGunFv`) that the port was calling under
  invented names. Check `symbols.txt` **by address**, not by the `fn_` name the link reports, before
  treating anything in this group as unknown.
