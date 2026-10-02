# carve-801fd67c — `MetroidPrime/ScriptObjects/Carve801FD67C`

**STATUS: DONE.** `fn_801FD67C` (`.text 0x801FD67C..0x801FD6F0`, 0x74 = 116 bytes, 29 instructions)
is carved into `src/MetroidPrime/ScriptObjects/Carve801FD67C.cpp`, objdiff reports
**100.00% fuzzy / 100.00% matched, 1 / 1 functions**, `tools/flip_test.sh` prints
`PASS -> kept as Matching`, and `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PASS carve-801fd67c`.

## A prior run of this item existed in this tree and was reverted

`build/goal/notes/carve-801fd67c.md` was already present in `wt-mp2-goal-L3` when I started
(mtime 17:47, item claimed by lane 3 at 17:49) and recorded the same verdict, but **none of its
changes were in the tree**: `git status` was clean and no `Carve801FD67C.cpp` existed. So I re-did
the item from the clean tree and re-measured everything rather than trusting that record. Where my
route differs from that note, it is recorded below under "Differences from the earlier attempt" —
the substance agrees, and `build/report.json` on the clean tree did **not** already show this
function matched, so this is not a `STALE:` item.

## What the item's reason said, and what was true when I measured

The reason was written by `carve-801fdae8` in another tree and **did not describe this tree**:

- It called `fn_801FDAE8` "the now-`Matching` `src/MetroidPrime/ScriptObjects/Carve801FDAE8.cpp`".
  **No such file exists here** — `ls src/MetroidPrime/ScriptObjects/` has no `Carve801FDAE8.*`, and
  `fn_801FDAE8` is still declared-never-defined at
  `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c:76-84`. Its twin was unclaimed here.
- It said "`stub_200` (`fn_801FD6F0`) already exists". **False here**: `stub_200` is
  `fn_801FEE88` (`src/MetroidPrime/PortLinkStubs.cpp:1191`). Nothing stood in for `fn_801FD6F0`,
  so the carve's new `bl` needed a stand-in the reason did not budget for.
- It said "one more data stub for `lbl_803B7BFC` would be the only new cost". True in kind. The
  cost landed as `stub_data_6` in `PortLinkStubs.cpp` — see "The two host costs".

The reason's *substance* held. Measured, `fn_801FDAE8` (0x801FDAE8, 0x74) and `fn_801FD67C` are
29 of 29 instructions with exactly two differing words: `addi r3,r30,20` vs `addi r3,r30,24`, and
`addi r0,r4,31740` (vtable 0x803B7BFC) vs `addi r0,r4,31716` (vtable 0x803B7BE4). `fn_801FD924`
(0x801FD924, 0x74) is the same again with the vtable 0x803B7BF0.

## Measured

| what | how |
|---|---|
| `.text 0x801FD67C..0x801FD6F0`, 0x74 = 116 bytes, 1 function, 29 instructions | `config/G2ME01/symbols.txt:8273`, then `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FD67C --stop-address=0x801FD6F0 build/G2ME01/main.elf` |
| the bytes are retail's, not ours | read the disc directly: `orig/G2ME01/sys/main.dol`, text section 1 at file offset `0x640` for address `0x80003840`, so this range is file offset `0x1FA47C`, 116 bytes. Compared with the 116 `main.elf` bytes word for word: identical. |
| `lbl_803B7BFC` is `.data 0x803B7BFC`, size 0xC | `symbols.txt:18319` |
| `internal_dereference__Q24rstl66basic_string<c,...>Fv` is at 0x802FE9B8, size 0x40 | `symbols.txt:13842`; `powerpc-eabi-nm build/G2ME01/main.elf` also shows it defined there |
| `Free__7CMemoryFPCv` is at 0x802CE388 | `powerpc-eabi-nm build/G2ME01/main.elf` |
| element size is 0x24 = 36 | the two retail loops that call only `fn_801FD638` step by 36: `fn_801FD5E8`'s `addi r31,r31,36` at 0x801FD610, and `fn_801FF66C`'s at 0x801FF694 inside `ScriptObjects/Carve801FF5A0.cpp` (a `Matching` unit). `fn_801FD6F0` reads only +0x04 and +0x0C of its argument, so the member at +0x14 is 0x10 bytes and closes the object at 0x14+0x10 = 0x24. |
| `~basic_string()` is `{ internal_dereference(); }` and the object is 0x10 bytes | `include/rstl/string.hpp:193` for the destructor, `:101-110` for the four members |
| retail's own store is `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO lbl_803B7BFC` | `powerpc-eabi-objdump -r build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD67C.o`: the two relocations at 0x22 and 0x2a |

