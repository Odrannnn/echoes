# progress-unit-csimplepool

`kind: progress`, target `Kyoto/CSimplePool` (DOL unit, stays `NonMatching`).

## Result

`main/Kyoto/CSimplePool` matched_functions **10 -> 12 of 21**. Global `All:` went
**11744 -> 11746 / 28465**; fuzzy and linked percentages unchanged (33.39% / 26.40% / 12.64%),
`linked 5727 -> 5727`. `./tools/goal_check.sh build/goal/item.json` -> **PASS**.
DOL sha1 still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

Two functions taken to a real exact match:

| function | before | after | what changed |
| --- | --- | --- | --- |
| `HasObject__11CSimplePoolCFRC10SObjectTag` | 81.32% | **100%** | rewritten into Prime 1's `bool result` shape |
| `ObjectUnreferenced__11CSimplePoolFRC10SObjectTag` | 90.96% | **100%** | `find()` result bound to a named `iterator` before `erase` |

No `asm`, no touched `tools/` / `build/goal/` / `port_link_baseline.txt`.

## Per function

### HasObject: 81.32% -> 100%

Prime 1's `src/Kyoto/CSimplePool.cpp` is a donor for the logic, but its `#if NONMATCHING`
form (`const bool canBuild = mFactory.CanBuild(tag);`) is wrong for this repo: Echoes stores
`mFactory` as a **pointer** (`IFactory* mFactory`, offset 0x18 in the 0x24-byte class) and the
class is `const`, so the null test is real work and must stay. Retail's `HasObject` also keeps
its answer in a third callee-saved register (`r30`) and returns it at the end instead of
returning early, which is why the donor's `if (!canBuild) { result = false; }` tail matters:

```cpp
bool CSimplePool::HasObject(const SObjectTag& tag) const {
  ResourceMap::const_iterator it = mResources.find(tag);
  bool result = true;
  if (!(it != mResources.end())) {
    const bool canBuild = mFactory != nullptr && mFactory->CanBuild(tag);
    if (!canBuild) {
      result = false;
    }
  }
  return result;
}
```

This is what removed the register pressure difference: 188 bytes, matching byte for byte
including the `r30`-held `result` and the `lwz r3, 0x18(r31)` factory load. The `!(it != end)`
spelling (rather than `it == end`) is the one that matches — it is what the donor writes and it
selects the same `r0`-scratch comparison shape retail emits.

### ObjectUnreferenced: 90.96% -> 100%

Retail emits **two** copies of the `find()` iterator (one at `0x18(r1)`, one at `0x20(r1)`)
before `erase`, and our nested call produced only one. Binding it to a named local makes the
compiler materialise the same second copy:

```cpp
void CSimplePool::ObjectUnreferenced(const SObjectTag& tag) {
  ResourceMap::iterator it = mResources.find(tag);
  mResources.erase(it);
}
```

96 bytes, exact. Tried and equally good, so any of these spellings is fine:
`const ResourceMap::iterator it = ...`, `ResourceMap::iterator& it = ...`, and adding a
`(void)it;` after the erase. `mResources.erase(mResources.find(tag));` is the 90.96% form.

## Not matched

### GetReferencedTags: stuck at 96.15% (208 bytes, 8 differ)

The body is already byte-identical apart from **where one load sits**. Retail:

```
stw r0, 0xc(r1)
lwz r4, 0x8(r4)      <- size() hoisted between two of the vector's null stores
stw r0, 0x10(r1)
stw r0, 0x14(r1)
bl  reserve
```

Ours emits the same four operations with `lwz r4, 0x8(r4)` last. Everything else in the
function — the `push_back_unsafe` unrolled copy, the `rbtree_traverse_forward` call, the
`cmplw r31, r31` end test, the copy-construct and destructor calls — matches exactly.

27 spellings tried, all landing on 96.15% or below, none reaching 100%:

- hoisting the size into a local: `const int count = mResources.size();` before the vector, after
  the vector, `const uint`, `static_cast<int>`, `static_cast<unsigned>`, extra parentheses
