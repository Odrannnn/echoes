# match-cworldshadow — `MetroidPrime/CWorldShadow` is now `Matching` (flip_test PASS)

**`./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS match-cworldshadow`**
(`ok flip_test MetroidPrime/CWorldShadow.cpp: PASS, Object(Matching) in configure.py`;
`ok counts: matched 11424 -> 11424   linked 5537 -> 5544`;
`ok gate.sh`, `ok check_symbol_names.py`, DOL sha1 `6ef9b491…`, all 86 RELs).

Not `STALE:` — the item arrived with `CanRender` at 72.96% and the unit `NonMatching`. It arrived
*also* with a `partials: 1`: an earlier run had already put all 7 functions at 100% but could not
keep the flip (see `docs/goal-notes/progress-prime1-cworldshadow.md`). This run is the rest of that
work: three link-level causes, none of them a matching problem.

## Starting position, measured on the clean tree (`c3ab6a92`)

```
main/MetroidPrime/CWorldShadow: 100% fuzzy, 100% matched (7 / 7 functions), complete: false
configure.py:497  Object(NonMatching, "MetroidPrime/CWorldShadow.cpp")
./tools/flip_test.sh MetroidPrime/CWorldShadow.cpp
    build failed:
      FAILED: [code=1] build/G2ME01/main.elf
      ### mwldeppc.exe Linker Error:
      #   undefined: 'CGraphics::mDepthNear'
      #     Referenced from 'CWorldShadow::BuildLightShadowTexture(...)' in CWorldShadow.o
      ### mwldeppc.exe Linker Error:
      #   undefined: 'CGraphics::mDepthFar'
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491…)
```

So the previous run's note was right that "the unit still needs `match` work on its own arity": the
7 functions matched, the object did not link.

## Cause 1 — `CGraphics::GetDepthNear()/GetDepthFar()` name two statics nothing defines

`CGraphics::mDepthNear` is `.sbss:0x80419968` and `CGraphics::mDepthFar` is `.sdata:0x80418AF0`, but
in retail both are **unnamed** (`lbl_80419968` / `lbl_80418AF0` in `config/G2ME01/symbols.txt`) and
defined by dtk's filler objects `build/G2ME01/obj/auto_10_80419910_sbss.o` and
`auto_09_80418AD4_sdata.o`, which **no unit in `configure.py` compiles**. `src/Kyoto/Graphics/
DolphinCGraphics.cpp` is where the C++ members are defined and it is a `NonMatching` unit, so retail's
object (which references them unnamed) is linked and nothing carries the C++ names. Read straight
out of the file with `build/binutils/powerpc-eabi-nm build/G2ME01/src/MetroidPrime/CWorldShadow.o`
— the only two undefined data symbols on the whole object.

Which two words they are, from retail's writer: `tools/dis.sh 0x802BFA38 0xA0` =
`CGraphics::SetDepthRange` has `stfs f5,-25624(r13)` / `stfs f6,-29328(r13)`, and
`tools/sda.py s:-0x6418` / `s:-0x7290` resolve them to 0x80419968 / 0x80418AF0.

Fix: read the retail symbols directly (`src/MetroidPrime/CWorldShadow.cpp`):

```cpp
extern "C" float lbl_80419968; // CGraphics::mDepthNear
extern "C" float lbl_80418AF0; // CGraphics::mDepthFar
```

and use them in place of the two `CGraphics::GetDepth*()` calls. This is the repo's existing
convention for unnamed retail data (`src/MetroidPrime/mainMid.cpp:130`, `PortBoot.cpp:97`). **Do not
"fix" it by defining the statics** — that would duplicate dtk's `.sbss`/`.sdata` bytes and shift
every later address. `CProjectedShadow.cpp:81` and `Weapons/CDecal.cpp:355` have the same latent
problem; both are `NonMatching`, so they cost nothing today.

## Cause 2 — one unreferenced weak copy that mwldeppc kept, shifting `.text` by 84 bytes

After cause 1 the DOL linked, but **every** checksum and all 86 RELs went off. Cause, measured by
diffing `powerpc-eabi-nm` of the linked ELF clean vs flipped: `__dt__Q24rstl42vector<6CLight,
Q24rstl17rmemory_allocator>Fv` landed at 0x800E22EC and pushed `CanRender` and 13,860 other symbols
0x54 bytes later.

