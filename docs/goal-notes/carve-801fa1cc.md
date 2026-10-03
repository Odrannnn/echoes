# carve-801fa1cc — `MetroidPrime/ScriptObjects/Carve801FA1CC`, `Matching`, 2/2 functions

**Result: the unit is `Matching`, `tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FA1CC.cpp` PASSes
(kept as Matching), and `tools/goal_check.sh build/goal/item.json` PASSes.**

`matched` 13540 -> 13542, `linked` 6588 -> 6590, `matched_code` 2030356 -> 2030652 (**+296 bytes, the
whole claim**), `total_functions` still 28465. `build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs unchanged. `tools/probe_sources.sh`:
952 files, 0 failed. The gate's per-function diff reports exactly one change and it is the right
one: `main/auto_03_801F9848_text: 2 function(s) moved into
main/MetroidPrime/ScriptObjects/Carve801FA1CC (exact count match - a split, not a loss)`.

## What I did

Carved `.text 0x801FA1CC..0x801FA2F4` (0x128 = 296 bytes, 2 functions) out of dtk's
`auto_03_801F9848_text` as its own unit. Four files, each entry in address order, source descending:

| file | line | content |
| --- | --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FA1CC.cpp` | new, 191 lines | the source and its own claim |
| `config/G2ME01/splits.txt` | 1413-1414 | `Carve801FA1CC.cpp: .text start:0x801FA1CC end:0x801FA2F4` |
| `configure.py` | 794 | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FA1CC.cpp")`, one line |
| `files.cmake` | 806 | `src/MetroidPrime/ScriptObjects/Carve801FA1CC.cpp` |

**No `PortLinkStubs.cpp` entry was needed or added.** Nothing in that file defines or stubs
`fn_801FA1CC` or `fn_801FA28C` (grepped), and the port link gap step is green: `link_gap.py`
reports 279 MISSING symbols, all accounted for, so this carve added no stand-in requirement.

| function | addr | size | insns | twin it is byte-for-byte |
| --- | --- | --- | --- | --- |
| `fn_801FA1CC` | 0x801FA1CC | 0xC0 = 192 | 48 | `reserve__Q24rstl50vector<13CInt32POINode,Q24rstl17rmemory_allocator>Fi`, `symbols.txt:11579`, 0x8029224C (Matching, `src/Kyoto/Animation/CAnimTreeSequence.cpp`) |
| `fn_801FA28C` | 0x801FA28C | 0x68 = 104 | 26 | `uninitialized_copy<Q24rstl120pointer_iterator<13CInt32POINode,...>,P13CInt32POINode>__4rstlF...`, `symbols.txt:11580`, 0x8029230C (Matching, same unit) |

Both addresses and sizes are `symbols.txt:8200-8201`. What the pair is: one **0x40-byte-element**
`rstl::vector`'s `reserve`, and the `uninitialized_copy` it moves the live elements with.

