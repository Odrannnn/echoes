# progress-example-pushbackunsafeq24rstl63vecto

`progress` item on `module:EmperorIngStage1` (module 16). **PASS.** The module's summed
`matched_functions` went **22 -> 23 of 241**; the project's went **13192 -> 13193 of 28465**
(`linked` 6245 -> 6246). `EmperorIngStage1/MetroidPrime/ScriptObjects/CEmperorIngStage1469C` is
**1/1 at 100.00%** and objdiff `complete`, so the function is in the binary with our object in
the link, which is the one rule.

## What I did, and what this copy really is

**It is not a twin of the DOL function `twin_scan.py` paired it with. It is the same template
instantiation**, emitted into the module because the module's own object needs it. Four
independent facts in `build/G2ME01/EmperorIngStage1/asm/auto_00_00000000_text.s` say so:

1. the element is `CJointCollisionDescription`, 0x68 bytes - the header already asserts it
   (`CHECK_SIZEOF(CJointCollisionDescription, 0x68)`), and the module's own copy constructor
   `fn_16_471C` (0x471C, 0xC0) walks exactly that header's member list, offsets included;
2. the container is retail's `rstl::vector` (+0x00 allocator, +0x04 count, +0x08 capacity,
   +0x0C items), which is what `fn_16_469C` reads;
3. the callee is this module's **own** three-level chain - `fn_16_46D4` (0x20) ->
   `fn_16_46F4` (0x28, `cmplwi r3,0 / beq`) -> `fn_16_471C` (0xC0) - which is
   `rstl::construct<CJointCollisionDescription>` / `construct_impl<...>` / the copy ctor. The
   DOL's is 0x8013703C / 0x8013705C / 0x801370F4, same three shapes, same sizes;
4. the three callers (`bl fn_16_469C` at +0x3F9C, +0x4174, +0x4470) push_back_unsafe a
   description built by the **imported** `SphereCollision__26CJointCollisionDescription...`
   (+0x3F70, +0x4148) and `OBBAutoSizeCollision__...` (+0x4420) into a local vector, then hand
   it to `__ct__22CCollisionActorManagerFR13CStateManager9TUniqueId7TAreaIdRCQ24rstl63vector<26CJointCollisionDescription,...>`
   (+0x3FF4, +0x41C0, +0x44F0) as a `const rstl::vector<CJointCollisionDescription>&`.

Nothing in the module has a `push_back_unsafe` with a 0x68 stride other than this one, and the
`CCollisionActorManager` ctor's parameter type names both the container and the element.

### The declaration that produced the shape (for the next module that has one)

```cpp
class CEmperorIngStage1JointVector {        // retail's rstl::vector layout, offsets stated
  rstl::rmemory_allocator mAllocator;       // +0x00
  int mCount;                               // +0x04
  int mCapacity;                            // +0x08
  CJointCollisionDescription* mItems;       // +0x0C
public:
  void push_back_unsafe(const CJointCollisionDescription& in);
};
// .text 0x469C, 0x38 bytes
void CEmperorIngStage1JointVector::push_back_unsafe(const CJointCollisionDescription& in) {
  fn_16_46D4(mItems + mCount++, in);        // this module's rstl::construct<T>, still retail
}
```
compiled with the module's own cflags, mwcceppc GC/1.3.2, emits
`push_back_unsafe__28CEmperorIngStage1JointVectorFRC26CJointCollisionDescription` (`T`, global)
and one `U fn_16_46D4`, and the 56 bytes are byte-identical to retail's.

Three things are load-bearing, all measured:
- **the callee is named, not inlined.** `rstl::construct` is unreachable without also emitting
  `construct<T>` and `construct_impl<T>`, which are this module's 0x46D4 and 0x46F4 and are **not**
  in this claim, so the only name the object can use is the retail one.
- **`mItems + mCount++` in that order.** MWCC loads the count into r5 once, `mulli` into r0,
  `addi` r5, `stw`, then `add r3, r6, r0`, then the call. It is that schedule, not the arithmetic.