`unit_fit.sh` listed four extra weak destructor instantiations (336 bytes) and said only `flip_test`
decides. The measured split is finer: **mwldeppc dropped three of the four and kept the
`rstl::vector<6CLight>` one.** The mechanism is in `docs/RUNNING_THE_DECOMP.md` ("The same holds for
weak *functions* named `fn_`"): mwldeppc deletes a duplicate weak copy only when an **earlier**
object defines the *same name*. Retail's copy of this destructor is real and is called from this
function — our `bl` at object offset 0x59c is `R_PPC_REL24
__dt__Q24rstl42vector<6CLight,Q24rstl17rmemory_allocator>Fv` — but retail's symbol table has no
name for it, so dtk called it `fn_80038F0C` at 0x80038F0C in `MetroidPrime/CStateManager.o`. Same
84 bytes (`bl …(ptr, -1)`, the MWCC destructor flag; `0x54` = the same length as our copy).

**Config change (one line, `config/G2ME01/symbols.txt`):**

```
-fn_80038F0C = .text:0x80038F0C; // type:function size:0x54
+__dt__Q24rstl42vector<6CLight,Q24rstl17rmemory_allocator>Fv = .text:0x80038F0C; // type:function size:0x54
```

`MetroidPrime/CStateManager.cpp` is `NonMatching`, so its retail object is linked and nothing needs
renaming in our source. Only `build/G2ME01/obj/MetroidPrime/CStateManager.o` changes, and its
`.text` bytes do not move (0x80038F0C before and after). `total_functions` is unchanged (28465).
Only prose mentions the old name — `src/MetaRender/Carve80270848.cpp:55,70` and
`docs/goal-notes/progress-prime1-cpausescreen.md:69` — left as they are.

The other three (`__dt__10CPVSVisSetFv`, `__dt__14CFrustumPlanesFv`,
`__dt__Q24rstl21single_ptr<8CTexture>Fv`) are unreferenced and mwldeppc drops them; that is the same
"CAi carries 224 bytes of these and still flips" case, and `unit_fit.sh`'s list cannot tell the two
kinds apart.

## Cause 3 — `rs_new`'s own `.rodata`, appended to the global string pool

After cause 2 only `build/G2ME01/main.dol` failed and all 86 RELs passed. `diff`ing the flipped DOL
against the clean one (**both produced by this tree**, so the clean one is retail's bytes — it is
the tree that reproduces `6ef9b491…`):

```
num diffs 1
0xdf1db: retail b'\xb0'  ours b'\xc0'
```

One byte, the immediate of `addi r0,r7,…` in `__ct__12CWorldShadowFUiUib`, i.e. the address
`rs_new` passes to `operator new(size_t, const char*, int)`. The symbol map showed the flipped build
putting `lbl_803A8AC0` in the ELF's `.rodata`, and the linked ELF listed every address identical
afterwards. Retail's constructor passes 0x803A8AB0 — the pooled `"\?\?(\?\?)"`, the head of retail's
string pool — while our object emitted its own 7-byte `.rodata` (`3f 3f 28 3f 3f 29 00`, which
`unit_fit.sh` flags as "NOT CLAIMED BY splits.txt") which mwldeppc appended, shifting every later
pool entry.

Fix, using the mechanism `include/Kyoto/Alloc/CMemory.hpp` documents for exactly this trap and that
`src/MetroidPrime/Factories/CStateMachineFactory.cpp` already uses:

```cpp
#define CMEMORY_NEW_FILE lbl_803A8AB0   // before any include
...
extern "C" const char lbl_803A8AB0[];
```

`lbl_803A8AB0` is `.rodata:0x803A8AB0`, `size:0x7 data:string`, defined by
`build/G2ME01/obj/auto_06_803A8A18_rodata.o`. After this our object emits **no** `.rodata` section
at all, and `unit_fit.sh`'s `.rodata` line disappears.

**A warning, because it cost an hour of this run.** The retail encoding is
`lis r7,0x803B ; addi r0,r7,-30032`, and 0x803B0000 − 30032 is **0x803A8AB0**, not 0x803A8AC0
(30032 = 0x7550). I first split `lbl_803A8AB8` in `symbols.txt` to name 0x803A8AC0 and got a
one-byte DOL difference that no symbol map showed. `symbols.txt` is back to its original text.

## Files touched

* `src/MetroidPrime/CWorldShadow.cpp` — 3 hunks: the `CMEMORY_NEW_FILE` define + `extern` +
  comment above the includes; `lbl_80419968`/`lbl_80418AF0` for the depth range; the two `extern`
  declarations. Nothing else in the file changed, and all 7 functions stayed at 100% throughout.
* `config/G2ME01/symbols.txt` — one rename (above).
* `configure.py` — `NonMatching` → `Matching` for `MetroidPrime/CWorldShadow.cpp`.
* `docs/HANDOFF.md` shows modified: that is `check_docs_claims.py --write` inside the judge's
  `gate.sh`, not an edit of mine.

## Measured

```
./tools/decomp_build.sh                          All: 32.81% fuzzy, 25.61% matched, 12.08% linked (11424 / 28465 functions)
sha1sum build/G2ME01/main.dol                    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/CWorldShadow.cpp   PASS -> kept as Matching
python3 tools/check_symbol_names.py              checked 514 units; 0 declared names are missing
./tools/probe_sources.sh                         probe: 752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
./tools/goal_check.sh build/goal/item.json       PASS
```

`linked` 5537 → 5544 (the unit's 7 functions); `matched` unchanged at 11424 because the previous run
had already matched them. `unit_fit.sh` still reports `.text` over by 336 (the three harmless weak
destructor copies) and `.sbss` 13 against a claimed 16 — both are inside the flip that now passes,
so neither is a blocker.
