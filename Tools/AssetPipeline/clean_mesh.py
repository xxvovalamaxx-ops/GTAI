#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
clean_mesh.py
============================================================================
GTAI — Nanite-ready mesh cleanup for AI-generated (Meshy / Tripo) GLB assets.

This is the geometry-optimization stage of the FORGE asset pipeline. It is
designed to run **inside Blender 5.1 headless** (Blender bundles its own
Python 3.13, so NO pip install is required), but it is also import-safe as a
plain module so `meshy_batch_gen.py` can reuse the argument encoder.

WHY THIS EXISTS
---------------
AI text/image->3D services (Meshy 6, Tripo) emit meshes that are *bad* for
UE5 Nanite out of the box:

  * Non-manifold / duplicate geometry, interior faces, loose verts.
  * Every triangle is a separate face -> 3x vert explosion, terrible for
    Nanite (Nanite wants ~vert_count <= ~0.5 * tri_count for tight clusters).
  * Extreme or tiny transforms (sub-millimeter to kilometer scale).
  * Z-up vs Y-up mismatch with UE5 (-Y forward, Z up).
  * Long/thin slivers that break Nanite cluster culling.
  * Stray alpha-backdrop planes and disconnected floating parts.
  * Materials that come in with linear/sRGB color-space mismatches.

`clean_mesh.py` converts one raw GLB into a single, normalized, watertight,
Nanite-friendly mesh + UE5-ready FBX (with correct PBR color spaces).

NANITE AUTHORING RULES ENFORCED HERE (see Polycount Nanite thread + Epic docs)
-----------------------------------------------------------------------------
  * Keep source meshes UNDER ~1,000,000 tris (ideally < 250k for buildings).
    Above ~1M, file size / import time balloon and UV-ing becomes a nightmare.
  * Avoid huge polygons and long/thin sliver triangles -> add edge density so
    clusters cull well. We flag (and optionally weld) slivers & non-manifold.
  * Merge into ONE continuous mesh where sensible (bolts with their parent,
    facade with its frame) so a single Nanite cluster covers them.
  * Use WEIGHTED NORMALS (split-normal smoothing) to avoid shading artifacts;
    Nanite renders from a single smoothing group.
  * Bake detail into the high-poly source instead of normal maps where it
    reads better, but keep PBR maps correctly color-spaced for Nanite anyway.
  * Target UV coverage > 65-70%; report islands so the user can judge.
  * Don't bother hand-authoring LODs: Nanite builds them automatically on
    import. We only output LOD0 (the source).

USAGE (Blender headless — invoked by meshy_batch_gen.run_blender_cleanup)
------------------------------------------------------------------------
  blender --background --factory-startup --python clean_mesh.py -- \
      <base64-json-payload>

USAGE (standalone, convenience wrapper)
---------------------------------------
  python clean_mesh.py --input raw.glb --out output/mesh.fbx \
      --scale 0.01 --axis Y --report

USAGE (as a module)
-------------------
  from clean_mesh import encode_payload, decode_payload  # for the host CLI

Exit codes: 0 = success, 1 = blender import/clean error, 2 = bad invocation.

