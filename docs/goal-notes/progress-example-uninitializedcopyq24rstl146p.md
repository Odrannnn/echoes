# progress-example-uninitializedcopyq24rstl146p

`module:ElitePirate`: **24 -> 25 matched functions**, from a new `Matching` unit
`ElitePirate/MetroidPrime/ScriptObjects/CElitePirateVecCopy` that owns exactly one function,
`fn_15_C02C` (`.text` `0xC02C..0xC094`, 0x68 = 104 bytes, 26 instructions), carved out of the middle
of the module's unclaimed `auto_00_00000178_text` run. No `asm`, no header change, no commit.

```
goal_check: item progress-example-uninitializedcopyq24rstl146p (progress) target=module:ElitePirate
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13191 -> 13192   linked 6245 -> 6246
  ok    check_symbol_names.py
  ok    All:  37.19% fuzzy, 30.62% matched, 13.51% linked (13192 / 28465 functions)
  ok    target rose: module:ElitePirate: 24 -> 25 / 241 functions
  ok    no asm added
goal_check: PASS progress-example-uninitializedcopyq24rstl146p
```

Measured separately, after the run:

```
cmp build/G2ME01/ElitePirate/ElitePirate.rel orig/G2ME01/files/RelProd/ElitePirate.rel  -> identical
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh        -> probe: 849 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0 duplicates)
python3 tools/check_symbol_names.py     -> checked 585 units; 0 declared names are missing from their object
python3 tools/check_files_cmake.py     -> every configured DOL object is either in files.cmake or excluded
python3 tools/check_decl_order.py --all-> ok: 1098 unit(s) checked, 37 permuted, all accounted for
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CElitePirateVecCopy.cpp
   .text  claimed 104  ours 104  retail 104  fits
   no extra functions: our object defines only what the retail unit object does
build/report.json, the unit: 100.0% fuzzy, 104/104 bytes, 1/1 matched functions
```

`total_functions` is still 28465, so the `splits.txt` edit cost nothing elsewhere.

## The declaration that produced the shape - copy this for the other 24

Two things, and the first is the one that is not a source question.

### 1. `mw_version="GC/2.7"` on the object. This is the whole reason the first attempt failed.

