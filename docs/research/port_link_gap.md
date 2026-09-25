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
what they reference, and classifies the remainder. Of **1369 undefined symbols**, 722 are
the C++ runtime, 23 are libc, 106 appear in Aurora's own sources, 1 (`AIStartDMA`) only in an
Aurora header, and **44 are genuinely unaccounted for**. Those 44 are the work.

> The first measurement said 63. The 19 that closed are the retail globals below, and closing them
> turned up two miscounts worth recording: the split of 63 was **29** unwritten functions, **19**
> retail globals, 8 game globals, 6 REL symbols and `BuildTime` - the text said 30 and 20, which
> sums to 64.

## The kinds of thing that is actually missing

**1. Functions nobody has written (29).** Declared `extern "C"` by the port's own sources and
called, with no definition anywhere. Each needs a body, and the decompilation is where it
comes from - which is why the two halves of this repository are the same project. Where
retail names the function, the port is calling it under a `fn_` name and the rename is the
first step.

Two of these are on the port's blocking path by name: **`CreateFrameEnd__7MakeMsgF…`
(0x800489AC, 204 bytes) is called by `CGameArchitectureSupport::Update`**, and
`SolveQuadratic__5CMathFfffRfRf` (0x802CC064, 188 bytes) by `CMayaSpline`. The largest is
`fn_80038624` at **9,675 bytes**, in `CStateManager`; the smallest are 8 bytes.

**2. Retail globals declared `extern` and never defined - CLOSED, all 19.** This was the subtle
class, and the one that is correct in the decompilation and impossible in a PC link. This is
valid C++:

```cpp
extern "C" int lbl_80419A10;   // CStateManager.cpp:33
lbl_80419A10 = x16a8;          // and assigned at :501
```

For the decompilation that is right: retail's own objects define those symbols and the DOL
links against them. There is no retail binary in a PC link, so every one of them needed a real
definition somewhere, holding the value the retail binary has. **A PC link is what finally
forces the decompilation's data to be complete**, and this was the list of where it was not.

All 19 are now defined in `src/MetroidPrime/main.cpp`, under one `extern "C"` block, with the
value read out of `build/G2ME01/main.elf` at the address `config/G2ME01/symbols.txt` gives.
Three things that reading needs to be said once:

- **The width is the retail instruction's, not dtk's `size:`.** dtk's `size:` is the gap to the
  next symbol, so `lbl_80419A10` claims 8 bytes and `lbl_8041A8BC` is the only one whose gap
  equals its type. `objdump -d -r build/G2ME01/obj/<unit>.o` gives the truth per symbol:
  `lhz`/`lwz`/`lfs`/`stb`/`stw` pin `lbl_8041E2E6` to two bytes and `lbl_80419A98` to one.
- **A symbol in `.bss` or `.sbss` has no contents in the ELF at all**, so its value at load is
  0 and the definition is a zero fill. That is 13 of the 19 - not a guess, and not a value
  recovered from anywhere else.
- **The port's GXVtxDescList is 8 bytes, retail's is 2.** `lbl_803DFA8C` is retail's 0xDC bytes
  = 110 entries, so the count is written out as 110 rather than computed from `sizeof`, which
  would silently give 27 in the port build and 110 in the matching one.

Two C++ traps cost real time here and are worth writing down:

- **GCC drops an uninitialised tentative definition that nothing in the TU reads.** Every one of
  these needs an explicit `= 0` (or `= {}`), or the symbol is silently absent from the object
  and the link is no better off.
- **Inside an `extern "C" { }` block GCC gives a `const` declaration internal linkage** unless
  it also says `extern`. A `const` retail global defined that way vanishes, because nothing
  references it. Six of the 19 are `const`.

`lbl_8041D394` / `lbl_8041D398` are the one place the retail *value* is not portable: they are
`.sdata2` words holding 0x803AADF2 and 0x803AADFC, which are the `.rodata` addresses of
`"ShotSmoke"` and `"Power2nd_1"`. A 64-bit host link cannot hold a guest address, so the
definition is the string itself - which is what `CPowerBeam::Unk9` does with it.

**3. Game globals and constants (8).** `gpRender`, the four `gpTweak*` pointers, and
`kInvalidUniqueId` / `kInvalidAreaId` / `kInvalidEditorId`.

**4. The REL module runtime (6).** `REL_loader_CannonBall` and five `lbl_57_rodata_*` labels -
module 57's loader and its rodata. This is port code (`platform/rel.cpp`), not decompilation.
These are the same class as the 19 above (declared where used, never defined) and belong to
whoever wires module 57, not to the decompilation.

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
- `fn_8004F770`
- `fn_800C08D4`
- `fn_800CB764`
- `fn_800E5C78`
- `fn_800E5D80`
- `fn_800E6AD0`
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