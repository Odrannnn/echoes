# progress-unit-cgamearea-undefined — DONE, +19 functions

`MetroidPrime/CGameArea` **252 -> 271 / 305** matched functions (global `matched_functions`
11889 -> 11908). `tools/goal_check.sh build/goal/item.json` prints **PASS**. No commit made.

## What the item actually was

The queue entry said "30 of the unit's 53 unmatched functions are bodies we never define". Measured
before acting: 53 unmatched, of which **34 are at 0%**, and 31 of those 34 are symbols our object
does not define at all (`nm build/G2ME01/src/MetroidPrime/CGameArea.o` vs
`build/G2ME01/obj/MetroidPrime/CGameArea.o`: exactly **33 names retail defines and we do not** —
the 31 `fn_*` plus `CPostConstructed`'s ctor and dtor, which we do define under a different
spelling). So the work list was those 33, and they are reachable.

**The shape that made them reachable.** Retail's symbol table has a *real* global symbol named
`fn_XXXXXX` at each of these addresses, and the tree already has the precedent for writing one:
`src/MetroidPrime/Player/CGameStateBlockDtor.cpp` defines `fn_80004A4C` as an
`extern "C" SGameStateBlock* fn_80004A4C(SGameStateBlock* self, int flag)` that reproduces an
mwceppc **deleting destructor** (hidden `int` flag in `r4`, `extsh. r0,r31 ; ble` before
`CMemory::Free(this)`), and that unit is `Matching`. So the unnamed deleting destructors here are
written the same way: an `extern "C"` function with the retail name and a `flag` parameter.

Second fact that made it cheap: **objdiff pairs functions by name and its per-function fuzzy score
does not compare a call's relocation target.** `rstl::vector<TEditorId>::operator=` scores 100%
against retail even though retail's `bl` goes to `fn_800542FC` (0x800542FC) and ours goes to
`clear__Q24rstl45vector<9TEditorId...Fv` (0xC1AC). Retail inlined these small members and dtk
named the resulting unnamed COMDAT copies `fn_<addr>`; we emit them under their proper names. The
only thing that has to be right is the instruction sequence of the function carrying the `fn_`
name.

## The 19 matched (`src/MetroidPrime/CGameArea.cpp`, all `extern "C"`)

Each was verified instruction-for-instruction against the retail object (`.tmp/opencode/cmp.py`:
branch targets and `R_PPC_REL24` operands normalised, everything else compared).

Deleting destructors, `if (self) { <destroy payload>; if ((short)flag > 0) CMemory::Free(self); }`:

| fn | retail | element / member it destroys | fixed by |
|---|---|---|---|
| `fn_8005E24C` | 0x84 | `vector<CMetroidModelInstance>`, stride 124 | `mModelInstances` |
| `fn_8005E2D0` | 0x54 | `vector<CWorldLight>` (trivial) | `mLightsA`/`mLightsB` |
| `fn_8005E324` | 0x58 | `single_ptr<CPortalArea>` | `mPortalArea` |
| `fn_8005E37C` | 0x84 | `vector<auto_ptr<char>>`, stride 8 | `mLayerScriptBuffers` |
| `fn_8005E468` | 0x58 | `single_ptr<CScriptObjectLoaderHelper::SLoadContext>` | `mScriptLoadState` |
| `fn_8005E4C0` | 0x84 | `vector<vector<CToken>>`, stride 16 | `mLayerTokens` |
| `fn_8005E544` | 0xA8 | `vector<pair<CARAMToken,int>>`, stride 36 | `mAramTokens` |
| `fn_8005E5EC` | 0x84 | `vector<vector<CRELFileToken>>`, stride 16 | `mLayerRelTokens` |
| `fn_8005E670` | 0x54 | `vector<CRELFileToken*>` (trivial) | `mSortedRelTokens` |
| `fn_8005E6C4` | 0x84 | `vector<vector<TEditorId>>`, stride 8 | `mLayerEditorIds` |
| `fn_8005E748` | 0x84 | `vector<pair<auto_ptr<char>,int>>`, stride 12 | (12 = 8 + 4) |
| `fn_80060434` | 0x88 | `list<vector<uint>>` | `x194_` |
| `fn_80060500` | 0x74 | `list<TUniqueId>` | `mDockIds` |
| `fn_80060750` | 0x74 | `list<CWorld::SLayerRelUnload>` | |
| `fn_80060628` | 0xB4 | `list<auto_ptr<CDvdRequest>>` | `mLoadTransactions` |
| `fn_80060378` | 0xBC | `list<pair<int, auto_ptr<CDvdRequest>>>` | |

Plus three small ones: `fn_800542FC` (0x0C, the inlined `vector<TEditorId>::clear()`, i.e.
`mCount = 0`), `fn_800604DC` (0x24, `~vector<uint>` as a forwarder) and `fn_800604BC` (0x20, the
destructor frame above it).

Two spellings mattered and are worth keeping:

* **The list walk must advance the cursor *before* the free.** `include/rstl/list.hpp`'s own
  `~list` does `it->get_value()->~T(); mAllocator.deallocate(it);` after `cur = next`. Written the
  other way round, mwceppc keeps the cursor in `r3` and reloads `next` every iteration: 5
  instructions out on all four `~list` bodies (`lwz r3,4(r29)` / `mr r3,r31` / `lwz r31,4(r31)`).
  With `cur = next` first it picks `r31`, which is retail's.
