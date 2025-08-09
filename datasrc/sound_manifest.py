# datasrc/sound_manifest.py
# Cross-platform manifest parsing and Opus duration (in ticks) extraction.

import os
import re
import sys
from dataclasses import dataclass
from typing import List, Tuple, Optional

# Root directory and manifest file (relative to project root / CWD)
MANIFEST_SOUNDS_DIR = "server_data/sounds"
MANIFEST_FILE = os.path.join(MANIFEST_SOUNDS_DIR, "sound.manifest")

# First custom enum value (1st manifest sound -> 41)
SPECIAL_SOUND_BASE_ID = 41

# Time base: 1 second = 50 ticks (Teeworlds/DDNet)
TICKS_PER_SECOND = 50
# Opus fixed sampling rate
OPUS_SAMPLE_RATE = 48000


@dataclass(frozen=True)
class Entry:
    """Single manifest entry with computed interval in ticks."""
    enum: str            # e.g. "SOUND_VOTE_MENU_CLICK"
    file: str            # relative path inside server_data/sounds (e.g. "ui/menu_click.opus")
    path: str            # absolute path used for duration probing (source/build)
    interval_ticks: int  # duration in ticks (1 sec = 50 ticks)


def _norm_enum_name(alias_or_file: str) -> str:
    """
    Normalize alias or filename into enum-friendly 'SOUND_...' identifier.
    Examples:
      'ui/menu_click.opus' -> 'SOUND_UI_MENU_CLICK'
      'GAME_ACCEPT'        -> 'SOUND_GAME_ACCEPT'
    """
    # If it's a path to .opus, make an alias from basename without extension.
    if "." in alias_or_file and alias_or_file.lower().endswith(".opus"):
        base = os.path.splitext(os.path.basename(alias_or_file))[0]
        alias = base
    else:
        alias = alias_or_file

    alias = re.sub(r"[^A-Za-z0-9]+", "_", alias).strip("_").upper()
    if not alias:
        alias = "UNKNOWN"
    if alias[0].isdigit():
        alias = "SND_" + alias
    if not alias.startswith("SOUND_"):
        alias = "SOUND_" + alias
    return alias


def _sanitize_relpath(p: str) -> Optional[str]:
    """
    Normalize a relative path and reject absolute paths or parent traversal.
    Returns normalized 'unix' style path (with forward slashes) or None.
    """
    p = p.replace("\\", "/").strip()
    if not p:
        return None
    # No absolute paths
    if os.path.isabs(p):
        return None
    # Normalize and check for traversal
    np = os.path.normpath(p).replace("\\", "/")
    if np.startswith("../") or np == "..":
        return None
    return np


def parse_manifest(path: str = MANIFEST_FILE) -> List[Tuple[str, str]]:
    """
    Parse sound.manifest and return ordered list of (enum_name, rel_file_path).
    Supported line formats (comments '#' and empty lines are ignored):
      - 'ui/menu_click.opus'
      - 'ALIAS = ui/menu_click.opus'
      - 'ALIAS ui/menu_click.opus'
    Duplicate enum/file names are skipped with a warning.
    """
    entries: List[Tuple[str, str]] = []
    seen_enum, seen_file = set(), set()

    if not os.path.isfile(path):
        print(f"[special_sounds] manifest not found: {path}", file=sys.stderr)
        return entries

    with open(path, "r", encoding="utf-8") as f:
        for ln, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "#" in line:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue

            alias, file_name = None, None
            if "=" in line:
                left, right = line.split("=", 1)
                alias = left.strip()
                file_name = right.strip()
            else:
                parts = line.split()
                if len(parts) == 1:
                    file_name = parts[0].strip()
                elif len(parts) >= 2:
                    alias = parts[0].strip()
                    file_name = parts[1].strip()

            if not file_name:
                print(f"[special_sounds] skip line {ln}: '{raw.rstrip()}'", file=sys.stderr)
                continue

            rel = _sanitize_relpath(file_name)
            if not rel:
                print(f"[special_sounds] invalid path (line {ln}): '{file_name}'", file=sys.stderr)
                continue

            enum_name = _norm_enum_name(alias or file_name)

            if enum_name in seen_enum:
                print(f"[special_sounds] duplicate enum '{enum_name}' (line {ln})", file=sys.stderr)
                continue
            if rel in seen_file:
                print(f"[special_sounds] duplicate file '{rel}' (line {ln})", file=sys.stderr)
                continue

            seen_enum.add(enum_name)
            seen_file.add(rel)
            entries.append((enum_name, rel))

    return entries


