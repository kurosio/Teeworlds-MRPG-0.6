#!/usr/bin/env python3
"""Static checks of the repository that no compiler can do.

The script needs nothing but the Python standard library and the repository
itself, so it runs in a second, locally and in CI, on every change.

What it verifies (errors stop the run):

  * schema routing  - the tables listed in Db::LOCAL_TABLES are exactly the
                      tables declared by mrpg_local.sql; a table that moves to
                      the other database without changing both places sends its
                      queries to the wrong server;
  * schema coverage - every tw_* / enum_* table the code names exists in one of
                      the SQL files, so a fresh install cannot miss a table that
                      some code path already queries (see
                      scripts/schema_allowlist.txt for the checked-in gaps);
  * header guards   - every #ifndef/#define pair matches, the guard names are
                      unique and every guarded header closes with #endif;
  * component paths - server component directories and C++ filenames use
                      lowercase snake_case;
  * text files      - no CRLF line endings or byte order mark in source,
                      scripts, SQL, and configuration files;
  * languages       - data/languages/*.json parse and follow the schema the
                      client expects.

Warnings are reported but do not fail the run unless --strict is given:

  * translations that translate the same English text differently in one file
    (one of the two is unreachable);
  * headers without any include guard (upstream files, harmless while the
    include order stays as it is);
  * raw SQL in a file that is not a *_db.cpp module.

Usage:
    scripts/check_style.py [-v|--verbose] [--strict] [--root DIR]

Exit code: 0 - no errors (and, with --strict, no warnings), 1 - findings,
2 - the repository could not be read.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

# ------------------------------------------------------------------ constants

SRC_DIR = "src"
LANGUAGES_DIR = "data/languages"
SHARED_SQL = "mrpg_shared.sql"
LOCAL_SQL = "mrpg_local.sql"
DB_TYPES = "src/engine/server/db/db_types.h"
ALLOWLIST = "scripts/schema_allowlist.txt"

# Third party code keeps its own style and is not touched by the checks.
THIRD_PARTY = ("/external/",)

# Text that must keep Unix line endings to be usable on the servers.
UNIX_TEXT_SUFFIXES = (".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".sh", ".py", ".sql", ".cmake", ".yml", ".yaml", ".cfg", ".conf", ".json")
UNIX_TEXT_NAMES = ("CMakeLists.txt", "Dockerfile", "Makefile")

# A statement of the database layer outside of a *_db.cpp module.
RAW_SQL_RE = re.compile(r"\bDb::(?:Query|Transaction)\s*\(")
RAW_SQL_ALLOWED = (
    # The startup probes and the one-shot migration of the world list run before
    # any game object exists; they have no module to live in.
    "src/engine/server/db/",
    "src/engine/server/multi_worlds.cpp",
    "src/engine/server/server.cpp",
    "src/engine/server/links.h",
    "src/test/",
)

TABLE_NAME_RE = re.compile(r"\b((?:tw|enum)_[a-z0-9_]+)\b")
# A string literal that is SQL; only there a tw_* / enum_* name is a table.
SQL_TEXT_RE = re.compile(r"\b(?:SELECT|INSERT|UPDATE|DELETE|REPLACE|JOIN|FROM|INTO|TABLE)\b", re.IGNORECASE)
C_STRING_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
KIND_OF_TABLE_RE = re.compile(r'KindOfTable\s*\(\s*(?:[^()]*?,\s*)?["\']([A-Za-z0-9_]+)["\']')
CREATE_TABLE_RE = re.compile(r"CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?`?([A-Za-z0-9_]+)`?", re.IGNORECASE)
GUARD_RE = re.compile(r"\s*#ifndef\s+(\S+)[ \t]*\r?\n\s*#define\s+(\S+)")
PRAGMA_ONCE_RE = re.compile(r"\s*#pragma\s+once\b")
COMPONENT_ROOT = Path("src/game/server/core/components")
SNAKE_CASE_RE = re.compile(r"[a-z][a-z0-9_]*\Z")


class Report:
    """Collects the findings of every check."""

    def __init__(self, verbose: bool) -> None:
        self.verbose = verbose
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.checks = 0

    def start(self, name: str) -> None:
        self.checks += 1
        self._section = name

    def error(self, message: str) -> None:
        self.errors.append(f"{self._section}: {message}")

    def warn(self, message: str) -> None:
        self.warnings.append(f"{self._section}: {message}")

    def info(self, message: str) -> None:
        if self.verbose:
            print(f"note: {self._section}: {message}")


# ------------------------------------------------------------------ utilities


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return None


def walk_source(root: Path, suffixes: tuple[str, ...], directories: tuple[str, ...] = (SRC_DIR,)):
    for directory in directories:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in suffixes:
                continue
            relative = path.relative_to(root).as_posix()
            if any(part in f"/{relative}" for part in THIRD_PARTY):
                continue
            yield path, relative


SKIP_DIRECTORIES = {".git", "build", "release", "libraries", "node_modules", "__pycache__", ".cache", "out", "dist"}


def tracked_files(root: Path) -> list[str]:
    """The files to check: what git tracks plus the new files of the work tree."""
    names: set[str] = set()
    try:
        output = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            check=True, capture_output=True, timeout=120,
        ).stdout
        names.update(name for name in output.decode("utf-8", "surrogateescape").split("\0") if name)
    except (OSError, subprocess.SubprocessError):
        pass
    for path in root.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(root)
        if any(part in SKIP_DIRECTORIES for part in relative.parts):
            continue
        names.add(relative.as_posix())
    return sorted(names)


def load_allowlist(root: Path) -> dict[str, str]:
    path = root / ALLOWLIST
    if not path.is_file():
        return {}
    allowed: dict[str, str] = {}
    for line in read_text(path).splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        parts = stripped.split(None, 1)
        reason = parts[1].strip() if len(parts) > 1 else ""
        if reason.startswith("#"):
            reason = reason[1:].strip()
        allowed[parts[0].lower()] = reason
    return allowed


def sql_tables(root: Path, name: str) -> set[str]:
    text = read_text(root / name)
    if text is None:
        return set()
    return {match.group(1).lower() for match in CREATE_TABLE_RE.finditer(text)}


# ------------------------------------------------------------------ checks


def check_schema_routing(root: Path, report: Report) -> None:
    """Db::LOCAL_TABLES must be exactly the tables of mrpg_local.sql."""
    report.start("schema routing")
    header = read_text(root / DB_TYPES)
    if header is None:
        report.error(f"{DB_TYPES} is missing")
        return

    block = re.search(r"LOCAL_TABLES\[\]\s*=\s*\{(.*?)\};", header, re.DOTALL)
    if not block:
        report.error(f"{DB_TYPES}: LOCAL_TABLES[] not found")
        return
    declared = {name.lower() for name in re.findall(r'"([A-Za-z0-9_]+)"', block.group(1))}
    imported = sql_tables(root, LOCAL_SQL)

    for name in sorted(declared - imported):
        report.error(f"{name} is routed to the local database but {LOCAL_SQL} does not declare it")
    for name in sorted(imported - declared):
        report.error(f"{name} is declared by {LOCAL_SQL} but not routed to the local database (see {DB_TYPES})")


def check_schema_coverage(root: Path, report: Report) -> None:
    """Every table the code names in SQL must exist in one of the SQL files."""
    report.start("schema coverage")
    known = sql_tables(root, SHARED_SQL) | sql_tables(root, LOCAL_SQL)
    if not known:
        report.error(f"{SHARED_SQL} and {LOCAL_SQL} declare no tables")
        return

    allowed = load_allowlist(root)
    used: dict[str, str] = {}
    for path, relative in walk_source(root, (".cpp", ".h", ".hpp", ".c")):
        # The tests query made up tables on purpose.
        if relative.startswith("src/test/"):
            continue
        text = read_text(path)
        if text is None:
            report.error(f"{relative}: not valid UTF-8")
            continue
        statements = [literal for literal in C_STRING_RE.findall(text) if SQL_TEXT_RE.search(literal)]
        statements.extend(KIND_OF_TABLE_RE.findall(text))
        for statement in statements:
            for match in TABLE_NAME_RE.finditer(statement):
                used.setdefault(match.group(1).lower(), relative)

    for name in sorted(set(used) - known):
        if name in allowed:
            report.info(f"{name} ({used[name]}) is a known gap, see {ALLOWLIST}")
            continue
        report.error(f"{name} is used by {used[name]} but neither SQL file declares it: "
                     f"add it to the dump, or record the gap in {ALLOWLIST}")


def leading_prologue_end(text: str) -> int:
    """Skip whitespace and leading comments before a header's guard."""
    position = 0
    while True:
        while position < len(text) and text[position].isspace():
            position += 1
        if text.startswith("//", position):
            newline = text.find("\n", position)
            position = len(text) if newline < 0 else newline + 1
            continue
        if text.startswith("/*", position):
            end = text.find("*/", position + 2)
            if end < 0:
                return position
            position = end + 2
            continue
        return position


