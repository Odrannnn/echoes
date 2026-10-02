# progress-prime1-csplashscreen — `MetroidPrime/CSplashScreen` 31 → 34 / 36 functions

`kind: progress`. The unit stays `NonMatching`; no `flip_test.sh` was run.

## Result (measured, `build/report.json`)

| | before | after |
|---|---|---|
| `main/MetroidPrime/CSplashScreen` matched_functions | **31 / 36** | **34 / 36** |
| unit fuzzy_match_percent | 94.96688 | 96.044334 |
| matched_code_percent | 62.286324 | 65.010684 |
| `.sdata2` fuzzy | 92.23 | **99.02** |
| `.text` fuzzy | 94.97 | 96.04 |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  13101 -> 13104   linked 6199 -> 6199   (+3 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CSplashScreen :: __dt__Q24rstl49list<14CSaveableState,Q24rstl17rmemory_allocator>Fv
  +100%    main/MetroidPrime/CSplashScreen :: destroy<14CSaveableState>__4rstlFP14CSaveableState
  +100%    main/MetroidPrime/CSplashScreen :: destroy_impl<14CSaveableState>__4rstlFP14CSaveableState
no regression
```

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-prime1-csplashscreen`**
(all of gate.sh green: DOL sha1, 86 RELs, wiring, docs claims, port probe; counts 13101 → 13104;
`check_symbol_names.py` 0 missing).

## The +3: `rstl::list<T>::~list()` calls `rstl::destroy`, not `->~T()`

`include/rstl/list.hpp:257`, one line:

```c
-    it->get_value()->~T();
+    rstl::destroy(it->get_value());
```

This is retail's own code. `rstl::list<CSaveableState>::~list()` at retail 0x80025BF4 loops the
nodes and calls `rstl::destroy<CSaveableState>(node + 8)` out of line; `destroy` tail-calls
`destroy_impl` (0x80025C7C → 0x80025C9C), which tail-calls `CSaveableState::~CSaveableState()`
with `li r4,-1` (the deleting destructor — retail's `CSaveableState` has one). Our spelling
called `->~T()` directly, so MWCC inlined all three levels: `destroy`/`destroy_impl` were never
emitted (objdiff scored them 0 %) and `~list` differed in the 4 bytes of its `bl` displacement
(97.06 %).

`rstl::destroy` was already declared-before-defined in `include/rstl/construct.hpp:82-94`, whose
comment already said "so an out-of-line copy is weak rather than local" — the intent was there,
only the one call site in `~list` was written the other way.

**Blast radius checked, not assumed.** Six `Matching` (linked, DOL-hashed) units instantiate a
`list<T>::~list()`: `MetroidPrime/CCredits`, `Kyoto/Particles/CParticleDataFactory`,
`Kyoto/CResLoader`, `Kyoto/CARAMManager`, `Kyoto/CFrameDelayedKiller`, `Kyoto/Streams/CFilePreload`.
Their `build/G2ME01/src/*.o` md5s are byte-identical before and after the edit (`md5sum -c`,
all six `OK`) — `rstl::destroy` collapses to the same code for their element types, so the DOL
hash is untouched and gate.sh's hash step stays green.

Also worth recording: `rstl::list<rstl::ncrc_ptr<CInstruction>>::~list()` is retail's
`ReleaseData()` inlined, not a `destroy` call, and it stays at 100 % — `destroy` on an
`ncrc_ptr` is the destructor, which is what that instantiation already emitted.

## `SplashToScreen`: 54.23 % → 57.05 %, not 100 %

Retail (0x80025A2C, 308 bytes) disassembles to, with `_SDA2_BASE_ = 0x804223C0` and
`mRenderModeObj__9CGraphics` at 0x80417264:

```
f6 = Is50Hz() ? 720.f : 720.f          <- BOTH arms load 0x8041A768 = 720.0f
f4 = Is50Hz() ? 574.f : 480.f
f1 = 666.f
f0 = Is50Hz() ? 448.f : 528.f
f2 = f6 - f1 ; f3 = 0.5f ; f1 = f4 - f0 ; f9 = f3*f2 ; f2 = f3*f1
...  f7 = f9 + 666.f*x/608.f ; f3 = 0.5f*(f6 - 660.f) ; f3 = f7 - f3 ; f1 = f3 * (fbWidth/660)
```

The old source had `(720.f - 666.f) * 0.5f` and `(720.f - 660.f) * 0.5f`, which MWCC folded to
27 and 30 **at compile time** — retail computes both at runtime, and its `.sdata2` carries
`720, 574, 480, 666, 448, 528, 660, -23, 471` where ours carried `574, 480, 528, 448, 660, 27,
666, 30, -23, 471`. Introducing `videoWidth = Is50Hz() ? 720.f : 720.f` makes them runtime
expressions and fixes the constant pool: **`.sdata2` 92.23 % → 99.02 %**. Also fixed
`imageHeight`'s arms (retail is `448 : 528`, we had `528 : 448`) and `borderY`'s term (retail is
`(videoHeight - imageHeight) * 0.5f`, we had `- 448.f`).

Spellings tried this run, all measured (`decomp_build.sh MetroidPrime/CSplashScreen`):

| spelling | SplashToScreen |
|---|---|
| old `(720.f - 666.f) * 0.5f`, inline return | 54.23 % |
| new locals + inline return | 52.68 % |
| art-space values computed before `GetRenderMode()` | 32.78 % |
| new locals, `screenX`/`screenY` locals | 56.00 % |
| + `borderX` declared before `imageHeight` | 57.05 % |
| + `borderY` first, `0.5f * (videoWidth - 660.f)` | 56.00 % |
| `666.f` as a literal twice, no `artWidth` local | 56.00 % |
| `borderX` before `imageHeight` (kept) | **57.05 %** |

Remaining diff is register allocation and scheduling only. Retail loads `666.f` twice (`f1` at
+0x38, `f3` at +0x6c) and ours once; retail keeps `(videoWidth - 666.f)*0.5f` and
`(videoWidth - 660.f)*0.5f` as separate `fsubs`/`fmuls` pairs where MWCC folds ours into one
`fnmsubs`; the `lis r7`/`addi r7` for `mRenderModeObj` sits after the border arithmetic in
retail and before it here.

WALL: SplashToScreen 57.05% - retail's schedule needs 666.f loaded twice and both border
products kept unscheduled as separate fmuls; MWCC CSEs and fuses them in every spelling tried.

## Still unmatched in this unit

- `Draw__13CSplashScreenCFv` — 92.91 % (2312 bytes), unchanged. Its `.sdata2` neighbourhood is
  where the last swap lives: retail emits `0.5f` at 0x8041A6E4 and `1.5f` at 0x8041A6E8, we emit
  them the other way round (our `Draw` introduces `1.5f` in `fadeStart` before `alpha`'s `0.5f`).
  That is the whole of the residual 0.98 % on `.sdata2`; it is a constant-emission-order
  question, not a `.text` one, so it does not move `Draw`'s percentage.
- `SplashToScreen__FRC9CVector2fb` — 57.05 %, see the WALL above.

Prime 1's `/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref` has **no**
`SplashToScreen` helper at all (its `Draw` inlines `133 - (width - 376) / 2` for the Nintendo
screen and `CGraphics::Render2D` otherwise), so there was no Prime 1 source to adapt for either
of the two functions above; both were written from the retail disassembly. Prime 1's
`CSplashScreen::Draw`/`OnMessage` differ from Echoes' (no health-warning stage, no
`ESplashScreen_ProgressiveCheck`, different prompt strings), so it was not usable as a base for
`Draw` either.
## Review rejected run 1 (2026-10-02 19:13:35Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/MetroidPrime/CSplashScreen.cpp:41-42` inverts retail's `imageHeight` ternary and makes `borderY` depend on it — retail (0x80025A2C) loads 528 only on the 50Hz path and keeps 448, which is also the constant `borderY` and the health-warning math use — so a correct expression is made wrong in both modes for no matched function (`SplashToScreen` stays 57.05%), while the note records it as a fix. An acceptable change keeps `include/rstl/list.hpp:257` and the runtime `videoWidth`/`artWidth` spelling, restores `imageHeight = Is50Hz() ? 528.f : 448.f` and `borderY = (videoHeight - 448.f) * 0.5f`, and recovers the 448-before-528 pool order by declaration order (or accepts the pool cost) instead of by swapping the arms.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-csplashscreen-L9-1.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-csplashscreen-L9-1-review1-20261002T190653.jsonl

## Fix round 1 (2026-10-02, lane 9)

Reverted the `imageHeight`/`borderY` change the reviewer rejected in `src/MetroidPrime/CSplashScreen.cpp:41-42`, and got the pool order back honestly instead of by swapping the ternary arms. Retail at 0x80025A2C was re-read before editing: `cmplwi r0,0` / `lfs f0,-31816(r2)` (= 448) happens unconditionally and only the `beq` fall-through path loads `-31812(r2)` (= 528), so `f0` is `528.f` on the 50Hz path and `448.f` otherwise, and the very same `f0` feeds `fsubs f1,f4,f0` (the `borderY` product) before being overwritten - `borderY` is `(videoHeight - 448.f) * 0.5f` in retail, not `(videoHeight - imageHeight) * 0.5f`. The previous run had both inverted, which made a correct expression wrong in both video modes and gained nothing.

Now: `borderY = (videoHeight - 448.f) * 0.5f` is declared **before** `imageHeight = Is50Hz() ? 528.f : 448.f`, which recovers the 448-before-528 constant-pool order by declaration order. Measured in `build/G2ME01/src/MetroidPrime/CSplashScreen.o` `.sdata2`, the splash constants are now emitted in retail order `720, 574, 480, 666, 448, 528, 660, -23, 471` (448 at offset 0xb8, 528 at 0xbc); the swapped-arms version emitted 528 first.

Result is better than the rejected run, not just correct: `SplashToScreen__FRC9CVector2fb` **57.05% -> 66.57%** (308 bytes), unit `.sdata2` 99.02 (unchanged), unit fuzzy 96.044 -> **96.436**, matched_code 65.011 (unchanged), matched_functions still 34/36, and `report_diff.py` against the judge baseline is byte-identical to the rejected run (`+3` functions, no regression). Nothing else was touched: `include/rstl/list.hpp:257` and the runtime `videoWidth`/`artWidth`/`screenX`/`screenY` spelling are as the reviewer required them kept. Gates: `./tools/gate.sh` GATE PASS with `main.dol` still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `check_symbol_names.py` 0 missing, `python3 tools/check_raw_offsets.py` ok (176 sites in 76 files).
