#!/usr/bin/env python3
"""Shared pieces for the P10 wiring gates.

Each gate compares two arms over the same corpus:

  native  the previous behaviour — the inline decision code in the script, or the
          executable oracle pinned by the accepted experiment;
  goose   the same decision through build/goose-hosts/<name>, i.e. the wired
          path the product scripts now use.

Nothing here builds anything: `gates/run.sh` builds the hosts first.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

EXP = Path(__file__).resolve().parent.parent
ROOT = EXP.parent.parent
HOSTS = ROOT / "build" / "goose-hosts"
CORPUS = ROOT / "experiments" / "EXP-2026-11-15-645-p10-05-owned-infra" / "tests"


def hx(data) -> str:
    """Hex of a field; '-' is the empty string, as the host protocol expects."""
    if isinstance(data, str):
        data = data.encode()
    return data.hex() if data else "-"


def unhex(field: str) -> bytes:
    return b"" if field == "-" else bytes.fromhex(field)


class Host:
    def __init__(self, name: str):
        path = HOSTS / name
        if not path.is_file():
            raise SystemExit(f"gate: {path} is missing; run gates/run.sh")
        self.proc = subprocess.Popen([str(path)], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1)

    def ask(self, line: str) -> str:
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
        reply = self.proc.stdout.readline().rstrip("\n")
        if not reply.startswith("OK "):
            raise SystemExit(f"gate: host refused {line.split()[0]}: {reply or 'no reply'}")
        return reply[3:]

    def text(self, line: str) -> bytes:
        return unhex(self.ask(line))

    def close(self) -> None:
        self.proc.stdin.close()
        self.proc.wait(timeout=30)


def cases(path: Path) -> list[list[str]]:
    """Non-comment rows, tab split."""
    rows = []
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        rows.append(line.split("\t"))
    return rows


def field(row: list[str], i: int) -> str:
    return row[i] if i < len(row) else ""


def decode(text: str) -> str:
    return text.encode().decode("unicode_escape") if "\\" in text else text


class Tally:
    def __init__(self, name: str):
        self.name = name
        self.values = 0
        self.fails = 0
        self.rows = 0

    def eq(self, row: int, kind: str, want, got) -> None:
        self.values += 1
        if want != got:
            self.fails += 1
            print(f"FAIL row {row} {kind}: want={want!r} got={got!r}")

    def done(self) -> int:
        print(f"{self.name} rows={self.rows} values={self.values} failures={self.fails}")
        return 0 if self.fails == 0 else 1


def oracle(py: Path, *args: str) -> dict[int, str]:
    """Run an accepted oracle and return {row: value} for '<row>\\t<value>' lines."""
    p = subprocess.run([sys.executable, str(py), *args], capture_output=True, text=True,
                       check=False)
    if p.returncode != 0:
        raise SystemExit(f"gate: oracle {py.name} failed:\n{p.stderr[-800:]}")
    out = {}
    for line in p.stdout.splitlines():
        if "\t" not in line:
            continue
        row, value = line.split("\t", 1)
        out[int(row)] = value
    return out
