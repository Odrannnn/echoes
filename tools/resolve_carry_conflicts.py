#!/usr/bin/env python3
"""tools/resolve_carry_conflicts.py - resolve a failed `git apply --3way` in the four files every
carve touches.

    python3 tools/resolve_carry_conflicts.py      (run in the worktree the apply left conflicted)

The goal loop's rebase (tools/run_goal.sh rebase_onto_tip) carries a judged change onto a tip
another lane moved. Measured 2026-10-02: of 316 judged passes 97 were released there for a whole
new agent run, 46 conflicting outside docs only in src/MetroidPrime/PortLinkStubs.cpp, about 65
involving it, and nearly all the rest only in configure.py, files.cmake and
config/G2ME01/splits.txt. None of those is a disagreement about code: two lanes each added a line
at the same place, or each rewrote the counted prose at the top of the stub file.

What is resolved, from the index's three stages (1 base, 2 the tip's, 3 the carried change's):

  configure.py, files.cmake, config/G2ME01/splits.txt
      A diff3 merge. A hunk whose base is empty - both sides only added there - keeps both, the
      side whose first `Carve<address>` is lower first, since those lists run by address. A hunk
      both sides rewrote is not resolved.

  src/MetroidPrime/PortLinkStubs.cpp
      The header comment (through the first ` */`) is the tip's: its counts are prose every carve
      rewrites, and nothing reads them. The body is merged as above, after renaming the carried
      side's new `stub_N` / `stub_data_N` where the tip took the same name for another symbol -
      both lanes number from the same base, so both pick the next free one - and after dropping
      a carried stub whose symbol the tip already stubs (sibling carves share callees; its
      comment stays, with a line naming the tip's stub). A symbol or a name defined twice
      afterwards is not resolved.

This decides nothing about whether the result is right: the caller re-judges it, so two carves
claiming overlapping ranges, or a link the merged stubs no longer close, fail there as before.

Prints one line per resolved path (with the renames, for the commit message) and stages it.
Exit 0: every conflict in these four paths is resolved; what is left conflicted, if anything, is
docs/*.md (tools/union_docs_conflicts.sh). Exit 1: a conflict here could not be resolved,
something outside these paths and docs/*.md conflicted, or nothing conflicted at all (the apply
failed outright) - the tree is left as it was.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

STUBS = "src/MetroidPrime/PortLinkStubs.cpp"
ADDITIVE = ("configure.py", "files.cmake", "config/G2ME01/splits.txt")
DEF = re.compile(r'^extern "C" (?:void|char) (stub_\w+?)(?:\(\)|\[\d+\]) asm\("([^"]+)"\)', re.M)
NUMBERED = re.compile(r"\b(stub_(?:data_)?)(\d+)\b")
CARVE = re.compile(r"Carve([0-9A-Fa-f]{8})")


class Unresolved(Exception):
    pass


def git(*args):
    return subprocess.run(("git",) + args, capture_output=True)


def stage(n, path):
    r = git("show", ":%d:%s" % (n, path))
    return r.stdout.decode(errors="surrogateescape") if r.returncode == 0 else ""


def merge(path, ours, base, theirs):
    """diff3-merge three texts; a both-added hunk keeps both sides, any other conflict raises."""
    with tempfile.TemporaryDirectory() as tmp:
        names = []
        for name, text in (("ours", ours), ("base", base), ("theirs", theirs)):
            p = pathlib.Path(tmp, name)
            p.write_text(text, errors="surrogateescape", newline="")
            names.append(str(p))
        r = git("merge-file", "-p", "--diff3", *names)
    if r.returncode < 0 or r.returncode >= 128:
        raise Unresolved("%s: git merge-file failed" % path)
    out, state, o, b, t = [], None, [], [], []
    for line in r.stdout.decode(errors="surrogateescape").splitlines(keepends=True):
        if state is None:
            if line.startswith("<<<<<<< "):
                state, o, b, t = "ours", [], [], []
            else:
                out.append(line)
        elif state == "ours" and line.startswith("||||||| "):
            state = "base"
        elif state in ("ours", "base") and line.rstrip("\r\n") == "=======":
            state = "theirs"
        elif state == "theirs" and line.startswith(">>>>>>> "):
            if b:
                raise Unresolved("%s: both sides rewrote the same lines (%r...)" % (path, b[0].strip()[:60]))
            ko, kt = CARVE.search("".join(o)), CARVE.search("".join(t))
            if ko and kt and int(kt.group(1), 16) < int(ko.group(1), 16):
                o, t = t, o
            # Both appended at the end of the file: the first side's last line may lack its newline.
            if o and not o[-1].endswith("\n"):
                o[-1] += "\n"
            out += o + t
            state = None
        else:
            {"ours": o, "base": b, "theirs": t}[state].append(line)
    if state is not None:
        raise Unresolved("%s: unterminated conflict hunk" % path)
    return "".join(out)


def split_header(path, text):
    m = re.search(r"^ \*/\n", text, re.M)
    if not m:
        raise Unresolved("%s: no header comment to split at" % path)
    return text[:m.end()], text[m.end():]


def defs(path, body):
    d = {}
    for ident, sym in DEF.findall(body):
        if ident in d:
            raise Unresolved("%s: %s is defined twice" % (path, ident))
        d[ident] = sym
    if len(set(d.values())) != len(d):
        raise Unresolved("%s: a symbol is stubbed twice" % path)
    return d


def resolve_stubs(path, ours, base, theirs):
    head, obody = split_header(path, ours)
    bbody, tbody = split_header(path, base)[1], split_header(path, theirs)[1]
    od, bd, td = defs(path, obody), defs(path, bbody), defs(path, tbody)
    top = {}
    for prefix, n in NUMBERED.findall(ours + base + theirs):   # retired numbers are not reused either
        top[prefix] = max(top.get(prefix, -1), int(n))
    renames = []
    by_sym = {sym: ident for ident, sym in od.items()}
    for ident, sym in td.items():
        if ident in bd:
            continue
        if sym in by_sym:
            # Sibling carves share callees: the tip already stubs this symbol, and a second
            # definition is a duplicate the host link refuses. The carried block keeps its comment.
            note = "// Defined once, as `%s`: the tip already stubbed `%s` when this block was carried onto it.\n" % (by_sym[sym], sym)
            line = r'^extern "C" (?:void|char) %s\b.*\n' % re.escape(ident)
            tbody, n = re.subn(line, lambda _: note, tbody, count=1, flags=re.M)
            tbody = re.sub(line, "", tbody, flags=re.M)
            if not n:
                raise Unresolved("%s: cannot drop the duplicate stub %s" % (path, ident))
            renames.append("%s dropped: %s is %s" % (ident, sym, by_sym[sym]))
            continue
        if ident not in od:
            continue
        m = NUMBERED.fullmatch(ident)
        if not m:
            raise Unresolved("%s: both sides added %s, for different symbols" % (path, ident))
        top[m.group(1)] += 1
        new = "%s%d" % (m.group(1), top[m.group(1)])
        tbody = re.sub(r"\b%s\b" % re.escape(ident), new, tbody)
        renames.append("%s -> %s" % (ident, new))
    merged = merge(path, obody, bbody, tbody)
    md = defs(path, merged)
    osyms, bsyms, tsyms = set(od.values()), set(bd.values()), set(td.values())
    # A stub may only go missing because one side retired it: it was in the base and one side lacks it.
    lost = [s for s in sorted(osyms | tsyms)
            if s not in md.values() and not (s in bsyms and (s not in osyms or s not in tsyms))]
    if lost:
        raise Unresolved("%s: the merge dropped the stub for %s" % (path, lost[0]))
    return head + merged, renames


def main():
    conflicted = git("diff", "--name-only", "--diff-filter=U").stdout.decode().split("\n")
    conflicted = [p for p in conflicted if p]
    if not conflicted:   # the apply failed outright: nothing was carried, so there is nothing to judge
        print("resolve_carry_conflicts: no conflicted paths", file=sys.stderr)
        return 1
    other = [p for p in conflicted if p != STUBS and p not in ADDITIVE and not re.fullmatch(r"docs/.*\.md", p)]
    if other:
        print("resolve_carry_conflicts: conflicts outside the resolvable paths: " + " ".join(other), file=sys.stderr)
        return 1
    done = []
    try:
        for path in conflicted:
            if path != STUBS and path not in ADDITIVE:
                continue
            base, ours, theirs = (stage(n, path) for n in (1, 2, 3))
            if not base or not ours or not theirs:
                raise Unresolved("%s: added or deleted on one side" % path)
            if path == STUBS:
                text, renames = resolve_stubs(path, ours, base, theirs)
            else:
                text, renames = merge(path, ours, base, theirs), []
            done.append((path, text, renames))
    except Unresolved as e:
        print("resolve_carry_conflicts: " + str(e), file=sys.stderr)
        return 1
    for path, text, renames in done:
        pathlib.Path(path).write_text(text, errors="surrogateescape", newline="")
        if git("add", "--", path).returncode != 0:
            print("resolve_carry_conflicts: cannot stage " + path, file=sys.stderr)
            return 1
        print(path + (" (%s)" % ", ".join(renames) if renames else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
