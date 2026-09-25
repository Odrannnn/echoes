#!/usr/bin/env python3
"""Rank unmatched retail functions by simple machine-code shape.

Only objects beneath ``build/G2ME01/**/obj`` are disassembled. This tool never
looks at source-built objects beneath ``build/G2ME01/src``.
"""

import argparse
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence


ROOT = Path(__file__).resolve().parent.parent
VERSION_DIR = ROOT / "build" / "G2ME01"
OBJDUMP = ROOT / "build" / "binutils" / "powerpc-eabi-objdump"
CLASSES = (
    "return-constant",
    "return-this",
    "empty",
    "tail-call",
    "single-load",
    "single-store",
    "other",
)
CLASS_RANK = {name: idx for idx, name in enumerate(CLASSES)}

INSTRUCTION_RE = re.compile(
    r"^\s*([0-9a-fA-F]+):\s*((?:[0-9a-fA-F]{2}\s+){4})"
    r"([A-Za-z_.][A-Za-z0-9_.]*)\s*(.*?)\s*$"
)
MEMORY_BASE_RE = re.compile(r"\((r\d+)\)", re.IGNORECASE)
REGISTER_RE = re.compile(r"r(?:[3-9]|10)$|f[1-8]$", re.IGNORECASE)

LOAD_OPS = {
    "lbz", "lbzu", "lbzx", "lbzux", "lhz", "lhzu", "lhzx", "lhzux",
    "lha", "lhau", "lhax", "lhaux", "lwz", "lwzu", "lwzx", "lwzux",
    "lfs", "lfsu", "lfsx", "lfsux", "lfd", "lfdu", "lfdx", "lfdux",
}
STORE_OPS = {
    "stb", "stbu", "stbx", "stbux", "sth", "sthu", "sthx", "sthux",
    "stw", "stwu", "stwx", "stwux", "stfs", "stfsu", "stfsx", "stfsux",
    "stfd", "stfdu", "stfdx", "stfdux",
}


def report_int(value: Any) -> int:
    """Report sizes/addresses are decimal strings, unlike objdump offsets."""
    if isinstance(value, int):
        return value
    text = str(value).strip()
    return int(text, 16 if text.lower().startswith("0x") else 10)


def parse_objdump(text: str) -> List[Dict[str, Any]]:
    instructions: List[Dict[str, Any]] = []
    for line in text.splitlines():
        match = INSTRUCTION_RE.match(line)
        if not match:
            continue
        address, _bytes, mnemonic, operands = match.groups()
        instructions.append(
            {
                "address": int(address, 16),
                "mnemonic": mnemonic.lower(),
                "operands": operands.strip(),
                "text": f"{mnemonic.lower()} {operands.strip()}".rstrip(),
            }
        )
    return instructions


def comma_operands(instruction: Dict[str, Any]) -> List[str]:
    return [part.strip().lower() for part in instruction["operands"].split(",")]


def is_stack_prologue(instructions: Sequence[Dict[str, Any]]) -> bool:
    if len(instructions) < 3:
        return False
    first, second, third = instructions[:3]
    op1, op2, op3 = map(comma_operands, (first, second, third))
    return (
        first["mnemonic"] == "stwu"
        and len(op1) == 2
        and op1[0] == "r1"
        and MEMORY_BASE_RE.search(op1[1]) is not None
        and MEMORY_BASE_RE.search(op1[1]).group(1).lower() == "r1"
        and second["mnemonic"] == "mflr"
        and op2 == ["r0"]
        and third["mnemonic"] == "stw"
        and len(op3) == 2
        and op3[0] == "r0"
        and MEMORY_BASE_RE.search(op3[1]) is not None
        and MEMORY_BASE_RE.search(op3[1]).group(1).lower() == "r1"
    )