- reading `mResources.mCount` directly instead of `size()`
- `mResources` via `const ResourceMap&`, `const ResourceMap&`, `*this`, `this->`
- loop declarations: `auto` vs `ResourceMap::iterator`, `const iterator`, declaring `end` first,
  `it`/`end` before vs after `reserve`, `end` called inline in the loop condition
- vector construction: `tags{}`, `tags(0)`, `tags(rstl::rmemory_allocator())`,
  `= rstl::vector<SObjectTag>()`, `rstl::vector<SObjectTag>& t = tags; t.reserve(...)`
- `push_back_unsafe` vs `push_back` (push_back is much worse, 60.19% — it calls `reserve`)

The only ones that changed the score at all moved it **down** (`iterfirst` 86.63%,
`endinline` 88.29%, `pushback` 60.19%, `size-in-ctor-arg` 90.38%). The current source is the
best of everything tried. `tags.reserve(mResources.size())` order matters: the size load is
scheduled last because it is written that way in the source.

This is register-scheduling / load-hoisting, not logic. A future attempt should try changing the
`reserve` declaration in `include/rstl/vector.hpp` (e.g. an out-of-line definition ordered
differently, or taking the count by a different route) rather than more spellings of this
function. Do not spend a second run re-trying the 27 spellings above.

WALL: GetReferencedTags__11CSimplePoolFv 96.15% - one load (mResources.size()) scheduled after
the vector's three null-stores instead of between the first and second; 27 source spellings all
landed on 96.15% or below.

### The eight `fn_*` functions cannot match by name

`fn_80300A1C` (76 B), `fn_80300A68` (100 B), `fn_80300DC0` (76 B), `fn_80300E6C` (136 B),
`fn_80300F94` (116 B), `fn_80301158` (96 B), `fn_803011B8` (424 B), `fn_80301360` (112 B) are
retail **unnamed** symbols in `config/G2ME01/symbols.txt`, so `report.json` reports them with no
`fuzzy_match_percent` at all (`None`, listed as 0.00% by the tools). objdiff matches functions
**by symbol name**, and our object emits those bodies under real template names, so they can
never pair with `fn_*` until someone renames them in `symbols.txt`. They are already in our
object:

| retail | our symbol |
| --- | --- |
| `fn_80300A1C` | `find__...FRC10SObjectTag` |
| `fn_80300A68` | `find_node__...CFRC10SObjectTag` |
| `fn_80300DC0` | `find__...FRC10SObjectTag` (the const overload, emitted at 0x934) |
| `fn_80300E6C` | `erase__...FQ...8iterator` |
| `fn_80300F94` | `__dt__Q24rstl90map<...>Fv` |
| `fn_80301158` | `__dt__Q24rstl199red_black_tree<...>Fv` |
| `fn_803011B8` | `insert_into__...` |
| `fn_80301360` | `create_node__...` (order in `.text` is by mwcceppc's reverse order) |

The bodies are real (e.g. `fn_80300A68` disassembles to exactly our `find_node`, and
`fn_80300E6C` to exactly our `erase(iterator)`); only the name pairing is missing. This is a
`tools/apply_rename.py` / `symbols.txt` job, deliberately **not** done here: renaming in
`symbols.txt` changes what the REL linker looks for, and these units are not `Matching` yet, so
`check_symbol_names.py` is the gate that would catch a wrong rename. **13 is therefore the
practical ceiling for this unit** until those eight are renamed. Not filed as `NEW:` — the work
is a rename in a judge-owned config, not a source change that raises a count on its own.

## Verified

- `./tools/decomp_build.sh` full build: `All: 33.39% fuzzy, 26.40% matched, 12.64% linked (11746 / 28465 functions)`
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-unit-csimplepool`
  (includes all 86 RELs, report diff, module wiring, docs claims, port probe)
- only `src/Kyoto/CSimplePool.cpp` modified; unit left `NonMatching` in `configure.py`