#!/usr/bin/env python3
"""
package_analyzer.py - GTAI (GTA7 / UE 5.8) asset-size profiler and oversize gate.

Walks a UE project's Content tree (or a cooked/staged output tree) and profiles
every .uasset / .umap package by on-disk size, grouping them by asset type
(inferred from path conventions the cooker preserves), then flags oversized
assets against per-type thresholds.

Why path-based classification?
    A fully correct .uasset header parse (FPackageFileSummary -> name table ->
    export table -> class name) is engine-version-fragile: the summary layout
    shifts between UE4 and UE5 and even between UE5 point releases, and an
    offset mistake yields *confidently wrong* type labels. The cooker keeps the
    source folder structure in both the editor Content tree and the cooked
    output, so path conventions (Content/Textures, Content/Maps, Content/Audio,
    ...) are a reliable, version-stable classifier. For authoritative per-asset
    type + memory size, use the in-editor Size Map (Window > Developer Tools >
    Size Map) and Asset Audit (Tools > Audit > Asset Audit). This tool is the
    CI-friendly, engine-free size gate that runs on any machine.

Integrity check:
    Every candidate file is verified to start with the UE package magic
    0x9E2A83C1. Files that fail are reported as suspect (corrupt / not a package)
    rather than silently profiled.

Standard library only (mirrors Tools/CI/validate_build.py) -- no UE install,
no third-party packages. Designed to gate CI: exit code is non-zero when the
number of flagged assets exceeds --max-failures (default 0 = any flag fails).

Usage:
    python Tools/DevOps/package_analyzer.py \
        --root "D:/Projects/GitHub Projects/GTAI/GTA7_UE5/Content" \
        --threshold 50 --top 30 --json report.json --csv report.csv

    # fail the build if more than 5 assets exceed their type budget:
    python Tools/DevOps/package_analyzer.py --root Content --max-failures 5
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from collections import defaultdict
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

# UE package magic (FPackageFileSummary::PackageFileTag). Little-endian int32.
UE_PACKAGE_MAGIC = 0x9E2A83C1

# Default per-type oversize thresholds, in megabytes. Tuned for an open-world
# GTA-style project where textures dominate and maps are legitimately huge.
# Override any of these with --type-threshold TYPE=MB (repeatable).
DEFAULT_TYPE_THRESHOLDS_MB: Dict[str, float] = {
    "Texture2D": 25.0,
    "StaticMesh": 50.0,
    "SkeletalMesh": 75.0,
    "Material": 10.0,
    "Blueprint": 10.0,
    "SoundWave": 15.0,
    "AnimSequence": 20.0,
    "Map": 200.0,
    "Other": 40.0,
}

# Path-segment -> asset-type mapping. Matched case-insensitively against the
# folder components of each asset's path. First match wins (order matters:
# more specific folders before generic ones).
TYPE_PATH_RULES: List[Tuple[str, str]] = [
    ("textures", "Texture2D"),
    ("texture", "Texture2D"),
    ("materials", "Material"),
    ("material", "Material"),
    ("meshes", "StaticMesh"),
    ("mesh", "StaticMesh"),
    ("skeletal", "SkeletalMesh"),
    ("characters", "SkeletalMesh"),
    ("audio", "SoundWave"),
    ("sounds", "SoundWave"),
    ("sound", "SoundWave"),
    ("music", "SoundWave"),
    ("animations", "AnimSequence"),
    ("animation", "AnimSequence"),
    ("anims", "AnimSequence"),
    ("blueprints", "Blueprint"),
    ("blueprint", "Blueprint"),
    ("maps", "Map"),
    ("levels", "Map"),
    ("world", "Map"),
]

# Extensions we treat as packages (maps use .umap, everything else .uasset).
PACKAGE_EXTENSIONS = (".uasset", ".umap")


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class AssetRecord:
    path: str
    size_bytes: int
    asset_type: str
    is_map: bool
    magic_ok: bool
    flagged: bool = False
    flag_reason: str = ""


@dataclass
class TypeSummary:
    asset_type: str
    count: int = 0
    total_bytes: int = 0
    max_bytes: int = 0
    avg_bytes: int = 0
    threshold_mb: float = 0.0
    flagged_count: int = 0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def fmt_size(num_bytes: int) -> str:
    """Human-readable byte size."""
    value = float(num_bytes)
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if abs(value) < 1024.0:
            return f"{value:,.1f} {unit}"
        value /= 1024.0
    return f"{value:,.1f} PB"


def classify_by_path(rel_path: str) -> str:
    """Infer asset type from folder-name conventions in the path."""
    lower = rel_path.lower().replace("\\", "/")
    parts = [p for p in lower.split("/") if p]
    # .umap is always a map regardless of folder.
    if lower.endswith(".umap"):
        return "Map"
    for needle, asset_type in TYPE_PATH_RULES:
        # match a whole folder component or a component that starts with it
        for part in parts:
            if part == needle or part.startswith(needle):
                return asset_type
    return "Other"


def read_magic_ok(path: str) -> bool:
    """True if the file begins with the UE package magic."""
    try:
        with open(path, "rb") as fh:
            head = fh.read(4)
        if len(head) < 4:
            return False
        magic = int.from_bytes(head, byteorder="little")
        return magic == UE_PACKAGE_MAGIC
    except OSError:
        return False


def iter_package_files(root: str):
    """Yield absolute paths of all .uasset/.umap files under root."""
    for dirpath, _dirnames, filenames in os.walk(root):
        for fname in filenames:
            lower = fname.lower()
            if lower.endswith(PACKAGE_EXTENSIONS):
                yield os.path.join(dirpath, fname)


# ---------------------------------------------------------------------------
# Core analysis
# ---------------------------------------------------------------------------

def analyze(root: str, type_thresholds: Dict[str, float]) -> Tuple[List[AssetRecord], Dict[str, TypeSummary]]:
    records: List[AssetRecord] = []

    for abs_path in iter_package_files(root):
        try:
            size = os.path.getsize(abs_path)
        except OSError:
            continue
        rel = os.path.relpath(abs_path, root)
        is_map = abs_path.lower().endswith(".umap")
        asset_type = classify_by_path(rel)
        magic_ok = read_magic_ok(abs_path)

        rec = AssetRecord(
            path=rel,
            size_bytes=size,
            asset_type=asset_type,
            is_map=is_map,
            magic_ok=magic_ok,
        )

        if not magic_ok:
            rec.flagged = True
            rec.flag_reason = "BAD_MAGIC (not a valid UE package / possibly corrupt)"
        else:
            threshold_mb = type_thresholds.get(asset_type, type_thresholds.get("Other", 40.0))
            if size > threshold_mb * 1024.0 * 1024.0:
                rec.flagged = True
                rec.flag_reason = (
                    f"OVERSIZED {asset_type} {fmt_size(size)} > "
                    f"budget {threshold_mb:g} MB"
                )
        records.append(rec)

    # Build per-type summaries.
    summaries: Dict[str, TypeSummary] = {}
    by_type: Dict[str, List[AssetRecord]] = defaultdict(list)
    for rec in records:
        by_type[rec.asset_type].append(rec)

    for asset_type, recs in by_type.items():
        total = sum(r.size_bytes for r in recs)
        max_b = max((r.size_bytes for r in recs), default=0)
        count = len(recs)
        summaries[asset_type] = TypeSummary(
            asset_type=asset_type,
            count=count,
            total_bytes=total,
            max_bytes=max_b,
            avg_bytes=(total // count) if count else 0,
            threshold_mb=type_thresholds.get(asset_type, type_thresholds.get("Other", 40.0)),
            flagged_count=sum(1 for r in recs if r.flagged),
        )

    return records, summaries


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def print_report(records: List[AssetRecord], summaries: Dict[str, TypeSummary],
                 root: str, top_n: int, thresholds: Dict[str, float]) -> None:
    total_bytes = sum(r.size_bytes for r in records)
    flagged = [r for r in records if r.flagged]
    bad_magic = [r for r in records if not r.magic_ok]

    print("=" * 78)
    print("GTAI package_analyzer - asset size profile")
    print("=" * 78)
    print(f"Scanned root : {root}")
    print(f"Assets found : {len(records):,}")
    print(f"Total size   : {fmt_size(total_bytes)}")
    print(f"Thresholds   : " + ", ".join(f"{t}={mb:g}MB" for t, mb in sorted(thresholds.items())))

    # Per-type table.
    print("\n--- By asset type ---")
    header = f"{'TYPE':<14}{'COUNT':>8}{'TOTAL':>14}{'MAX':>14}{'AVG':>12}{'FLAGGED':>9}"
    print(header)
    print("-" * len(header))
    for asset_type in sorted(summaries, key=lambda t: summaries[t].total_bytes, reverse=True):
        s = summaries[asset_type]
        print(f"{asset_type:<14}{s.count:>8}{fmt_size(s.total_bytes):>14}"
              f"{fmt_size(s.max_bytes):>14}{fmt_size(s.avg_bytes):>12}{s.flagged_count:>9}")

    # Top-N largest assets overall.
    print(f"\n--- Top {top_n} largest assets ---")
    largest = sorted(records, key=lambda r: r.size_bytes, reverse=True)[:top_n]
    for i, r in enumerate(largest, 1):
        mark = "!!" if r.flagged else "  "
        print(f"{i:>3}. {mark} {fmt_size(r.size_bytes):>12}  [{r.asset_type:<12}] {r.path}")

    # Flagged assets.
    print(f"\n--- Flagged assets ({len(flagged)}) ---")
    if not flagged:
        print("  none")
    else:
        for r in sorted(flagged, key=lambda r: r.size_bytes, reverse=True):
            print(f"  {fmt_size(r.size_bytes):>12}  {r.path}")
            print(f"        -> {r.flag_reason}")
    if bad_magic:
        print(f"\n  WARNING: {len(bad_magic)} file(s) failed the UE package magic check "
              f"(possible corruption or non-package file with a .uasset/.umap extension).")

    print("=" * 78)


def write_json(path_json: str, records: List[AssetRecord],
               summaries: Dict[str, TypeSummary], root: str, thresholds: Dict[str, float]) -> None:
    payload = {
        "root": root,
        "thresholds_mb": thresholds,
        "totals": {
            "asset_count": len(records),
            "total_bytes": sum(r.size_bytes for r in records),
            "flagged_count": sum(1 for r in records if r.flagged),
            "bad_magic_count": sum(1 for r in records if not r.magic_ok),
        },
        "by_type": {t: asdict(s) for t, s in summaries.items()},
        "flagged": [asdict(r) for r in records if r.flagged],
        "assets": [asdict(r) for r in records],
    }
    with open(path_json, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2)


def write_csv(path_csv: str, records: List[AssetRecord]) -> None:
    import csv
    with open(path_csv, "w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["path", "asset_type", "size_bytes", "is_map", "magic_ok", "flagged", "flag_reason"])
        for r in sorted(records, key=lambda x: x.size_bytes, reverse=True):
            writer.writerow([r.path, r.asset_type, r.size_bytes, r.is_map, r.magic_ok, r.flagged, r.flag_reason])


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_type_threshold(spec: str) -> Tuple[str, float]:
    if "=" not in spec:
        raise argparse.ArgumentTypeError(f"--type-threshold must be TYPE=MB, got {spec!r}")
    key, _, val = spec.partition("=")
    try:
        mb = float(val)
    except ValueError:
        raise argparse.ArgumentTypeError(f"MB must be numeric in {spec!r}")
    return key.strip(), mb


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Profile UE .uasset/.umap sizes and flag oversized assets (GTAI/GTA7).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--root", required=True,
                   help="Directory to scan (project Content tree or cooked/staged output).")
    p.add_argument("--threshold", type=float, default=None,
                   help="Global oversize threshold in MB applied to ALL asset types "
                        "(overrides per-type defaults). Useful for a single hard limit.")
    p.add_argument("--type-threshold", action="append", default=[], type=parse_type_threshold,
                   metavar="TYPE=MB",
                   help="Override a per-type threshold, e.g. Texture2D=30. Repeatable.")
    p.add_argument("--top", type=int, default=30, help="Number of largest assets to list.")
    p.add_argument("--json", default=None, help="Write full JSON report to this path.")
    p.add_argument("--csv", default=None, help="Write all assets (sorted by size) to this CSV path.")
    p.add_argument("--max-failures", type=int, default=0,
                   help="Exit non-zero if MORE than this many assets are flagged "
                        "(0 = fail on any flag). Set higher to allow a known backlog.")
    p.add_argument("--quiet", action="store_true", help="Suppress the console report (use with --json/--csv).")
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_arg_parser().parse_args(argv)

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"ERROR: --root is not a directory: {root}", file=sys.stderr)
        return 2

    # Build effective thresholds.
    thresholds = dict(DEFAULT_TYPE_THRESHOLDS_MB)
    for key, mb in args.type_threshold:
        thresholds[key] = mb
    if args.threshold is not None:
        for key in thresholds:
            thresholds[key] = args.threshold

    records, summaries = analyze(root, thresholds)

    if not args.quiet:
        print_report(records, summaries, root, args.top, thresholds)

    if args.json:
        write_json(args.json, records, summaries, root, thresholds)
        if not args.quiet:
            print(f"\nJSON report written to {args.json}")
    if args.csv:
        write_csv(args.csv, records)
        if not args.quiet:
            print(f"CSV report written to {args.csv}")

    flagged_count = sum(1 for r in records if r.flagged)
    if flagged_count > args.max_failures:
        if not args.quiet:
            print(f"\nGATE FAILED: {flagged_count} flagged asset(s) "
                  f"exceeds allowed max-failures={args.max_failures}")
        return 1

    if not args.quiet:
        print("\nGATE PASSED: no oversized assets beyond the allowed threshold.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
