# What the port still needs in order to link

Measured 2026-09-25 with `python3 tools/link_gap.py`. Regenerate the numbers with
`--list`; the list at the bottom is the ratchet the tool checks, so a symbol appearing that
is not listed here fails, and a listed symbol that is no longer missing also fails.

## Why this had to be measured

The port builds its game sources as an **OBJECT library**, so there is no link step and
therefore nothing that ever reports what is missing. `MP_SDK_HEADERS_ONLY` is on by default
and `PORT_NOTES.md` records it as the only verified configuration. "The game does not link"
was true and unquantified.

`tools/link_gap.py` compiles `mp_game` for the host, subtracts what the objects define from
what they reference, and classifies the remainder. Of **1376 undefined symbols**, 722 are
the C++ runtime, 23 are libc, 106 appear in Aurora's own sources, 1 (`AIStartDMA`) only in an
Aurora header, and **63 are genuinely unaccounted for**. Those 63 are the work.

## The four kinds of thing that is actually missing

**1. Functions nobody has written (30).** Declared `extern "C"` by the port's own sources and
called, with no definition anywhere. Each needs a body, and the decompilation is where it
comes from - which is why the two halves of this repository are the same project. Where
retail names the function, the port is calling it under a `fn_` name and the rename is the
first step.

Two of these are on the port's blocking path by name: **`CreateFrameEnd__7MakeMsgF…`
(0x800489AC, 204 bytes) is called by `CGameArchitectureSupport::Update`**, and
`SolveQuadratic__5CMathFfffRfRf` (0x802CC064, 188 bytes) by `CMayaSpline`. The largest is
`fn_80038624` at **9,675 bytes**, in `CStateManager`; the smallest are 8 bytes.

**2. Retail globals declared `extern` and never defined (20).** The subtle class, and the one
that is correct in the decompilation and impossible in a PC link. This is valid C++:

```cpp
extern "C" int lbl_80419A10;   // CStateManager.cpp:33
lbl_80419A10 = x16a8;          // and assigned at :501
```

For the decompilation that is right: retail's own objects define those symbols and the DOL
links against them. There is no retail binary in a PC link, so every one of them needs a real
definition somewhere, holding the value the retail binary has. **A PC link is what finally
forces the decompilation's data to be complete**, and this is the list of where it is not.

**3. Game globals and constants (8).** `gpRender`, the four `gpTweak*` pointers, and
`kInvalidUniqueId` / `kInvalidAreaId` / `kInvalidEditorId`.

**4. The REL module runtime (6).** `REL_loader_CannonBall` and five `lbl_57_rodata_*` labels -
module 57's loader and its rodata. This is port code (`platform/rel.cpp`), not decompilation.

Plus one oddity: `BuildTime`, which is a build timestamp the retail binary carries.

## What this does not tell you

The 106 symbols attributed to Aurora's sources are attributed because the identifier appears
in a file under `extern/aurora/lib`. That is strong evidence, not proof - a name in a source
file is not the same as a definition in an object. **The authoritative answer is an actual
link**, and the only symbol where the distinction is already known to bite is `AIStartDMA`,
which appears in an Aurora *header* and in none of its sources. Do not treat the 106 as
resolved until a link has succeeded.

## The list

- `BuildTime`
- `REL_loader_CannonBall`
- `fn_8001D658`
- `fn_80038624`
- `fn_8003C054`
- `fn_800489AC`
- `fn_8004F770`
- `fn_800C08D4`
- `fn_800CB764`
- `fn_800E5C78`
- `fn_800E5D80`
- `fn_800E6AD0`
- `fn_80142520`
- `fn_8015B9B0`
- `fn_801C5990`
- `fn_801CA0F8`
- `fn_801D9F5C`
- `fn_801D9F90`
- `fn_801EBBC8`
- `fn_801ECD8C`
- `fn_802275B8`
- `fn_80227624`
- `fn_80227694`
- `fn_8029AF00`
- `fn_8029EFCC`
- `fn_802CB608`
- `fn_802CC064`
- `fn_8030184C`
- `fn_803111A4`
- `fn_8033CEE8`
- `fn_8033D2EC`
- `gpRender`
- `gpTweakContents`
- `gpTweakGame`
- `gpTweakPlayerA`
- `gpTweakPlayerGun`
- `kInvalidAreaId`
- `kInvalidEditorId`
- `kInvalidUniqueId`
- `lbl_57_rodata_0`
- `lbl_57_rodata_10`
- `lbl_57_rodata_14`
- `lbl_57_rodata_4`
- `lbl_57_rodata_8`
- `lbl_803DFA8C`
- `lbl_80418D00`
- `lbl_80418FB8`
- `lbl_80418FBC`
- `lbl_804191E0`
- `lbl_80419730`
- `lbl_80419745`
- `lbl_804199CC`
- `lbl_80419A10`
- `lbl_80419A18`
- `lbl_80419A98`
- `lbl_80419A9C`
- `lbl_80419AA0`
- `lbl_8041A8BC`
- `lbl_8041A8D0`
- `lbl_8041D248`
- `lbl_8041D394`
- `lbl_8041D398`
- `lbl_8041E2E6`