- **the stride comes through the member**, from `sizeof(CJointCollisionDescription)`.

### Why a local class and not the instantiation itself - **negative result, measured 2026-10-02**

An explicit specialisation of a member of a class template,

```cpp
namespace rstl { template <> void vector<CJointCollisionDescription, rmemory_allocator>::
                     push_back_unsafe(const CJointCollisionDescription& in) {
                     construct(mItems + mCount++, in); } }
```

**compiles clean under mwcceppc 1.3.2 and produces an object with no `.text` at all** - a
408-byte `.o` whose only section is `.comment`. MWCC accepts the syntax and instantiates nothing,
so this one instantiation cannot be *requested* from a translation unit, and the out-of-line
definition of `vector<T,Alloc>::push_back_unsafe` itself is emitted only when something calls it
(forcing a call would add a second function to the object and break the one-range-per-unit claim).
The local class above is the same arrangement `EmperorIngStage1Accessors.cpp` records for this
module's accessors: state the offsets literally so the unit is layout-immune.

## The four files, and the rename

- `src/MetroidPrime/ScriptObjects/CEmperorIngStage1469C.cpp` - new, the 56 bytes and the evidence
  above; in `files.cmake` behind an empty host branch (its callee does not exist on the host), the
  guard `CIngBoostBallGuardianA91C.cpp` uses.
- `config/G2ME01/rels/EmperorIngStage1/splits.txt` - `CEmperorIngStage1469C.cpp .text
  0x0000469C..0x000046D4`, one contiguous range, no unclaimed gap.
- `configure.py` - `Object(Matching, "MetroidPrime/ScriptObjects/CEmperorIngStage1469C.cpp")` in
  the `EmperorIngStage1` `Rel(...)` block.
- `config/G2ME01/rels/EmperorIngStage1/symbols.txt` - **one** rename, replacing the line rather
  than being inserted beside it (a dtk parse error):
  `fn_16_469C` -> `push_back_unsafe__28CEmperorIngStage1JointVectorFRC26CJointCollisionDescription`.
  **No callee rename**: `fn_16_46D4` keeps its retail name, so the call matches the reference
  relocation by name. The three `bl fn_16_469C` sites in the auto object follow the rename and
  resolve against this unit's definition.
- `files.cmake` - listed, with the one-line reason.

**No `force_active:` needed, and that is measured.** The renamed symbol is not in
`build/G2ME01/EmperorIngStage1/ldscript.lcf`'s FORCEACTIVE list, but `auto_00_00000000_text.o`
calls it three times (+0x3F9C, +0x4174, +0x4470, each inside an unclaimed function), so dtk's
own object holds the reference. `audit_rel_claim.py` prints `preplf 241 text symbols, plf 241,
0 dropped by -strip_partial`, and the module hashes - which is the dead-strip test.

## What I measured

```
./tools/decomp_build.sh -r EmperorIngStage1
  All: 37.20% fuzzy, 30.63% matched, 13.51% linked (13193 / 28465 functions)
  87 files OK
```
- module sum, `build/report.json`: `.../CEmperorIngStage1469C` **1/1 at 100.00%**,
  `.../CEmperorIngStage1Rel` 3/3, `.../EmperorIngStage1Accessors` 14/14, `.../REL/REL_Setup` 5/5.
  Sum **23**, was 22 (`build/goal/judge/report.base.json`). `total_functions` still **28465**.
