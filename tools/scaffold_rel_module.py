#!/usr/bin/env python3
"""Generate the initial decompilation scaffolding for a G2ME01 REL module."""

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


SECTION_LAYOUT = {
    1: (".text", "type:code align:4"),
    2: (".ctors", "type:rodata align:4"),
    3: (".dtors", "type:rodata align:4"),
    4: (".rodata", "type:rodata align:8"),
    5: (".data", "type:data align:8"),
    6: (".bss", "type:bss align:8"),
}
SECTION_IDS = {name: section_id for section_id, (name, _) in SECTION_LAYOUT.items()}
REL_SETUP_NAMES = {
    "_unresolved",
    "_epilog",
    "_prolog",
    "ModuleConstructors",
    "ModuleDestructors",
}
SYMBOL_RE = re.compile(
    r"^(.*?) = (\.[A-Za-z0-9_]+):0x([0-9A-Fa-f]+); // type:([A-Za-z0-9_]+)(.*)$"
)
SIZE_RE = re.compile(r"(?:^|\s)size:0x([0-9A-Fa-f]+)(?:\s|$)")
MODULE_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
CLASS_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")


@dataclass
class Section:
    section_id: int
    name: str
    size: int


@dataclass
class Function:
    name: str
    section: str
    address: int
    size: int


def read_sections(rel_path: Path) -> Tuple[Dict[str, Section], int, int, int]:
    data = rel_path.read_bytes()
    if len(data) < 0x4C:
        raise ValueError(f"{rel_path}: REL is shorter than its 0x4C-byte header")

    num_sections, section_table = struct.unpack_from(">II", data, 0x0C)
    version = struct.unpack_from(">I", data, 0x1C)[0]
    if version != 3:
        raise ValueError(f"{rel_path}: expected a version-3 REL, found version {version}")
    if section_table < 0x4C or section_table + num_sections * 8 > len(data):
        raise ValueError(f"{rel_path}: section table is outside the REL file")

    sections: Dict[str, Section] = {}
    for section_id in range(1, num_sections):
        offset_word, size = struct.unpack_from(">II", data, section_table + section_id * 8)
        if size == 0:
            continue
        if section_id not in SECTION_LAYOUT:
            raise ValueError(
                f"{rel_path}: non-empty section {section_id} has no known G2ME01 section name"
            )
        name, _ = SECTION_LAYOUT[section_id]
        file_offset = offset_word & ~1
        if name != ".bss" and (file_offset == 0 or file_offset + size > len(data)):
            raise ValueError(f"{rel_path}: {name} has invalid file bounds")
        sections[name] = Section(section_id, name, size)

    # The REL header stores the unresolved-handler section index and offset.
    unresolved_section = data[0x32]
    unresolved_address = struct.unpack_from(">I", data, 0x3C)[0]
    return sections, unresolved_section, unresolved_address, num_sections


def read_functions(symbols_path: Path, sections: Dict[str, Section]) -> List[Function]:
    functions = []
    for line_number, line in enumerate(symbols_path.read_text().splitlines(), 1):
        match = SYMBOL_RE.match(line)
        if not match or match.group(4) != "function":
            continue
        name, section, address, _kind, metadata = match.groups()
        size_match = SIZE_RE.search(metadata)
        if size_match is None:
            raise ValueError(f"{symbols_path}:{line_number}: function has no size")
        if section not in sections:
            raise ValueError(
                f"{symbols_path}:{line_number}: function {name!r} refers to absent section {section}"
            )
        function = Function(name, section, int(address, 16), int(size_match.group(1), 16))
        if function.address + function.size > sections[section].size:
            raise ValueError(
                f"{symbols_path}:{line_number}: function {name!r} exceeds {section} bounds"
            )
        functions.append(function)
    return functions


