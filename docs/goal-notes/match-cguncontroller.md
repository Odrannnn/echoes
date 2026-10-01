# match-cguncontroller — `MetroidPrime/Weapons/GunController/CGunController`

`kind: match`. **The unit is `Matching`.** `tools/flip_test.sh` -> `PASS -> kept as Matching`, and
`tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS match-cguncontroller`**. This is a
complete flip, not the partial the previous two runs filed.

Baseline re-measured on the clean tree first: **11 / 12 functions matched**, unit fuzzy 91.52%,
`All: 32.82% fuzzy, 25.64% matched, 12.15% linked (11433 / 28465)`. Only `fn_801DC820` was left
(232 bytes, 0.00%). After: **12 / 12**, unit fuzzy **100.00%**, `matched_code` 2504 -> 2736 of 2736,
`All:` counts 11433 -> **11434** matched and 5572 -> **5584** linked.

## What `fn_801DC820` is

`CPASAnimParmData`'s **copy constructor**. `EnterStruck` is its only caller in retail (`bl
fn_801DC820` at 0x801DCA04, the one call site in the whole DOL), and this unit's split
(0x801DC1B8..0x801DCC68) claims its address, so this is the unit that has to define it.

Two earlier runs left notes here saying it was unrecoverable because it is unnamed in
`config/G2ME01/symbols.txt` and Prime 1's source has no such copy. Both were right about the
symbol and wrong about the obstacle: **an unnamed symbol is a declaration problem, not a
recovering-the-source problem.** `EnterStruck`'s own copy at `r1+196` told us a copy of a whole
`CPASAnimParmData` happens; all that was missing was somewhere for the compiler to put the
definition and a header to declare it in.

## The three changes

### 1. `include/Kyoto/Animation/CPASAnimParmData.hpp` — declare it

```cpp
CPASAnimParmData(const CPASAnimParmData& other);
```

Declaring it (rather than leaving it implicit) is what lets this unit define it. Declared in the
header, defined in the `.cpp` that claims its bytes — the same arrangement as `fn_800D042C`, which
`include/Collision/CCollisionInfo.hpp` declares and `src/MetroidPrime/Player/CMorphBall.cpp`
defines, because the `Collision/` unit that would own it is byte-exact and adding a definition
there would change `main.dol`.

### 2. `src/.../CGunController.cpp` — define it, in retail's position

```cpp
CPASAnimParmData::CPASAnimParmData(const CPASAnimParmData& other)
  : mStateId(other.mStateId)
  , mParms(other.mParms) {}
