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

## What I would add to any of this

The lessons above share a shape, and it is worth naming: **almost every one is about the
difference between what a check reports and what it measures.** The recurring move is to find a
signal that looks like evidence, ask what it would take for it to be wrong, and discover that the
answer is "nothing" — or discover that it is wrong in a way the signal cannot express.

That is a cheap question, it applies to any reverse-engineering, porting, or long-running
refactoring work, and it is worth asking of one's own tools more often than of one's own
conclusions.
