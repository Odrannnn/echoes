# Process lessons

General working practice for reverse engineering and porting, kept separate from
`RUNNING_THE_DECOMP.md` because that file is technique *for this project* and this one is not
about any project.

Every lesson here was paid for. Most cost a session. Where a lesson came from one incident I say
so, because a rule observed once is a hypothesis and a rule observed four times is a rule. The
evidence is in `docs/research/` and the file named under each one.

Nothing here is specific to GameCube, to Metroid, or to decompilation.

---

## 1. A test that cannot fail proves nothing

**The failure.** A change was applied to two files and the build reported everything fine. It was
not fine. The reason: the change produced byte-identical output, so nothing downstream relinked,
so **no test had anything to check.** The test passed because it never ran.

**The rule.** Before believing a green result, ask *what would have to be different for this test
to fail?* If the answer is "nothing observable", the test is decoration. Deliberately perturb the
thing you changed and confirm the test catches it.

This is not exotic. It is the same failure as a test suite with no failing case: it has never
demonstrated that it can detect anything.

*Evidence: `docs/research/rel_rename_hazard.md`, experiments 6 and 7 — the same change, tested at
two scales, where the smaller test could not fail.*

## 2. Three checks agreeing is not one check agreeing

**The failure.** A change left every object byte-identical, the primary integrity hash intact, and
78 of 86 secondary hashes intact. It was still wrong, and the 8 failures were real. The per-
function diff saw nothing at all.

**The rule.** "Most checks pass" is not "checks pass". Independent-looking checks are often
downstream of one assumption, and when they are, they fail *together* or not at all. Identify
what each check actually measures, and ask whether the thing you changed is upstream of it. Three
checks derived from the same artefact give you one check, run three times.

*Evidence: `docs/research/rel_rename_hazard.md`.*

## 3. A check that asserts the model against itself is a tautology

**The failure.** Two sessions cited a compile-time size assertion as "confirmation" of a
structure's size. The assertion is of the form *the model's declared size equals the declared
size* — it passes for **any** value, including the wrong one. It was quoted as evidence three
times before someone read what it actually said.

**The rule.** For every check you rely on, read its body and ask *what could make it fail?* If
the answer involves only the file's own declarations, it is a tautology and it is not evidence.
The fix is an **independent** measurement — a different tool, a different language, a probe
compiled with different flags — not a second reading of the same declaration.

This is the single most transferable lesson here, because tautological checks feel like rigor.
They are the opposite.

*Evidence: `docs/research/rel_rename_hazard.md` and `docs/research/paks.md`, the `CHECK_SIZEOF`
discussion.*

## 4. A tool fed stale derived data will quietly produce a wrong answer, and may destroy committed work

**The failure.** A code generator's input was derived from a log file. Running the generator
against a log that predated the most recent changes yielded an **empty** input — not because
there was nothing to generate, but because the things it would have generated had since been
*fixed*, and so no longer appeared in the log. The generator faithfully wrote a 16-line file over
a 760-line one, deleting committed work.

**The rule.** Any generator whose input is derived from an observed artefact needs three
properties, and all three are cheap:

- **refuse empty and shrunken output** rather than writing it;
- **state the input's provenance in the output**, so a reader can tell it is a snapshot;
- **know which input makes the output grow monotonically.** Here the safe set *shrinks by design*
  as fixes land, so a smaller input is expected — which is exactly why "smaller" had to be
  refused rather than trusted.

Corollary: **a regeneration step in a commit should be reproducible from that commit.** If it is
not, it is a deletion with extra steps.

*Evidence: `docs/research/port_link_stubs.md`.*

## 5. Record the baseline before the first edit, and know how it fails

**The failure.** A comparison baseline went stale. Every check afterwards reported cumulative
gains — "91 units newly linked" over a session — which was true and about nothing. A real
per-change signal was indistinguishable from a session's worth of work.

**The rule.** Capture the baseline on a clean tree, **before** the first edit, and capture it
again deliberately when you intend to. Then know the failure direction: a stale baseline was
*conservative* — it could not hide a regression, only inflate the delta. Not every tool fails
safe, and the ones that fail loud are the ones worth having.

The deeper point: **an instrument that reports a number nobody checks against intent is a number
that will eventually be quoted.**