def check_header_guards(root: Path, report: Report) -> None:
    """The #ifndef/#define pairs must match, be unique and end with #endif."""
    report.start("header guards")
    owners: dict[str, str] = {}
    unguarded: list[str] = []
    for path, relative in walk_source(root, (".h", ".hpp", ".hh")):
        text = read_text(path)
        if text is None:
            report.error(f"{relative}: not valid UTF-8")
            continue
        start = leading_prologue_end(text)
        if PRAGMA_ONCE_RE.match(text, start):
            continue
        match = GUARD_RE.match(text, start)
        if not match:
            unguarded.append(relative)
            continue
        ifndef, define = match.group(1), match.group(2)
        if ifndef != define:
            report.error(f"{relative}: #ifndef {ifndef} does not match #define {define}")
        if "#endif" not in text[match.end():]:
            report.error(f"{relative}: {ifndef} is never closed with #endif")
        if ifndef in owners:
            report.error(f"{relative}: guard {ifndef} is already used by {owners[ifndef]}")
        else:
            owners[ifndef] = relative

    if unguarded:
        report.warn(f"{len(unguarded)} of {len(owners) + len(unguarded)} headers have no include guard "
                    f"(upstream files, see --verbose)")
        for relative in unguarded:
            report.info(f"{relative}: no include guard")