REL objects default to `GC/1.3.2` (`configure.py`'s `Rel(...)`, line 316-334). Under 1.3.2 this
function comes out **25 of 26 words**, with the 26th - `lwz r31,0(r3)`, the load of the by-value
`begin` iterator - sunk to sixth place:

```
retail / GC2.7                                GC1.3.2
  stw r31,0x1c(r1)                              stw r31,0x1c(r1)
  lwz r31,0(r3)        <- 2nd                    stw r30,0x18(r1)
  stw r30,0x18(r1)                                mr  r30,r5
  mr  r30,r5                                     stw r29,0x14(r1)
  stw r29,0x14(r1)                               mr  r29,r4
  mr  r29,r4                                     lwz r31,0(r3)   <- 6th
```

Everything else - frame, loop, stride, epilogue - is identical between the two compilers, and under
1.3.2 the **module hash breaks** (`ElitePirate.rel` differs from retail in 19 bytes at
`0xC100..0xC113`, which is that one word moving) while objdiff would still read 100% per function.
`GC/2.7` reproduces retail instruction for instruction. Same per-object override
`CLumiteRelTail.cpp` (`configure.py:1684`), `CSandBossRelTail.cpp` (`configure.py:1892`) and
`CGameOptions.cpp` (`configure.py:586`) already use, for the same kind of reason: a scheduler-order
difference between the two code generators and nothing else.

**Generalisable, and the point of this item: the 24 other copies of this shape in 24 modules should
expect the same thing.** Check which compiler reproduces retail's prologue *before* writing any of
them. Under the default REL compiler this shape is 25/26 words, which reads as a source problem and
is not one.

### 2. Two one-word iterator classes passed **by value**, the bound re-read, the element as `char*`.

```cpp
struct SElitePirateVecIter {          // one word, converting ctor
  void* mCur;
  SElitePirateVecIter(void* cur) : mCur(cur) {}
};

void* fn_15_C02C(SElitePirateVecIter begin, SElitePirateVecIter end, void* dst) {
  SElitePirateVecIter cur = begin;
  char* out = static_cast< char* >(dst);
  for (; cur.mCur != end.mCur; cur.mCur = static_cast< char* >(cur.mCur) + 104, out += 104) {
    fn_15_32C0(out, static_cast< const void* >(cur.mCur));
  }
  return out;
}
```

What is load-bearing, and why:

- **`begin`/`end` arrive by value as one-word classes.** `lwz r31,0(r3)` and `lwz r0,0(r29)` are
  the callee reading the *caller's* temporaries; mwcceppc passes a class-typed by-value parameter
  as an address. The converting constructor is what makes the caller build those temporaries - it
  produces `stw r6,0x8` / `stw r6,0xc` for `end` at `addi r4,r1,0xc` and `stw r0,0x10` /
  `stw r0,0x14` for `begin` at `addi r3,r1,0x14` in `fn_15_BF6C` (`.text` `0xBF6C`), byte for byte.
  A probe that reproduces `fn_15_BF6C`'s prologue exactly is how that was confirmed.
- **The bound is re-read from `end` every iteration** (`lwz r0,0(r29)` at the loop head), so it must
  not be hoisted into a local. `cur` is a *copy* of `begin`; `end` is kept by address in r29.
- **The element is reached as `char*` and its copy as an `extern "C"` name.** Declaring the record
  with a real copy constructor makes mwcceppc emit a weak inline copy into this object, which the
  0x68-byte claim has no room for and `unit_fit.sh` would list as a function retail's object does not
  define. Nothing in the body reads an offset, so the unit stays layout-immune.
- **The stride 104 is a literal here and does not have to be.** mwcceppc's induction-variable walk
  turns `++ptr` on a `T*` into `addi rPtr,rPtr,sizeof(T)`, which is why both matched twins stride by
  `sizeof` their element (`fn_801FF6B8` by 36, the `rstl` instantiation at `0x80137584` by 104) even
  though `rstl::pointer_iterator::operator++` is spelled `++this->current` on a plain `T*`
  (`include/rstl/pointer_iterator.hpp:78`). Measured with a scratch probe on this unit's exact
  command line: a one-word iterator over a 0x68-byte struct emits `addi r31,r31,0x68` and
  `addi r30,r30,0x68`. The `char*` spelling and the `T*` spelling give identical bytes.

## What the function really is, read off this module's own bytes

`rstl::uninitialized_copy` over a 0x68-byte record. Read from
`build/G2ME01/ElitePirate/asm/auto_00_00000178_text.s`:

- **Stride 0x68, in the bytes twice.** `fn_15_BF6C` (the block's `reserve`) allocates
  `mulli r3,r28,0x68` / `bl allocate__Q24rstl17rmemory_allocatorFi` and bounds its old-buffer walk
  at `add r31,r30,r0` with `r0 = self->x04 * 0x68`; `fn_15_C02C` strides both cursors by 0x68.
- **The callee is this module's own copy constructor**, `.text:0x32C0` = `fn_15_32C0` (0x20 bytes),
  forwarding to `fn_15_32E0` (0x28) = `if (dst == nullptr) return;` + `bl fn_15_3308`, which is the
  `if (p) p->T::T(const T&)` shape with destination in r3 and source in r4.
- **`fn_15_BF6C` calls it once** (`bl fn_15_C02C` at `.text 0xBFCC`), between building the two
  iterator temporaries and the destroy loop, so `fn_15_C02C` **is reachable from code inside the
  module** and needed no `force_active:` entry. The `ForgottenObject` / `ScriptCoin` dead-strip trap
  is an orphan; this is not one. (Checked rather than assumed: `fn_15_C02C` is absent from the
  module's generated `ldscript.lcf` FORCEACTIVE block both before and after the claim.)
- **The record's layout**, from `fn_15_3308` (0xC0 bytes) in ascending offset order: a `TUniqueId`
  at +0x00 (`lwz r5,0` / `lwz r0,4` -> `stw r5,0` / `stw r0,4`), two `bool`s at +8 and +9
  (`lbz`/`stb` each), eight floats at +0x0C..+0x28 (two `CVector3f`), an `rstl::basic_string` at
  +0x2C (`addi r4,r31,0x2c` / `addi r3,r30,0x2c` / `bl __ct__Q24rstl66basic_string<...>`), a `u16`
  at +0x3C, a float at +0x40, and a `CMatrix3f` at +0x44 (`bl __ct__9CMatrix3fFRC9CMatrix3f`).
  0x44 + 0x24 = 0x68. `fn_15_BF6C`'s destroy loop calls
  `internal_dereference__Q24rstl66basic_string<...>` on `elem + 0x2C`, which is that string.
- **The block** is `rstl::vector`'s three words at the same offsets: `x04` count, `x08` capacity
  (signed `cmpw`), `x0c` base pointer. `fn_15_36CC` fills it with `fn_15_BF6C(&block, 0x0e)` and
  then `fn_15_3424(self, lbl_15_rodata_0, 3, &block, ...)`.

## The carve: four files, one change

- `config/G2ME01/rels/ElitePirate/splits.txt:12` - `CElitePirateVecCopy.cpp` claims
  `.text 0x0000C02C end:0x0000C094`. It splits the unclaimed run into
  `auto_00_00000178_text` (0x178..0xC02C, 214 functions) and `auto_00_0000C094_text`
  (`fn_15_C094`, the `optional_object<CAABox>` ctor that `CElitePirateRel.cpp`'s `fn_15_10` calls),
  so nothing is left unclaimed and nothing is claimed twice.
- `configure.py:2121-2134` - the new `Object(Matching, ..., mw_version="GC/2.7")` in the existing
  `Rel("ElitePirate", ...)` block, with the reason above and the module's surrounding claim
  bookkeeping.
- `files.cmake:1189-1196` - the source, with the `CMetareeSwarmDes.cpp` / `DigitalGuardianVecList.cpp`
  reason: no `RELMain`/`RELExit`, so no flat-link collision, and the one relocation outside itself
  (`fn_15_32C0`) is behind the `#ifdef __MWERKS__` guard. Measured: the host object defines nothing
  and `nm -u` on it prints nothing, so the port's undefined count does not move (286 before and
  after, 0 duplicates).
- `src/MetroidPrime/ScriptObjects/CElitePirateVecCopy.cpp` - the claim itself.

`flip_test.sh` does not apply to a REL unit: it looks the source up under `extern/musyx/src/` and
reports "no source file ... this would pass while proving nothing". For a REL unit the acceptance
test is the module's sha1 against `config/G2ME01/config.yml`, which is `build/G2ME01/ok` - and that
target is in the default ninja graph, so `./tools/decomp_build.sh` fails on it. It failed on the
GC/1.3.2 attempt and passes now, with `ElitePirate.rel` `cmp`-equal to retail. The unit was already
`Matching`, so the check here is "does the build still reproduce retail with our object in the link",
which is what the hash answers.

## Traps hit (so nobody re-hits them)

- **`build/report.json`'s `address` is the object-relative offset and both address fields are
  decimal strings** (a previous lane's note; repeated here because it costs the same again).
- The `auto_*` units carry no `matched_functions` key at all, so a function sitting in one is
  invisible to the count. That is why this carve raises a number and a percentage alone does not.
- Scratch probes were `build/probe/*` (gitignored, now deleted). `tools/probe_offsets.cpp` is
  tracked and a scratch probe left there fails `goal_check.sh`'s "no judge-owned path touched".
  `build/probe/cc.sh` is the scratch compiler wrapper: the MWCC binaries are `.exe` under
  `wibo`, so a probe is `build/tools/wibo build/tools/sjiswrap.exe .../mwcceppc.exe <this unit's
  cflags> -c probe.cpp`, copied out of the object's ninja stanza in `build.ninja`.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were touched **only** by `gate.sh`'s
  `sync_state_block.py` / `check_docs_claims.py` derived-count rewrite (matched 13191 -> 13192, REL
  units 1662 -> 1663, probe 848 -> 849 files, 117 -> 118 units of our own code in 80 modules). Not
  edited by hand; the driver discards them.

No `NEW:` line is owed: the item's target moved 24 -> 25, the remaining work is the 24 sibling
copies the item's own `reason` already enumerates as the queue source, and the two findings above
(`mw_version`, the by-value iterator ABI) are codegen rules, which belong in notes rather than in
items.
