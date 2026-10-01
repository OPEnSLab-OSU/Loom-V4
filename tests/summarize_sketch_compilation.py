"""Combine completed sketch results, checking that their inputs still match the worktree.

Reports are supplied oldest first. A later result replaces an earlier result, including
a later failure. Never resurrect an old success after a failed retest. This only reads
compiler evidence and writes a new summary; it does not run compilers or upload firmware.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summarize(requested, reports, output):
    sketches = [str(Path(line.strip()).resolve()) for line in requested.read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    if not sketches or len(sketches) != len(set(sketches)):
        raise ValueError("Requested sketch directories must be nonempty and unique")
    latest = {}
    for report in reports:
        # Shared source must match even for an interrupted run's completed rows.
        for source in json.loads((report / "source_hashes.json").read_text(encoding="utf-8-sig")):
            if digest(Path(source["path"])) != source["sha256"].lower():
                raise ValueError(f"Shared source changed after build: {source['path']}")
        inputs = json.loads((report / "sketch_hashes.json").read_text(encoding="utf-8-sig"))
        configuration = json.loads((report / "configuration.json").read_text(encoding="utf-8-sig"))
        with (report / "compile_report.csv").open(encoding="utf-8-sig", newline="") as stream:
            for row in csv.DictReader(stream):
                latest[row["sketch"]] = (row, inputs, configuration, report)
    rows = []
    for sketch in sketches:
        if sketch not in latest:
            raise ValueError(f"No completed result for {sketch}")
        row, inputs, configuration, report = latest[sketch]
        if row["exit_code"] != "0":
            raise ValueError(f"Latest compilation failed: {sketch}")
        sketch_inputs = [entry for entry in inputs if entry["sketch"] == sketch]
        if not sketch_inputs:
            raise ValueError(f"Input hashes missing: {sketch}")
        for entry in sketch_inputs:
            if digest(Path(entry["path"])) != entry["sha256"].lower():
                raise ValueError(f"Sketch input changed after build: {entry['path']}")
        prefix = f"{int(row['index']):03d}_" + re.sub(r"[^a-zA-Z0-9_.-]", "_", Path(sketch).name)
        binary = report / "firmware" / (prefix + ".bin")
        elf = report / "firmware" / (prefix + ".elf")
        if not binary.is_file() or not elf.is_file():
            raise ValueError(f"Firmware evidence missing: {sketch}")
        rows.append({"sketch": sketch, "result": row["result"], "loom_warnings": int(row["loom_warnings"]),
                     "report": str(report.resolve()), "log": row["log"], "board": configuration["board"],
                     "cpp_flags": configuration["cpp_flags"], "archive": bool(configuration["precompiled_library"]),
                     "bin_sha256": digest(binary), "elf_sha256": digest(elf)})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"requested": str(requested.resolve()), "count": len(rows),
                                  "all_passed": True, "rows": rows}, indent=2), encoding="utf-8")
    print(f"Accepted {len(rows)} current-input sketch builds; {sum(row['loom_warnings'] for row in rows)} Loom warning lines. Summary: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--requested", required=True, type=Path)
    parser.add_argument("--reports", required=True, nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args()
    summarize(arguments.requested, arguments.reports, arguments.output)
