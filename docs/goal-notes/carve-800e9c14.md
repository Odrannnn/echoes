# carve-800e9c14 - gate fix round

## Gate fix round

`raw-offsets` was the only failing `gate.sh` step; `build/gate-raw.log` named exactly one problem:

```
raw offsets not accounted for:
  src/MetroidPrime/Carve800E9C14.c                                   1 sites, no section in raw_offsets.md
```

**No code was touched.** The carve's `Matching` state, the six call sites and the `+0x2C8` load
are unchanged; this round is the `docs/research/raw_offsets.md` bookkeeping the checker asks for
when a new file gains a raw offset. `files-cmake`, `docs`, `decl-order` and `module-order` were
already `ok` and were re-run to confirm they stayed ok.

### `docs/research/raw_offsets.md`

1. **New section `## \`src/MetroidPrime/Carve800E9C14.c\` (1 site)`**, appended after the
   `src/Kyoto/Animation/CAnimCharacterSet.cpp` section at the end of the file - the same place
   the previous `progress-unit-canimcharacterset` head put its own, so the newest `##` blocks
   read together. `1` and the site are the tool's, not recalled:
   `python3 tools/check_raw_offsets.py --list` prints the file at `1` and `line 43  +0x2C8`.
   The body is **Kind B, unmodelled member, reached through a free function**, and the
   identification is measured rather than inferred:

   - the member is retail's `CPhysicsActor::unk2`, the lazily heap-allocated 60-byte object
     built from `mskNullBox` - `li r3, 0x3c` / `bl __nw__FUlPCcPCc` (0x3C = 60), stored with
     `stb r0, 0x2c4(r31)` / `stw r4, 0x2c8(r31)`. Those are retail's offsets and are already
     recorded with that disassembly in `docs/goal-notes/progress-prime1-cphysicsactor.md:95-99`,
     so the section cites them rather than re-deriving them.
   - the six callers are the six `bl fn_800E9C14` in `build/G2ME01/asm/` (two in
     `MetroidPrime/CGroundMovement.s`, two in `auto_03_80218E28_text.s`, one in
     `Player/CPlayerDynamics.s`, one in `Player/CPlayer.s`), each null-testing the result and
     passing it to a `CAABox`-shaped call with `mskNullBox__6CAABox` - `mr. r28, r3 / beq` at
     0x80186580, `cmplwi r3, 0x0` at 0x8012A488, `fn_80258A68` / `fn_80258970` at 0x8001391C /
     0x8001392C. All quoted addresses read off the asm.
   - blocker: `include/MetroidPrime/CPhysicsActor.hpp:249` has `void* unk2` and no accessor, and
     `CHECK_SIZEOF(CPhysicsActor, 0x2d0)` (`CPhysicsActor.hpp:252`) already holds - so this is a
     getter with no layout consequence, **not** the per-class repair the other kind B entries
     need. Recorded that the offset itself is still unverified in this build: only the class
     total is asserted, never its per-member offsets, so `offsetof(CPhysicsActor, unk2)` is the
     thing to check before landing the accessor.

   One number from the carve's own source comment was **dropped rather than repeated**: the
   comment claims "186 two-instruction `lwz r3, X(r3)` / `blr` accessors in the asm", and a
   sweep of `build/G2ME01/asm/**/*.s` for `lwz r3, 0x…(rN)` immediately followed by `blr`
   counts 328 for `rN == r3` and 348 over every base register. The figure is not reproducible
   and the doc's rule is to measure, so the section says the asm "is full of" the shape instead.

2. **The total in `## The debt, measured` is now one measured line: 168 sites in 72 files.**
   `python3 tools/check_raw_offsets.py` prints `168 raw-offset site(s) in 72 file(s)` and the 72
   `##` headings sum to 168 - both re-derived after the edit, not copied from either.

   The section previously held **three different totals at once** (161 in 68, 165 in 69, and a
   narrative of the earlier drift), each individually defensible and none of them the number
   the tool prints, because every head had *appended* its own sentence instead of replacing the
   one before - the exact failure the file records about itself twice. All three are gone, and
   the paragraph that replaces them states the superseded history once and points at the
   tool. This is a doc-only correction: `check_raw_offsets.py` compares per-file heading counts
   and never the total, which is why three of them could sit there unnoticed.

3. **`src/Kyoto/Animation/CAnimCharacterSet.cpp`'s section** said "It is 1 of 165 sites in 69
   files (measured 2026-10-01)". That figure was right when written, so it is annotated rather
   than rewritten: it now reads as 1 of the 168-in-72 total at the top of the file, with the
   165-in-69 reading kept and marked as history.

### Verification

```
python3 tools/check_raw_offsets.py    ok: 168 raw-offset site(s) in 72 file(s), all documented
                                       in raw_offsets.md          (was: 1 unaccounted file)
python3 tools/check_raw_offsets.py --list | tail -1
                                      total: 168 raw-offset sites in 72 file(s)
headings in raw_offsets.md           72 headings, summing to 168  (counted independently)

python3 tools/check_docs_claims.py    ok          (re-run: the doc edit must not have moved a
                                                derived claim)
python3 tools/check_decl_order.py     ok
python3 tools/check_files_cmake.py    ok
python3 tools/gen_module_order.py --check   ok
```

The last four were already `ok` in `build/goal/check.out`; they are re-run here because this
round edited a file the gate reads. `tools/` was not modified, nothing was committed, stashed or
checked out, and no matched code's behaviour changed - the diff is `docs/research/raw_offsets.md`
alone on top of the item's existing unstaged change.