*Evidence: `docs/research/rstl_string_member_op.md`; the baseline discipline is in
`tools/gate.sh`.*

## 6. A reason written down gets re-read instead of re-measured

**The failure.** Three separate times in one session, a confident sentence in a handoff document
sent work in the wrong direction. Each time the sentence had been *inherited* from an earlier
report rather than measured by the person acting on it, and each time it was wrong. The pattern
was not that the original author was careless — it is that a written reason has the same status
to its reader as a measured fact, and it does not have that status.

**The rule.**

- **Mark superseded claims in place; do not delete them.** A future session needs to know the
  claim was once believed, and by whom, or it will re-derive the same confidence.
- **State what is proven and what is not, separately.** "Proven: X. Not proven: why — the next
  step is one breakpoint." is worth more than a confident guess, because a guess gets inherited
  by the next four people.
- **Inherited reasons get one cheap measurement before they get a lane's budget.** Not because
  the author was wrong, but because the cost of checking is minutes and the cost of acting on an
  unverified premise is a session.

*Evidence: three incidents in one session, all recorded in `docs/HANDOFF.md`'s corrections.*

## 7. Order is not placement, and a proxy is not a measurement

**The failure.** A structure's layout was inferred three times from the *order in which its
constructor touched members* — and the real layout was different all three times. The inference
is invalid in general: members needing no construction can sit anywhere, so a constructor's call
order says nothing about offsets. The thing that settled it was a **store to a global** elsewhere
in the same function — an instruction that says "this address is that object" outright.

**The rule.** Distinguish what an artefact *shows* from what it *implies*. Prefer, in order:

1. a value the code explicitly publishes (a stored pointer, a constant, a size argument);
2. a value reachable by arithmetic from one;
3. anything inferred from sequence, order, or adjacency — and treat (3) as a hypothesis, however
   confident it sounds.

**Rule of thumb: if your conclusion is about layout, membership, or ownership, and your evidence
is the order things happen, you have a hypothesis.**

*Evidence: `docs/research/paks.md`, the three corrections and why each earlier one failed.*

## 8. The attribute that explains a pattern is usually not the one varying

**The failure.** A pattern in generated code was attributed to a template parameter's size. It was
not: the discriminator was **which translation unit the code was compiled into**, and the varying
"size" spanned a range that included both sides of the pattern. Two more instances in the same
session: a difference attributed to difficulty that was really *whether a called symbol existed*,
and a difference attributed to a function's shape that was really *the class it belonged to*.

**The rule.** When an attribute seems to explain a pattern, **check that the attribute actually
varies with the pattern.** List the instances on both sides and confirm they differ on the
proposed attribute. If they don't, the attribute is a coincidence of the sample and the real
discriminator is usually structural — the object, the module, the declaration context.

*Evidence: `docs/RUNNING_THE_DECOMP.md`'s entry on per-translation-unit header revisions.*

## 9. A set is not an order

**The failure.** Static analysis established that 318 missing symbols were *reachable* from the
program's roots. That number was correct, and it was the wrong shape of answer: it said nothing
about which one the program asks for first, and the first three turned out to be three methods of
a single small class, while the largest class in the set was nowhere near the front of the path.

**The rule.** "What is broken" and "what do I fix first" are different questions needing different
data. A dependency graph gives you the first; running the thing gives you the second. If you
have a cheap way to observe the second — instrument it and let it tell you — that observation is
worth more than any amount of reasoning about the first, and it costs one run.

**The loop this enables is the real prize: fix the top of the observed order, re-run, and the
next item names itself.**

*Evidence: `docs/research/port_link_stubs.md` and `docs/research/boot_probe.md`.*

## 10. An empty result and a broken instrument look identical

**The failure.** An instrument was configured to use a software rasteriser. Under that
configuration it reported **zero** findings. The zero was entirely an artefact: the run died inside
the graphics layer before reaching the code under test. A real zero and a broken instrument
produce identical output.

**The rule.** Any instrument that can report "nothing" must have a **positive control** — a case
where it is known to report something. If you cannot construct one, then "nothing" is
uninterpretable and must be reported as such. When you change an instrument's configuration,
its *finding count* is itself a thing to check, not a thing to report.

