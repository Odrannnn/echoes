# progress-unit-ccamerafilter — `MetroidPrime/Cameras/CCameraFilter`, 20/27 → 22/27

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-unit-ccamerafilter`**
(gate.sh clean, `matched 12266 -> 12268`, `linked 5863 -> 5863`, `check_symbol_names.py` clean,
`All: 34.66% fuzzy, 28.04% matched`, `target rose: 20 -> 22 / 27`, `no asm added`).

## What landed

One source change, `src/MetroidPrime/Cameras/CCameraFilter.cpp` (+36 lines, +1 static table):
the pooled literal table transcribed from retail's own `.rodata`. **Two** functions went to 100%
and `.rodata` went 22.77% → 77.83%; `.text` 86.28% → 86.52%; unit `matched_code_percent`
44.15% → 58.18%. No function anywhere got worse (`report_diff.py` inside `gate.sh` is what says so).

Per function, before → after (all from `build/report.json`, never recalled):

| function | bytes | before | after |
| --- | --- | --- | --- |
| `CCameraFilterPass::SetFilter` | 1156 | 99.9931% | **100%** |
| `CCameraFilterPass::DrawCinematicPlaceholderLabel` | 284 | 91.4085% | **100%** |
| `CCameraFilterPass::DrawFullScreenTexturedQuadQuarters` | 536 | 99.9328% | 99.9328% (unchanged) |
| `CCameraFilterPass::DrawFilterShape` | 244 | 96.7213% | 96.7213% (unchanged) |
| `CCameraFilterPass::DrawScanLines` | 616 | 95.6169% | 95.6169% (unchanged) |
| `CCameraFilterPass::DrawDialogBox` | 964 | 21.8589% | 21.8589% (unchanged) |
| `CCameraBlurPass::Draw() const` | 1932 | 69.2236% | 69.2236% (unchanged) |

## The mechanism (this is the reusable part)

`SetFilter` sat at 99.9931% on **two instructions in a 1156-byte function**, which reads like
register allocation and is not. Both are the same instruction:

```
    lis  r3, <sym>@ha
    addi r4, r3, <sym>@l        ; R_PPC_ADDR16_LO, addend 0
    li   r3, 12
    addi r4, r4, 520            ; ours emitted 22
    li   r5, 0
    bl   __nw__FUlPCcPCc
