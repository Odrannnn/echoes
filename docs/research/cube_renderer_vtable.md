# `CCubeRenderer`'s vtable, all 82 slots, measured

Written 2026-09-26 by lane `display`, from `config/G2ME01/symbols.txt` and
`build/G2ME01/main.elf`. Every address below is read out of the linked ELF; none is recalled.

`__vt__13CCubeRenderer` is `.data` **0x803B8C10, size 0x150 = 344 bytes**: two header words
(offset-to-top, typeinfo) and **82 function slots**. Slot *i* lives at byte `8 + 4*i`, so
`lwz r12,N(r12)` on a vtable pointer names slot `(N - 8) / 4`. Slot 0 is
`__dt__13CCubeRendererFv` (0x80270848), which is the check that the two header words are
header words: the destructor is always slot 0.

Reproduce it with:

```sh
build/binutils/powerpc-eabi-objdump -s --start-address=0x803B8C10 --stop-address=0x803B8D60 \
  build/G2ME01/main.elf
```

## Why this exists: it answers `docs/research/boot_path.md` step 21c

`boot_path.md` step 21c says, of the frame loop's draw call:

> **Re-measured 2026-09-26 by lane e2 and still not identified**: nothing in
> `config/G2ME01/symbols.txt` or `include/MetaRender/` names vtable slot +0x94, and
> `fn_80049244` is still unwritten.

**Slot +0x94 is `CCubeRenderer::BeginScene`** (0x8026FBFC, 0x180 = 384 bytes) - slot 35. The
instruction is at 0x800061BC:

```
800061b8:  lwz   r3,-27272(r13)    ; gpRender
800061bc:  lwz   r12,0(r3)         ; vptr
800061c0:  lwz   r12,148(r12)      ; 148 = 0x94 = slot 35 = CCubeRenderer::BeginScene
800061c4:  mtctr r12
800061c8:  bctrl
```

So the first frame of the port's frame loop calls `BeginScene`, not a draw: that is one more
reason step 21c's "the first frame renders nothing" conclusion is right, and the slot is no
longer unknown.

**The other half of step 21c is a different subsystem and should not be listed with it.**
`fn_80049244` (0x80049244, 0x118) walks a linked list of `rstl::rc_ptr<CIOWin>`
(`lwz r30,0(r3)` then `lwz r30,12(r30)`), and for each entry calls
`CopyInto(CRcPtrData*)`, then vtable slot 4 (offset 0x18) and slot 2 (offset 0x10) of the
`CIOWin`, then `rc_ptr<CIOWin>::ReleaseData`. It is the IOWin close/flush path, not a render.
It belongs with `CIOWinManager` in correction 3 of `boot_path.md`, not with the draw.

## The table

`->` marks a slot this tree's `include/MetaRender/CCubeRenderer.hpp` **does not** declare with
the signature retail mangles. Those are the header's remaining falsehoods, and each one is a
`Matching` unit that cannot be written until it is fixed.