**The twin comparison is exact and measured this run.** Disassembling both 0x128-byte runs of
`build/G2ME01/main.elf` (0x801FA1CC..0x801FA2F4 and 0x8029224C..0x80292374) and comparing decoded
words: **71 of 74 words identical**; the three that differ are exactly the three `bl` words
(`allocate__Q24rstl17rmemory_allocatorFi` at index 10, `Free__7CMemoryFPCv` at index 40, the
element's `construct` at index 60). No data address differs, and the frame, register moves, loop
bodies, branch displacements and epilogues are all the same words. The `bl` to the copy helper at
index 24 is byte-identical to the twin's own, because the two callees sit at the same relative
offset (+0xC0) inside their respective runs.

I also compiled the twin itself (`src/Kyoto/Animation/CAnimTreeSequence.cpp`) and confirmed its
`reserve` comes out byte-identical to retail's 0x8029224C, so the twin is a live ground truth and
not just a similar shape.

## This unit is C++, not C, and one thing in it is load-bearing

`docs/RUNNING_THE_DECOMP.md`'s carve recipe says plain C so the `fn_` names do not mangle. I wrote
it in C first and it is **not** byte-exact, for one measured reason: the destroy loop.

Retail destroys each element through its vtable — `lwz r12,0x0(e)` / `lwz r12,0x8(r12)` /
`mtctr r12` / `bctrl` with `li r4,-1` (0x801FA24C-0x801FA258) — which is the **deleting-destructor
slot with mwcc's own complete-object flag**, i.e. what an explicit `p->~T()` compiles to for a
class with a *virtual* destructor. Written as a hand-rolled indirect call through a loaded vtable
slot it does not come out in that shape. So the element has to be a real class with a virtual
destructor, which makes the file `.cpp`.

The symbols still stay unmangled because every definition is `extern "C"`: `powerpc-eabi-nm` on
the object shows exactly

```
         U Free__7CMemoryFPCv
         U allocate__Q24rstl17rmemory_allocatorFi
         U fn_801F9800
00000000 T fn_801FA1CC
000000c0 T fn_801FA28C
```

and `objdump -h` shows one `.text` of exactly 0x128 and nothing but `.comment` — no vtable, no
data, no destructor copy. That is the thing the plain-`.c` rule protects, and
`tools/unit_fit.sh` agrees: `.text claimed 296 ours 296 retail 296 fits`, and "no extra functions:
our object defines only what the retail unit object does". `ScriptObjects/Carve801FF5A0.cpp` in the
same directory is `extern "C"` for the same reason and is this function's 0x24-stride twin.

## The measured finding worth keeping: the destroy loop must come from an inline helper

With the destroy loop written inline in `fn_801FA1CC` — `SElem* p = self->x0c; for (SElem* e = p +
self->x04; p != e; ++p) p->~SElem();` — the body is the right length (48 instructions to retail's
48) and the right arithmetic, and still differs in **five** words, all of it one thing:
mwcceppc puts the new buffer in `r31` and the loop bound in `r29` where retail has them the other
way round.

```
  +12  retail: mr       r29,r3            ours: mr       r31,r3
  +16  retail: mr       r5,r29            ours: mr       r5,r31
  +28  retail: add      r31,r30,r0        ours: add      r29,r30,r0
  +37  retail: cmplw    r30,r31           ours: cmplw    r30,r29
  +41  retail: stw      r29,12(r27)       ours: stw      r31,12(r27)
```

Routing it through `sDestroy(begin, end)` — retail's own `rstl::destroy`/`destroy_impl` shape at
`include/rstl/construct.hpp:100-114` — is what fixes it, and the result is **byte-exact over the
whole 0x128-byte run**: the only differing words left are the four `bl`s, which are relocations the
linker resolves. `fn_801FA28C` was byte-exact in every spelling I tried, so this is specific to the
function that has both the buffer and the loop live at once.

Spellings measured this run that all still swap those two registers (each 5 differing words, same
five): raw `char*` cursor with a `static_cast<SElem*>` for the destructor; a `do { } while` loop
instead of `for`; a redundant `SElem* const items = self->x0c;` local; `void* const` new-buffer
local; `SElem* const` new-buffer local; `SElem* newData = 0;` assigned after declaration. What
finally worked is the *only* one that changes which function the loop is emitted in.

## What the bytes prove about the classes

- **The vector is `rstl::vector`, not a guess.** `fn_801FA1CC` reads `+0x8` for the growth test
  (`lwz r0,8(r3)` / `cmpw` / `ble`, 0x801FA1E4-0x801FA1EC), `+0x4` for the live count, `+0xC` for
  the base pointer, and writes `+0xC` and `+0x8` back — `mAllocator, mCount, mCapacity, mItems` at
  `include/rstl/vector.hpp:16-21`. `Carve801F97C8.c`'s own header records the same four offsets for
  this very vector's `push_back_unsafe` at 0x801F97C8 and names this function as its `reserve`.
- **The element is 0x40 and polymorphic**: `slwi ...,6` at 0x801FA1F0/0x801FA208 is `count * 64`,
  the destroy loop steps by 64 (`addi r30,r30,64`, 0x801FA25C), and the vtable call above is the
  other half.
- **The two iterator arguments are by value.** `fn_801FA28C` keeps `mr r29,r4` for `end` and
  re-reads the bound every iteration (`lwz r0,0(r29)` / `cmplw r31,r0` / `bne`,
  0x801FA2C8-0x801FA2D0), which is only reachable if both arrive by value as one-word classes.
  `fn_801FA1CC` materialises both on its own stack, one 8-byte slot per argument holding the same
  pointer twice — 0x8/0xc for `end`, 0x10/0x14 for `begin` — with `addi r4,r1,0xc` and
  `addi r3,r1,0x14` the addresses handed over, and a `lwz r0,12(r27)` reload between the stores.
  `Carve801FF5A0.cpp`'s header records the identical eight stores on the identical point.

## The callees are all real

`fn_801F9800` (0x801F9800, 0x20, `symbols.txt:8191`) is `rstl::construct` for this same element and
is defined by `src/MetroidPrime/ScriptObjects/Carve801F97C8.c`, a `Matching` unit, so the `bl` at
0x801FA2BC lands on a body written in retail's bytes rather than a stand-in.
`allocate__Q24rstl17rmemory_allocatorFi` (0x802FDAB8, `symbols.txt:13824`) and `Free__7CMemoryFPCv`
(0x802CE388, `symbols.txt:12992`) are declared by their retail names and never defined here, so the
object carries no inline copy of either. The one caller of this reserve in the DOL is retail's own
`bl` at 0x801F969C inside `SetupColliders__20CCameraColliderGroupFfffif` (0x801F9628, 0x1A0), which
stays in dtk's object, so nothing was split out of a neighbouring claim: `fn_801FA1CC` ends exactly
where `fn_801FA28C` begins and `fn_801FA28C` ends exactly at 0x801FA2F4 = `fn_801FA2F4`
(`symbols.txt:8202`, 0xD8, still unclaimed and left to dtk).

## Checks run, all green

```
sha1sum build/G2ME01/main.dol                -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                      -> All: 37.64% fuzzy, 31.07% matched, 13.94% linked (13542 / 28465)
  main/MetroidPrime/ScriptObjects/Carve801FA1CC: 100.00% fuzzy, 100.00% matched (2 / 2 functions)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FA1CC.cpp -> PASS -> kept as Matching
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FA1CC.cpp  -> .text claimed 296 ours 296 retail 296 fits
python3 tools/check_symbol_names.py          -> checked 600 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FA1CC -> ok: 1 unit(s) checked
python3 tools/check_files_cmake.py           -> every configured DOL object is either in files.cmake or excluded with a reason
./tools/probe_sources.sh                     -> 952 files, 0 failed; link: LINKED (287 undefined, 0 duplicates)
python3 tools/link_gap.py                    -> ok: 279 MISSING symbol(s), all accounted for
./tools/goal_check.sh build/goal/item.json   -> PASS carve-801fa1cc
```

`report.json` diff against `build/goal/judge/report.base.json`, per unit: **only two units move** —
`main/MetroidPrime/ScriptObjects/Carve801FA1CC` appears at (2 matched / 2 total) and
`main/auto_03_801F9848_text` goes (0/9) -> (0/7). No other unit's matched count changed and no
function anywhere got worse.

`report.json`'s `total_functions` is **28465** after the `splits.txt` edit, as required.

## Notes for the next lane

- **Do the spelling experiments with `tools/probe_cc.sh <src> <out.o>`**, not by editing a real
  carve unit and rebuilding: it compiles one scratch source with the exact MWCC GC/2.7 flags a DOL
  unit gets. I hand-rolled the same command instead and it worked, but `probe_cc.sh` is the one to
  reach for; its argument order and the quoting trap on the two `-pragma` options are in
  `RUNNING_THE_DECOMP.md`'s tool table.
- This range is the **0x40-stride** clone of the 0x24-stride block already claimed by
  `Carve801FF5A0.cpp` / `Carve801FF720.cpp` / `Carve801FF8A0.cpp` / `Carve801FFA20.cpp`, and the
  `reserve`+`uninitialized_copy` pair recurs per script-object type across the whole ScriptObjects
  region. The same source shape works for any of them; only the stride, the construct callee and
  the two allocator call targets change. `auto_03_801F9848_text` now carries
  0x801F9848..0x801FA1CC and `auto_fn_801FA2F4_text` starts at 0x801FA2F4, so the next runnable
  claim in this neighbourhood starts at `fn_801FA2F4` (0xD8).
- No `NEW:` item is filed from this run: nothing here is a new blocker, and the wall-shaped
  findings (the six loop spellings that swap r29/r31) are recorded above rather than queued.