def check_component_paths(root: Path, report: Report) -> None:
    """Component directories and C++ filenames use lowercase snake_case."""
    report.start("component paths")
    base = root / COMPONENT_ROOT
    if not base.is_dir():
        report.warn(f"{COMPONENT_ROOT.as_posix()} is missing")
        return
    for path in sorted(base.rglob("*")):
        relative = path.relative_to(root)
        if path.is_dir() and not SNAKE_CASE_RE.fullmatch(path.name):
            report.error(f"{relative.as_posix()}: component directory must use lowercase snake_case")
        elif path.is_file() and path.suffix in {".cpp", ".h", ".hpp"}:
            if not SNAKE_CASE_RE.fullmatch(path.stem):
                report.error(f"{relative.as_posix()}: C++ filename must use lowercase snake_case")
            if path.suffix == ".h":
                text = read_text(path)
                start = leading_prologue_end(text or "")
                match = GUARD_RE.match(text or "", start)
                expected = re.sub(r"[^A-Za-z0-9]+", "_", path.relative_to(root / "src").with_suffix("").as_posix()).upper().strip("_") + "_H"
                if not match or match.group(1) != match.group(2):
                    report.error(f"{relative.as_posix()}: expected a matching #ifndef/#define header guard")
                elif match.group(1) != expected:
                    report.error(f"{relative.as_posix()}: expected header guard {expected}")


def check_text_files(root: Path, report: Report) -> None:
    """Scripts, SQL files and configs must stay plain UTF-8 with LF endings."""
    report.start("text files")
    files = tracked_files(root)
    if not files:
        report.warn("git is unavailable, the line endings of tracked files are not checked")
        return
    for name in files:
        if not name.endswith(UNIX_TEXT_SUFFIXES) and Path(name).name not in UNIX_TEXT_NAMES:
            continue
        data = (root / name).read_bytes() if (root / name).is_file() else b""
        if not data:
            continue
        if data.startswith(b"\xef\xbb\xbf"):
            report.error(f"{name}: starts with a UTF-8 byte order mark")
        if b"\r\n" in data:
            count = data.count(b"\r\n")
            report.error(f"{name}: {count} CRLF line ending(s), the file must use LF")