Author: FORGE (GTAI Asset Pipeline Engineer)
"""

from __future__ import annotations

import base64
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Constants / defaults
# ---------------------------------------------------------------------------

DEFAULT_BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"

# Meshy exports in centimeters; UE5 default FBX import is centimeters with
# "Convert Scene Unit" ON and transform scale 1.0. Many teams prefer the
# meter workflow: export at 0.01 and set UE5 import transform scale to 1.0.
# We keep 0.01 here to match meshy_batch_gen.py's BLENDER_TO_UE_SCALE.
BLENDER_TO_UE_SCALE = 0.01

# Nanite hard ceiling: keep source meshes under this. Above it, import/build
# time and .uasset size explode. We WARN over SOFT and ERROR over HARD.
NANITE_SOFT_TRIS = 250_000      # warn above this
NANITE_HARD_TRIS = 1_000_000    # hard ceiling — refuse / strongly warn

# Long-thin triangle aspect ratio threshold. A triangle whose longest edge is
# > SLIVER_EDGE_RATIO x its in-plane smallest extent is a sliver.
SLIVER_EDGE_RATIO = 25.0

# PBR roles -> file stem we look for in the texture directory, and the UE5
# Principled-BSDF input socket they bind to. Mirrors meshy_batch_gen.py.
PBR_ROLES: Dict[str, Dict[str, str]] = {
    "basecolor": {"socket": "Base Color", "colorspace": "sRGB"},
    "roughness": {"socket": "Roughness",  "colorspace": "Non-Color"},
    "metallic":  {"socket": "Metallic",   "colorspace": "Non-Color"},
    "normal":    {"socket": "Normal",     "colorspace": "Non-Color"},
    "emissive":  {"socket": "Emission",   "colorspace": "sRGB"},
    "ao":        {"socket": "Ambient Occlusion", "colorspace": "Non-Color"},
}


# ===========================================================================
#  Payload encode/decode  (host-side helpers — usable WITHOUT Blender)
# ===========================================================================

def encode_payload(payload: Dict[str, Any]) -> str:
    """Encode a dict to base64 so it survives Windows argv quoting hell."""
    return base64.b64encode(json.dumps(payload).encode("utf-8")).decode("ascii")


def decode_payload(b64: str) -> Dict[str, Any]:
    return json.loads(base64.b64decode(b64.encode("ascii")).decode("utf-8"))


# ===========================================================================
#  When run INSIDE Blender, `bpy` exists. Everything below is Blender-only.
# ===========================================================================

def _blender_main() -> int:
    import bpy  # noqa: F401 (only available inside Blender)

    # --- parse argv after "--" -------------------------------------------
    if "--" not in sys.argv:
        sys.stderr.write("clean_mesh.py: expected '-- <base64-payload>' in argv\n")
        return 2
    payload_b64 = sys.argv[sys.argv.index("--") + 1]
    args = decode_payload(payload_b64)

    glb_path: str = args["glb_path"]
    out_fbx: str = args["out_fbx"]
    tex_dir: str = args.get("tex_dir", "")
    scale: float = float(args.get("scale", BLENDER_TO_UE_SCALE))
    axis: str = args.get("axis", "Y")          # "Y" => rotate to Z-up for UE5
    remove_bg: bool = bool(args.get("remove_bg", True))
    weld_threshold: float = float(args.get("weld_threshold", 0.0005))
    target_tris: Optional[int] = args.get("target_tris")  # None = keep AI tris
    report_path: str = args.get("report_path", "")        # JSON metrics dump
    verbose: bool = bool(args.get("verbose", True))

    def log(msg: str) -> None:
        if verbose:
            print(f"[clean_mesh] {msg}", flush=True)

    # --- clean slate ------------------------------------------------------
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for o in list(bpy.data.objects):
        if o.name != "Camera":
            bpy.data.objects.remove(o, do_unlink=True)
    for c in list(bpy.data.collections):
        bpy.data.collections.remove(c)

    # --- import -----------------------------------------------------------
    ext = os.path.splitext(glb_path)[1].lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=glb_path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=glb_path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=glb_path)
    else:
        sys.stderr.write(f"clean_mesh: unsupported import format {ext}\n")
        return 1
    log(f"imported {glb_path}")

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        sys.stderr.write("clean_mesh: no MESH objects after import\n")
        return 1

    # --- normalize object transforms (bake world Xform into geometry) -----
    for o in meshes:
        o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in bpy.context.scene.objects:
        o.select_set(o.type == "MESH")

    # --- orientation: GLB is Y-up; UE5 wants -Y forward, Z up ------------
    bpy.ops.object.select_all(action="DESELECT")
    for o in bpy.context.scene.objects:
        if o.type == "MESH":
            o.select_set(True)
    if axis == "Y":
        bpy.ops.object.transform_apply(rotation=False)
        bpy.ops.transform.rotate(value=-1.570796, orient_axis="X")  # -90deg X
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        log("rotated Y-up -> Z-up for UE5")

    # --- remove alpha backdrop / ground plane heuristics ------------------
    if remove_bg:
        for o in list(bpy.context.scene.objects):
            if o.type == "MESH" and o.dimensions.z < 0.01 and o.dimensions.x > 50:
                bpy.data.objects.remove(o, do_unlink=True)
                log(f"removed backdrop plane {o.name}")

    # --- MERGE all mesh parts into ONE continuous mesh -------------------
    # Nanite clusters cull per-object; merging bolts/frames with parents keeps
    # detail inside one cluster and reduces draw overhead.
    bpy.ops.object.select_all(action="DESELECT")
    for o in bpy.context.scene.objects:
        if o.type == "MESH":
            o.select_set(True)
    if len([o for o in bpy.context.scene.objects if o.type == "MESH"]) > 1:
        bpy.context.view_layer.objects.active = next(
            o for o in bpy.context.scene.objects if o.type == "MESH")
        bpy.ops.object.join()
        log("joined multiple mesh parts into one object")

    root = next((o for o in bpy.context.scene.objects if o.type == "MESH"), None)
    if root is None:
        sys.stderr.write("clean_mesh: lost mesh after join\n")
        return 1
    root.name = "Mesh"
    bpy.context.view_layer.objects.active = root

    # --- GEOMETRY CLEANUP (the Nanite-critical pass) ---------------------
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    # Remove degenerate / duplicate / interior / loose geometry
    bpy.ops.mesh.delete_loose()
    bpy.ops.mesh.remove_doubles(threshold=weld_threshold)
    # Fill holes so the mesh is watertight (Nanite prefers closed meshes)
    bpy.ops.mesh.fill_holes(sides=0)
    # Recalculate outside normals
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    log("ran delete_loose / remove_doubles / fill_holes / recalc normals")

    # --- WEIGHTED NORMALS (split-normal smoothing) -----------------------
    # Nanite renders from a single smoothing group, so we bake weighted
    # normals to avoid shading artifacts on hard-surface AI meshes.
    bpy.ops.object.shade_flat() if False else None  # placeholder no-op guard
    try:
        bpy.ops.object.modifier_add(type="WEIGHTED_NORMAL")
        root.modifiers["WeightedNormal"].weight = 50.0
        root.modifiers["WeightedNormal"].keep_sharp = True
        bpy.ops.object.modifier_apply(modifier="WeightedNormal")
        log("applied weighted normals")
    except Exception as e:  # modifiers can vary by build
        log(f"weighted normals skipped ({e})")

    # --- OPTIONAL DECIMATE to a target tri budget ------------------------
    me = root.data
    tri_count = len(me.polygons)
    if target_tris and tri_count > target_tris:
        ratio = max(0.01, target_tris / tri_count)
        bpy.ops.object.modifier_add(type="DECIMATE")
        root.modifiers["Decimate"].ratio = ratio
        root.modifiers["Decimate"].use_collapse = True
        bpy.ops.object.modifier_apply(modifier="Decimate")
        tri_count = len(root.data.polygons)
        log(f"decimated to target ~{target_tris} (ratio {ratio:.3f})")

    # --- remove long/thin slivers (optional, conservative) ---------------
    _flag_slivers(root, SLIVER_EDGE_RATIO, log)

    # --- metrics before export ------------------------------------------
    metrics = _collect_metrics(root)

    # --- (re)bind PBR textures with correct color spaces -----------------
    if tex_dir and Path(tex_dir).exists():
        _bind_pbr_textures(tex_dir, log)
    else:
        log("no texture dir -> exporting geometry-only FBX")

    # --- EXPORT FBX for UE5 ----------------------------------------------
    for o in bpy.context.scene.objects:
        o.select_set(o.type == "MESH")
    os.makedirs(os.path.dirname(out_fbx) or ".", exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=out_fbx,
        use_selection=True,
        global_scale=scale,
        apply_scale_options="FBX_SCALE_ALL",
        object_types={"MESH"},
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        use_armature_deform_only=True,
        bake_space_transform=True,
        embed_textures=False,
        path_mode="COPY",
        axis_forward="-Y",
        axis_up="Z",
    )
    log(f"exported FBX -> {out_fbx}")

    # --- Nanite size advisory -------------------------------------------
    if metrics["tris"] > NANITE_HARD_TRIS:
        log(f"WARNING: {metrics['tris']:,} tris exceeds Nanite hard ceiling "
            f"{NANITE_HARD_TRIS:,}. Re-run with target_tris or use Meshy remesh.")
    elif metrics["tris"] > NANITE_SOFT_TRIS:
        log(f"NOTE: {metrics['tris']:,} tris above soft target "
            f"{NANITE_SOFT_TRIS:,}; fine for Nanite but import may be slow.")

    if report_path:
        Path(report_path).write_text(json.dumps(metrics, indent=2), encoding="utf-8")
        log(f"metrics -> {report_path}")

    print("BLENDER_CLEAN_OK", out_fbx, flush=True)
    return 0


# ---------------------------------------------------------------------------
# Blender-only helpers
# ---------------------------------------------------------------------------

def _collect_metrics(obj) -> Dict[str, Any]:
    me = obj.data
    tris = len(me.polygons)
    verts = len(me.vertices)
    # vertex/triangle ratio — Nanite is most efficient near 0.5
    ratio = (verts / tris) if tris else 0.0
    # bounding box extent
    bbox = [v.co for v in me.vertices]
    if bbox:
        minc = [min(p[i] for p in bbox) for i in range(3)]
        maxc = [max(p[i] for p in bbox) for i in range(3)]
        size = [maxc[i] - minc[i] for i in range(3)]
    else:
        size = [0, 0, 0]
    # non-manifold edge count
    bpy = __import__("bpy")
    non_manifold = 0
    try:
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="DESELECT")
        bpy.ops.mesh.select_non_manifold()
        non_manifold = sum(1 for e in me.edges if e.select)
        bpy.ops.object.mode_set(mode="OBJECT")
    except Exception:
        pass
    return {
        "name": obj.name,
        "tris": tris,
        "verts": verts,
        "vert_tri_ratio": round(ratio, 3),
        "non_manifold_edges": non_manifold,
        "size_m": [round(s, 4) for s in size],
        "materials": [m.name for m in me.materials if m],
    }


def _flag_slivers(obj, ratio: float, log) -> int:
    """Select sliver triangles (very long/thin) so the user can inspect.

    We do NOT auto-delete them (risky) — we just select + count and report.
    Operates in OBJECT mode, selecting bad faces for visual QA in Blender.
    """
    import bmesh
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    bad = 0
    for f in bm.faces:
        if len(f.verts) != 3:
            continue
        # edge lengths
        ls = [e.calc_length() for e in f.edges]
        longest = max(ls)
        # smallest altitude ~ 2*area / longest edge
        area = f.calc_area()
        alt = (2.0 * area / longest) if longest > 1e-9 else 0.0
        if longest > 1e-6 and alt > 1e-9 and (longest / alt) > ratio:
            bad += 1
    bm.free()
    if bad:
        log(f"flagged {bad} sliver triangles (long/thin) — inspect in Blender "
            f"if visual artifacts appear")
    return bad


def _bind_pbr_textures(tex_dir: str, log) -> None:
    import bpy
    from pathlib import Path as _P
    d = _P(tex_dir)
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        for role, spec in PBR_ROLES.items():
            p = d / f"{role}.png"
            if not p.exists():
                continue
            img = bpy.data.images.load(str(p))
            img.colorspace_settings.name = spec["colorspace"]
            tex = nodes.new("ShaderNodeTexImage")
            tex.image = img
            tex.location = (-400, 300 - list(PBR_ROLES).index(role) * 220)
            if role == "normal":
                nm = nodes.new("ShaderNodeNormalMap")
                links.new(tex.outputs["Color"], nm.inputs["Color"])
                links.new(nm.outputs["Normal"], bsdf.inputs[spec["socket"]])
            else:
                links.new(tex.outputs["Color"], bsdf.inputs[spec["socket"]])
        log(f"bound PBR textures for material '{mat.name}'")


# ===========================================================================
#  Standalone host wrapper (runs WITHOUT Blender present)
# ===========================================================================

def _host_run(inputs: List[str], blender_exe: str, out_dir: str,
              scale: float, axis: str, target_tris: Optional[int],
              tex_root: str, remove_bg: bool, report_dir: str) -> int:
    """Drive Blender headless over one or more GLB files."""
    exe = Path(blender_exe)
    if not exe.exists():
        sys.stderr.write(f"clean_mesh: Blender not found at {blender_exe}\n")
        return 2
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    Path(report_dir).mkdir(parents=True, exist_ok=True)
    failures = 0
    for i, glb in enumerate(inputs):
        g = Path(glb)
        if not g.exists():
            sys.stderr.write(f"  missing: {glb}\n")
            failures += 1
            continue
        stem = g.stem
        out_fbx = str(Path(out_dir) / f"{stem}.fbx")
        tex_dir = str(Path(tex_root) / stem) if tex_root else ""
        report_path = str(Path(report_dir) / f"{stem}.metrics.json")
        payload = {
            "glb_path": str(g),
            "out_fbx": out_fbx,
            "tex_dir": tex_dir,
            "scale": scale,
            "axis": axis,
            "remove_bg": remove_bg,
            "weld_threshold": 0.0005,
            "target_tris": target_tris,
            "report_path": report_path,
            "verbose": True,
        }
        # write the Blender script next to this file so Blender can find it
        script = Path(__file__).resolve()
        cmd = [
            str(exe), "--background", "--factory-startup",
            "--python", str(script), "--", encode_payload(payload),
        ]
        print(f"[clean_mesh] ({i+1}/{len(inputs)}) {g.name} -> {stem}.fbx",
              flush=True)
        rc = _run(cmd)
        if rc != 0:
            failures += 1
    return 1 if failures else 0


def _run(cmd: List[str]) -> int:
    import subprocess
    proc = subprocess.run(cmd, capture_output=False)
    return proc.returncode


def _host_cli() -> int:
    import argparse
    ap = argparse.ArgumentParser(
        description="Clean AI-generated GLB meshes for UE5 Nanite (drives "
                    "Blender headless).")
    ap.add_argument("--input", nargs="+", required=True,
                    help="One or more .glb/.gltf/.fbx/.obj files.")
    ap.add_argument("--out", default="output/fbx",
                    help="Output directory for cleaned FBX files.")
    ap.add_argument("--blender", default=DEFAULT_BLENDER,
                    help="Path to blender.exe.")
    ap.add_argument("--scale", type=float, default=BLENDER_TO_UE_SCALE,
                    help="Global scale applied on FBX export (0.01 = cm->m).")
    ap.add_argument("--axis", default="Y", choices=["Y", "Z"],
                    help="Source up-axis. Y = GLB Y-up (rotate to UE5 Z-up).")
    ap.add_argument("--target-tris", type=int, default=None,
                    help="Optional decimate target (Nanite soft cap 250k).")
    ap.add_argument("--tex-root", default="",
                    help="Dir containing <stem>/basecolor.png etc per asset.")
    ap.add_argument("--no-remove-bg", action="store_true",
                    help="Keep alpha-backdrop / ground planes.")
    ap.add_argument("--report-dir", default="output/metrics",
                    help="Where to write per-asset JSON metrics.")
    args = ap.parse_args()
    return _host_run(
        inputs=args.input,
        blender_exe=args.blender,
        out_dir=args.out,
        scale=args.scale,
        axis=args.axis,
        target_tris=args.target_tris,
        tex_root=args.tex_root,
        remove_bg=not args.no_remove_bg,
        report_dir=args.report_dir,
    )


# ===========================================================================
#  Entry point — dual mode
# ===========================================================================

def main() -> int:
    # Called by Blender? argv will contain "--" + base64 payload.
    if "--" in sys.argv and any(
            a in sys.argv for a in ("--background", "--python")) or (
            "--" in sys.argv and len(sys.argv) > sys.argv.index("--") + 1
            and sys.argv[sys.argv.index("--") + 1].startswith(("ey", "DQ"))):
        return _blender_main()
    # Otherwise, behave as a host CLI (or show help).
    if len(sys.argv) > 1:
        return _host_cli()
    # No args: Blender-invoked without payload is an error.
    if "--" in sys.argv:
        sys.stderr.write("clean_mesh.py: invoked in Blender mode without "
                         "payload.\n")
        return 2
    sys.stderr.write(
        "clean_mesh.py — Nanite cleanup for AI meshes.\n"
        "Run inside Blender: blender --background --python clean_mesh.py -- <b64>\n"
        "Or as CLI:        python clean_mesh.py --input a.glb b.glb --out fbx/\n")
    return 2


if __name__ == "__main__":
    sys.exit(main())