def strip_frame_patterns(
    instructions: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    body = list(instructions)
    if is_stack_prologue(body):
        body = body[3:]

    # Keep blr as the body terminator; strip the frame restore immediately
    # before it so trivial return bodies remain recognizable.
    if len(body) >= 4:
        load, restore_lr, restore_sp, ret = body[-4:]
        load_ops = comma_operands(load)
        lr_ops = comma_operands(restore_lr)
        sp_ops = comma_operands(restore_sp)
        load_base = MEMORY_BASE_RE.search(load_ops[1]) if len(load_ops) == 2 else None
        if (
            load["mnemonic"] == "lwz"
            and load_ops[0] == "r0"
            and load_base is not None
            and load_base.group(1).lower() == "r1"
            and restore_lr["mnemonic"] == "mtlr"
            and lr_ops == ["r0"]
            and restore_sp["mnemonic"] == "addi"
            and len(sp_ops) == 3
            and sp_ops[:2] == ["r1", "r1"]
            and ret["mnemonic"] == "blr"
        ):
            body = body[:-4] + [ret]
    return body


def memory_base(operand: str) -> Optional[str]:
    match = MEMORY_BASE_RE.search(operand)
    return match.group(1).lower() if match else None


def one_object_load(instruction: Dict[str, Any]) -> bool:
    operands = comma_operands(instruction)
    if instruction["mnemonic"] in LOAD_OPS and len(operands) == 2:
        return memory_base(operands[1]) == "r3"
    # Indexed PowerPC loads are written as destination, base, index.
    return (
        instruction["mnemonic"] in LOAD_OPS
        and instruction["mnemonic"].endswith(("x", "ux"))
        and len(operands) == 3
        and operands[1] == "r3"
    )


def one_object_store(instruction: Dict[str, Any]) -> bool:
    operands = comma_operands(instruction)
    if instruction["mnemonic"] in STORE_OPS and len(operands) == 2:
        return memory_base(operands[1]) == "r3"
    return (
        instruction["mnemonic"] in STORE_OPS
        and instruction["mnemonic"].endswith(("x", "ux"))
        and len(operands) == 3
        and operands[1] == "r3"
    )


def is_arg_setup(instruction: Dict[str, Any]) -> bool:
    if instruction["mnemonic"] == "nop":
        return True
    if instruction["mnemonic"].endswith("."):
        return False
    operands = comma_operands(instruction)
    return bool(operands and REGISTER_RE.fullmatch(operands[0]))


def classify(body: Sequence[Dict[str, Any]]) -> str:
    if len(body) == 1 and body[0]["mnemonic"] == "blr":
        return "empty"

    if len(body) == 2 and body[-1]["mnemonic"] == "blr":
        first = body[0]
        operands = comma_operands(first)
        if first["mnemonic"] == "li" and len(operands) == 2 and operands[0] == "r3":
            return "return-constant"
        if first["mnemonic"] == "lwz" and len(operands) == 2 and operands[0] == "r3":
            if memory_base(operands[1]) != "r3":
                return "return-constant"
        if first["mnemonic"] == "mr" and operands == ["r3", "r3"]:
            return "return-this"
        if (
            first["mnemonic"] == "addi"
            and len(operands) == 3
            and operands[:2] == ["r3", "r3"]
        ):
            return "return-this"
        if one_object_load(first):
            return "single-load"
        if one_object_store(first):
            return "single-store"

    if body and body[-1]["mnemonic"] == "b" and all(
        is_arg_setup(instruction) for instruction in body[:-1]
    ):
        return "tail-call"
    return "other"


def retail_path(raw_path: Any) -> Path:
    if not raw_path:
        raise ValueError("unit has no retail object path")
    path = Path(str(raw_path))
    if not path.is_absolute():
        path = ROOT / path
    resolved = path.resolve()
    try:
        relative = resolved.relative_to(VERSION_DIR.resolve())
    except ValueError as exc:
        raise ValueError(f"object is outside {VERSION_DIR}: {raw_path}") from exc
    if "obj" not in relative.parts or "src" in relative.parts or resolved.suffix != ".o":
        raise ValueError(f"not a retail object path: {raw_path}")
    return resolved


def disassemble(path: Path) -> List[Dict[str, Any]]:
    result = subprocess.run(
        [str(OBJDUMP), "-d", str(path)],
        check=True,
        capture_output=True,
        text=True,
        errors="replace",
    )
    return parse_objdump(result.stdout)


def unmatched_functions(report: Dict[str, Any], unit_filter: Optional[str]) -> List[Dict[str, Any]]:
    selected: List[Dict[str, Any]] = []
    for unit in report.get("units", []):
        unit_name = str(unit.get("name", ""))
        if unit_filter and unit_filter not in unit_name:
            continue
        for function in unit.get("functions", []):
            match_score = function.get("fuzzy_match_percent")
            if match_score is None or float(match_score) != 100.0:
                selected.append({"unit": unit_name, **function})
    return selected


def instruction_json(instructions: Sequence[Dict[str, Any]]) -> List[Dict[str, Any]]:
    return [
        {
            "address": f"0x{instruction['address']:x}",
            "mnemonic": instruction["mnemonic"],
            "operands": instruction["operands"],
        }
        for instruction in instructions
    ]


def analyze(
    functions: List[Dict[str, Any]], object_paths: Dict[str, Any]
) -> List[Dict[str, Any]]:
    cache: Dict[Path, List[Dict[str, Any]]] = {}
    for function in functions:
        entry: Dict[str, Any] = {
            "unit": function["unit"],
            "symbol": function.get("name", "<unknown>"),
            "size": report_int(function.get("size", 0)),
            "address": report_int(function.get("address", 0)),
            "class": "other",
            "object": None,
            "instructions": [],
            "body_instructions": [],
        }
        try:
            raw_object = object_paths.get(entry["unit"])
            path = retail_path(raw_object)
            entry["object"] = str(path.relative_to(ROOT))
            if path not in cache:
                cache[path] = disassemble(path)
            start = entry["address"]
            end = start + entry["size"]
            instructions = [
                instruction
                for instruction in cache[path]
                if start <= instruction["address"] < end
            ]
            entry["instructions"] = instruction_json(instructions)
            body = strip_frame_patterns(instructions)
            entry["body_instructions"] = instruction_json(body)
            if not instructions:
                entry["analysis_error"] = "no disassembled instructions in report address range"
            else:
                entry["class"] = classify(body)
        except (OSError, ValueError, subprocess.CalledProcessError) as exc:
            entry["analysis_error"] = str(exc)
        function.update(entry)
    return functions


def summary(functions: Sequence[Dict[str, Any]]) -> Dict[str, Dict[str, int]]:
    totals = {name: {"functions": 0, "bytes": 0} for name in CLASSES}
    for function in functions:
        bucket = totals[function["class"]]
        bucket["functions"] += 1
        bucket["bytes"] += function["size"]
    return totals


def non_negative_int(text: str) -> int:
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Find small, mechanically simple unmatched retail functions."
    )
    parser.add_argument("--unit", help="restrict to unit names containing SUBSTRING")
    parser.add_argument("--limit", type=non_negative_int, default=40, help="ranked entries to print (default: 40)")
    parser.add_argument("--json", metavar="OUT", help="write the full analysis as JSON")
    args = parser.parse_args()

    report_path = VERSION_DIR / "report.json"
    objdiff_path = ROOT / "objdiff.json"
    try:
        with report_path.open(encoding="utf-8") as stream:
            report = json.load(stream)
        with objdiff_path.open(encoding="utf-8") as stream:
            objdiff = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"error: cannot read project reports: {exc}", file=sys.stderr)
        return 2

    object_paths = {
        unit["name"]: unit.get("target_path")
        for unit in objdiff.get("units", [])
        if unit.get("name")
    }
    functions = unmatched_functions(report, args.unit)
    analyze(functions, object_paths)
    totals = summary(functions)

    print(f"Unmatched functions: {len(functions)}")
    for class_name in CLASSES:
        bucket = totals[class_name]
        print(f"{class_name:16} {bucket['functions']:5} functions  {bucket['bytes']:9} bytes")
    classified = len(functions) - totals["other"]["functions"]
    fraction = classified / len(functions) if functions else 0.0
    print(f"Classified: {classified}/{len(functions)} ({fraction:.1%}); other includes non-trivial bodies and any disassembly errors.")

    ranked = sorted(
        functions,
        key=lambda function: (
            CLASS_RANK[function["class"]],
            function["size"],
            function["unit"],
            function["symbol"],
        ),
    )
    print(f"\nTop {args.limit} cheapest unmatched functions:")
    for function in ranked[: args.limit]:
        print(
            f"{function['class']:16} {function['size']:5} B  "
            f"{function['unit']}  {function['symbol']}"
        )

    errors = Counter(function.get("analysis_error") for function in functions if function.get("analysis_error"))
    if errors:
        print(f"\nDisassembly errors: {sum(errors.values())} functions", file=sys.stderr)
        for message, count in errors.most_common():
            print(f"  {count}: {message}", file=sys.stderr)

    if args.json:
        output = {
            "total_unmatched": len(functions),
            "classified": classified,
            "classification_fraction": fraction,
            "summary": totals,
            "functions": functions,
        }
        try:
            with Path(args.json).open("w", encoding="utf-8") as stream:
                json.dump(output, stream, indent=2)
                stream.write("\n")
        except OSError as exc:
            print(f"error: cannot write JSON output: {exc}", file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