- `EmperorIngStage1.rel` sha1 `a0a9964016d7569c980c22daf7789bd6f32fc226`, `cmp`-equal to
  `orig/G2ME01/files/RelProd/EmperorIngStage1.rel` and equal to `config/G2ME01/config.yml`;
  `main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/audit_rel_claim.py EmperorIngStage1`: `ok CEmperorIngStage1469C.cpp
  0x0000469C..0x000046D4 1/1`, **0 claims with a problem**.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CEmperorIngStage1469C.cpp`:
  `.text claimed 56 ours 56 retail 56 fits` / `no extra functions`.
- `python3 tools/check_decl_order.py --unit CEmperorIngStage1469C`: `ok`.
- `python3 tools/check_symbol_names.py`: `checked 585 units; 0 declared names are missing`.
- port: `849 files, 0 failed`, `LINKED (286 undefined, 0 duplicates)`; the judge's baseline is
  **286**, so listing the file in `files.cmake` cost the port's link nothing.
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, gate green,
  `target rose: module:EmperorIngStage1: 22 -> 23 / 241 functions`, `no asm added`.
  gate's per-function diff: `SPLIT ... auto_00_00000000_text: 96 function(s) moved into
  ...CEmperorIngStage1469C, ...auto_00_000046D4_text (exact count match - a split, not a loss)`.

`check_docs_claims.py` without the judge's `--write` reports only the derived counts this change
moves (the HANDOFF state block's 13193/28465, 6246, REL 1663, the 118-units module-wiring count
and the probe's 849 files). Per `docs/goal-unit-prompt.md` the judge rewrites those from the
tree, so I did not hand-edit the docs.

## `flip_test.sh` cannot judge REL units here - pre-existing, not caused by this change

```
$ ./tools/flip_test.sh MetroidPrime/ScriptObjects/CEmperorIngStage1469C.cpp
    no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CEmperorIngStage1469C.cpp) ...
  FAIL  -> reverted
```
It is **not** a missing file. `flip_test.sh`'s `unit_info` picks the source root by counting
unbalanced `(` before the entry, and on `configure.py` as committed that says `extern/musyx/src`
for **every** REL unit - `progress-twin-rel-emperoringstage1`'s notes record the same FAIL for
that item's committed `Matching` units and replay the regex against `git show HEAD:configure.py`
to show it. It is a latent tool defect, not a property of my unit, and a `progress` item is not
judged on the flip. **The REL equivalent of the flip is the module sha1, and it holds.** Not filed
as `NEW:` - a tooling defect does not raise a count.

## What is left in the module (218 unmatched functions)

- **Measured, and ready:** `fn_16_46D4` / `fn_16_46F4` / `fn_16_471C`, `.text 0x46D4..0x47DC`
  (0x108, three functions) - this module's `rstl::construct<CJointCollisionDescription>`,
  `construct_impl<...>` and the copy constructor, which together are exactly what MWCC emits when
  a translation unit calls `rstl::construct<CJointCollisionDescription>`: a scratch compile
  produced `construct<26CJointCollisionDescription>__4rstlFPvRC26CJointCollisionDescription` at
  0x20 bytes, `construct_impl<...>` at 0x28 and `__ct__26CJointCollisionDescription...` at 0xC0 -
  the same three sizes as 0x46D4/0x46F4/0x471C. **Two traps I measured but did not solve:** that
  compile also emits `__dt__Q24rstl66basic_string<...>Fv`, which is not in 0x46D4..0x47DC and
  would have to be kept out; and `construct<T>` comes out as a **local** (`t`) symbol, so it will
  need `force_active:` where `fn_16_46D4` is not in FORCEACTIVE. I did not match them, so I am
  not claiming they will.
- `auto_00_0000A2A0_text` (32) is the module's generated entity loader plus a 30-way typedef
  switch; it needs the `CEntity`/`CPatterned` hierarchy this tree does not model. Untouched.
- The rest is the module's class code. Start from a named class the way the previous item's notes
  say, not from the twin list.

NEW: progress-twin-rel-emperoringstage2tentacle | progress | module:EmperorIngStage2Tentacle | fn_17_EB4 (.text 0xEB4, 0x38 B) is the same vector<CJointCollisionDescription>::push_back_unsafe as this item's fn_16_469C, calls this module's own fn_17_EEC, and has one caller at +0xC9C holding the link reference; copy the declaration in CEmperorIngStage1469C.cpp