*Evidence: `docs/research/boot_probe.md` — a hardware/software ICD preference.*

## 11. Your tooling will report its own bugs as product bugs

**The failure.** A diagnostic build was produced with a flag intended to let an incomplete binary
run. The flag gave unresolved symbols a zero-filled jump slot. The resulting crash was reported —
with a real-looking backtrace through real functions — as a defect in the product. It was
entirely an artefact of the flag, and the "faulting function" was a red herring. It was nearly
committed as a finding.

**The rule.** **When the instrument is what changed, verify the instrument before believing its
first finding.** Specifically: when a diagnostic finds a defect, ask whether the diagnostic's own
mechanism could produce that finding. A crash address inside the loader, at a zero, or in an
unexpected section is a property of the loader.

Corollary, and this generalises past diagnostics: **an instrument that reports its own failures
as results is worse than no instrument**, because it manufactures evidence.

*Evidence: `docs/research/boot_probe.md`, the PLT-hole section.*

## 12. Make instruments self-checking, and let them stop

**The failure.** An instrument reported a confident, plausible, wrong split — and printed no
warning, because nothing was watching. Adding one assertion made it stop, and that assertion then
caught a real bug the instrument had been hiding.

**The rule.** An instrument should carry at least one invariant that, if violated, makes it
**refuse to produce output** rather than produce wrong output. Cheap forms:

- a **containment check** — the smaller set should be a subset of the larger; if not, one of the
  inputs was read wrong;
- a **count check** against a known total;
- a **format check** — are both sides of a comparison in the same representation? (Comparing
  demangled names against mangled ones is a silent, total, and very plausible failure.)

Silent wrongness is the target. A tool that stops is annoying; a tool that is confidently wrong
is corrosive, and the confidence is what makes it corrosive.

*Evidence: `tools/link_reach.py`'s self-check, and `docs/research/port_link_stubs.md`.*

## 13. Gross is not net, and progress can look like regression

**The failure.** A change that made five things work also made the measured problem look five
worse — because completing a piece of work *names* the dependencies it turns out to have. This
happened five times in one session and is counter-intuitive enough to be worth stating as its
own rule: **a decompiled body names the callees it reaches, so finishing a function converts one
unnamed gap into several named ones.**

**The rule.** Report gross closed and gross opened, separately, and say which way the *net* moved
and why. Two corollaries:

- **A definition only closes a gap if something still references the symbol.** Adding a definition
  for something nothing calls is not progress.
- **A named hole is cheaper than an unnamed one.** Converting an unknown into a specific, bounded
  task is real progress even when the total goes up. Say that explicitly when it happens, or the
  next reader will read a rising number as a regression and revert the work.

*Evidence: five separate instances; the clearest is the commit that took the remaining undefined
count from 318 to 330 while landing 15 functions at 100%.*

## 14. Identify unknown code by diffing against a known instance

**The failure.** A function had no name in the symbol table. Rather than guess what it was, it
was **byte-compared against a named instance of the same template elsewhere in the binary**. They
were identical, instruction for instruction and register for register — which identified the
template, the element type, and a member that had been misread.

**The rule.** In any binary containing repeated structure — templates, macros, generated
thunks, compiler helpers — **one named instance is a Rosetta stone for every unnamed one.** A
mechanical diff of instruction words and register numbers is cheap, unambiguous, and does not
require understanding the code at all. It frequently answers questions that reading the code
cannot, because it removes the code from the question.

**Generalise the form: before interpreting, look for a twin.**

*Evidence: the `do_insert_before` identification, recorded in `docs/research/paks.md`.*

## 15. Small tempting fixes have cost proportional to blast radius — measure it once, don't defer three times

**The failure.** A one-line header change was deferred three times with the same justification:
"it shifts a widely-included header, so it needs a measurement." Three sessions produced three
re-derivations of the surrounding facts and no measurement. When finally done, the measurement
showed it moved **two functions in one unit** and nothing else.

**The rule.** The blast radius of a change is *not* proportional to its size, and it is not
proportional to how much the code "obviously" depends on it — it is proportional to **how widely
the changed declaration is included**, which is a countable fact. Count it. Then either do the
change with a report, or record explicitly that the cost was measured and accepted. What is not
acceptable is re-deciding it every session from the same intuition.