## The body

```cpp
extern "C" CCarve801FD67C* fn_801FD67C(CCarve801FD67C* self, int flag) {
  if (self != nullptr) {
    self->x00_vtable = reinterpret_cast< void* >(lbl_803B7BFC);
    fn_801FD6F0(&self->x14_member, -1);
    self->x04_name.~basic_string();
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
```

Three spellings are load-bearing, and two of them cost a compile each:

1. **`self->x04_name.~basic_string();` — the destructor must be named.** Left to the compiler as
   the implicit teardown of a member this translation unit never reads, mwcceppc **deleted it**:
   the first compile scored 85.86% with no `addic.`/`beq`/`bl internal_dereference` at all, which
   is retail's `0x801FD6B4..0x801FD6C0`. Naming it produces them, and `addic. r0,r30,4 / beq` is
   mwcceppc's own dead null test on the *member's address* (the same idiom
   `src/MetroidPrime/CStateManager.cpp:227` measures). With it: 99.66%, and with the vtable named
   too, 100.00%.
2. **The vtable is stored as the symbol `lbl_803B7BFC`, not as the constant `0x803B7BFC`.** The two
   emit the same two instructions and the same linked bytes — with the constant the full build's
   `main.dol` sha1 was already retail's `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` — but objdiff
   **pairs relocations**, and the constant emits no `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair:
   99.66% fuzzy / **0 functions matched** on a unit whose `.text` is retail's word for word. This
   is the trap the whole item is about: a green DOL is not a matching unit.
3. **`static_cast< short >(flag) > 0`, not `flag > 0`** — `extsh. r0,r31` says the test is on a
   signed short (`src/MetroidPrime/Carve800E10EC.cpp:58-68`, a `Matching` unit, spells it the
   same way).

`void*` + `if (self)` + `return self` is what produces `mr. r30,r3 / beq`, the `mr r31,r4`, and the
`mr r3,r30` in the epilogue that sits after both early exits — the same three instructions as
`Carve800E1548.c:112-121` and `Carve800E10EC.cpp:58-68`, both `Matching`.

**Why `.cpp` and not `.c`:** `internal_dereference__Q24rstl66basic_string<c,...>Fv` is not made of
identifier characters, so a `.c` cannot declare it (it fails at the first `<`), and a `.c` cannot
include `rstl/string.hpp`. Same measurement and reason as `src/MetroidPrime/Carve800E10EC.cpp:22-31`.
`extern "C"` keeps the one symbol unmangled, so objdiff pairs it exactly as for a `.c`.

## The carve: four files

- `src/MetroidPrime/ScriptObjects/Carve801FD67C.cpp` — new, the claim and its header
- `config/G2ME01/splits.txt:1365` — `.text start:0x801FD67C end:0x801FD6F0`, in address order
- `files.cmake:579` — the unit
- `configure.py:772` — `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD67C.cpp")`

Boundaries: `Carve801FD638.c` ends exactly at 0x801FD67C, and this claim ends at 0x801FD6F0 with
`fn_801FD6F0` (0x84) unclaimed above it; the next claim, `Carve801FD8E0.c`, starts at 0x801FD8E0.
No gap is spanned.

`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD67C` is clean, and
`./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD67C.cpp` reports
`.text claimed 116 ours 116 retail 116 fits` and `no extra functions`.

## The two host costs

**1. `fn_801FD6F0` needed a stand-in.** The carve's `bl fn_801FD6F0` is in retail's bytes, so it
cannot be dropped, and nothing in the host build defined it. `src/MetroidPrime/PortLinkStubs.cpp`
now uses the **existing `stub_197` name for `fn_801FD6F0`**, replacing the `fn_801FD67C` stub the
carve makes unnecessary — an exchange, so that file's function count does not move, and no
cross-reference in its prose had to be repointed. Measured after the change: `grep -cE 'asm\("'` is
196 and `grep -cE 'asm\("(fn_|lbl_)'` is 36. `stub_201`..`stub_2xx` are already taken by the ninth
sync's block, which is why the name is reused rather than a new number taken.

