# progress-prime1-cworldshadow — `MetroidPrime/CWorldShadow` 4/7 → 5/7 functions, unit 91.23% → 98.36% fuzzy, data 0/80 → 80/80

**`goal_check.sh`: PASS.** `matched 10368 -> 10369`, `linked 5048 -> 5048` (unchanged — nothing
flipped, which is correct for a `progress` item), `+1 functions at 100%, 0 units newly linked`, no
function anywhere worse, no `asm` added, full `gate.sh` green (DOL sha1, 86 RELs, docs claims,
decl order, port probe, port link gap, reach stubs).

The one function the item named that could be finished is finished:
`EnableModelProjectedShadow` 79.65% → **100%**. `BuildLightShadowTexture` went 92.81% → 98.92% and
`CanRender` 72.15% → 72.96%, and the unit's `.sdata2`/`.sbss` now match byte for byte, which they did
not before (`matched_data` went from absent to `80/80`, 100%).

---

## Starting position, measured on the clean tree at `9b4156a2` (`build/goal/judge/report.base.json`)

```
main/MetroidPrime/CWorldShadow: 91.23458% fuzzy, 12.696493% matched (4 / 7 functions)
  100.0        264 B  __ct__12CWorldShadowFUiUib
  100.0        112 B  __dt__12CWorldShadowFv
   72.14815    108 B  CanRender__12CWorldShadowFRC13CStateManager
   92.81067   2324 B  BuildLightShadowTexture__12CWorldShadowFRC13CStateManager7TAreaIdUiRC6CAABoxbb
   79.649124   456 B  EnableModelProjectedShadow__12CWorldShadowCFRC12CTransform4fUif
  100.0         32 B  DisableModelProjectedShadow__12CWorldShadowCFv
  100.0         12 B  ResetBlur__12CWorldShadowFv
```

Not `STALE:` — two of the three named functions were genuinely unmatched.

## How I compared

`./tools/dis.sh <addr> <size>` against `build/G2ME01/main.elf` for retail, and
`build/binutils/powerpc-eabi-objdump -dr build/G2ME01/src/MetroidPrime/CWorldShadow.o` for ours,
side by side with relocations resolved to symbol names (`.tmp/opencode/rawcmp.py`, `.tmp/opencode/cmpfn.py`
— scratch, gitignored). Retail addresses from `build/report.json`:
`EnableModelProjectedShadow` 0x800E1810/456, `BuildLightShadowTexture` 0x800E1B18/2324,
`CanRender` 0x800E22EC/108.

---

## `EnableModelProjectedShadow` — 79.649% → 100.0% (456 B, the whole function)

**Prime 1's source was already in the tree**, member names adapted, and it was already
instruction-for-instruction identical to retail apart from one thing. The diff was 20 instructions
of MSL's inlined square root where retail has a `bl`:

```
retail  lfs  f1,<2.0f>
        bl   8001d658 <sqrt__Ff>            # 0x8001D658, the SDK's out-of-line float sqrt
        stfs f1,sqrt2
ours    lfd  f4,<2.0 as double>  frsqrte ... fnmsub ...   # 20 instructions, three double constants
```

`static float sqrt2 = sqrt(2.0);` — `libc/math.h:222` defines `sqrt(double)` as an inline
`frsqrte`-plus-Newton-steps sequence unless `MSL_NO_INLINE_SQRT` is set, and `sqrtf` is a
`static inline` in the same header, so both spellings expand. Retail's argument is a **float**
(`lfs`) and the result is a **float** (`stfs`), and the call target is `sqrt__Ff` at 0x8001D658 —
which `config/G2ME01/symbols.txt:540` puts inside `MetroidPrime/CEulerAngles`'s range, and which
`src/MetroidPrime/CEulerAngles.cpp:14` already defines under that name. So:

```cpp
extern "C" float sqrt__Ff(float x);
...
static float sqrt2 = sqrt__Ff(2.0f);
```

**What I checked before choosing it** (both wrong, so do not retry them):

* `CMath::SqrtF(2.0f)` — semantically the same call, but `SqrtF__5CMathFf` is at **0x802CDF30**,
  not 0x8001D658 (`symbols.txt:12978`), so the `bl` would resolve to a different address.
* `#define MSL_NO_INLINE_SQRT` + `sqrt(2.0f)` — promotes to `double`, so the call is
  `sqrt(double)` = `sqrt` at 0x80352F38 (`symbols.txt:15677`) and the argument becomes `lfd`.

