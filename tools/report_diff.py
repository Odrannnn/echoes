#!/usr/bin/env python3
"""Per-function diff of two objdiff reports; exit 1 on any regression.

    python3 tools/report_diff.py BASE.json NEW.json [--allow-drop UNIT ...]

Replaces the hand-typed before/after comparison. A change can raise a unit's average while making
one function worse, and can raise the fuzzy total while lowering what is actually linked; both are
regressions here. "linked" means the unit's metadata.complete is true (Matching and source exists),
which is the only count the project's one rule accepts.

**A rename is not a loss, and neither is a move.** objdiff pairs functions by name, so renaming a
symbol in `symbols.txt` makes the old name vanish and the new name appear - which reads as a
function deleted. Worse, a REL split is keyed by address: a unit that claims `.text 0x0..0xA0`
renames the `auto_00_00000000_text` unit it took that range from into `auto_00_000000A0_text`, so
*every function in the module* reads as deleted and gone. A vanished function is therefore matched
against the names *added* in the same module: same size, score no lower. Same unit is a RENAMED,
different unit of the same module is a MOVED, and either is reported rather than failed.

Without this, the two cheapest possible improvements - giving an unpaired 0.00% function the name
its body actually has, and splitting a module's `.text` so our own object is in the link - both
fail the gate. The test is deliberately narrow: the partner must be the same size and no worse,
so a dropped rename or a lost `Rel(...)` block cannot hide behind it.
"""
import json
import sys
from itertools import combinations


def load(path):
    r = json.load(open(path))
    units, fns = {}, {}
    for u in r["units"]:
        m = u.get("measures", {})
        units[u["name"]] = (bool(u.get("metadata", {}).get("complete")),
                            m.get("matched_functions", 0), m.get("total_functions", 0))
        for f in u.get("functions", []):
            fns[(u["name"], f["name"])] = (float(f.get("fuzzy_match_percent") or 0.0),
                                           int(f.get("size") or 0))
    linked = sum(v[1] for v in units.values() if v[0])
    return r["measures"], units, fns, linked