**2. `lbl_803B7BFC` grew the host gap by one**, and it is `stub_data_6` in the same file — the
file's own data-object section, whose stated purpose is "a vtable or typeinfo stub is zero-filled:
harmless to take the address of, and a crash if used". `lbl_803B7BFC` *is* a vtable (0xC bytes,
`{0, 0, 0x801FF4BC}` in retail), so that is its kind. That makes seven `stub_data_*`; the header
paragraph's numerals were left alone on purpose, because every carve that lands while another item
is being carried rewrites that paragraph and a change touching it cannot be carried — the count is
recorded in the new block's own comment instead, which is what the note above `stub_801e515c_0`
asks for.

## Differences from the earlier attempt in this tree

Both routes reach the same result; the earlier one put `fn_801FD6F0` in a **new** `stub_226` and
gave `lbl_803B7BFC` a home in a **new host-only** `src/MetroidPrime/PortScriptObjectVtables.cpp`
rather than in `PortLinkStubs.cpp`. Mine reuses `stub_197` and adds `stub_data_6`, so it is **one
file smaller** and touches no stub number another lane could take. Nothing in either approach
changes a byte of the DOL.

## Verification

```
All:  36.96% fuzzy, 30.40% matched, 13.40% linked (13033 / 28465 functions)
main/MetroidPrime/ScriptObjects/Carve801FD67C: 100.00% fuzzy, 100.00% matched (1 / 1 functions)
sha1sum build/G2ME01/main.dol = 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/unit_fit.sh: .text claimed 116 ours 116 retail 116 fits; no extra functions
tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD67C: ok, none out of retail order
tools/check_symbol_names.py: checked 576 units; 0 declared names are missing from their object
tools/check_files_cmake.py: every configured DOL object is either in files.cmake or excluded with a reason
probe: 814 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)
tools/link_gap.py --rebuild: 281 MISSING symbol(s), all accounted for in port_link_gap_list.md
tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD67C.cpp: PASS -> kept as Matching
tools/goal_check.sh build/goal/item.json: PASS carve-801fd67c
```

`report_diff.py`: `matched 13032 -> 13033   linked 6136 -> 6137   (+1 functions at 100%, 1 units
newly linked)`, `no regression`, and the only other entry is
`SPLIT main/auto_03_801FD67C_text: 6 function(s) accounted for across 2 new unit(s) ... a split, not
a loss`. The port's undefined count is **287, unchanged** from the judge's baseline
(`build/goal/judge/undef.base.count` = 287) and `duplicate definitions 0`.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`. **Those are
the judge's own `MP_GATE_DOCS_WRITE=1` derived-count rewrites** (`check_docs_claims.py --write`
inside `goal_check.sh`), not my edits.

## Follow-ups

Neither is a lesson and neither is a wall, so both are filed. Each names one real unit and would
raise a count.

NEW: carve-801fd6f0 | match | MetroidPrime/ScriptObjects/Carve801FD6F0 | fn_801FD6F0 (0x84, symbols.txt:8274) is the 0x24-byte element's other deleting destructor, starts exactly where this unit's claim ends (0x801FD6F0), is the same mr./beq/extsh./ble/CMemory::Free shape, reads a count at +4 with mulli by 20 and a data pointer at +0xC, and retires stub_197

NEW: carve-801fdae8 | match | MetroidPrime/ScriptObjects/Carve801FDAE8 | fn_801FDAE8 (0x74, symbols.txt:8287) is the exact twin of the unit this item landed - same 29 instructions, addi r3,r30,24 and vtable lbl_803B7BE4 - still unclaimed in this tree, clean boundaries at 0x801FDAE8, and retiring stub_199 would pay off

`fn_801FD774` (0x801FD774, 0x60) is what `fn_801FD6F0` calls at 0x801FD73C, and is the next thing
unblocked by the first.

WALL: none. Two spellings were iterated (implicit vs named member destructor, constant vs named
vtable) and both are recorded above with the score each one gave.

## One thing the earlier attempt measured that I did not reproduce

The earlier attempt's notes reported `link_check: STRICT FAIL - regression gate: 288 undefined
against a baseline of 287 (GREW)` with `NEW lbl_803B7BFC` **and** `NEW fn_8000447C`, and argued the
second was a pre-existing disagreement between `build/goal/judge/undef.base.txt` (which carries
`fn_8000447C`) and the committed `docs/research/port_link_baseline.txt` (which does not). **I did
not reproduce a regression here**: `tools/link_check.sh` reports 287 undefined, 0 duplicates,
against the judge's baseline of 287. `docs/research/port_link_baseline.txt` is judge-owned, so
whether that disagreement still exists is not this item's to settle.
---

# Third run of this item, 2026-10-02 — re-did it from the clean tree, and it landed

**STATUS: DONE.** `fn_801FD67C` (`.text 0x801FD67C..0x801FD6F0`, 0x74 = 116 bytes, 29 instructions)
is carved into `src/MetroidPrime/ScriptObjects/Carve801FD67C.cpp`, objdiff reports
**100.00% fuzzy / 100.00% matched, 1 / 1 functions**, `tools/flip_test.sh` prints
`PASS  -> kept as Matching`, and `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PASS carve-801fd67c`, with `matched 13040 -> 13041` and `linked 6144 -> 6145`.

## This is the second time a run of this item found the tree clean

The notes above (from the run before last) say exactly this, and it happened again: when I started,
`git status` was clean, `src/MetroidPrime/ScriptObjects/Carve801FD67C.cpp` did not exist, and
`build/report.json` still had `fn_801FD67C` sitting in `main/auto_03_801FD67C_text` at
**0 matched functions**, of `total_functions: 13040` overall. So this is **not** a `STALE:` item on this
branch: the two earlier verdicts in this file were both about a tree state that did not survive. I
re-measured everything below from scratch rather than trusting either.

## What is different about this tree: the item's reason is finally accurate, and there is a matched twin

The reason was written by `carve-801fdae8` in another tree and **did not describe the last two
trees** — it called `fn_801FDAE8` "the now-`Matching` `src/MetroidPrime/ScriptObjects/Carve801FDAE8.cpp`"
while no such file existed, and it said `stub_200` (`fn_801FD6F0`) already exists when nothing stood in
for it. **On this branch both are true**: `git log --oneline -1` is `8c0ec77b match: carve-801fdae8`,
`configure.py:776` has `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAE8.cpp")`, and
`src/MetroidPrime/PortLinkStubs.cpp:1160` is `extern "C" void stub_227() asm("fn_801FD6F0");`. So the
reason's budget is now right: one data stub is the only new cost, no function stub has to be invented.