Removing the inline sequence also removed three double constants from `.sdata2` (the unit's
`.sdata2` went from 80 to 88 bytes with `4000000000000000` / `4008000000000000` / `3fe0000000000000`
in it, retail has 64). That is most of the unit's data match.

**Per the item's request:** Prime 1's source for this function **needed one edit beyond Prime 1's
text** — the sqrt spelling. Prime 1 says `sqrt(2.0)` too; I did not measure what prime-ref's own
build emits for it, so I am not claiming which compiler version inlines it where. Everything else in
Prime 1's body matched unchanged once the sqrt was a real call.

## `BuildLightShadowTexture` — 92.811% → 98.924% (2324 B)

Four changes, each measured on its own. Three are **literally Prime 1's text that this tree had not
adopted**; the fourth is Prime 1's *shape*.

| step | change | Prime 1 | before → after |
|---|---|---|---|
| 1 | four separate `GetViewport().mX` reads instead of one `const CViewport` copy | `prime-ref` L80-83, verbatim | 92.811 → **97.480** |
| 2a | `motionBlur && mBlurReset != true` instead of `!mBlurReset` | `prime-ref` L130, verbatim | → **98.70** |
| 2b | add upstream's unread `static int unkInt = 0;` | `prime-ref` L149, verbatim | → **98.70** |
| 3 | bind no `data` local: `area.GetPostConstructed()->mLightsA[...]` / `->mPvs.get()` | Prime 1's shape (it has no `CPostConstructed`) | → **98.924** |

* **1.** `CViewport` in this tree is 24 bytes (`include/Kyoto/Graphics/CGraphics.hpp:164`: four
  ints and two floats). A whole-struct copy loads all six and needs a 24-byte stack slot; retail
  loads `lwz r30,0(r8) / r29,4 / r28,8 / r27,12` from `&mViewport` — the four ints only — and keeps
  them in registers. That alone was the whole 16-byte frame difference (`stwu r1,-592` → `-576`).
* **2a.** Same test, opposite branch: retail is `lbz r0,136(r23); cmplwi r0,1; beq` and `!` gives
  `cmplwi r0,0; bne`. Prime 1 writes `!= true` too.
* **2b.** Retail has a lazy-initialised function-local static here whose value nothing reads: the
  guard load, `li r3,0`, `li r0,1`, `stw r3`, `stb r0` — seven instructions. Prime 1 has
  `static int unkInt = 0;` on the line above `SetFlag1`. **Nothing was deleted to get this**; it is
  an initialisation this tree was missing.
* **3.** Retail reads `lwz r5,260(r29)` twice — once for `mLightsA[160]`, once for `mPvs[212]` —
  re-deriving the post-constructed block from `area` after the `GetCenterPoint` call. Binding it to a
  local pins it in a saved register across the call instead, which also pushed the area pointer into
  `r3` rather than `r29`. Dropping the local fixed the prologue's first divergence.

After 3 the prologue is **byte-identical** to retail, instruction for instruction, down to
`mr r24,r5 / lwz r3,128(r3) / mr r28,r6 / lwz r0,0(r5) / mr r27,r7 / mr r25,r8 / mr r26,r9`.

### What still blocks 100% on this function — 4 instructions of 2324

**(a) `DrawUnsortedGeometry` arity — 2 instructions, and it is an interface decision, not a source
spelling.** Retail's call site is

```
lwz r3,<gpRender> ; li r5,0 ; li r6,0 ; lwz r4,0(r24) ; lwz r12,32(r12) ; mtctr r12 ; bctrl
```

— three int arguments. This tree's `include/MetaRender/IRenderer.hpp:90` declares
`virtual void DrawUnsortedGeometry(int areaId) = 0;`, one. The vtable slot is the same one
(retail slot 6, `lwz r12,32(r12)` in both), and retail's own override is
`DrawUnsortedGeometry__13CCubeRendererFi` (`symbols.txt:10723`) — one int. The three-argument form
is in the **pre-merge** interface, and the divergence is already written down in this repo:
`src/MetaRender/PortCCubeRenderer.cpp:188-189` records that a wholesale interface replacement
changed `DrawUnsortedGeometry(int,int,int)` to `DrawUnsortedGeometry(int)`.

Closing it means changing `IRenderer.hpp`, `CCubeRenderer.cpp` and `PortCCubeRenderer.cpp`
together and renaming a retail symbol (`...Fi` → `...Fiii`). That is a cross-cutting interface
change on a shared header other lanes have open — out of scope for this item, and the orchestrator
should decide it, not me. **I did not file it as `NEW:`**: its only effect on a count is on this
item's own unit, so it is a restatement of the current item, which the brief says not to file.

