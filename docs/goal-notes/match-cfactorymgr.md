# match-cfactorymgr — Kyoto/CFactoryMgr

`kind: match`, target `Kyoto/CFactoryMgr`. Worked in `../wt-mp2-goal-L1` (branch `goal/lane-1`).
Not committed.

## Result

PARTIAL. `Kyoto/CFactoryMgr` went **19 -> 21 / 22** matched functions; the project-wide matched
count went **11430 -> 11432 / 28465**. `tools/goal_check.sh build/goal/item.json` printed:

```
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11430 -> 11432   linked 5572 -> 5572
  flip  flip_test Kyoto/CFactoryMgr.cpp: FAIL - judged below as partial progress
  ok    target rose: main/Kyoto/CFactoryMgr: 19 -> 21 / 22 functions
  ok    no asm added
goal_check: PARTIAL match-cfactorymgr - flip_test Kyoto/CFactoryMgr.cpp: FAIL, but the target rose
```

The unit does not flip because `MakeObjectFromMemory` is still at 99.18% (see WALL below).

## What I changed

One file, `include/rstl/pair.hpp` (+13 lines): a `construct_impl` overload for
`pair< int, T* >` that copies by assignment instead of going through `construct`'s generic
`new (dest) T(src)`.

## Why, and the measurement behind it

Both remaining `create_node` instantiations sat at 81.85%. Retail's `create_node` for
`rstl::map<int, CFactoryFnReturn (*)(...)>` (`.text:0x802F9DA0`, 0x68 bytes) is:

```
bl allocate; cmplwi r3,0; beq end
stw r27,0(r3); lwz r4,0(r31); stw r28,4(r3); lwz r0,4(r31)
stw r29,8(r3); stw r30,12(r3); stw r4,16(r3); stw r0,20(r3)
```

Ours had an extra `addic. r5,r3,0x10` + `beq` and stored the value through `r5` instead of
`this + 0x10`. That guard is the placement new in `construct`: `red_black_tree::node` holds its
value as raw `uchar` storage and fills it with `construct(get_value(), value)`.

The generic placement-new path is correct for the other five instantiations and must stay:
`__ct__...red_black_tree<...>4node` for `CResFactory` (measured **100%**) really does emit
`addic. r9,r3,0x10; beqlr` and copy through `r9`, and `CMetaAnimPlay`, `CCharLayoutInfo` and
`DolphinCAudioSys` are at 100% with it. So this is not "the node ctor should not use
placement new" - it is a triviality distinction, and `pair<int, fnptr>` is the one value type here
whose copy is trivial. The two existing plain-copy overloads in `pair.hpp`
(`pair<uint,uint>`, `pair<int,float>`) are exactly this precedent, so the new one follows them.

Deliberately narrow: only a **pointer** second member. `pair<int, auto_ptr<T>>`
(`src/MetroidPrime/CGameArea.cpp`) and `pair<int, TEditorId>` have non-trivial copies and must keep
the generic path; nothing in the tree instantiates `pair<int, T*>` except CFactoryMgr's two maps.

Two spellings I tried first and rejected (both measured, both reverted):

- `P mValue` member instead of `uchar mValue[sizeof(P)]` in `red_black_tree.hpp`: broke the DOL
  hash and dropped `insert_into` to 62.76% (CFactoryMgr), `create_node` to 70.64% (CResFactory),
  `insert_into` to 68.82% (CCharLayoutInfo), `create_node` to 75.10% (CMetaAnimPlay).
- `*get_value() = value` instead of `construct(get_value(), value)`: gets `create_node` to 100%
  but drops `insert_into` to 62.76% (CFactoryMgr), `create_node` to 70.64% (CResFactory),
  `insert_into` to 68.82% (CCharLayoutInfo), `__ct__...node` to 77.89% (DolphinCAudioSys).

Both are the same lesson: the node's value copy has to stay inside `construct`, because
`construct` is what distinguishes the trivial from the non-trivial value types.

## Blast radius, measured

Compared all 28465 per-function `fuzzy_match_percent` values in `build/report.json` before and
after (`/tmp/base_fns.json` vs the rebuilt report): **0 new, 0 gone, 0 worse, 2 better** - the two
`create_node`s at 81.8461 -> 100.0000. `All:` stayed at 32.82% fuzzy / 25.63% matched / 12.11%
linked. `Kyoto/Animation/*` and `Kyoto/Audio/DolphinCAudioSys` stay byte-identical, so `main.dol` still hashes
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and all 86 RELs still `cmp` equal.

## Gates

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                All: 32.82% fuzzy, 25.63% matched, 12.11% linked
                                       (11432 / 28465 functions)   # was 11430
                                       main/Kyoto/CFactoryMgr: 99.76% fuzzy, 70.97% matched (21 / 22)
python3 tools/check_symbol_names.py    checked 514 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit Kyoto/CFactoryMgr
                                       ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json   PARTIAL (above)
```

`tools/unit_fit.sh Kyoto/CFactoryMgr.cpp` reports `.text` over its claimed range by 732 bytes in 7
COMDAT-weak destructors (`__dt__IObj`, `__dt__CMemoryInStream`, `__dt__CFactoryFnReturn`,
`__dt__auto_ptr<CInputStream>`, `__dt__auto_ptr<uchar>`, and the two
`__dt__red_black_tree<...>`). That is the documented harmless case - `Kyoto/Audio/CAi` carries 224
bytes of the same kind and still flips - so it is not what blocks the flip here.

## WALL

`MakeObjectFromMemory__11CFactoryMgr...` 99.18% (1224 bytes) - pure register allocation; all 306
instructions match opcode-for-opcode and in order, only the physical registers differ, and only
the seven prologue `mr`s plus their uses. Retail `p2->r25, p3->r31, p1->r30, p4->r26, p5->r27,
p6->r29, p7->r28`; we get `p2->r25, p3->r27, p1->r26, p4->r28, p5->r29, p6->r31, p7->r30`.
Same spill order, different free-list walk.

Spellings tried this run, with scores, so the next run does not repeat them:

| spelling | result |
| --- | --- |
| unchanged (`FMemFactoryFunc factory = memIt->second;`) | 99.1830% |
| `const FMemFactoryFunc factory = ...` | 99.1830% (no change) |
| `if (!buffer.owner())` instead of `if (!data.owner())` | 99.1634% (worse) |
| use `memIt->second` inline, drop the `factory` local | 97.3213%, size 1220 (worse) |

## NEW

None filed. The only remaining gap in the unit is `MakeObjectFromMemory`'s register allocation,
which is a codegen wall rather than work whose success would raise a count on its own, and it is
covered by the `WALL:` line above.