# -------------------- Opus duration (Ogg parsing) --------------------

def _opus_duration_ticks_from_bytes(data: bytes, ticks_per_sec: int = TICKS_PER_SECOND) -> int:
    """
    Compute .opus duration in ticks from raw Ogg bytes.

    Algorithm:
      - Iterate Ogg pages ("OggS").
      - Detect the Opus stream by finding a page whose payload starts with "OpusHead",
        store its stream serial number and pre-skip (little-endian uint16).
      - Track the last valid granulepos for that serial only.
      - Duration in samples = max(last_granulepos - pre_skip, 0).
      - Convert to ticks: (samples * TPS + 48000/2) // 48000 (half-up rounding).
    """
    if not data:
        return 0

    pos = 0
    data_len = len(data)

    serial_target: Optional[int] = None
    pre_skip = 0
    last_gp = 0

    while True:
        idx = data.find(b"OggS", pos)
        if idx < 0 or idx + 27 > data_len:
            break

        version = data[idx + 4]
        if version != 0:
            pos = idx + 1
            continue

        granulepos = int.from_bytes(data[idx + 6: idx + 14], "little", signed=False)
        serialno = int.from_bytes(data[idx + 14: idx + 18], "little", signed=False)
        seg_count = data[idx + 26]

        lace_off = idx + 27
        if lace_off + seg_count > data_len:
            break
        lacing = data[lace_off: lace_off + seg_count]
        body_len = int(sum(lacing)) if seg_count else 0
        body_off = lace_off + seg_count

        page_total_len = 27 + seg_count + body_len
        pos = idx + page_total_len
        if body_off + body_len > data_len:
            break  # truncated

        # Detect "OpusHead" to lock serial
        if serial_target is None and body_len >= 12:
            body_hdr = data[body_off: body_off + 12]
            if body_hdr[:8] == b"OpusHead":
                pre_skip = int.from_bytes(body_hdr[10:12], "little", signed=False)
                serial_target = serialno

        # Track granulepos only for the selected serial
        if serial_target is not None and serialno == serial_target:
            if granulepos != 0xFFFFFFFFFFFFFFFF:  # sentinel
                last_gp = granulepos

    samples = last_gp - pre_skip if last_gp > pre_skip else 0
    ticks = (samples * int(ticks_per_sec) + (OPUS_SAMPLE_RATE // 2)) // OPUS_SAMPLE_RATE
    return max(ticks, 0)


def opus_duration_ticks(path: str, ticks_per_sec: int = TICKS_PER_SECOND) -> int:
    """Read file and return duration in ticks. Pure Python, cross-platform."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except Exception:
        return 0
    return _opus_duration_ticks_from_bytes(data, ticks_per_sec)


def build_entries_with_intervals(path: str = MANIFEST_FILE,
                                 sounds_dir: str = MANIFEST_SOUNDS_DIR,
                                 ticks_per_sec: int = TICKS_PER_SECOND,
                                 warn_missing: bool = True) -> List[Entry]:
    """
    Build ordered list of Entry(enum, file, path, interval_ticks) from manifest.
    'file' is a normalized relative path inside sounds_dir (supports subfolders).
    Duration is returned in ticks (1 sec = 50 ticks).
    """
    # Resolve paths
    if not os.path.isabs(path):
        manifest_path = os.path.abspath(os.path.join(os.getcwd(), path))
    else:
        manifest_path = path
    if not os.path.isabs(sounds_dir):
        base_sounds = os.path.abspath(os.path.join(os.getcwd(), sounds_dir))
    else:
        base_sounds = sounds_dir

    raw = parse_manifest(manifest_path)
    entries: List[Entry] = []
    missing: List[str] = []

    for enum_name, rel_file in raw:
        full = os.path.join(base_sounds, rel_file)
        if not os.path.isfile(full):
            missing.append(rel_file)
            entries.append(Entry(enum=enum_name, file=rel_file, path=full, interval_ticks=0))
            continue

        ticks = opus_duration_ticks(full, ticks_per_sec)
        entries.append(Entry(enum=enum_name, file=rel_file, path=full, interval_ticks=ticks))

    if warn_missing and missing:
        print("[special_sounds] warning: missing files:", ", ".join(missing), file=sys.stderr)

    return entries