**(b) The `Render2D` argument setup — 2 instructions, register allocation.**

```
retail  lha r24,4(r26)   lha r25,6(r26)  bl <f>  neg r0,r24  mr r8,r3  mr r3,r26
        slwi r5,r24,1     slwi r6,r25,1   slwi r7,r0,1  li r4,0
ours    lha r25,4(r26)   lha r24,6(r26)  bl <f>  extsh r5,r25  extsh r4,r24  neg r0,r5
        mr r8,r3         slwi r6,r4,1    mr r3,r26  slwi r5,r5,1  slwi r7,r0,1  li r4,0
```

Same values, same calls, same argument registers; the two `extsh` are redundant (the `lha` already
sign-extended) and come from the width landing in `r25` instead of `r24`. I did not find a source
spelling for it, and the arguments are already written the way Prime 1 writes them
(`prime-ref` L136-138, `(-mTexture->GetWidth()) * 2` and all).

`WALL: CWorldShadow::BuildLightShadowTexture 98.92% - 2 instrs need IRenderer's retail 3-int DrawUnsortedGeometry arity, 2 more are Render2D register allocation`

## `CanRender` — 72.148% → 72.963% (108 B)

**Not in the item's list** (no Prime 1 counterpart — it is Echoes-only, and the header says its own
name is a guess), but it is in the item's target unit, so raising it raises the judged count. Four
spellings, all built and measured this run:

| spelling | result |
|---|---|
| `if (fn) return false; return !darkWorld && visor == combat;` (the tree's original) | 72.148% |
| `if (fn) return false; if (darkWorld) return false; if (visor == combat) return true; return false;` (**kept**) | **72.963%** |
| ... same but the last two lines are `return visor == combat;` | 72.963% |
| `if (fn \|\| darkWorld) return false; return visor == combat;` (worse — do not retry) | 65.19% |

Retail keeps `mgr` in `r31` and materialises the result straight into `r3`, tail-duplicating
`li r3,0` / `li r3,1` at each exit. The original spelling made MWCC compute the `&&` into `r31` and
`mr r3,r31` at the end, which also forced `mgr` into `r30` and cost a saved-register pair — hence
the second spelling. What is left is block layout plus one bool normalisation: retail **branches** on
the visor comparison (`cmpwi r3,0; beq; b; li r3,1; b; li r3,0`) and shares one `li r3,0` block
between the dark-world and visor paths, where we duplicate the block and lower `x == 0` to
`cntlzw` + `rlwinm`/`srwi`.

`WALL: CWorldShadow::CanRender 72.96% - retail shares one false block and branches on the visor compare; four spellings measured, none produce it`

---

## Not a `match` item, and not close to one

`./tools/unit_fit.sh MetroidPrime/CWorldShadow.cpp` (measured, this tree):

```
   .text      claimed   3308   ours   3636   over by 328
   .sdata2    claimed     64   ours     64   fits
   extra:    +  104  __dt__10CPVSVisSetFv
   extra:    +   88  __dt__Q24rstl21single_ptr<8CTexture>Fv
   extra:    +   84  __dt__Q24rstl42vector<6CLight>...Fv
   extra:    +   60  __dt__14CFrustumPlanesFv
```

The over-run is those four COMDAT/template destructors, 336 bytes, and it is **pre-existing**: this
change removed 20 instructions from `EnableModelProjectedShadow` and added none. `python3
tools/check_decl_order.py --unit MetroidPrime/CWorldShadow` → ok. So the unit still needs
`match` work on its own arity before a flip is in reach; per the brief I did not run `flip_test.sh`
to decide anything.

## Gates

```
./tools/decomp_build.sh main/MetroidPrime/CWorldShadow   -> All: 31.50% fuzzy, 23.96% matched
                                                            (10369 / 28465 functions)
./tools/goal_check.sh build/goal/item.json              -> PASS progress-prime1-cworldshadow
tools/gate.sh (inside the judge)                        -> GATE PASS 9b4156a2+2 changed
```

`docs/HANDOFF.md` shows as modified in `git status`: that is `check_docs_claims.py --write` inside
the judge's `gate.sh`, not an edit of mine. Nothing committed.

## Files touched

* `src/MetroidPrime/CWorldShadow.cpp` — the only file I edited. 6 hunks: the `sqrt__Ff` declaration
  and its use, the viewport reads, `mBlurReset != true`, `static int unkInt`, dropping the `data`
  local, and the `CanRender` restructure.
* Scratch, gitignored, not part of the change: `.tmp/opencode/{cmpfn.py,rawcmp.py,cmpfn.sh,blst.diff}`.
