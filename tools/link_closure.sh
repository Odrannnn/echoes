#!/bin/bash
# The port's link closure: the complete set of symbols the link is missing, in ONE pass, with no
# link.
#
#   tools/link_closure.sh [--report] [--json OUT] [--verify-ld LOG] [--selftest]
#
# Why this exists. `tools/link_check.sh` measures the port's undefined count by *running the
# linker* and scraping its diagnostics. That is the ground truth and it is right, but it is a
# ~100 s build plus a link every time, and it only ever describes the tree as it is *now*.
#
# Landing `CCubeRenderer`'s key function emitted a vtable whose 83 relocations all have to
# resolve, and closing them was done by reading the linker's output, fixing a wave, relinking,
# and repeating: six waves, six full port builds. The set was knowable in one pass the whole
# time, because it is a pure function of the object files. This reports it without linking.
#
#   $ tools/link_closure.sh
#   link_closure: 322 undefined symbol(s) the link is missing - one pass, no link
#   link_closure:   2 from vtable relocations, 320 from ordinary references
#   link_closure:   4 data, 318 function
#   link_closure:   vtable census: 472 vtable/ABI-table section(s) in the link reference 1230
#                   distinct symbol(s), of which 2 are unresolved
#   link_closure:   measured in 11.69s over 655 link-line object(s)
#
# `--verify-ld` is the part that makes the number trustworthy, and it is the part to run whenever
# this tool is doubted: it diffs this tool's set against a real `ld` log for the same tree and
# the diff must be empty. A tool that reports a *subset* is worse than useless here, because a
# subset looks like an answer.
#
# ## `ld` is not hiding anything, and that was measured rather than assumed
#
# The six waves were not the linker truncating its report. `ld.bfd` printed 2000 of 2000
# undefined references from a single link of a 2000-symbol object, and the port's own link prints
# 507 reference lines / 322 unique symbols and exits through `collect2: error: ld returned 1 exit
# status` - a complete run. The waves came from *fixing* symbols and relinking. `ld.bfd` also
# reports vtable relocations, in a form that names the section:
#
#   ld.bfd: vt.o:(.data.rel.ro._ZTV7Derived[_ZTV7Derived]+0x28): undefined reference to `Derived::B()'
#
# The second trap is also false: `nm -u` on a relocatable `.o` **does** print a vtable's
# unresolved slots, because they are `STB_GLOBAL`/`SHN_UNDEF` entries in the object's symbol
# table, not relocations alone. What `nm` cannot tell you is *which section* the reference came
# from, so it cannot separate a vtable slot from a call - which is the only reason this tool
# reads relocation sections, and it is a reason for the *attribution*, not the count. Both
# claims are re-measured by `--selftest` every run, from a compiled probe, so they cannot rot.
#
# Grouping is what makes the number usable, and it is where `ld`'s COMDAT rule earns its keep:
# discarding a duplicate group's relocations changes the *set* by nothing (two copies of a group
# have the same relocations, so a discarded copy's references are the kept copy's references) but
# changes the *attribution* for 2 of the 322, which without it are credited to 4 requiring
# objects instead of 2. "322 undefined" is not actionable; "these 2 come out of `CActor.cpp.o`'s
# and `CPhysicsActor.cpp.o`'s vtables" is.
#
# ## The cost, and what this replaces
#
# 11.7 s against a ~100 s port build, and it needs no compiler, no cmake and no ninja - only the
# configured build tree. It reads `build.ninja` for the link line and the ELF tables directly.
#
# It is NOT wired into `tools/gate.sh`, and the argument is in the report rather than in a
# comment: a gate step on it would have to run after the port build, would duplicate
# `link_check.sh`'s measurement of the same quantity by a different method, and the *one* thing
# that makes it worth having - `--verify-ld`, a diff against a real `ld` - needs a real `ld`,
# which is the thing it exists to avoid. It is a measurement instrument for whoever is closing
# symbols, not a gate.
#
# Exit codes: 0 measured, 1 `--verify-ld` found a difference, 2 no usable build tree,
# 3 the build tree is stale (refused rather than believed), 4 a link input could not be read.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

# `MP_TOOLCHAIN` is what `tools/link_check.sh` reads, and it is the variable a caller will
# already have set. The compiler is used for exactly two things: `-###` to learn which libc,
# libstdc++ and crt objects *this* driver adds, and `-print-file-name` to resolve `-lfoo`. Both
# are optional - with no compiler the tool says so and excludes them, rather than reporting
# every libc symbol as missing and calling it a finding.
export MP_TOOLCHAIN="${MP_TOOLCHAIN:-$REPO_ROOT/../MetroidPrimePort/build/review-tools}"
export PATH="$MP_TOOLCHAIN/bin:$PATH"

exec python3 "$REPO_ROOT/tools/link_closure.py" "$@"