More useful: **`fn_801FD67C` is not merely the same shape as `fn_801FDAE8`, it is byte-identical to a
`Matching` unit.** Re-measured on this branch, `fn_801FD924` (0x801FD924, 0x74, claimed by the
`Matching` `src/MetroidPrime/ScriptObjects/Carve801FD924.cpp`) and `fn_801FD67C` differ in **one
immediate out of 29**: `addi r0,r4,31728` against `addi r0,r4,31740`, i.e. `lbl_803B7BF0` against
`lbl_803B7BFC`. Same member offset (`addi r3,r30,20`, so the same +0x14 member and the same 0x24 = 36
byte element), same `bl fn_801FD6F0` at +0x14, same `internal_dereference__Q24rstl66basic_string` at
0x802FE9B8, same `Free__7CMemoryFPCv` at 0x802CE388. The run above in this file called the third copy
(`fn_801FDAE8`, member at +0x18) the twin and did not notice that **the exact twin is `fn_801FD924`,
and it is `Matching`**, which is worth knowing: nothing here had to be guessed and no spelling had to
be found by iteration.

## Measured this run

| what | how |
|---|---|
| `fn_801FD67C = .text:0x801FD67C; // type:function size:0x74`, and the claim ends where `fn_801FD6F0` (0x84) begins | `config/G2ME01/symbols.txt:8273` and `:8274` |
| the 116 bytes are retail's, not ours | `python3 tools/dol_read.py 0x801FD67C 0x74` reads `orig/G2ME01/sys/main.dol` at file offset `0x1FA47C`; the 29 words it printed are identical word for word to `build/G2ME01/main.elf`'s at the same address |
| 0x801FD67C..0x801FD6F0 is one function and nothing else | dtk's `build/G2ME01/asm/auto_03_801FD67C_text.s`: `# 0x801FD67C..0x801FD8E0 \| size: 0x264`, first header `# .text:0x0 \| 0x801FD67C \| size: 0x74`, second `# .text:0x74 \| 0x801FD6F0 \| size: 0x84` |
| element stride 0x24 = 36 | two independent retail walks of this element: `fn_801FD5E8`'s `addi r31,r31,36` at 0x801FD610, and `fn_801FF66C`'s `bl fn_801FD638` at 0x801FF690 followed by `addi r31,r31,36` at 0x801FF694 (`ScriptObjects/Carve801FF5A0.cpp`, `Matching`) |
| the `-1` is `rstl::destroy_impl`'s, not invented | `fn_801FD658` (0x801FD658, 0x24) disassembles to `li r4,-1` at 0x801FD660 and `bl 801fd67c` at 0x801FD668; it is in the claim in front, `ScriptObjects/Carve801FD638.c` (0x801FD638..0x801FD67C, `Matching`) |
| the layout | `fn_801FEE88` (0x801FEE88, 0x68, this class's copy constructor) stores the base vtable `lbl_803B7BCC` at +0x00 (`lis r5,0x803b` + `addi r0,r5,31692`), then `lbl_803B7BFC` over it (`lis r3,0x803b` + `addi r0,r3,31740`), copy-constructs the `rstl::string` at +0x04 (`addi r3,r30,4 / bl __ct__Q24rstl66basic_string<...>`), and calls `fn_801FE8B8` on +0x14 (`addi r3,r30,20 / addi r4,r31,20`) |
| `lbl_803B7BFC = .data:0x803B7BFC; // size:0xC` | `symbols.txt:18319`; `objdump -s --start-address=0x803B7BF0 --stop-address=0x803B7C08 build/G2ME01/main.elf` gives `00000000 00000000 801ff4b4` then `00000000 801ff4bc`, i.e. at 0x803B7BFC `{0, 0, &fn_801FF4BC}` |
| `fn_801FF4BC` is claimed and defined | `Carve801FF4A4.c` claims 0x801FF4A4..0x801FF4C4 and defines `int fn_801FF4BC(void) { return 1; }` at line 31; `Matching` |
| naming the vtable is what puts retail's relocations in our object | `powerpc-eabi-objdump -r build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD67C.o`: `R_PPC_ADDR16_HA lbl_803B7BFC` at 0x22 and `R_PPC_ADDR16_LO lbl_803B7BFC` at 0x2a, plus `R_PPC_REL24` for `fn_801FD6F0`, the string destructor and `Free__7CMemoryFPCv` |

## The body, and why it went straight to 100% without iterating

```cpp
extern "C" void* fn_801FD67C(SCarve801FD67CElement* self, short flag) {
  if (self) {
    self->mVTable = lbl_803B7BFC;
    fn_801FD6F0(&self->mMember, -1);
    self->mName.~basic_string();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
```

The first compile of this body scored **100.00%** — there was nothing to iterate, because all three
load-bearing spellings were already measured twice on the two identical `Matching` siblings and are
recorded in their headers:

1. **`self->mName.~basic_string();` must be a *named* destructor call.** Left to scope exit, mwcceppc
   emits no `addic. r0,r30,4 / beq` at all, and retail has that test (0x801FD6B4..0x801FD6B8) — the
   run above in this file measured 85.86% for the implicit spelling. The name after the `~` has to be
   the unqualified injected class name (`~basic_string()`, not `~rstl::string()`); MWCC 2.7 rejects the
   qualified one.
2. **The vtable is the symbol `lbl_803B7BFC`, not the constant.** Spelling the constant leaves the four
   instruction words identical — a green `main.dol` — and loses the `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO`
   pair objdiff pairs, i.e. **0 matched functions on a unit whose `.text` is retail's word for word**.
   The run above measured 99.66% fuzzy / 0 matched for that, and `Carve801FDAE8.cpp`'s header records
   the same trade measured three times before it.
3. **`short flag`, not `int`.** `extsh. r0,r31` at 0x801FD6C4 is the only thing that pins it, and the
   call passes a literal `-1`, so both spellings give `li r4,-1` — only `short` gives `extsh.`.

`void*` + `if (self)` + `return self` produces `mr. r30,r3 / beq`, keeps `r31` for the flag, and puts
`mr r3,r30` in the epilogue after both early exits.

## The carve: four files

- `src/MetroidPrime/ScriptObjects/Carve801FD67C.cpp` — new, the claim and its header
- `config/G2ME01/splits.txt:1368-1369` — `.text start:0x801FD67C end:0x801FD6F0`, in address order
  between `Carve801FD638.c` (ends exactly at 0x801FD67C) and `Carve801FD8E0.c` (starts 0x801FD8E0)
- `files.cmake:579` — the unit, in the same place
- `configure.py:775` — `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD67C.cpp")`, one
  `Object(` per line

`total_functions` is still **28465** after the `splits.txt` edit.

## The one host cost, and a numeral in that file's header that was wrong

**`stub_197` retired, `stub_data_8` added** (`src/MetroidPrime/PortLinkStubs.cpp`). `stub_197` was
`asm("fn_801FD67C")`, and the new unit defines that symbol for the port's link as well, so keeping it
would be two definitions of one symbol; `lbl_803B7BFC` is new to the port's link because this unit
restores the vptr in retail's bytes, so it needs the zero-filled data stub like its two siblings'.
`fn_801FD6F0` needed nothing — `stub_227` is already there for `Carve801FD924.cpp`.

**A numeral I corrected, and it is worth recording because the paragraph itself warns about it.**
That header's breakdown line read *"196 total, 36 unmangled, 8 data"* while the file actually held
**195 / 35 / 8** — the total had been carried from an earlier carve that added a function stub without
moving it, exactly the failure the paragraph documents two lines above itself. Measured after my
change: `grep -cE 'asm\("'` = **195**, `grep -cE 'asm\("(fn_|lbl_)'` = **35**,
`^extern "C" void stub_*() asm(` = **186** function stubs, `^extern "C" char stub_data_*` = **9** data
stubs, so functions 187 -> 186, data 8 -> 9 and the total did not move at 195. I rewrote the two
affected paragraphs to those measured figures and said which grep derives each term; the judge owns
the `docs/` counts, but this is a source header and it is now true.

## Verification

```
All:  36.97% fuzzy, 30.41% matched, 13.40% linked (13041 / 28465 functions)
main/MetroidPrime/ScriptObjects/Carve801FD67C: 100.00% fuzzy, 100.00% matched (1 / 1 functions)
sha1sum build/G2ME01/main.dol = 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/unit_fit.sh: .text claimed 116 ours 116 retail 116 fits; no extra functions
tools/check_symbol_names.py: checked 578 units; 0 declared names are missing from their object
tools/check_files_cmake.py: every configured DOL object is either in files.cmake or excluded with a reason
probe: 819 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)
tools/link_gap.py --rebuild: 281 MISSING symbol(s), all accounted for in port_link_gap_list.md
tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD67C.cpp: PASS -> kept as Matching
tools/goal_check.sh build/goal/item.json: PASS carve-801fd67c
    ok  no judge-owned path touched
    ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok  counts: matched 13040 -> 13041   linked 6144 -> 6145
```

The port's undefined count is **287**, unchanged from the judge's baseline
(`build/goal/judge/undef.base.count`), and duplicate definitions **0**.

`tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD67C` prints `0 unit(s) checked`,
which is the tool not resolving that name (it resolves `1063` units when run bare, and reports `36`
permuted, all accounted for in `decl_order.md`, none of them this one). The unit has a single
function, so the reverse-declaration-order rule has nothing to bite on here either way.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status` — those are the
judge's own `MP_GATE_DOCS_WRITE=1` derived-count rewrites (13040 -> 13041, 818 -> 819 probe files),
not my edits; the diff is those numerals and nothing else.

`fn_801FD67C` was never in `docs/research/port_link_gap_list.md` (it was supplied by `stub_197`), and
`lbl_803B7BFC` is not in it either (`stub_data_8` supplies it), so that file needs no edit and
`link_gap.py` still reports 281, all accounted for.

## WALL: none

No spelling was iterated this run: the first compile of the body scored 100.00%. The two spellings
that cost the earlier runs a compile each (implicit vs named member destructor, constant vs named
vtable) are listed above with the score each gives, so the next lane does not have to rediscover them.

## Follow-ups

Both `NEW:` lines the run above filed, re-measured on this branch:

- **Still open, still correct**: `fn_801FD6F0` (0x801FD6F0, 0x84) is unclaimed, starts exactly where
  this unit's claim ends, and still has `stub_227`. Its callee `fn_801FD774` (0x801FD774, 0x60) is the
  next thing after it.
- **Now stale**: `NEW: carve-801fdae8` — `fn_801FDAE8` is claimed and `Matching`
  (`configure.py:776`), landed in commit `8c0ec77b`. It should not be requeued.

I did not file a new `NEW:` line. The three remaining copies of this 0x74-byte shape are now all
claimed or `Matching`, and the callee they all share (`fn_801FD6F0`) is already queued by the line
above, so anything I filed would be the same work twice.