| slot | offset | retail address | retail symbol | header |
| --- | --- | --- | --- | --- |
| 0 | 0x008 | 0x80270848 | `__dt__13CCubeRendererFv` | ok |
| 1 | 0x00C | 0x8026FE94 | `AddStaticGeometry` | `->` wrong args |
| 2 | 0x010 | 0x802638FC | `EnablePVS` | ok |
| 3 | 0x014 | 0x802638A8 | `DisablePVS` | ok |
| 4 | 0x018 | 0x80264194 | `UnkA` | ok |
| 5 | 0x01C | 0x8026FE10 | `RemoveStaticGeometry` | ok |
| 6 | 0x020 | 0x8026411C | `DrawUnsortedGeometry` | ok |
| 7 | 0x024 | 0x8026F214 | `DrawSortedGeometry` | ok |
| 8 | 0x028 | 0x8026402C | `DrawStaticGeometry` | ok |
| 9 | 0x02C | 0x8026A368 | `DrawAreaGeometry` | ok |
| 10 | 0x030 | 0x802640A4 | `PostRenderFogs` | ok |
| 11 | 0x034 | 0x80263FB8 | `UnkB` | ok |
| 12 | 0x038 | 0x80263EAC | `UnkC` | ok |
| 13 | 0x03C | 0x8026B9FC | `UnkD` | ok |
| 14 | 0x040 | 0x8026FDEC | `SetModelMatrix` | ok - **`Matching`**, `src/MetaRender/Carve8026FDEC.cpp` |
| 15 | 0x044 | 0x8026FAA4 | `AddParticleGen` | ok |
| 16 | 0x048 | 0x8026FA60 | `AddParticleGen2` | ok |
| 17 | 0x04C | 0x8026F86C | `AddPlaneObject` | `->` 2nd arg is `CVector3f`, not `CAABox` |
| 18 | 0x050 | 0x8026F7FC | `AddDrawable` | ok |
| 19 | 0x054 | 0x8026E7E4 | `SetDrawableCallback` | ok - **`Matching`**, `Carve8026E7F0.cpp` |
| 20 | 0x058 | 0x8026FD7C | `SetWorldViewpoint` | `->` takes `const CTransform4f&` |
| 21 | 0x05C | 0x8026EC20 | `SetPerspective(float x6)` | `->` **overload**, see below |
| 22 | 0x060 | 0x8026EC00 | `SetPerspective(float x5)` | `->` **overload**, see below |
| 23 | 0x064 | 0x8026EA2C | `SetViewportOrtho` | ok |
| 24 | 0x068 | 0x8026E9B8 | `SetViewport` | ok |
| 25 | 0x06C | 0x8026E990 | `SetDepthReadWrite` | ok - **`Matching`**, `Carve8026E7F0.cpp` |
| 26 | 0x070 | 0x8026E960 | `SetBlendMode_AdditiveAlpha` | ok - **`Matching`** |
| 27 | 0x074 | 0x8026E930 | `SetBlendMode_AlphaBlended` | ok - **`Matching`** |
| 28 | 0x078 | 0x8026E900 | `SetBlendMode_NoColorWrite` | ok - **`Matching`** |
| 29 | 0x07C | 0x8026E8D0 | `SetBlendMode_ColorMultiply` | ok - **`Matching`** |
| 30 | 0x080 | 0x8026E8A0 | `SetBlendMode_InvertDst` | ok - **`Matching`** |
| 31 | 0x084 | 0x8026E870 | `SetBlendMode_InvertSrc` | ok - **`Matching`** |
| 32 | 0x088 | 0x8026E840 | `SetBlendMode_Replace` | ok - **`Matching`** |
| 33 | 0x08C | 0x8026E810 | `SetBlendMode_AdditiveDestColor` | ok - **`Matching`** |
| 34 | 0x090 | 0x8026E78C | `SetDebugOption` | `->` takes `(EDebugOption, int)` |
| 35 | 0x094 | 0x8026FBFC | **`BeginScene`** | ok. **This is `boot_path.md` step 21c's slot +0x94** |
| 36 | 0x098 | 0x8026FB80 | `EndScene` | ok |
| 37 | 0x09C | 0x8026EE0C | `BeginPrimitive` | ok. See "the five `Begin*` methods" below |
| 38 | 0x0A0 | 0x8026EDE4 | `BeginLines` | ok. **blocked**, see below |
| 39 | 0x0A4 | 0x8026EDBC | `BeginLineStrip` | ok. **blocked** |
| 40 | 0x0A8 | 0x8026ED94 | `BeginTriangles` | ok. **blocked** |
| 41 | 0x0AC | 0x8026ED6C | `BeginTriangleStrip` | ok. **blocked** |
| 42 | 0x0B0 | 0x8026ED44 | `BeginTriangleFan` | ok. **blocked** |
| 43 | 0x0B4 | 0x8026ECF8 | `PrimVertex` | ok. **unwritten** (writes 0xCC008000) |
| 44 | 0x0B8 | 0x8026ECDC | `PrimNormal` | ok - **`Matching`**, `Carve8026ECDC.cpp` |
| 45 | 0x0BC | 0x8026EF30 | `PrimColor(float x4)` | ok - **`Matching`**, `Carve8026EF24.cpp` |
| 46 | 0x0C0 | 0x8026EF24 | `PrimColor(const CColor&)` | ok - **`Matching`** |
| 47 | 0x0C4 | 0x8026EC78 | `EndPrimitive` | ok. **unwritten**: a do/while over **slot 43**, i.e. `PrimVertex(lbl_804174B0)` |
| 48 | 0x0C8 | 0x8026EC54 | `SetAmbientColor` | ok - **`Matching`**, `Carve8026EC54.cpp` |
| 49 | 0x0CC | 0x80262D3C | `DrawString` | `->` takes `(const char*, int, int)` |
| 50 | 0x0D0 | 0x8026E7F0 | `GetFPS` | ok - **`Matching`**, `Carve8026E7F0.cpp` |
| 51 | 0x0D4 | 0x8026E5BC | `CacheReflection` | `->` takes 3 args |
| 52 | 0x0D8 | 0x8026E588 | `DrawSpaceWarp` | `->` takes `(const CVector3f&, float)` |
| 53 | 0x0DC | 0x8026B45C | `fn_8026B45C` (1084 B) - unnamed | `->` |
| 54 | 0x0E0 | 0x8026B2B8 | `fn_8026B2B8` (420 B) - unnamed | `->` |
| 55 | 0x0E4 | 0x8026364C | `fn_8026364C` (604 B) - unnamed | `->` |
| 56 | 0x0E8 | 0x80262E04 | `fn_80262E04` (480 B) - unnamed | `->` |
| 57 | 0x0EC | 0x80267ED8 | `fn_80267ED8` (384 B) - unnamed | `->` |
| 58 | 0x0F0 | 0x802679DC | `fn_802679DC` (104 B) - unnamed | `->` |
| 59 | 0x0F4 | 0x80267A44 | `fn_80267A44` (1172 B) - unnamed | `->` |
| 60 | 0x0F8 | 0x8026DD94 | `SetWireframeFlags(int)` | `->` header says no args |
| 61 | 0x0FC | 0x8026DD5C | `SetWorldFog(ERglFogMode, float, float, const CColor&)` | `->` header says no args |
| 62 | 0x100 | 0x8026CED8 | `fn_8026CED8` (148 B) - unnamed | `->` |
| 63 | 0x104 | 0x8026BE18 | `fn_8026BE18` (48 B) - unnamed | `->` |
| 64 | 0x108 | 0x80267214 | `fn_80267214` (1568 B) - unnamed | `->` |
| 65 | 0x10C | 0x80264978 | `fn_80264978` (1556 B) - unnamed | `->` |
| 66 | 0x110 | 0x80265D70 | `fn_80265D70` (3572 B) - unnamed | `->` |
| 67 | 0x114 | 0x8026A9C0 | `fn_8026A9C0` (2296 B) - unnamed | `->` |
| 68 | 0x118 | 0x8026DD20 | `GetStaticWorldDataSize` | ok |
| 69 | 0x11C | 0x8026A338 | `fn_8026A338` (48 B) - unnamed | `->` |
| 70 | 0x120 | 0x8026A300 | `fn_8026A300` (56 B) - unnamed | `->` |
| 71 | 0x124 | 0x80263D14 | `fn_80263D14` (408 B) - unnamed | `->` |
| 72 | 0x128 | 0x80266B94 | `fn_80266B94` (76 B) - unnamed | `->` |
| 73 | 0x12C | 0x80266B64 | `fn_80266B64` (48 B) - unnamed | `->` |
| 74 | 0x130 | 0x8018B9D4 | `fn_8018B9D4` - **not in this class's range at all** | `->` |
| 75 | 0x134 | 0x80264F8C | `fn_80264F8C` (3556 B) - unnamed | `->` |
| 76 | 0x138 | 0x80263550 | `fn_80263550` (252 B) - unnamed | `->` |
| 77 | 0x13C | 0x80262FE4 | `fn_80262FE4` (1388 B) - unnamed | **byte offset 316 is what `CStateManager::fn_80039CCC` dispatches on** |
| 78 | 0x140 | 0x00000000 | *pure virtual* | |
| 79 | 0x144 | 0x00000000 | *pure virtual* | |
| 80 | 0x148 | 0x80273FA4 | `@4@__dt__13CCubeRendererFv` | |
| 81 | 0x14C | 0x80273F9C | `@4@AddParticleGen` | |

