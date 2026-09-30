# progress-prime1-cscriptplatform-ridervec

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| `RemoveRider__15CScriptPlatformF9TUniqueId` | 35.22% / 128 B | **100.00%** / 204 B |
| `fn_800A1180` | 0.00% | **100.00%** |
| `fn_800A1148` | 0.00% | **100.00%** |
| unit `matched_functions` | 20 / 60 | **23** / 60 |
| unit `fuzzy_match_percent` | 21.83% | 23.04% |
| tree `matched_functions` | 10011 / 28465 | **10014** / 28465 |

`tools/gate.sh build/goal/judge/report.base.json` prints its own line
`per-function diff  matched 10011 -> 10014  linked 4896 -> 4896  (+3 functions at 100%, 0 units newly linked)`.
A full per-function diff against the judge baseline over **every** unit:

```
better 3   worse 0   missing-now 0
  BETTER  35.22 -> 100.00 RemoveRider__15CScriptPlatformF9TUniqueId
  BETTER   0.00 -> 100.00 fn_800A1148
  BETTER   0.00 -> 100.00 fn_800A1180
```

## Two findings, both of which correct the item's `reason`

### 1. The erase chain already existed in our object, byte-identical

`reason` says `fn_800A1004 -> fn_800A1050 -> fn_800A1148 -> fn_800A1180` "does not exist in our
object". It does, and every link was already byte-for-byte retail. Census, by dumping both `.text`
sections and searching ours for a byte-identical copy of each retail function at *any* offset:

```
$ build/binutils/powerpc-eabi-objcopy -O binary --only-section=.text \
      build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o  /tmp/retail_text.bin  # 18000 B
$ build/binutils/powerpc-eabi-objcopy -O binary --only-section=.text \
      build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptPlatform.o  /tmp/ours_text.bin    # 7584 B
```

| retail symbol | size | identical copy in our `.text` at | what it is in our object |
|---|---|---|---|
| `fn_800A1004` | 76 B | 0x1ca8 | `erase<vector<SRiders>>::erase(iterator)` (`w`) |
| `fn_800A1050` | 248 B | 0x1cf4 | `erase<vector<SRiders>>::erase(iterator, iterator)` (`w`) |
| `fn_800A1148` | 56 B | 0x108c | `rstl::destroy<pointer_iterator<SRiders,...>>` (`l`) |
| `fn_800A1180` | 28 B | 0x10c4 | `rstl::destroy_impl<pointer_iterator<SRiders,...>>` (`l`) |

`fn_800A31A0` (132 B, the `rstl::vector` destructor) and `fn_800A4038`/`fn_800A4090` (88 B each) are
identical too. So no helper had to be created; only two of the four could not be *counted*.

### 2. `extern "C"` is how a C++ unit acquires a retail `fn_XXXXXXXX` name — that was the real blocker

`config/G2ME01/symbols.txt` lines 3141-3144 name those four after their own addresses
(`fn_800A1004 = .text:0x800A1004`), so retail's DOL has **no symbol** there and dtk auto-named them.
objdiff pairs a target function to a base function *by name*, so with our mangled
`erase__Q24rstl43vector<7SRiders,...>` spelling they can never score, however good the bytes are.
MWCC mangles a plain global `void f()` to `fFv` and a member to `f__<len><Class>CFv`, so no
declaration reaches `fn_800A1180`. **`extern "C"` does**, because it suppresses mangling — and that is
already this tree's convention for unnamed retail functions, in 268 places:

```python
# every function in build/report.json at fuzzy_match_percent == 100.0 whose retail name is fn_[0-9A-F]{8}
... -> 268, e.g. main/MetroidPrime/CAnimData fn_80027B44, main/MetroidPrime/CStateManager fn_800436CC
# src/MetroidPrime/CAnimData.cpp:368
extern "C" void fn_80027B44(const SModelHolder* holder, int part) { ... }
```

That was the missing piece, and it turned the two small links of the chain from 0.00% to 100.00%.

## What I changed