def rodata_split_point(symbols_path: Path, sections: Dict[str, Section]) -> Optional[int]:
    rodata = sections.get(".rodata")
    if rodata is None:
        return None
    for line_number, line in enumerate(symbols_path.read_text().splitlines(), 1):
        match = SYMBOL_RE.match(line)
        if not match:
            continue
        name, section, address, _kind, _metadata = match.groups()
        if section == ".rodata" and name in ("@stringBase0", "stringBase0"):
            point = int(address, 16)
            if point > rodata.size:
                raise ValueError(
                    f"{symbols_path}:{line_number}: string base exceeds .rodata bounds"
                )
            return point

    # In some REL symbol maps the same setup literal pool is an anonymous object
    # rather than @stringBase0 (SkyRipple uses a trailing 0x84-byte object).
    for line_number, line in enumerate(symbols_path.read_text().splitlines(), 1):
        match = SYMBOL_RE.match(line)
        if not match:
            continue
        _name, section, address, kind, metadata = match.groups()
        size_match = SIZE_RE.search(metadata)
        if section != ".rodata" or kind != "object" or size_match is None:
            continue
        point = int(address, 16)
        size = int(size_match.group(1), 16)
        if size == 0x84 and point + size == rodata.size:
            return point
    return None


def split_artifact(
    module: str,
    class_name: str,
    sections: Dict[str, Section],
    points: Dict[str, int],
    string_point: Optional[int],
) -> Tuple[str, List[str]]:
    ranges: Dict[str, Tuple[int, int]] = {}
    notes = []

    for section_name, section in sections.items():
        if section_name in (".ctors", ".dtors"):
            notes.append(f"{section_name} is left to dtk (as in the ForgottenObject split).")
            continue

        point = points.get(section_name, section.size)
        if section_name == ".rodata" and string_point is not None:
            point = string_point
        if point == 0:
            ranges[section_name] = (0, 0)
            notes.append(f"{section_name} is wholly owned by REL/REL_Setup.cpp.")
        elif point == section.size:
            ranges[section_name] = (section.size, section.size)
        else:
            ranges[section_name] = (point, section.size)

    sections_lines = ["Sections:"]
    for section_id, (name, attributes) in SECTION_LAYOUT.items():
        if name in sections:
            sections_lines.append(f"\t{name:<12}{attributes}")
        else:
            notes.append(f"{name} is empty (no split range emitted).")

    class_lines = []
    setup_lines = []
    for section_id, (name, _attributes) in SECTION_LAYOUT.items():
        if name not in sections or name in (".ctors", ".dtors"):
            # .ctors/.dtors are left to dtk, as the working ForgottenObject split
            # does; assigning them makes dtk reject the split.
            continue
        start, _end = ranges[name]
        end = sections[name].size
        if start > 0:
            class_lines.append(f"\t{name:<12}start:0x{0:08X} end:0x{start:08X}")
        if start < end:
            setup_lines.append(f"\t{name:<12}start:0x{start:08X} end:0x{end:08X}")

    output = sections_lines[:]
    if class_lines:
        output.extend(["", f"MetroidPrime/ScriptObjects/{class_name}.cpp:", *class_lines])
    if setup_lines:
        output.extend(["", "REL/REL_Setup.cpp:", *setup_lines])
    return "\n".join(output) + "\n", notes


def configure_snippet(module: str, class_name: str) -> str:
    return (
        "    Rel(\n"
        f'        "{module}",\n'
        "        [\n"
        f'            Object(NonMatching, "MetroidPrime/ScriptObjects/{class_name}.cpp"),\n'
        "        ],\n"
        "    ),\n"
    )


def source_skeleton(
    functions: List[Function], class_name: str, text_split: Optional[int]
) -> str:
    module_functions = []
    setup_functions = []
    for function in functions:
        is_setup = function.name in REL_SETUP_NAMES or (
            function.section == ".text"
            and text_split is not None
            and function.address >= text_split
        )
        (setup_functions if is_setup else module_functions).append(function)

    key = lambda function: (function.address, function.section)
    lines = [f"// Functions for {class_name}."]
    module_functions.sort(key=key)
    setup_functions.sort(key=key)
    if module_functions:
        lines.extend(
            f"// 0x{function.address:08X}  {function.name}  size 0x{function.size:X}"
            for function in module_functions
        )
    else:
        lines.append("// No module-owned functions.")
    lines.extend(["", "// REL/REL_Setup.cpp functions."])
    if setup_functions:
        lines.extend(
            f"// 0x{function.address:08X}  {function.name}  size 0x{function.size:X}"
            for function in setup_functions
        )
    else:
        lines.append("// No REL/REL_Setup functions found in symbols.txt.")
    return "\n".join(lines) + "\n"