* **`fn_80060434`'s value destructor must go through a function, not an explicit `->~vector()`.**
  `it->get_value()->~vector()` emits an extra `li r4,-1` (the flagged destructor call) that
  retail does not have, because retail reaches the value through `fn_800604BC`.

## Not matched, and why (measured, so nobody re-derives it)

* **`fn_8005ECE0` (0x4C) - the 72-byte `CWorldLight` copy assignment.** Best attempt:
  `extern "C" void fn_8005ECE0(CWorldLight* dest, const CWorldLight* src) { *dest = *src; }`.
  mwceppc emits an 8-instruction thunk onto the weak `_ZN11CWorldLightaSERKS_` COMDAT instead of
  retail's 19 `lfd`/`stfd` pairs (19 differing instructions). The nine-double interleaved bitcopy is
  only reachable by writing the member copies out by hand, which would be assembly by another name
  and would not produce `lfd`/`stfd` anyway. Its three callers (`PostConstructArea` 0x8005A924 /
  0x8005A9E8, `~CPortalArea` 0x8005B4AC, `vector<CWorldLight>::reserve` 0x8005F4D0) all have a
  72-byte stride, so the identification is not in doubt - only the emission is.
* **`fn_8005E400` / `fn_8005E7CC` / `fn_8005E804`** (0x68 / 0x38 / 0x68) - retail's out-of-line
  `rstl::destroy` / `destroy_impl` for `vector<auto_ptr<char>>` and
  `vector<pair<auto_ptr<char>,int>>`. Our `include/rstl/construct.hpp` `destroy_impl(T*)` is
  `if (is_trivially_destructible<T>::value) return; in->~T();` - **no null test** - while retail's
  loop tests the element pointer (`cmplwi r30,0 ; beq`) before the `lbz`. Writing the guard by hand
  would be a dead `if (it != nullptr)` on a pointer that walks to `end`; fixing it in
  `construct.hpp` is shared-header surgery affecting every `rstl::destroy` user in the DOL. Not
  attempted.
* **`fn_800546BC` / `fn_80054D1C`** (0x70 each, `create_node`) - both allocate a **20-byte** node
  (`li r3,20`) with the value at `+8` and so 12 bytes of payload. `fn_800546BC` copies it raw
  (word, word, byte at `+8`); `fn_80054D1C` copies (word, **byte at +4**, word at `+8`) and then
  clears `4(src)`, i.e. steals an `auto_ptr`. Two different 12-byte element types with the same
  layout prefix, and `include/MetroidPrime/CGameArea.hpp`'s members at those offsets are guesses
  (`x194_`, `x1ac_` are named from the offsets). Guessing the element type would be the wrong kind
  of guess: the 20-byte node and the 12-byte value are firm, the names are not.
* **`fn_80054CAC` / `fn_80054C84`** (0x70 / 0x28) - `do_insert_before` / `push_back` for the same
  20-byte-node list. `fn_80054CAC` is `node* = create_node(prev, next, val)` followed by the
  relink and `++mCount`; depends on the same unknown element type as the two above.
* **`fn_8005D6C4` / `fn_8005D738` / `fn_8005D7C4`** (0x74 / 0x8C / 0x90) - not identified.
  `fn_8005D6C4` is called from `UpdateDependencyLoading` (0x8005D610) and itself calls
  `fn_8005D7C4`; `fn_8005D738` has no caller in the DOL at all.
* **`fn_8005F508`** (0xA4) - `vector<TEditorId>::reserve(int)` inlined into `operator=`. Our
  out-of-line `reserve__Q24rstl45vector<9TEditorId...Fi` is **172 bytes against retail's 164**, so
  the inlined copy is not the out-of-line body; a spelling hunt is needed, not a transcription.
* **`CPostConstructed`'s ctor (0x2DC) and dtor (0x308)** - both defined by us, both 0%. Real
  decompilation work, not transcription; the dtor is what calls 16 of the 19 functions above.

## For the next run on this unit

The remaining 34 unmatched functions split cleanly: **12 are unreachable by transcription** (a
compiler-generated bitcopy, three `rstl::destroy` bodies that need `construct.hpp`, and four list
functions that need an element type nobody has identified yet), and the rest are real
decompilation. `PostConstructArea` (0x8005A25C, 94.8%, 348 differing instructions) and
`StartStreamingMainArea` (0x80057D44, 98.1%, 195) are the two biggest blocks left; the previous
run's notes recorded that everything else under 99% was register allocation.

## Notes

* `docs/HANDOFF.md`'s derived count block was rewritten by `tools/gate.sh` itself (it runs with
  `MP_GATE_DOCS_WRITE=1`); I did not edit that file by hand.
* `.tmp/opencode/cmp.py` is the differ used here (a copy of `tools/try_batch.py`'s normalisation
  without the variant runner); it is under `.tmp/`, which is not tracked.
* `MetroidPrime/CGameArea.o` is **not** in the `main.dol` link (`build.ninja` links
  `build/G2ME01/obj/MetroidPrime/CGameArea.o`, the retail object), so nothing here can move the
  DOL sha1 - which is why the gate's hash checks pass unchanged.