One file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`, three bodies.

**`RemoveRider` (35.22% -> 100.00%).** The erase chain was never what stopped it: it was written as
a hand-rolled `for` loop comparing `it->mUid == id`, while retail builds a whole `SRiders` temporary
on the stack and searches with `rstl::find` — the spelling `IsRider` (already 100%) and `IsSlave`
(95.29%) use. The `rstl::find` rewrite alone gave 88.96%; inverting the `if` gave 100.00%.

**`fn_800A1180` and `fn_800A1148` (0.00% -> 100.00% each).** `rstl::destroy` and
`rstl::destroy_impl` for `pointer_iterator<SRiders, ...>`, named as retail names them, with the
`pointer_iterator` (one word) replaced by the ABI-identical `SRiders* const*` so the `extern "C"`
signature keeps retail's argument passing.

Both are declared **after** `IsRider` and **before** `RemoveRider`, i.e. descending by retail offset
(`fn_800A1180` 0xf80, then `fn_800A1148` 0xf48) — mwcceppc emits definitions in reverse source order,
so the file is already ordered that way. `python3 tools/check_decl_order.py --unit
MetroidPrime/ScriptObjects/CScriptPlatform` -> `ok: 1 unit(s) checked, none emits its functions out of
retail order`. `.text` grew 7584 -> 7744 B, exactly the 76+56+28 the three added bodies cost, and
`tools/unit_fit.sh` still lists neither of the new names as extra (it pairs them with retail's).

### Codegen rules measured here (all reusable)

- **MWCC lays out `if (cond) { return false; } ...; return true;` with the early return as the
  fall-through**, so the test becomes `bne <over the false path>` and the false path is `li r3,0;
  b <end>`. The natural `if (it != end) { erase; return true; } return false;` gives `beq <skip>` plus
  an extra unconditional `b <end>` after `li r3,1` — one instruction more, wrong shape. That is the
  whole of the 88.96% -> 100.00% step, nothing else changed.
- **objdiff compares a `bl` by the callee's bytes, not the callee's name.** The pretty diff still
  prints `DIFF_ARG_  bl fn_800A1004  |  bl erase__Q24rstl43vector<7SRiders,...>FQ...` while the
  function scores 100.00%. So an unnameable callee does **not** block its callers — the previous run's
  "those four cannot match until the out-of-line erase reproduces it" was a byte problem wearing a
  naming problem's clothes. Do not write off a function because it calls a `fn_800*` helper.
- **An empty loop body does not get deleted if it contains `p->~T()` for a class with no declared
  destructor** — that is exactly why the template's `destroy_impl` survives as a 28-byte loop
  (`lwz/lwz/b/addi 60/cmplw/bne/blr`), and `fn_800A1180` reproduces it with a plain
  `for (SRiders* cur = *first; cur != *last; ++cur) { cur->~SRiders(); }`. A literal empty body would
  have been optimised away.
- **MWCC gives the first-declared local `r1+0xc` and the second `r1+0x8`, but emits the `lwz`/`addi`
  pair for each in declaration order.** `fn_800A1148` needs the *opposite* orders: `last` (r4) loaded
  first but stored at `r1+8`, `first` (r3) loaded second but stored at `r1+0xc`. Two locals with
  initialisers give one order or the other (98.29%, then 99.71%, neither enough); declaring both
  uninitialised and assigning in the order `localLast = *last; localFirst = *first;` gets both
  orders at once and lands on 100.00% in one step.

## What is left in the chain, and why it is not here

`fn_800A1004` (76 B) and `fn_800A1050` (248 B) are still 0.00%. Both are
`rstl::vector<SRiders>::erase`, and naming them is the same one-line trick — what is left is writing
them as hand-rolled pointer C++ that MWCC allocates identically to the template, which is a
different job from this item's. Everything measured for whoever takes it, from
`build/G2ME01/obj/.../CScriptPlatform.o` (relocations in brackets):

- **`fn_800A1050`** — frame 0x30, `stmw r26,24(r1)`. r3 = `iterator*` out (sret, 1 word),
  r4 = `this` (the vector), r5 = `iterator* last`, r6 = `iterator* first`; **three further stack words
  at `r1+8/12/16` are stored by the caller and never read** — dead outgoing-argument space, so the
  signature can stay at four words. Order: `destroy(first, last)` via `fn_800A1148`; then
  `r26 = (first - mItems) / 60` computed as the signed-division-by-60 sequence
  (`subf/mulhw 0x2C2C2C2C/add/srawi 5/srwi 31/add/mulli 60`), `r27 = mItems + r26*60`; then the shift
  loop, whose body is a plain `SRiders` **assignment** — `sth` the 2-byte `mUid` at +0, `stb` the
  optional flag at +8, `lfs/stfs` the timer at +4 only when the flag is set, then
  `CTransform4f::CTransform4f(const CTransform4f&)` for the 48 bytes at +12 — with
  `cmplwi r27,0; beq` guarding it and `r27/r26/r31` each += 60/+1/+60; loop condition is
  `mItems + mCount*60`; finally `mCount = r26` and `*out = first`.
- **`fn_800A1004`** — frame 0x20, only r31 saved. r3 = out, r4 = `this`, r5 = `iterator* first`;
  computes `first + 60` into r7, then stores `[r1+8] = [r1+12] = first+60` and `[r1+16] = first`, sets
  r5 = `r1+16`, r6 = `r1+12`, and calls `fn_800A1050`. i.e. `erase(it) { return erase(it, it + 1); }`.

`DecayRiders` (1.33%), `MoveRiders` (0.45%) and `DragSlaves` (0.83%) call the same chain, so the
chain is no longer what stops them — their own bodies are, and that is a separate item.
`fn_800A14DC` (100 B) is the one helper of the group genuinely absent from our object, still the open
`progress-prime1-cscriptplatform-slavevec`.

Decoded for whoever takes `DecayRiders` (0x3480, frame 0x50, `f31 = dt`, cursor local at `r1+36`
seeded from `riders.mItems`; relocations `lbl_8041B01C` = 0.0f, `fn_800A1004`, `kInvalidUniqueId`,
`DeliverScriptMsg`):

```cpp
for (it = riders.begin(); it != riders.end(); ) {
  if (it->mDecayTimer.mValid) {                       // lbz r0,8(r3); beq next
    it->mDecayTimer.mValue -= dt;                     // lfs/lfs/fsubs/stfs/lfs
    if (!(it->mDecayTimer.mValue <= 0.f)) {           // fcmpo; cror eq,lt,eq; bne next
      riders.erase(it);
      it = <erase's return value>;                    // 0x3528: stw r8,36(r1) - the ITERATOR
      mgr.DeliverScriptMsg(<16-byte msg at r1+40>);
      continue;
    }
  }
  ++it;                                               // addi r0,r3,0x3c
}
```

Two traps there: the expiry test is `f1 <= 0.f` written `fcmpo / cror eq,lt,eq / bne`, and the loop
advances from **`erase`'s returned iterator**, so that call site is `it = riders.erase(it)`, not a
pointer walk. The message is 16 bytes — three `TUniqueId`-sized shorts at +0/+2/+4
(`kInvalidUniqueId`, `kInvalidUniqueId`, the rider's id), `0x584F4E50` at +8 (that is
`'X','O','N','P'`, materialised `lis r3,22607; addi r5,r3,20048`, which a plain `0x584F4E50` literal
may not reproduce — MWCC would normally use `ori`) and `-1` at +12. A second, apparently dead, 16-byte
struct with the same three shorts is built at `r1+8` after the erase: retail's source constructs the
message twice and that has to be reproduced, not deleted.

Walls already recorded by the previous run, re-measured unchanged and **not** re-filed:
`GetSortingBounds` 92.12% is the same 92.12% as `CScriptActor::GetSortingBounds` in this tree with
identical source; `GetTouchBounds` 93.10%, `IsSlave` 95.29% and `BuildNearListFromRiders` 98.01% are
all the cyclic shift of the five callee-saved registers; `__dt__` 77.78% and `__ct__` 42.52% are the
member-order problem filed as `progress-prime1-cscriptplatform-dtor`.

## Verified

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                       -> All: 30.83% fuzzy, 23.11% matched, 11.74% linked (10014 / 28465 functions)
./tools/probe_sources.sh                      -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 503 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                              -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`gate.sh` is **ok on every step except `docs claims`**, which reports only the two derived
state-block numbers this change moved:

```
missing: 'matched    10014 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8603 / 16726 functions'  (HANDOFF state block: DOL matched)
```

Per the brief I did not hand-edit `docs/HANDOFF.md`: `tools/gate.sh:115` runs
`check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and `goal_check.sh:105` invokes the gate with
`MP_GATE_DOCS_WRITE=1`, so the judge rewrites them. All 86 RELs `cmp`-equal with sha1s matching
`config/G2ME01/config.yml` is the gate's own `hashes vs config.yml  ok` step.

Diff is **one file**, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`: no `.s`, no `asm`, no
`tools/`, no `config/`, no `docs/`, no `build/goal/`. Not committed. `objdiff.json` was deliberately
not touched: it is generated and gitignored, so a `symbol_mappings` entry there would make this look
matched in a way no committed change supports.

## NEW:

NEW: progress-prime1-cscriptplatform-erase-pair | progress | MetroidPrime/ScriptObjects/CScriptPlatform | fn_800A1004 (76 B) and fn_800A1050 (248 B) are the rstl::vector<SRiders>::erase pair, already byte-identical in our object but 0.00% because retail names them after their own addresses; extern "C" fixes the naming (fn_800A1148/fn_800A1180 done this way and are now 100%), and the 248-byte body is decoded in docs/goal-notes/progress-prime1-cscriptplatform-ridervec.md - it needs sr26 = (first - mItems)/60, a shift loop that assigns mUid/optional timer/CTransform4f per element and ends with mCount = r26 and *out = first