def add_configure_call(config_path: Path, module: str, snippet: str) -> str:
    text = config_path.read_text()
    existing = re.search(rf'\bRel\(\s*"{re.escape(module)}"\s*,', text)
    if existing:
        raise ValueError(f"configure.py already has a Rel entry for {module}")
    if "# Begin RELs" not in text:
        raise ValueError("could not find the '# Begin RELs' list in configure.py")
    marker = "\n]\n\n\n# Optional callback to adjust link order"
    insert_at = text.rfind(marker)
    if insert_at < 0:
        raise ValueError("could not find the end of the REL list in configure.py")
    return text[:insert_at] + "\n" + snippet.rstrip("\n") + text[insert_at:]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Print or write initial scaffolding for a G2ME01 REL module."
    )
    parser.add_argument("module", help="module name under orig/G2ME01/files/RelProd")
    parser.add_argument("class_name", nargs="?", help="class name (default: CScript<ModuleName>)")
    parser.add_argument("--write", action="store_true", help="write the three generated artifacts")
    args = parser.parse_args()

    if not MODULE_RE.fullmatch(args.module):
        parser.error("module name must be a plain identifier")
    class_name = args.class_name or f"CScript{args.module}"
    if not CLASS_RE.fullmatch(class_name):
        parser.error("class name must be a plain identifier")

    root = Path(__file__).resolve().parents[1]
    rel_path = root / "orig/G2ME01/files/RelProd" / f"{args.module}.rel"
    symbols_path = root / "config/G2ME01/rels" / args.module / "symbols.txt"
    config_path = root / "configure.py"
    if not rel_path.is_file():
        parser.error(f"REL file not found: {rel_path}")
    if not symbols_path.is_file():
        parser.error(f"symbols file not found: {symbols_path}")

    try:
        sections, unresolved_section, unresolved_address, _num_sections = read_sections(rel_path)
        functions = read_functions(symbols_path, sections)
        points = {}
        if ".text" in sections:
            setup_starts = [
                function.address
                for function in functions
                if function.section == ".text" and function.name in REL_SETUP_NAMES
            ]
            if setup_starts:
                points[".text"] = min(setup_starts)
            elif unresolved_section == SECTION_IDS[".text"]:
                points[".text"] = unresolved_address
            else:
                points[".text"] = sections[".text"].size
            if not 0 <= points[".text"] <= sections[".text"].size:
                raise ValueError("REL/REL_Setup text split exceeds .text size")

        string_point = rodata_split_point(symbols_path, sections)
        splits, notes = split_artifact(
            args.module, class_name, sections, points, string_point
        )
        snippet = configure_snippet(args.module, class_name)
        source = source_skeleton(functions, class_name, points.get(".text"))
    except (OSError, ValueError, struct.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    splits_path = root / "config/G2ME01/rels" / args.module / "splits.txt"
    source_path = root / "src/MetroidPrime/ScriptObjects" / f"{class_name}.cpp"
    if args.write:
        try:
            updated_configure = add_configure_call(config_path, args.module, snippet)
            splits_path.parent.mkdir(parents=True, exist_ok=True)
            source_path.parent.mkdir(parents=True, exist_ok=True)
            splits_path.write_text(splits)
            source_path.write_text(source)
            config_path.write_text(updated_configure)
        except (OSError, ValueError) as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        print(f"Wrote {splits_path.relative_to(root)}")
        print("Added Rel entry to configure.py")
        print(f"Wrote {source_path.relative_to(root)}")
    else:
        print(f"--- {splits_path.relative_to(root)} ---")
        print(splits, end="")
        print("--- configure.py snippet ---")
        print(snippet, end="")
        print(f"--- {source_path.relative_to(root)} ---")
        print(source, end="")

    if notes:
        print("Notes:")
        for note in notes:
            print(f"  - {note}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
