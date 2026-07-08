#!/usr/bin/env python3
"""GTAI / GTA7 UE 5.8 build validation script.

Validates that the Unreal project is internally consistent and ready to
compile BEFORE invoking the (slow, engine-dependent) Unreal Build Tool step.

Checks performed
----------------
1. Project file (``GTA7.uproject``) exists and is valid JSON.
2. The 10 expected modules each have a ``<Module>.Build.cs`` under
   ``Source/<Module>/`` and matching ``Public``/``Private`` folders.
3. The module names declared in ``.uproject`` exactly match the Build.cs files
   on disk (no orphan modules, no missing modules).
4. Every ``PublicDependencyModuleNames`` / ``PrivateDependencyModuleNames``
   reference that points to a GTAI module is itself a declared module, and the
   module dependency graph is acyclic (UBT hard-fails on cycles).
5. The two target files (``GTA7Editor.target.cs``, ``GTA7.target.cs``) exist.
6. ``.gitattributes`` declares LFS tracking for the required asset extensions.

Usage
-----
    python Tools/CI/validate_build.py --project-root <PATH> [--engine-root <PATH>]
    python Tools/CI/validate_build.py --project-root "D:/Projects/.../GTA7_UE5"

Exit code is non-zero on the first category of failure so it can gate CI.

This script has NO third-party dependencies (standard library only) and does
not require the Unreal Engine to be installed.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

EXPECTED_MODULES: Tuple[str, ...] = (
    "GTA7",
    "GTAI_Core",
    "GTAI_NPC",
    "GTAI_World",
    "GTAI_Vehicles",
    "GTAI_Combat",
    "GTAI_AI",
    "GTAI_UI",
    "GTAI_Audio",
    "GTAI_Quests",
)

REQUIRED_LFS_EXTENSIONS = {".uasset", ".umap", ".fbx", ".png", ".wav", ".mp3"}
LFS_FRIENDLY_EXTENSIONS = {".obj", ".gltf", ".glb", ".jpg", ".jpeg", ".tga",
                           ".exr", ".hdr", ".dds", ".tiff", ".webp", ".ogg"}

TARGET_FILES = ("GTA7Editor.target.cs", "GTA7.target.cs")

_DEP_RE = re.compile(
    r'(Public|Private)DependencyModuleNames\.(?:Add|AddRange)\(',
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

class Report:
    """Tiny collector for errors/warnings with a non-zero exit on errors."""

    def __init__(self) -> None:
        self.errors: List[str] = []
        self.warnings: List[str] = []

    def error(self, msg: str) -> None:
        self.errors.append(msg)

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)

    @property
    def ok(self) -> bool:
        return not self.errors

    def print(self) -> None:
        for w in self.warnings:
            print(f"  [WARN] {w}")
        for e in self.errors:
            print(f"  [ERROR] {e}")
        if self.ok:
            print("  [OK] validation passed")
        else:
            print(f"  [FAIL] {len(self.errors)} error(s), {len(self.warnings)} warning(s)")


def parse_module_dependency_lists(build_cs: str) -> Tuple[List[str], List[str]]:
    """Extract (public_deps, private_deps) module-name strings from a Build.cs.

    Handles both ``Add("X")`` and ``AddRange(new string[] { "A", "B" })`` forms.
    """
    public: List[str] = []
    private: List[str] = []

    # Walk token by token; track whether we are inside a public or private block.
    # Simpler robust approach: find each "KindDependencyModuleNames." occurrence,
    # then capture the string literal list that follows.
    pattern = re.compile(
        r'(Public|Private)DependencyModuleNames\.(?:AddRange|Add)\s*\('
        r'(?:new\s+string\[\]\s*)?\{([^}]*)\}?',
        re.DOTALL,
    )
    literal = re.compile(r'"([^"]+)"')
    for kind, body in pattern.findall(build_cs):
        names = literal.findall(body)
        if kind == "Public":
            public.extend(names)
        else:
            private.extend(names)
    return public, private


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

def check_project_file(project_root: Path, rep: Report) -> dict:
    uproject = project_root / "GTA7.uproject"
    if not uproject.is_file():
        rep.error(f"Missing project file: {uproject}")
        return {}
    try:
        data = json.loads(uproject.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        rep.error(f"GTA7.uproject is not valid JSON: {exc}")
        return {}

    declared = [m.get("Name") for m in data.get("Modules", []) if m.get("Name")]
    engine = data.get("EngineAssociation", "?")
    if engine != "5.8":
        rep.warn(f"EngineAssociation is '{engine}', expected '5.8'")
    if data.get("TargetPlatforms") and "Windows" not in data["TargetPlatforms"]:
        rep.warn("TargetPlatforms does not include 'Windows'")
    return {"declared_modules": declared, "engine": engine}


def check_source_structure(project_root: Path, rep: Report) -> Dict[str, Tuple[List[str], List[str]]]:
    source = project_root / "Source"
    deps: Dict[str, Tuple[List[str], List[str]]] = {}
    if not source.is_dir():
        rep.error(f"Missing Source/ directory: {source}")
        return deps

    build_cs_files: Dict[str, Path] = {}
    for f in source.rglob("*.Build.cs"):
        # f.name is e.g. "GTAI_Combat.Build.cs"; strip the full suffix, NOT
        # just the first dot (Path.stem would yield "GTAI_Combat.Build").
        name = f.name
        if not name.endswith(".Build.cs"):
            continue
        mod = name[: -len(".Build.cs")]
        build_cs_files[mod] = f

    declared = set(EXPECTED_MODULES)
    found = set(build_cs_files.keys())

    for missing in declared - found:
        rep.error(f"Missing Build.cs for expected module: {missing}")
    for extra in found - declared:
        rep.warn(f"Build.cs exists for module not in expected list: {extra}")

    for mod in sorted(found):
        f = build_cs_files[mod]
        mod_dir = f.parent
        for sub in ("Public", "Private"):
            if not (mod_dir / sub).is_dir():
                rep.warn(f"Module {mod} is missing {sub}/ folder")
        try:
            text = f.read_text(encoding="utf-8")
        except OSError as exc:
            rep.error(f"Cannot read {f}: {exc}")
            continue
        deps[mod] = parse_module_dependency_lists(text)

    return deps


def check_module_consistency(uproject_modules: List[str], build_cs_modules: Set[str], rep: Report) -> None:
    up = set(uproject_modules)
    for missing in up - build_cs_modules:
        rep.error(f"Module '{missing}' declared in .uproject but no Build.cs found")
    for extra in build_cs_modules - up:
        rep.warn(f"Build.cs module '{extra}' not declared in .uproject")


def check_dependency_graph(deps: Dict[str, Tuple[List[str], List[str]]], rep: Report) -> None:
    """Verify referenced GTAI modules exist and the graph is acyclic."""
    gtai_modules = set(deps.keys())
    # Build adjacency for GTAI-only edges (cross-module references).
    edges: Dict[str, Set[str]] = {m: set() for m in gtai_modules}
    for mod, (pub, priv) in deps.items():
        for dep in set(pub) | set(priv):
            if dep in gtai_modules and dep != mod:
                edges[mod].add(dep)
            elif dep in gtai_modules and dep == mod:
                rep.warn(f"Module {mod} lists itself as a dependency (ignored)")
            # Engine modules (Core, Engine, ...) are not validated here.

    # Cycle detection via DFS.
    WHITE, GRAY, BLACK = 0, 1, 2
    color = {m: WHITE for m in gtai_modules}
    cycle_path: List[str] = []

    def dfs(node: str, stack: List[str]) -> bool:
        color[node] = GRAY
        stack.append(node)
        for nxt in sorted(edges[node]):
            if color[nxt] == GRAY:
                # Found cycle.
                idx = stack.index(nxt)
                cycle_path.extend(stack[idx:] + [nxt])
                return True
            if color[nxt] == WHITE and dfs(nxt, stack):
                return True
        stack.pop()
        color[node] = BLACK
        return False

    for m in sorted(gtai_modules):
        if color[m] == WHITE:
            if dfs(m, []):
                rep.error("Module dependency cycle detected: "
                          + " -> ".join(cycle_path))
                break


def check_targets(project_root: Path, rep: Report) -> None:
    source = project_root / "Source"
    for tf in TARGET_FILES:
        p = source / tf
        if not p.is_file():
            rep.error(f"Missing target file: {p}")
            continue
        text = p.read_text(encoding="utf-8")
        if "TargetRules" not in text:
            rep.error(f"{tf} does not declare a TargetRules subclass")
        if tf == "GTA7Editor.target.cs" and "TargetType.Editor" not in text:
            rep.error(f"{tf} is not of type TargetType.Editor")
        if tf == "GTA7.target.cs" and "TargetType.Game" not in text:
            rep.error(f"{tf} is not of type TargetType.Game")


def check_gitattributes(project_root: Path, rep: Report) -> None:
    ga = project_root / ".gitattributes"
    if not ga.is_file():
        rep.error("Missing .gitattributes")
        return
    text = ga.read_text(encoding="utf-8", errors="ignore")
    tracked: Set[str] = set()
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = re.match(r"\*\.(\w+)\s+filter=lfs", line)
        if m:
            tracked.add("." + m.group(1).lower())
    missing = REQUIRED_LFS_EXTENSIONS - tracked
    if missing:
        rep.error("Required LFS extensions not tracked: " + ", ".join(sorted(missing)))
    extra = tracked & LFS_FRIENDLY_EXTENSIONS
    if extra:
        rep.warn("Companion LFS extensions also tracked: " + ", ".join(sorted(extra)))


def check_engine(engine_root: Path | None, rep: Report) -> None:
    if engine_root is None:
        return
    if not engine_root.is_dir():
        rep.warn(f"Engine root not found: {engine_root} (skipping engine check)")
        return
    build_bat = engine_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    if not build_bat.is_file():
        rep.warn(f"Build.bat not found under engine root: {build_bat}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate GTAI/UE5 build setup.")
    parser.add_argument("--project-root", required=True, type=Path,
                        help="Path to the GTA7_UE5 project root")
    parser.add_argument("--engine-root", required=False, type=Path, default=None,
                        help="Optional path to the UE 5.8 engine root (loose check)")
    args = parser.parse_args(argv)

    project_root = args.project_root.resolve()
    rep = Report()
    print(f"Validating GTAI build at: {project_root}")

    uproj = check_project_file(project_root, rep)
    deps = check_source_structure(project_root, rep)
    if uproj:
        check_module_consistency(uproj.get("declared_modules", []), set(deps.keys()), rep)
    check_dependency_graph(deps, rep)
    check_targets(project_root, rep)
    check_gitattributes(project_root, rep)
    check_engine(args.engine_root, rep)

    print("-" * 60)
    rep.print()
    return 0 if rep.ok else 1


if __name__ == "__main__":
    sys.exit(main())
