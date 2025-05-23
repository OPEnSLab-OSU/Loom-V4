import argparse
import csv
import json
from collections import OrderedDict
from dataclasses import dataclass, field, asdict
from itertools import tee
from pprint import pprint


def peek(iter):
    [forked] = tee(iter, 1)
    return next(forked)


@dataclass
class StackHeader:
    filename: str
    funcname: str
    linenum: int
    startMemUsage: int
    startTime: int


@dataclass
class StackFooter:
    stopMemUsage: int | None = None
    stopTime: int | None = None


@dataclass
class StackFrame:
    header: StackHeader | None = None
    footer: StackFooter | None = None
    children: list[StackFrame] = field(default_factory=list)

    def asdict(self):
        data = OrderedDict()

        if self.header is None:
            data["header"] = "missing! (summaries are probably corrupted)"

        if self.header is not None:
            data["name"] = (
                f"{self.header.filename}:{self.header.funcname}:{self.header.linenum}"
            )
            data["free memory at call"] = self.header.startMemUsage

        if self.footer is not None:
            data["free memory at return"] = self.footer.stopMemUsage

        if self.header is not None:
            data["call time"] = self.header.startTime

        if self.footer is not None:
            data["return time"] = self.footer.stopTime

        if self.footer is None:
            data["footer"] = "missing!"

        if self.header is not None and self.footer is not None:
            memUsage = self.footer.stopMemUsage - self.header.startMemUsage
            data["time delta (ms)"] = self.footer.stopTime - self.header.startTime
            data["memory leaked (bytes)"] = -memUsage
            data["mem %"] = float(memUsage) / 32000 * 100

        children = [c.asdict() for c in self.children]
        if len(children) != 0:
            data["children"] = children

        return data


def peek_row(rows):
    row = [s.strip() for s in peek(rows)]
    [kind, depth, file, func, line, mem, time] = row

    if line == "":
        line = "-1"

    return [kind, int(depth), file, func, int(line), int(mem), int(time)]


def parse_stack_frames(rows, expected_depth=0):
    frames = []

    try:
        while True:
            [kind, depth, file, func, line, mem, time] = peek_row(rows)

            if kind == "end":
                break

            if depth > expected_depth:
                header = None
            else:
                next(rows)
                header = StackHeader(file, func, line, mem, time)

            frame = StackFrame(header=header)
            frames.append(frame)

            frame.children = parse_stack_frames(rows, expected_depth=expected_depth + 1)

            [kind, depth, _, _, _, mem, time] = peek_row(rows)

            if depth < expected_depth:
                break

            next(rows)
            frame.footer = StackFooter(mem, time)

    except StopIteration:
        pass

    return frames


parser = argparse.ArgumentParser(
    prog="decode_func_summaries",
    description="Translates the output of function summaries into human-readable JSON",
)

parser.add_argument("filename")

args = parser.parse_args()

with open(
    args.filename,
    "r",
    newline="",
) as f:
    reader = csv.reader(f, delimiter=",")
    [rows] = tee(reader, 1)  # makes peekable
    frames = parse_stack_frames(rows)
    frames = [f.asdict() for f in frames]
    print(json.dumps(frames, indent=2))
