# progress-prime1-ccollisionsurface

`WorldFormat/CCollisionSurface`: **1/4 -> 4/4 functions matched** (unit stays `NonMatching`;
`flip_test.sh` not run, as the brief requires for a `progress` item). Unit went 82.53% fuzzy /
21.23% matched code -> **100.00% / 100.00%**. Project total 10384 -> 10387 matched functions,
`tools/report_diff.py` reports **no regression** and no unit elsewhere got worse.

## The one thing that mattered: `GetNormal` returns `CVector3f`, not `CUnitVector3f`

Prime 1's `CCollisionSurface::GetNormal` is spelled as returning `CUnitVector3f`, and its source
`return tmp;` works only because a `CUnitVector3f` is constructible from a `CVector3f` there. That
spelling does **not** port. Echoes' retail `GetNormal` (0x802473E4, 0x98 bytes) ends:

```
80247458: bl 8001994c <Cross__9CVector3fFRC9CVector3fRC9CVector3f>
8024745c: mr r3,r31                ; the function's own sret, saved at entry
80247460: addi r4,r1,8
80247464: bl 802cada4 <AsNormalized__9CVector3fCFv>
```

`AsNormalized` is `CVector3f CVector3f::AsNormalized() const` and is called with **`this` = the
Cross result and `r3` = GetNormal's own return slot** - the value is returned in place, with no
`__ct__13CUnitVector3fFRC9CVector3f` call anywhere. Our old spelling built a `CUnitVector3f`
through the out-of-line ctor, which is 6 extra instructions retail does not have.

Confirmed from the other side: retail `GetPlane` (0x80247360) and `GetEdgePlane` (0x80247248) each
call `GetNormal` and then **explicitly** call `__ct__13CUnitVector3fFRC9CVector3f` on the result
(`addi r3,r1,20; addi r4,r1,8; bl 802ca12c`). The unit-return happens in the *caller*, so
`GetNormal` itself returns a plain `CVector3f`. Prime 1's declaration is a Prime 1 spelling of the
same function; the Echoes declaration differs. `CUnitVector3f` derives from `CVector3f` with no
extra members, so every caller that binds the result to `const CVector3f&` is unaffected - checked
all four call sites (`CMapUniverse.cpp:136`, `CMapWorld.cpp:478`, `CDecal.cpp:421`,
`CMetroidAreaCollider.cpp:318`) and none needed an edit.

## Per function, before -> after, and whether Prime 1's source helped

| function | before | after | Prime 1's source |
|---|---|---|---|
| `GetNormal` | 72.16% | **100.00%** | structure only; return type had to be changed to `CVector3f` and Prime 1's `return tmp;` had to become `return CVector3f::Cross(baDiff, caDiff).AsNormalized();` |
| `GetPlane` | 81.55% | **100.00%** | same fix - Prime 1's `const CUnitVector3f norm = GetNormal();` is a copy-init; Echoes needs the explicit ctor call |
| `GetEdgePlane` | 79.14% | **100.00%** | not in Prime 1 at all (Echoes-only, "guessed name"); fixed by the same `GetNormal` change plus naming `mVertices[edge]` once |

`GetPlane` and `GetEdgePlane` both went to 100% the instant `GetNormal`'s return type was corrected -
they were only wrong because of the call they make. Prime 1's source contributed the operand
decomposition and the `baDiff`/`caDiff` naming, which is what the register allocator then lines up.

## Spellings measured in this run

`GetNormal` (all with the corrected return type, scores are objdiff fuzzy % for the function):

| spelling | score |
|---|---|
| `return CVector3f::Cross(baDiff, caDiff).AsNormalized();` (named `baDiff`/`caDiff`, no `tmp`) | **100.00** |
| same body with `const CVector3f&` locals | **100.00** |
| `CVector3f tmp = Cross(...); return tmp.AsNormalized();` | 83.97 - a copy materialises before the `AsNormalized` call |
| same, but `CVector3f ret = tmp.AsNormalized(); return ret;` | 66.16 |
| `caDiff` declared before `baDiff`, inlined return | 99.79 - one instruction out |
| `caDiff` before `baDiff`, named `tmp` | 83.84 |
| `CVector3f cross = Cross(mVertices[1]-..., mVertices[2]-...); return cross.AsNormalized();` | 77.55 |
| `return Cross(mVertices[1]-mVertices[0], mVertices[2]-mVertices[0]).AsNormalized();` (all inline) | 93.34 |
| `CVector3f tmp(Cross(...)); return tmp.AsNormalized();` | 83.97 |

