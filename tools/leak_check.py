#!/usr/bin/env python3
"""Fail if the files about to be published, or the commit messages, carry machine-local or private data.

Scans every tracked and untracked-but-not-ignored file (working tree and index copies), the
messages of every commit, and optionally a pending message. Binary files are scanned through their
printable ASCII / UTF-16 strings. Extra project-specific patterns can be kept outside the repo and
passed with --extra (one regex per line, '#' comments).

    python tools/leak_check.py [--extra FILE] [--message TEXT]
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

TEXT_RULES = {
    "drive path": r"(?<![A-Za-z])[A-Za-z]:[\\/]",
    "msys drive path": r"(?:^|[\s\"'(=])/[a-zA-Z]/\w",
    "user profile dir": r"[\\/](?:users|home|documents and settings)[\\/]",
    "appdata": r"\bappdata\b",
    "game/launcher install": r"steamapps|steam games|steamlibrary|modorganizer|\bmo2\b|program files",
    "private network address": r"\b(?:10\.\d{1,3}|192\.168|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}\b",
    "email": r"[\w.+-]+@[\w-]+\.[\w.-]*[a-z]",
    "secret": r"ghp_|gho_|github_pat_|\bsk-[A-Za-z0-9]{20}|AKIA[0-9A-Z]{16}|xox[abprs]-|BEGIN [A-Z ]*PRIVATE KEY"
              r"|(?:api[_-]?key|secret|passw(?:or)?d|token)\s*[:=]\s*[\"'][^\"'\s]{8,}",
    "dotenv": r"(?<![\w-])\.env\b",
    "named todo": r"\b(?:TODO|FIXME|XXX)\([^)]+\)",
    "scratch/worktree": r"scratchpad|\.worktrees?\b",
}
# Binary strings are noisy (glyph names, random bytes): only full path shapes count there.
BINARY_RULES = {
    "drive path": r"(?<![A-Za-z])[A-Za-z]:[\\/][\w .-]{3,}",
    "user profile dir": r"[\\/](?:users|home)[\\/][\w .-]{2,}",
    "build path": r"\.pdb\b|[\\/]build[\\/]|[\\/]\.xmake[\\/]",
    "email": TEXT_RULES["email"],
    "secret": TEXT_RULES["secret"],
}
ALLOWED_EMAILS = re.compile(r"^noreply@(?:anthropic|github)\.com$|@users\.noreply\.github\.com$", re.I)
# The rule definitions above would flag themselves.
SELF = "tools/leak_check.py"
BUILD_OUTPUT = {".pdb", ".dll", ".exe", ".lib", ".exp", ".ilk", ".obj", ".idb", ".ipch"}


def git(*args: str) -> bytes:
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True).stdout


def compile_rules(rules: dict[str, str]) -> list[tuple[str, re.Pattern]]:
    return [(name, re.compile(rx, re.I | re.M)) for name, rx in rules.items()]


def strings(data: bytes) -> str:
    ascii_runs = re.findall(rb"[\x20-\x7e]{6,}", data)
    utf16_runs = re.findall(rb"(?:[\x20-\x7e]\x00){6,}", data)
    return "\n".join([r.decode("ascii") for r in ascii_runs] + [r.decode("utf-16-le") for r in utf16_runs])


def scan_text(label: str, text: str, rules) -> list[str]:
    hits = []
    for number, line in enumerate(text.splitlines(), 1):
        for name, rx in rules:
            for match in rx.finditer(line):
                if name == "email" and ALLOWED_EMAILS.search(match.group(0)):
                    continue
                hits.append(f"{label}:{number}: [{name}] {line.strip()[:160]}")
    return hits


def scan_blob(label: str, data: bytes, text_rules, binary_rules) -> list[str]:
    if b"\x00" in data[:8192]:
        return scan_text(label + " (strings)", strings(data), binary_rules)
    return scan_text(label, data.decode("utf-8", "replace"), text_rules)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--extra", type=Path, help="file of additional regexes, one per line")
    parser.add_argument("--message", help="a commit message to check before committing")
    args = parser.parse_args()

    extra = {}
    if args.extra:
        for line in args.extra.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.lstrip().startswith("#"):
                extra[f"extra {len(extra) + 1}"] = line.strip()
    text_rules = compile_rules({**TEXT_RULES, **extra})
    binary_rules = compile_rules({**BINARY_RULES, **extra})

    hits = []
    files = git("ls-files", "-z", "-c", "-o", "--exclude-standard").decode().split("\0")
    staged = set(git("diff", "--cached", "--name-only", "-z").decode().split("\0"))
    for name in sorted(f for f in set(files) if f):
        if Path(name).suffix.lower() in BUILD_OUTPUT:
            hits.append(f"{name}: [build output] must not be committed")
        hits += scan_text("(path) " + name, name, text_rules)
        path = ROOT / name
        if name == SELF:
            continue
        if path.is_file():
            hits += scan_blob(name, path.read_bytes(), text_rules, binary_rules)
        if name in staged:
            index = subprocess.run(["git", "show", f":{name}"], cwd=ROOT, capture_output=True)
            if index.returncode == 0 and (not path.is_file() or index.stdout != path.read_bytes()):
                hits += scan_blob(name + " (index)", index.stdout, text_rules, binary_rules)

    log = git("log", "--all", "--format=%H%x00%B%x00%ae%x00%ce%x1e").decode("utf-8", "replace")
    for record in filter(str.strip, log.split("\x1e")):
        sha, body, author, committer = record.strip("\n").split("\0")
        hits += scan_text(f"commit {sha[:9]} message", body, text_rules)
        for who in (author, committer):
            if not ALLOWED_EMAILS.search(who):
                hits.append(f"commit {sha[:9]}: [identity] email {who} is not a noreply address")
    if args.message:
        hits += scan_text("pending message", args.message, text_rules)

    for hit in hits:
        print(hit)
    print(f"leak_check: {len(set(files)) - 1} files scanned, {len(hits)} finding(s)")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