## The header's declaration order is not retail's vtable order, and that is a finding

**From slot 4 onwards the two disagree, and it is not a small disagreement.** Retail's
slots 4-13 are ten `fn_*` functions, slots 53-59 and 62-67 and 69-77 are twenty more, and
this tree's `include/MetaRender/CCubeRenderer.hpp` fills those positions with invented names
(`UnkA`, `UnkB`, ..., `UnkL`, `Something`, `DrawThermalModel`, `SetThermal`, and so on). So
**the header's `Unk*` names are not identified with retail's slots at all**, and the apparent
agreement at 21 named slots is partly luck: `SetWireframeFlags` is declared 30th in the
header and sits at retail slot 60.

The consequence for a lane writing any of those bodies is blunt: **it cannot write them from
the header's declaration order.** It has to start from the retail address above, or from the
slot, and be prepared to correct the header's order as it goes. It also means the `316` in
`fn_80039CCC` is a byte that happens to match today, not a slot anyone has checked - objdiff
compares relocation-free bytes in an unlinked object, so it can confirm the offset and say
nothing about which function belongs there.

## Retail's two `SetPerspective` are one name, overloaded

Slots 21 and 22 are `SetPerspective__13CCubeRendererFffffff` and
`SetPerspective__13CCubeRendererFfffff` - **the same name with six and five `float`s**. So the
base class has two virtuals whose names collide, which C++ allows (each gets its own slot) and
this tree's header does not: it spells them `SetPerspective1` and `SetPerspective2`, the second
with no parameters at all. Fixing it means overloading the name in both `IRenderer.hpp` and
`CCubeRenderer.hpp`, and MWCC 2.7 with `-lang=c++` is C++98 - whether it accepts two virtual
overloads in one class is **not tested**, and it is the first thing to try for
0x8026EC00..0x8026EC54 (84 bytes, 2 functions).