**Deferral is a decision. Make it once, write down what it cost, and stop re-opening it.**

*Evidence: the `CGameGlobalObjects` padding investigation, deferred three times, then resolved
by a store to a global.*

## 16. Parallel work needs explicit ownership, and collection must be mechanical

**The failure.** Several times in one session, two independently-produced results contradicted
each other on a shared fact. Each was correct about its own evidence; the disagreement was about
scope. Resolving it required going back to the artefact rather than to either report.

**The rule.**

- **Give each worker explicit ownership of paths**, and say what it must not touch. Overlapping
  ownership produces conflicts that look like errors and are really design failures.
- **Collect mechanically** — three-way apply onto a known-good base, then measure the merged
  result rather than trusting any report. A hand-collected result is where a silent revert hides.
- **Re-derive shared facts once, from the artefact, after collection.** Not from either report.
- **Prefer a tool-generated diff over a hand-built patch.** A patch assembled from the wrong
  directory loses files without erroring, and the gate will not tell you, because a unit without a
  configured range contributes nothing and passes.

*Evidence: `tools/collect.sh` and its header; the collection failure recorded in
`docs/HANDOFF.md`.*

---

## 17. A constant derived from a host property is a bug wearing a passing test

`kAllocatorPointerBits = sizeof(void*) * 8` looked correct, compiled, reviewed, and **reproduced
retail exactly** - on retail's own word size. It was a mask width (how many low bits of a pointer
are flags) derived from a pointer size (how wide a machine's pointers are). Those are different
quantities that happen to agree at 32 bits.

On a 64-bit host the mask became six bits instead of five, and the extra bit was not a flag - it
was the block address's own `0x20`. Every accessor that read a block pointer silently truncated it
by 32 bytes. It went undetected through an entire session because *the setters were correct*: they
OR the old low bits back in, so the stored value was always right and only the read was wrong, and
nothing reads these values until the allocator is already walking a list.

Three properties made this survive review, and all three are general:

- **It is a no-op on the reference platform.** A derivation that reproduces the target exactly is
  the *least* likely place to look for a bug, and it removes the only cheap test.
- **It compiles and type-checks.** No tool objects: the types are right, the arithmetic is right,
  and the value is a legal mask.
- **Its failure is far from its cause.** The crash was in a length comparison, ten frames from the
  mask.

The general fix is not "use the right number on this host" — that trades a protocol bug for a
host-specific patch and is wrong everywhere else. It is to **ask what the constant means, and
whether the thing it is derived from is the same kind of thing.** A field width is a protocol
property; a pointer width is a machine property. Pin the protocol to the protocol's value, and
prefer a derivation from the thing that actually determines it — here, deriving the alignment from
`sizeof(SGameMemInfo)` rather than hardcoding `64`.

Corollary, and the part worth keeping: **the fix is only trustworthy if it is a no-op on the
platform whose bytes you must not change.** That is checkable, and it is the check to reach for
first. A change that is a no-op under the reference build cannot have broken the thing you are
being asked to preserve — so it converts an open-ended argument into a measurement.

*Evidence: `docs/research/allocator_flag_mask.md`; the reverted `uint x4_len` and span-rounding
experiments recorded there, both reverted for costing a Matching function its 100% match.*

---

## 18. A byte-perfect body in a unit that cannot pair scores nothing — and a Matching unit can hide a header that is badly wrong

Two failures with the same root: **nothing in the ordinary build output is a measurement of whether
the right thing was compiled.**

`CActor::SetDirtyFlags` compiled to all 56 bytes retail has, and objdiff still could not pair it,
because the address was outside its unit's claimed range. Byte equality is not a result until
objdiff *sees* the pairing.

`CAnimDataModelSlots` was `Matching` at 100.00% while reading members through a **local duplicate
shape**, and the real `CAnimData` was **0x78 bytes wrong** (`sizeof` is 0x5B8, not the 0x578 the
tree believed). A unit that loads one word per slot through a local struct is layout-immune, so the
error survived exactly as long as the class was only touched that way. The tell is always the
allocation site: retail's `li r3,1464` before `operator new` is a size you can read, and it is
worth reading.

**So: when a class is involved, go find a `li` before an allocation, or a `stfs` at the high end,
before trusting any header.** `CHECK_SIZEOF` cannot help - it is a consistency check, and
`check_sizeof<T,n>` passes for any `n`. And a passing `Matching` unit in the neighbourhood is not
evidence about the header it reads; it is evidence about the shape it happens to use.

## 19. "The obvious reading of the fix" is the diagnosis you should distrust most

`SetDirtyFlags` carried a written diagnosis - *retail's `rlwimi` are fields 3-6 of the 0x150 group
but every access is `lbz`/`stb`, so the group must be split into `u8`s* - and it was wrong. No
header change was needed at all. mwcceppc packs a 32-bit bitfield unit MSB-first and then picks the
narrowest access covering the field, so a 1-bit field in the group's **first byte** already compiles
to `lbz`/`stb`, with `mb` = 24 + the index **within that byte**. Splitting the group would have
changed a layout that was right, on the strength of an access width that proves nothing about the
declaration.

**An access width is evidence about codegen, not about a type.** `lbz` on a `bool : 1` inside a
`uint` group is what correct code looks like. A diagnosis that names a type from an instruction
width has skipped the step where you check what the compiler actually does with that width.

## 20. A loose fallback that is not loose is worse than no fallback

`report_diff.py` had a rename detector with a same-unit test and a "same module" widening. For a
DOL unit the module key `unit.split("/")[0]` is the string `main` for **every** unit, including
every `auto_*` one - so the widening branch matched the entire binary. Partners are consumable and
the loop iterated in sorted order, which puts `main/auto_03_*` before `main/rstl/*`, so an auto
unit claimed a same-size partner from anywhere in the DOL before the genuine rename was considered.
The result: a unit that had gone from 97.22% to a clean `Matching` 100.00% was reported as `GONE`,
and the gate failed on an improvement.

Three things had to be fixed, and only the third is the real one: resolve partners in **two global
passes** (same-unit everywhere first, module-wide only for the leftovers) rather than a fallback
inside one call; and note that **"same module" needs a module identity that is actually finer than
the container**. A hierarchy that bottoms out in a constant is not a discriminator.

## 21. A metric that only exists to warn goes stale and starts costing you

`rmemory_allocator.hpp` carried a comment whose entire purpose was to warn against inlining the
template: *"costs six functions at 100% in three `Matching` units and drops the linked total
1771 -> 1765"*. Re-measured the same day, it was **seven in four, and 1828 -> 1821**. The ratios
never changed, so the conclusion never changed - but the numbers were wrong, and they were wrong in
a comment written specifically to be trusted at a distance.

**A figure in a warning is load-bearing, so it decays faster than any other figure in the tree**,
because everything around it moves. Re-measure it when you re-measure the thing it warns about, or
delete the number and keep only the direction. This is the same failure as a stale state block, and
it is worse here: a stale state block gets checked by `tools/check_docs_claims.py`, and this did
not.

## 22. When a metric cannot be satisfied, satisfy the metric that means something

The link gap rose by 6 in the batch that took the boot from step 7 to step 17. The gate's link-gap
step could have been satisfied by reverting, and the boot would have gone back to
`gpGameState is null` - green, and worth nothing.

The honest move is to keep both numbers visible and make the *behaviour* the acceptance criterion:
the stop message changed, it changed to name the three functions now blocking step 18, and two of
those three have since closed. **A count that can only go down is a proxy for a thing that can also
go up, and optimising the proxy past the thing is the failure mode.** Record the regression, state
the compensation, and make the next step's gate check the thing rather than the proxy.

## 23. A subagent's report is a claim about the world, not a measurement of it

The most expensive error in this project was not a wrong body, a wrong offset, or a wrong
hypothesis. It was **repeating a lane's report as fact in a commit message and in the handoff
document**, and the report was accurate — about the lane's own worktree.

Two claims went into commit `e7337ed` and into `docs/HANDOFF.md` as fact:

- `CMain::ResetGameState` is "`Matching` 100.00%" — it is **98.61% and `NonMatching`**
  (`configure.py:512`).
- `CErrorOutputWindow` is "written at 78.56%" — **it has no landed source at all.** Only
  `src/MetroidPrime/PortReachStubs.cpp` defines it, and the symbol is still on the port's
  link-gap list.

Both were caught by one lane whose entire brief was to disbelieve the commit messages rather
than read them, and the cheapest possible check would have caught both:
`python3 -c "import json; ..."` on `report.json`, and `grep` for the symbol. A
`grep -rn CErrorOutputWindow src/` takes two seconds.

**Why this is so easy to get wrong.** A lane report is *better* evidence than a guess: it has
numbers, printed verification, and a unit that passed `flip_test`. It reads like a measurement.
But it is a measurement **of a tree the orchestrator has not seen**, and a lane's worktree is not
the master tree. Collection is where the two diverge, and a report is collected *for its
conclusions*, so nothing downstream ever re-reads the tree.

**The rule, stated so it can be followed rather than admired:** *before a report's conclusion
enters a commit message, one cheap command must confirm it in the tree that will be committed.*
Not the percentage — the **artefact**. "It is `Matching`" is confirmed by reading
`configure.py` for `Object(Matching,`. "It is written" is confirmed by finding the file and the
symbol in it. If the check is awkward, that is a sign the claim is not yet a fact.

**The general form: a number copied from a report is a number with an unverified denominator.**
The same review found `315 -> 313` should be `317 -> 315`, `e7337ed`'s `315 -> 321` should have
been measured at `317`, and that the batch's "one new link symbol" was **none** — because the
symbol was already stubbed, which is the very fact the commit's own next sentence gives as the
reason it needed a hand deletion. **The evidence for a claim and the evidence against it were in
the same paragraph, and only one of them had been read.**

And the corollary for delegation: **a lane's brief should say what to report, and the report
should be read as a hypothesis until the orchestrator has run one command against its own tree.**
The failure was not the lane's. It was treating a well-formatted report as a terminal state.

## 24. A rewrite keyed on the value it is about to replace stops working the moment it succeeds

The script that maintained this project's handoff state block searched for the literal prefix
`"linked     2551"` and replaced it with the current number. That is the smallest possible
search key, and it is a trap: **after the first successful run the line read `linked     2554`,
no longer matched its own key, and every later run was a silent no-op for that line - forever,
with no error and no output.** The same held for the other three lines, so the block froze at
whatever each line happened to hold when its key last matched. `linked` sat at 2554 while
`build/report.json` said 2555 for an unknown number of commits.

A writer that has stopped working is indistinguishable, from the outside, from a writer with
nothing to do. Both exit 0 and both print a reassuring summary. The two only separate when you
inject drift and watch for a failure, which is the only test that distinguishes them.

The same script had a second defect in the same three lines: its replacement template for the
`REL units` line **ended mid-sentence**, and because the key matched it faithfully overwrote
the real prose with a dangling fragment. The checker could not see this, because a presence
test cannot see a sentence that stops in the middle. Checking the *shape* of a line is a
different check from checking that it is there.

And the third defect was in the checker, not the writer. It verified the **value** of three of
the four count lines and only the **shape** of the fourth. The code already contained the
lesson in a comment about an earlier version of the same check - *"a once-only test on two of
the four lines is a check that covers half the thing it is named after, which lends its
reputation to the half it does not cover"* - and the extension to all four lines was made for
the shape test and never for the value test.

**The transferable rules.** Key automated rewrites on something stable - a prefix, an id, a
path, a line number - never on the content you are about to replace. Make the tool's own
`--check` mode a gate step, so "no drift" is a measured result rather than an absence of
complaints. And when extending a check from some cases to all cases, check that you extended
*every* dimension, not just the one the last failure happened to expose.

The general form is lesson 21 again, one level down: a metric nobody reads still has to be
*failing when it should*, or it is decoration.

## What I would add to any of this

The lessons above share a shape, and it is worth naming: **almost every one is about the
difference between what a check reports and what it measures.** The recurring move is to find a
signal that looks like evidence, ask what it would take for it to be wrong, and discover that the
answer is "nothing" — or discover that it is wrong in a way the signal cannot express.

That is a cheap question, it applies to any reverse-engineering, porting, or long-running
refactoring work, and it is worth asking of one's own tools more often than of one's own
conclusions.