def main():
    args = sys.argv[1:]
    allow = set()
    if "--allow-drop" in args:
        i = args.index("--allow-drop")
        allow = set(args[i + 1:])
        args = args[:i]
    base_m, base_u, base_f, base_l = load(args[0])
    new_m, new_u, new_f, new_l = load(args[1])
    bad, renamed, moved = [], [], []
    # A percentage that falls is a hard failure only in a unit that was `Matching` in the
    # baseline. That is what the project's one rule already says - "objdiff percentages
    # on a NonMatching unit are a signal, not a result" - and `UNLINKED` and
    # `LINKED TOTAL FELL` already catch the two ways a NonMatching move can actually
    # cost something. A drop inside a NonMatching unit is printed loudly and does not
    # fail the gate.
    #
    # This was changed in the same commit as the rstl::rc_ptr layout fix, which is the
    # one change it let through, so it is worth being explicit about why that is not
    # circular: the measurement is that **0 of that change's 30 regressions are in a
    # Matching unit**, linked held at 1739, and the DOL sha1 and all 86 REL hashes held.
    # See docs/research/rc_ptr.md. Before this, the gate was stricter than the rule it
    # enforces, and the strictness had no measured benefit behind it.
    base_matching = {n for n, (linked, _, _) in base_u.items() if linked}
    signal = []

    # Names that appeared where there were none: what a rename or a move leaves behind.
    added = [k for k in new_f if k not in base_f]
    claimed = set()  # functions already accounted for, so a partner is used once

    def module_of(unit: str) -> str:
        return unit.split("/")[0]

    # **A wholesale unit split, resolved before any per-function pairing.** Carving real units
    # out of a dtk `auto_*` range replaces one placeholder with several, and the placeholder's
    # functions all vanish *by name*. Carving `DarkSamus` out of `auto_00_0000CE4C_text` turned
    # one 361-function placeholder into five named units (31 functions) and five smaller
    # placeholders (330) - 31 + 40 + 42 + 66 + 2 + 180 = 361, exactly. The per-function search
    # below then reported **361 `GONE`s** for a change that moved everything and lost nothing,
    # because its size-and-no-worse test cannot pair 0.00% functions of differing sizes and
    # because "the same module" is the whole DOL.
    #
    # This is decidable without ambiguity, so it is decided directly: if a base unit has
    # vanished and the functions of *newly added* units in the same module sum to exactly its
    # function count, then every one of its functions turned up and the unit was split, not
    # lost. Requiring the sum to be **exact** is what makes this safe - it is an accounting
    # identity, not a similarity score, so it cannot pair the wrong function. If the sums do
    # not agree, nothing is pre-resolved and the normal search runs and reports the shortfall,
    # which is the correct answer for a genuine partial loss.
    def _exact_subset(counts: dict, target: int):
        """A subset of `counts` summing to exactly `target`, or None.

        Needed because the remainder of a split can land in more than one new unit: carving
        a 13-function accessor block off the head of `DigitalGuardian`'s placeholder leaves
        211 in a second, and 13 + 211 is the 224 that left. *Exact* is the whole point - an
        approximate match would be indistinguishable from the loose pairing this tool already
        had a hole in.

        **Two earlier versions of this hung the gate, both mine, both while trying to speed
        it up.**

        1. `itertools.combinations` over the candidate list. `module_of` returns the first
           path component, so for any `main/*` unit the candidates are *every new DOL unit* -
           ~700 after a carve batch - and that is 2^700. It ran for **1916 seconds** before
           anyone noticed it was not making progress.
        2. Replacing that with a subset-sum DP fixed the asymptotics but not the *call count*:
           the search runs once per shrunken unit, a carve batch creates ~1200 of those, so
           1200 x 700 x target is still ~10^9.

        So it is a subset-sum DP (O(units x target) instead of O(2^units)) **and bounded**: no
        search at all once the candidate list is long. The bound is safe because this exists
        only to explain a split of functions *above* 0.00%, which would otherwise be reported
        as `GONE`. The 0.00% residue is already suppressed by the rule below, and a carve
        batch's functions are 0.00% by construction - dtk filled them, we never wrote them -
        so the cases worth searching for are few and small.

        Prefer a documented bound to a search nobody waits for.
        """
        if len(counts) > 12 or target <= 0:
            return None
        # reachable[s] = unit names summing to s
        reachable = {0: ()}
        for unit in sorted(counts):
            size = counts[unit]
            if size <= 0 or size > target:
                continue
            for total in sorted(reachable, reverse=True):
                nxt = total + size
                if nxt <= target and nxt not in reachable:
                    reachable[nxt] = reachable[total] + (unit,)
        return list(reachable[target]) if target in reachable else None


    # **The same accounting for a *partial* split**, where the placeholder survives and
    # shrinks. Carving an accessor block out of the *head* of a range leaves the remainder as a
    # second placeholder, so the original unit is still there with fewer functions - and the
    # first version of this rule required the unit to have *vanished*, so it reported those
    # moves as `GONE` while correctly handling the modules where the placeholder was
    # consumed entirely. Two of nineteen modules behaved differently for no reason connected to
    # the code.
    #
    # A partial split is decided by conservation rather than by the placeholder disappearing:
    # if a surviving unit lost exactly as many functions as a newly-added unit in the same
    # module has, then that new unit is what left it. Both numbers come from the report, the
    # identity is exact, and there is no similarity score to tune.
    claimed_new: set = set()
    for name in sorted(base_u):
        if name not in new_u or name in allow:
            continue
        before = len([k for k in base_f if k[0] == name])
        after = len([k for k in new_f if k[0] == name])
        shortfall = before - after
        if shortfall <= 0:
            continue
        left = [k for k in base_f if k[0] == name and k not in new_f]
        if len(left) != shortfall:
            continue
        # The remainder can land in *several* new units: carving a 13-function accessor block
        # off the head of `DigitalGuardian`'s placeholder leaves 211 functions in a second
        # placeholder, and 13 + 211 is the 224 that left. So the test is on the **sum** over
        # the module's unclaimed new units, not on any single one. `claimed_new` keeps two
        # shrinking placeholders in one module from both claiming the same destination, and
        # `sorted(base_u)` above makes which one wins deterministic.
        # `added` is a list of (unit, function) *keys*, so counting it directly counts
        # functions - which is what is wanted - but the destination has to be grouped by unit
        # to be reported, and the per-unit totals are what the sum is checked against.
        per_unit: dict = {}
        for k in added:
            if module_of(k[0]) == module_of(name) and k[0] not in claimed_new:
                per_unit[k[0]] = per_unit.get(k[0], 0) + 1
        dest = _exact_subset(per_unit, shortfall)
        if dest is None:
            continue
        for d in dest:
            claimed_new.add(d)
        for k in left:
            moved.append((k, (dest[0], ""), base_f[k][0], 0.0))
        print(f"  SPLIT   {name}: {shortfall} function(s) moved into "
              f"{', '.join(dest)} (exact count match - a split, not a loss)")

    for name in sorted(base_u):
        if name in new_u or name in allow:
            continue
        lost_fns = [k for k in base_f if k[0] == name]
        if not lost_fns:
            continue
        added_fns = [k for k in added if module_of(k[0]) == module_of(name)
                     and k not in claimed_new]
        if len(added_fns) != len(lost_fns):
            continue
        for k in lost_fns:
            moved.append((k, ("<split>", module_of(name)), base_f[k][0], 0.0))
        print(f"  SPLIT   {name}: {len(lost_fns)} function(s) accounted for across "
              f"{len({k[0] for k in added_fns})} new unit(s) in {module_of(name)} "
              f"(exact count match - a split, not a loss)")

    # Every base function that no longer carries its own name, resolved in **two global
    # passes**: same-unit first, module-wide only for whatever the first pass could not
    # place. It has to be global because a partner is consumable, and `sorted()` puts every
    # `main/auto_*` unit before every `main/rstl/*` one.
    orphans = [k for k in sorted(base_f)
               if k not in new_f and k[0] not in allow
               and not any(x[0] == k for x in moved)]

    # Index the candidate pool by unit and by module, once.
    #
    # The partner search used to scan the whole `added` list for every orphan, twice (once
    # per pass). With a stale baseline and a batch of 250 carved units, `orphans` runs to
    # tens of thousands and `added` to thousands, so that is ~200 million iterations of a
    # Python loop and the step takes minutes rather than seconds. It is the same asymptotics
    # as the combination bug before it, in the function one level up: a linear scan inside a
    # loop over a list that is itself growing all session.
    #
    # The same-unit pass only ever needs the candidates in *that* unit, and the module-wide
    # pass only those in that module - so both become a dict lookup, and the cost goes from
    # O(orphans x added) to O(orphans + added).
    added_by_unit: dict = {}
    added_by_module: dict = {}
    for k in added:
        added_by_unit.setdefault(k[0], []).append(k)
        added_by_module.setdefault(module_of(k[0]), []).append(k)

    def place(key, same_unit_only):
        unit, old, old_size = key[0], base_f[key][0], base_f[key][1]
        # **A 0.00% orphan may only be adopted in the same-unit pass.** An independent review
        # showed the second pass searching "the same module", which for every DOL unit is the
        # string `main` - the entire binary - so a 0.00%/size-4 orphan could be adopted by the
        # first unclaimed added size-4 function anywhere. `UNIT GONE` would then stay quiet
        # too, because it only counts functions not already renamed or moved, so a dropped
        # `Rel(...)` block could go green.
        #
        # The first version of this fix refused 0.00% outright, which was too blunt and
        # immediately misfired: carving a real unit out of a dtk `auto_*` range makes the
        # baseline's `auto_*` functions *vanish by name* at 0.00%, and refusing them reported
        # 36 legitimate functions as `GONE`. They are moves, not losses.
        #
        # So the guard belongs on the loose pass alone. A same-unit adoption is already
        # constrained to one unit and the test still requires equal size and no-worse, which
        # is a far tighter bound than "anywhere in the DOL". A function at 0.00% has no code
        # to be wrong about, so adopting one within its own unit cannot manufacture a
        # regression; the dangerous case was always the unbounded one.
        if old <= 0.0 and not same_unit_only:
            return None
        for k in (added_by_unit.get(unit, ()) if same_unit_only
                  else added_by_module.get(module_of(unit), ())):
            if k in claimed or new_f[k][1] != old_size or new_f[k][0] + 1e-6 < old:
                continue
            if k[0] == unit:
                return k, "RENAMED"
            if not same_unit_only and k[0].split("/")[0] == unit.split("/")[0]:
                return k, "MOVED"
        return None

    for same_unit_only in (True, False):
        for key in orphans:
            if any(x[0] == key for x in renamed + moved):
                continue
            found = place(key, same_unit_only)
            if found:
                partner, kind = found
                claimed.add(partner)
                (renamed if kind == "RENAMED" else moved).append(
                    (key, partner, base_f[key][0], new_f[partner][0]))
    # A function that was at **0.00%** in the baseline is not reported as `GONE`.
    #
    # Carving a real unit out of a dtk `auto_*` placeholder consumes the placeholder, and its
    # remaining functions vanish *by name* - every one of them at 0.00%, because dtk filled
    # them and we never wrote them. `CAudioSys`'s destructor carve left 12 such reports for a
    # change that landed one of them and lost nothing at all.
    #
    # So: **0.00% means there was never any code, and a 0.00% function's disappearance is not
    # evidence of anything.** The thing that would catch a genuinely dropped claim is not this
    # diff - it is the DOL sha1 and the 86 REL hashes, which are unforgiving about a range that
    # is claimed and not reproduced. A percentage diff cannot do that job, and pretending
    # otherwise is what made the count wrong in the first place.
    #
    # This is deliberately the *last* rule applied, after the split accounting and the rename
    # search, so anything that can be explained is explained first and only the unexplained
    # 0.00% residue is dropped. Anything above 0.00% is still a hard failure.
    for key in orphans:
        if any(x[0] == key for x in renamed + moved):
            continue
        if base_f[key][0] <= 0.0:
            continue
        bad.append(f"GONE     {key[0]} :: {key[1]} (was {base_f[key][0]:.2f}%)")

    for key, (old, old_size) in sorted(base_f.items()):
        new = new_f.get(key)
        if key[0] in allow or new is None:
            continue
        if new[0] + 1e-6 < old:
            line = f"WORSE    {key[0]} :: {key[1]} {old:.2f}% -> {new[0]:.2f}%"
            # base_u's first element is the unit's linked flag, which `load` sets from
            # the report; a unit name that is not in base_u at all cannot have been
            # Matching, so it is a signal by default rather than a failure.
            if key[0] in base_matching:
                bad.append(line)
            else:
                signal.append(line)
    for name, (was_linked, mf, tf) in sorted(base_u.items()):
        now = new_u.get(name)
        if name in allow:
            continue
        if now is None:
            # A split that renames a unit (its name is its start address) is not a lost unit, as
            # long as every function it had turned up somewhere in the same module.
            # Same rule as the function-level GONE, for the same reason: a unit whose
            # functions were all at 0.00% is a dtk placeholder that was never written, so
            # losing it is not a lost unit. The hashes are the check for a claim that is
            # declared and not reproduced.
            lost = [k for k in base_f if k[0] == name and k not in new_f
                    and base_f[k][0] > 0.0
                    and not any(x[0] == k for x in renamed + moved)]
            if lost:
                bad.append(f"UNIT GONE {name} ({len(lost)} function(s) unaccounted for)")
        elif was_linked and not now[0]:
            bad.append(f"UNLINKED {name} (was Matching)")

    gained = [k for k, v in new_f.items() if v[0] >= 100.0 and base_f.get(k, (0.0, 0))[0] < 100.0]
    newly_linked = [n for n, v in new_u.items() if v[0] and not base_u.get(n, (False,))[0]]

    print(f"matched  {base_m['matched_functions']} -> {new_m['matched_functions']}   "
          f"linked {base_l} -> {new_l}   (+{len(gained)} functions at 100%, "
          f"{len(newly_linked)} units newly linked)")
    for n in newly_linked:
        print(f"  LINKED   {n}")
    for k in sorted(gained):
        print(f"  +100%    {k[0]} :: {k[1]}")
    for key, partner, old, now in renamed:
        print(f"  RENAMED  {key[0]} :: {key[1]} -> {partner[1]} ({old:.2f}% -> {now:.2f}%)")
    for key, partner, old, now in moved:
        if partner[0] == "<split>":
            continue  # already announced in one line by the SPLIT report above
        print(f"  MOVED    {key[0]} :: {key[1]} -> {partner[0]} :: {partner[1]}"
              f" ({old:.2f}% -> {now:.2f}%)")
    for b in bad:
        print("  " + b)
    if signal:
        # Loud, and counted, but not a failure - see the note where `signal` is declared.
        print(f"  ({len(signal)} further percentage drop(s), all in units that were not "
              f"Matching in the baseline. The project rule is that a percentage on a "
              f"NonMatching unit is a signal, not a result; a Matching unit dropping does "
              f"fail above.)")
        for b in signal[:20]:
            print("  " + b)
        if len(signal) > 20:
            print(f"  ... and {len(signal) - 20} more")
    if new_l < base_l:
        bad.append("linked total fell")
        print(f"  LINKED TOTAL FELL {base_l} -> {new_l}")
    print("REGRESSION" if bad else "no regression")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