```

The body is `rstl::reserved_vector`'s copy constructor (`include/rstl/reserved_vector.hpp`).
**Where it is declared in the file is load-bearing:** retail's order is `LoadFidgetAnimAsync`
0x801DC7F0, `fn_801DC820` 0x801DC820, `EnterStruck` 0x801DC908, and mwceppc emits definitions in
reverse source order, so this sits between `EnterStruck` and `LoadFidgetAnimAsync`. One slot off,
the bytes are still byte-identical and the flip still fails — `python3 tools/check_decl_order.py
--unit MetroidPrime/Weapons/GunController/CGunController` -> `none emits its functions out of retail
order`.

### 3. `include/Kyoto/Animation/CPASAnimParm.hpp` — `CPASAnimParm` is trivially constructible

This is the one that makes the 232 bytes. The header had only
`RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(CPASAnimParm)`. Change it to
`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CPASAnimParm)` (which subsumes the destructible one).

**Measured, in this order, on this unit:**

| spelling of the copy ctor | our size | fn_801DC820 |
|---|---|---|
| `mParms.mCount = other...; memcpy(mParms.data(), other..., mParms.mCount * sizeof(CPASAnimParm))` | 0x58 | 0.00% |
| `: mStateId(other.mStateId), mParms(other.mParms)`, `CPASAnimParm` only destructible | 0x50 | 0.00% |
| same, `CPASAnimParm` trivially constructible | **0xE8** | **byte-identical** |

Why: `rstl::construct` has two forms. `construct_impl` (the trivially-constructible one) is
`*static_cast< T* >(dest) = src`; the default is `new (dest) T(src)`, and mwcceppc expands
placement `new` into "call `operator new`, **test the result against null**, then construct" — and
that test survives inlining. So the 0x50-byte version is a one-element-per-iteration loop with a
`cmplwi`/`beq` null guard in it; retail has no null test and copies eight elements per iteration
(`srwi. r0,r5,3` / `mtctr` / a 64-byte body / an `andi. r5,r5,7` remainder loop). Retail's own
lowering is the evidence for which form it used. The same note already exists on
`fn_800D042C` in `include/Collision/CCollisionInfo.hpp`, for the same reason.

**The `memcpy` spelling is a trap worth recording:** it looks right, it is 0x58 bytes, and it is
not retail's function. Retail's 232 bytes read the count back out of the **destination**
(`lwz r5,4(r3)` at 0x801DC838) and loop on it, which only `uninitialized_copy_n` over the vector's
own count does.

## Evidence

- `fn_801DC820`: our `.text` 0x6F4..0x7DC == retail object `.text` 0x668..0x750, 232/232 bytes
  `IDENTICAL` (compared with `objcopy -O binary` on `build/G2ME01/obj/.../CGunController.o` and
  `build/G2ME01/src/.../CGunController.o`).
- `./tools/flip_test.sh MetroidPrime/Weapons/GunController/CGunController.cpp` -> `PASS -> kept as
  Matching`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors`; `python3
  tools/check_symbol_names.py` -> `514 units; 0 declared names are missing from their object`.
- `./tools/decomp_build.sh` -> `All: 32.82% fuzzy, 25.64% matched, 12.15% linked (11434 / 28465
  functions)` (matched did not fall; it rose).
- `./tools/goal_check.sh build/goal/item.json` -> **PASS**, whole gate including the DOL sha1, all
  86 RELs, the report diff and the port probe.
- `unit_fit.sh` reports 6 functions ours has that the retail object does not, 676 bytes, all weak
  COMDAT template destructors (`__dt__Q24rstl42vector<6CToken,...>`, `__dt__9CGSFidgetFv`,
  `__dt__16CPASAnimParmDataFv`, `__dt__Q24rstl33reserved_vector<12CPASAnimParm,8>Fv`, ...) plus
  `__ct__16CPASAnimParmDataFRC16CPASAnimParmData` itself, which retail names differently. That is
  the harmless class it describes, and `flip_test.sh` decided it.

## Files changed

- `include/Kyoto/Animation/CPASAnimParmData.hpp` — copy constructor declared (+11 with the note).
- `include/Kyoto/Animation/CPASAnimParm.hpp` — `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` in place of
  `RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE` (+9/-1 with the note).
- `src/MetroidPrime/Weapons/GunController/CGunController.cpp` — the copy constructor defined
  between `EnterStruck` and `LoadFidgetAnimAsync`, with the note.
- `configure.py` — `NonMatching` -> `Matching` for this one unit. **That edit is
  `flip_test.sh`'s own**, not mine; it left it behind when it kept the unit.

No `config/` file, no `splits.txt`, no `files.cmake`, no `tools/`, no `build/goal/`, no `asm`, no
commit. `git status --short` shows exactly those four files. `check_docs_claims.py` reports only
the three HANDOFF state-block counts as stale, which the driver re-derives.

## Notes for whoever picks up `CPASAnimParm`

`RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CPASAnimParm)` is in a shared header, so it can move code
in any unit that copies a `CPASAnimParm`. Nothing moved here — the DOL hash is unchanged and
`All:`'s matched count rose — but that header edit is the one thing in this change with a blast
radius, and a future item that needs the placement-`new` form of `construct` for this type should
know why the flag is set. No `NEW:` item: this one is finished.