The 52-byte one is not a forwarder: it divides `f2` by `f3`, scales by a `.sdata` float at
0x80418AE8, and shuffles `f3`/`f4` before calling `fn_802C235C`.

## The five `Begin*` methods: blocked on a virtual call, not on a body

`BeginLines`, `BeginLineStrip`, `BeginTriangles`, `BeginTriangleStrip` and `BeginTriangleFan`
(0x8026ED44..0x8026EE0C, 200 bytes, 5 functions) are one contiguous run, all five bodies are
`BeginPrimitive(<type>, nverts)`, all five are already declared correctly in the header, and
all five **reach 100.00%** - in a unit that cannot be linked, because `BeginPrimitive` *is*
virtual (slot 37) and mwcceppc emits `bctrl` where retail has `bl`.

Three fixes were tried:

| fix | direct call? | cost |
| --- | --- | --- |
| drop `virtual` from `IRenderer::BeginPrimitive` | yes | **removes a vtable slot.** `CStateManager::fn_80039CCC`'s `lwz r12,316(r12)` (slot 77) becomes `312`, and `CActor::RenderInternal`, `CPlayerGun::fn_801D0CD0`, `fn_801D0D10`, `CScriptForgottenObject::RenderInternal` all drop off 100.00% |
| hide the base with a non-virtual `CCubeRenderer::BeginPrimitive` of a different signature | yes | slot count survives, but the derived function then does not fill slot 37, which is wrong about retail |
| `class CCubeRenderer final` | would be | **mwcceppc 2.7 `-lang=c++` rejects `final`** as a syntax error, with and without `#pragma cpp_extensions on` |

Retail had every `CCubeRenderer` definition in one translation unit, where mwcceppc
devirtualises a call whose definition it can see. A carve cannot: it would have to carry
`BeginPrimitive`'s own 0x118 bytes. So the five wait for a lane that writes `BeginPrimitive`
(0x8026EE0C) with them - which is one unit, 0x200 bytes, 6 functions, and needs no header
change beyond what has already landed.

## `CCubeRenderer`'s measured layout

Measured with mwcceppc's own flags via `tools/probe_cc.sh` and `objdump -s`, not a host
`sizeof`:

| offset | member | evidence |
| --- | --- | --- |
| 0x00 | vptr | slot 0 is `__dt__13CCubeRendererFv` |
| 0x04 | `x4_pad[0x94]` | the old header's `x8_pad` name was four bytes out |
| 0x98 | `TDrawableCallback x98_drawableCallback` | `stw r4,152(r3)` at 0x8026E7E4 |
| 0x9C | `const void* x9c_drawableContext` | `stw r5,156(r3)` at 0x8026E7E4 |
| 0x314 | three `bool : 1` | the old `x318_` names were four bytes out too |
| 0x34C | `CColor x34c_color` | `stw r0,844(r3)` / `addi r3,r3,844` at 0x8026EF24, 0x8026EF30 |
| 0x350 | `CVector3f x350_normal` | `stfs f0,848(r3)` at 0x8026ECDC |
| 0x35C | `sizeof(CCubeRenderer)` | measured; retail's own `sizeof` is **not** measured |

`PrimVertex` reads all three of 0x350/0x354/0x358 and `PrimColor` writes 0x34C, so retail's
class is at least 0x35C and this header is now exactly that. What is *not* claimed is that
the padding is padding.