def check_languages(root: Path, report: Report) -> None:
    """The translation files must parse and follow the client schema."""
    report.start("languages")
    directory = root / LANGUAGES_DIR
    if not directory.is_dir():
        report.warn(f"{LANGUAGES_DIR} is missing")
        return

    for path in sorted(directory.glob("*.json")):
        relative = path.relative_to(root).as_posix()
        text = read_text(path)
        if text is None:
            report.error(f"{relative}: not valid UTF-8")
            continue
        try:
            document = json.loads(text)
        except json.JSONDecodeError as error:
            report.error(f"{relative}: invalid JSON at line {error.lineno}: {error.msg}")
            continue
        if not isinstance(document, dict):
            report.error(f"{relative}: the top level must be an object")
            continue
        if path.name == "index.json":
            if not isinstance(document.get("language indices"), list):
                report.error(f"{relative}: needs a 'language indices' list")
            continue
        entries = document.get("translated strings")
        if not isinstance(entries, list):
            report.error(f"{relative}: needs a 'translated strings' list")
            continue
        if not isinstance(document.get("authors"), dict):
            report.warn(f"{relative}: no 'authors' object")

        translations: dict[str, set[str]] = {}
        for index, entry in enumerate(entries):
            if not isinstance(entry, dict) or not isinstance(entry.get("or"), str) or not isinstance(entry.get("tr"), str):
                report.error(f"{relative}: entry {index} is not an object with the strings 'or' and 'tr'")
                continue
            translations.setdefault(entry["or"], set()).add(entry["tr"])

        conflicting = sorted(original for original, values in translations.items() if len(values) > 1)
        if conflicting:
            report.warn(f"{relative}: {len(conflicting)} text(s) translated twice, only one is used: "
                        + ", ".join(repr(original) for original in conflicting[:5]))


def check_raw_sql(root: Path, report: Report) -> None:
    """SQL belongs to the *_db.cpp modules, the architecture of the database layer."""
    report.start("sql modules")
    findings: dict[str, list[int]] = {}
    for path, relative in walk_source(root, (".cpp", ".h")):
        if path.stem.endswith("_db") or any(relative.startswith(prefix) for prefix in RAW_SQL_ALLOWED):
            continue
        text = read_text(path)
        if text is None:
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            if RAW_SQL_RE.search(line):
                findings.setdefault(relative, []).append(number)

    for relative, lines in sorted(findings.items()):
        report.warn(f"{relative}:{lines[0]}: SQL outside of a *_db.cpp module "
                    f"({len(lines)} statement(s)); move it into the module that owns the table")


# ------------------------------------------------------------------ entry


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Static checks of the repository.")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent),
                        help="repository root (default: the parent of this script)")
    parser.add_argument("-v", "--verbose", action="store_true", help="print every finding")
    parser.add_argument("--strict", action="store_true", help="treat warnings as errors")
    parser.add_argument("--no-color", action="store_true", help="never colorize the output")
    arguments = parser.parse_args(argv)

    root = Path(arguments.root).resolve()
    if not (root / SRC_DIR).is_dir():
        print(f"error: {root} does not look like the repository root", file=sys.stderr)
        return 2

    report = Report(arguments.verbose)
    for check in (check_schema_routing, check_schema_coverage, check_header_guards,
                  check_component_paths, check_text_files, check_languages, check_raw_sql):
        check(root, report)

    color = not arguments.no_color and sys.stdout.isatty() and os.environ.get("NO_COLOR") is None
    red = "\033[31m" if color else ""
    yellow = "\033[33m" if color else ""
    green = "\033[32m" if color else ""
    reset = "\033[0m" if color else ""

    for message in report.errors:
        print(f"{red}error{reset}: {message}")
    shown = report.warnings if arguments.verbose else report.warnings[:10]
    for message in shown:
        print(f"{yellow}warning{reset}: {message}")
    if len(shown) < len(report.warnings):
        print(f"{yellow}warning{reset}: ... and {len(report.warnings) - len(shown)} more (run with --verbose)")

    failed = bool(report.errors) or (arguments.strict and bool(report.warnings))
    summary = f"{report.checks} checks, {len(report.errors)} error(s), {len(report.warnings)} warning(s)"
    print(f"{red if failed else green}{'FAILED' if failed else 'OK'}{reset}: {summary}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