The rule this shows: **do not name the Cross result**. Naming it makes mwcceppc keep a separate
stack object and copy it into the call to `AsNormalized`; letting the temporary live only as the
receiver of the member call lets it be built in place in the caller's argument slot. Two named
differentials (`baDiff`, `caDiff`) are fine and are what retail has.

`GetEdgePlane` (fuzzy % for the function, with `GetNormal` already fixed):

| spelling | score |
|---|---|
| `const CVector3f& v = mVertices[edge];` used for both the subtract and the `Dot` | **100.00** |
| same but `const CUnitVector3f edgeNormal(...)` and non-const `CVector3f edgeDirection` | **100.00** |
| `const CVector3f& n = mVertices[nextVertex[edge]]; ... n - v` | **100.00** |
| same via the `GetVert()` accessors | **100.00** |
| no `v` (index `mVertices[edge]` at both uses) | 87.73 |
| `const CVector3f v = mVertices[edge];` (by value, not by reference) | 62.83 |
| operand order flipped (`mVertices[edge] - mVertices[nextVertex[edge]]`) | 87.30 |
| `static const int nextVertex[]` (promoted the table to static storage) | 69.17 |
| Cross written inline, no `edgeDirection` name | 87.56 |
| `nextVertex[]` declared **before** the `GetNormal()` call | 21.71 |
| all three names `const` + `v` | 100.00 |

The `v` binding is the same lesson as in `GetNormal`: a **reference** to `mVertices[edge]` lets the
compiler keep one address in a register, a by-value copy does not. Moving the `nextVertex` table
above the `GetNormal()` call collapses the frame (21.71%) - that table wants its own scope.

## Files changed

- `include/WorldFormat/CCollisionSurface.hpp:17` - `CUnitVector3f GetNormal() const;` ->
  `CVector3f GetNormal() const;`
- `src/WorldFormat/CCollisionSurface.cpp:6-27` - the three function bodies above.

No header layout, member name or `configure.py` change: `CCollisionSurface` is still `0x30` and
`CHECK_SIZEOF` still passes, and the class layout was never wrong - only the return type of one
method was.

## Verified

- `tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-prime1-ccollisionsurface`**
  (gate ok, counts 10384 -> 10387, target 1 -> 4 / 4, no asm added, `All:` 10387 / 28465).
- `tools/gate.sh` -> `GATE PASS`; DOL sha1, all 86 RELs, `check_symbol_names.py`, decl order,
  files.cmake, module wiring, port probe, port link gap all ok.
- `tools/report_diff.py` -> `+100%` on the three functions, `no regression`.
- `python3 tools/check_decl_order.py --unit WorldFormat/CCollisionSurface` -> ok.

The unit is at 100% on every function objdiff can see but is deliberately left `NonMatching`: this
is a `progress` item, and `flip_test.sh` is the only thing allowed to flip a unit.

## Lesson worth keeping (Echoes, not Prime 1)

Prime 1's `CCollisionSurface` declares `GetNormal` as returning `CUnitVector3f`; Echoes' returns
`CVector3f` under the **same mangled name** (`GetNormal__17CCollisionSurfaceCFv` - the return type
is not in the mangling, so the symbol table cannot tell them apart). Reading only Prime 1 for an
Echoes unit will silently produce a wrong return type here, and it costs 3 functions. The
discriminator is the **caller**: if a retail caller invokes the `CUnitVector3f(const CVector3f&)`
ctor on the result, the callee does not return a `CUnitVector3f`. Worth checking for the other
`progress-prime1-*` items whose Prime 1 class has a `CUnitVector3f`-returning accessor.

No `NEW:` lines: the item is finished, nothing is blocked.