```

objdiff masks the `ADDR16_LO` field, so the only unrelocated byte in the sequence is that last
`addi`, and it must equal retail's. Reading the two objects' relocations side by side:

```
ours   : ADDR16_LO -> @stringBase0     (our .rodata+0x78)   +22   = "??(??)"
retail : ADDR16_LO -> lbl_803A82A8    (its .rodata+0x278)  +520  = "??(??)"
```

Both land on the same string. **mwcceppc addresses a pooled literal as `pool_base + offset`, where
`pool_base` is the pool's *first* entry** — so the immediate encodes how much string precedes it.
Retail's pool starts 498 bytes early, on a run of 25 enum-name literals. Ours started on
`"CINEMATIC PLACEHOLDER"` itself, so every literal in the unit was off by the length of whatever
should have come before it. `DrawCinematicPlaceholderLabel` was the same bug twice over: it wanted
`addi r3,r3,0; addi r4,r3,498` and emitted `addi r4,r3,0` — 91.4% for two instructions, and the
misalignment it caused is why its 8-byte length error looked like 28 differing instructions.

So: transcribe the pool. `build/G2ME01/asm/MetroidPrime/Cameras/CCameraFilter.s` `.rodata:0x278`
is a 0x210-byte run of 25 literals, `PassThru   ` … `CookieCutterDepthRandomStatic   `,
`NoBlur  `/`LoBlur  `/`HiBlur  `, `CINEMATIC PLACEHOLDER` at +498, `??(??)` at +520. Declaring them
as one file-scope `static const char* const kEnumNames[]` puts them in the pool ahead of everything
else, MWCC picks them as `@stringBase0`, and both call sites emit retail's immediates exactly.
Verified after the change with `tools/`-style inspection of our own object:

```
0x00225c addi r4,r3,0  reloc type=4 addend=0 sym=@stringBase0 | next li r3,12 addi r4,r4,520
```

**The table's owner in the original source is unresolved and no code here reads it.** It is
transcribed retail data, not invented, and the file comment says so — but the honest description is
"we reproduced literals retail demonstrably has, because retail's two call sites encode their
length". `.rodata` is still 77.83% and not 100%, so the pool is right and the `.rodata` is not
finished; see the DrawDialogBox note below.

## Read this before diffing anything in this repo

`objdiff.json` for this unit has `target_path: build/G2ME01/obj/…` and `base_path: build/G2ME01/src/…`,
which is the **opposite** of the naming in `tools/fnmap.py` and `tools/dol_fd.py`:

- `build/G2ME01/src/<unit>.o` — **ours**, built by ninja from `src/<unit>.cpp` by the `mwcc_sjis`
  rule. `touch src/…/CCameraFilter.cpp && ninja build/G2ME01/src/…/CCameraFilter.o` rebuilds only
  this one and leaves `obj/` alone; that is the one-command way to settle it.
- `build/G2ME01/obj/<unit>.o` — **retail**, dtk's assembly of `build/G2ME01/asm/<unit>.s`.
  27 functions, no `__dt__`; ours has 30 because MWCC emits three template destructors.
- `build/G2ME01/asm/<unit>.s` is **retail**, and is the best view available: it carries the byte
  comments, the `@NNN`/`lbl_…` reloc annotations and the whole `.rodata`.

I lost a chunk of this run reading the two objects the wrong way round. `build/G2ME01/main.elf` is
*not* a third opinion for a claimed unit: it holds our code there. `tools/dol_read.py <addr> <n>`
reads the real disc and settles every question.

## The five that did not move

Remaining diffs are exact instruction lists, relocation-masked, from our object against
`build/G2ME01/obj/…`:

**`DrawFullScreenTexturedQuadQuarters` — 9 instructions, all stack slots.** Two adjacent blocks are
swapped. Retail: the `CVector3f` temp at `r1+44`, the `rstl::pair<CVector2f,CVector2f>` from
`SetViewportOrtho` at `r1+56`. Ours: the pair at 44, the temp at 60. Same frame (`stwu r1,-208`),
same 28 bytes total, same order of every other block (32, 72..87, 88) — only these two are reversed.
Spent: removing the `CVector3f v0` local (Prime 1's spelling, pass the temporary) → 98.6493%;
declaring `v0` before `vp` → 99.8881% and a new 4-word copy; `v0(0.f,0.f,0.f)` → 96.2537%;
`v0 = CVector3f(…)` instead of three setters → 98.5821%; `const rstl::pair<…>&` bound to the
returned temporary → 80.1866%; `rb` declared before `v0` → 99.9328% (no change); `CVector3f v0` local
to the loop → 98.5821%. Current source is the best of the seven at 99.9328%.

**`DrawScanLines` — 12 instructions, a permutation of five float values.** Both sides are 154
instructions. Ours materialises them in the order `[const, const(double), lt.y, lt.x, const, rb.x]`
into `f25…f30`; retail's is `[lt.x, const, const, const(double), lt.y, rb.x]`. The *values* agree —
`fsubs f0,f0,f26` (ours) is `fsubs f0,f0,f28` (retail) on the same value, the one both load with
`lfd` — so this is the order MWCC first needs each value, driven by expression order in the loop.
Spent: `4.f * CCast::ToReal32(i)` (Prime 1's spelling) → 89.0000%; hoisting `lt.GetY() + offset`
into `fi` → 91.7857%; `lt.GetY() + offset + 4.f*i` → 91.1104%; `(fi + offset) + lt.GetY()` →
94.6753%; `4.f * i` (int lhs) → 95.6169% (no change); `offset` as `0.0f/2.0f` → no change. The
`CVector3f v0` local is unused here and removing it changes nothing (95.6169%).

**`DrawFilterShape` — retail is 244 bytes / 61 instructions, ours 236 / 59.** Retail's
`kFS_CinematicPlaceholderLabel` block is `mr r3,r4; mr r4,r5; bl DrawCinematicPlaceholderLabel; b`.
`DrawCinematicPlaceholderLabel` is `static void (void)` (`…17CCameraFilterPassFv` in both objects),
so `r3`/`r4` are set to `color`/`tex` for a call that takes no arguments: **the two `mr`s are dead
code that retail's compiler did not eliminate and mine does.** The jump table
(`.data:0x0`, 11 entries) and the block order already agree, and every other case is
byte-identical, so this is not a case-ordering or jump-table problem. Spent: braced `case` body →
96.7213% (no change); no `break`, falling through to `CookieCutterDepth` → 75.8361% (worse);
`CCameraFilterPass::` scoped call → no change; `static_cast<void>(color)` / `(tex)` → no change;
case moved above `kFS_DialogBox` → no change. I see no spelling that makes mwcceppc *keep* dead
argument setup, and a source change cannot conjure it.

**`DrawDialogBox` — 21.8589%, 964 bytes, our logic is simply not retail's.** Prime 1's
`CCameraFilter.cpp` has no `DrawDialogBox` (its `DrawFilterShape` has neither this case nor
`kFS_CinematicPlaceholderLabel`), so there is no donor for it. `sDialogBoxOffsetY/Width/Height/Border`
in the file are still guesses. This is a from-the-disassembly rewrite of ~964 bytes and is also
what is left of the unit's `.rodata` (77.83%): the missing literals are almost certainly the
dialog box's own.

**`CCameraBlurPass::Draw() const` — 69.2236%, retail 1932 bytes, ours 1460.** 472 bytes of real
code missing. `CCameraBlurPass` is already 5/5 matched otherwise (`__ct__`, `Update`, `SetBlur`,
`DisableBlur`, `GetFbCopy`, plus the two 4- and 12-byte ones), so this is one body, not a
structurally wrong class.

## Not filed as `NEW:`

Both remaining rewrites live in the unit this item already targets, so requeuing `progress-unit-ccamerafilter`
is the right queue action; a second `NEW:` would just be the same work under a new name.

WALL: CCameraFilterPass::DrawScanLines 95.6169% - five float values in a different materialisation order; six expression spellings tried, all worse, remaining diff is register assignment only
WALL: CCameraFilterPass::DrawFullScreenTexturedQuadQuarters 99.9328% - the CVector3f temp and the SetViewportOrtho pair have their stack blocks swapped; seven local-declaration spellings tried, all worse
WALL: CCameraFilterPass::DrawFilterShape 96.7213% - retail keeps two dead argument-setup `mr`s before a `void(void)` call; five case-body spellings tried, no source makes mwcceppc emit them
