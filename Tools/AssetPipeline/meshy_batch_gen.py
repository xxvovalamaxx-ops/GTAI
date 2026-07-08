#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
meshy_batch_gen.py
===================================================================
GTAI — AI 3D Asset Generation Pipeline (Meshy 6 -> Blender -> UE5)
===================================================================

A production-grade, Windows-friendly pipeline that turns a list of text
prompts into UE5-ready FBX assets with PBR textures.

Pipeline stages
---------------
  1. PROMPT    Build a consistent prompt from a style-aware template.
  2. GENERATE  Call the Meshy 6 REST API (preview -> refine -> optional remesh).
  3. DOWNLOAD  Fetch the GLB (and texture maps) from Meshy's CDN.
  4. CLEANUP   Run Blender headless to import GLB, fix orientation/scale,
               clean materials, and export a UE5-ready FBX + texture set.
  5. MANIFEST  Emit a JSON manifest (one entry per asset) + a UE5 CSV
               datatable for placement.

Constraints honored
-------------------
  * Python 3.13 + Blender 5.1 on Windows (MSYS/git-bash path style supported).
  * Meshy REST API (https://api.meshy.ai/openapi/v2).
  * FBX output for UE5 with PBR maps (albedo/basecolor, normal, roughness, metallic).
  * Batch generation (100+ assets) with concurrency control + resume.
  * Style-guide system: prompt templates enforce visual consistency.

USAGE
-----
  # Single prompt
  python meshy_batch_gen.py --prompt "NYC brownstone building, 5 stories, brick facade"

  # Batch from a JSON/YAML brief
  python meshy_batch_gen.py --input assets/brief.json --workers 6

  # Dry run in test mode (no credits; mock responses from Meshy test key)
  python meshy_batch_gen.py --input brief.json --test-mode --dry-run

  # Resume a previously interrupted run
  python meshy_batch_gen.py --input brief.json --resume

Author: GTAI Asset Pipeline Engineer
"""

from __future__ import annotations

import argparse
import asyncio
import atexit
import base64
import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    import requests
except ImportError:  # pragma: no cover
    sys.stderr.write("ERROR: 'requests' is required.  pip install requests\n")
    raise

# ----------------------------------------------------------------------------
# Configuration / Constants
# ----------------------------------------------------------------------------

MESHY_API_BASE = "https://api.meshy.ai/openapi/v2"
MESHY_TEST_KEY = "msy_dummy_api_key_for_test_mode_12345678"  # no credits consumed

# Map our semantic texture role -> Meshy key -> UE5 slot name
# UE5 uses *Base Color* (not albedo), *Metallic*, *Roughness*, *Normal*.
PBR_MAP_ROLES = {
    "basecolor": {"meshy_key": "basecolor", "ue_slot": "BaseColor", "ext": "diffuse"},
    "normal": {"meshy_key": "normal", "ue_slot": "Normal", "ext": "normal"},
    "roughness": {"meshy_key": "roughness", "ue_slot": "Roughness", "ext": "roughness"},
    "metallic": {"meshy_key": "metallic", "ue_slot": "Metallic", "ext": "metallic"},
}

# Default Blender location for Windows. Override with --blender.
DEFAULT_BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"

# Meshy's default export unit is centimeters. UE5 default import works at
# centimeters if you leave the FBX import "Convert Scene Unit" ON and set
# the transform scale to 1.0 — but many teams prefer the canonical
# "FBX import scale 0.01" (meters) workflow that the Meshy game-asset guide
# documents for Unity and which Epic's FBX importer applies. We export from
# Blender at a 0.01 scale and document the matching UE5 import settings.
# (See asset_pipeline.md -> Stage 4 for the exact UE5 import dialog settings.)
BLENDER_TO_UE_SCALE = 0.01

# Default poly budgets by asset class (triangles). Tune per class.
DEFAULT_POLY_BUDGET = {
    "building": 120000,
    "vehicle": 60000,
    "prop": 15000,
    "character": 40000,
    "npc": 40000,
    "default": 50000,
}


# ----------------------------------------------------------------------------
# Logging helper
# ----------------------------------------------------------------------------

class Log:
    COLORS = {
        "INFO": "\033[36m", "OK": "\033[32m", "WARN": "\033[33m",
        "ERR": "\033[31m", "STEP": "\033[35m", "RESET": "\033[0m",
    }

    @staticmethod
    def _msg(level: str, msg: str) -> None:
        c = Log.COLORS.get(level, "")
        ts = time.strftime("%H:%M:%S")
        print(f"{c}[{level}]{Log.COLORS['RESET']} {ts} {msg}", flush=True)

    @staticmethod
    def info(m): Log._msg("INFO", m)
    @staticmethod
    def ok(m): Log._msg("OK", m)
    @staticmethod
    def warn(m): Log._msg("WARN", m)
    @staticmethod
    def err(m): Log._msg("ERR", m)
    @staticmethod
    def step(m): Log._msg("STEP", m)


# ----------------------------------------------------------------------------
# Style guide system (prompt templates => visual consistency)
# ----------------------------------------------------------------------------

# A "style guide" is a named bundle of:
#   art_style   : Meshy art_style enum (realistic | cartoon | low-poly)
#   mood        : lighting/material mood appended to every prompt
#   negative    : global negative_prompt
#   surface_mode: for remesh (hard for buildings/vehicles, organic for life)
#   topology    : quad (animatable) vs triangle (static)
#   prefix/suffix: literal text wrapped around the user concept
#
# The GTAI default enforces the NYC open-world visual language:
#   * cohesive realistic PBR look
#   * consistent "game-ready, UV-unwrapped" framing
#   * shared negative prompt so meshes don't come out with floating parts
STYLE_GUIDES: Dict[str, Dict[str, Any]] = {
    "gtai_nyc_realistic": {
        "art_style": "realistic",
        "mood": "overcast New York daytime, subtle ambient occlusion, "
                "weathered but readable surfaces, PBR textures",
        "negative": "floating geometry, disconnected parts, blurry textures, "
                    "multiple objects, distorted proportions, low poly, cartoon",
        "surface_mode": "hard",
        "topology": "triangle",
        "prefix": "NYC open-world game asset,",
        "suffix": "game-ready, UV-unwrapped, clean topology, isolated object",
    },
    "gtai_nyc_stylized": {
        "art_style": "low-poly",
        "mood": "stylized hand-painted adjacent look, bold readable silhouettes",
        "negative": "photorealistic, blurry, noisy, floating parts, multiple objects",
        "surface_mode": "hard",
        "topology": "triangle",
        "prefix": "NYC open-world stylized game asset,",
        "suffix": "low poly, flat shaded, game-ready, isolated object",
    },
    "gtai_character": {
        "art_style": "realistic",
        "mood": "neutral studio lighting, realistic skin and fabric PBR",
        "negative": "extra limbs, fused fingers, distorted face, floating parts, cartoon",
        "surface_mode": "organic",
        "topology": "quad",
        "prefix": "NYC pedestrian character,",
        "suffix": "bipedal, full body, T-pose ready, game-ready, isolated",
    },
}


def render_prompt(concept: str, style: str, asset_class: str = "default") -> Tuple[str, str]:
    """Apply a style guide to a raw concept and return (prompt, negative_prompt)."""
    guide = STYLE_GUIDES.get(style, STYLE_GUIDES["gtai_nyc_realistic"])
    concept = concept.strip().strip(",").strip()
    prompt = f"{guide['prefix']} {concept}, {guide['mood']}, {guide['suffix']}"
    # Normalize whitespace
    prompt = " ".join(prompt.split())
    return prompt, guide["negative"]


# ----------------------------------------------------------------------------
# Asset brief model
# ----------------------------------------------------------------------------

@dataclass
class AssetBrief:
    name: str
    concept: str
    asset_class: str = "default"       # building | vehicle | prop | character | npc
    style: str = "gtai_nyc_realistic"
    target_polys: Optional[int] = None  # if set, run a remesh pass
    enable_nanite: Optional[bool] = None
    custom_negative: Optional[str] = None
    # Output slot overrides (e.g. which district / folder it lands in UE5)
    district: str = "Default"
    position: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    rotation_y: float = 0.0
    scale: float = 1.0

    @staticmethod
    def from_dict(d: Dict[str, Any]) -> "AssetBrief":
        valid = {k: d[k] for k in d if k in AssetBrief.__dataclass_fields__}
        return AssetBrief(**valid)

    def poly_budget(self) -> int:
        return self.target_polys or DEFAULT_POLY_BUDGET.get(
            self.asset_class, DEFAULT_POLY_BUDGET["default"])


# ----------------------------------------------------------------------------
# Meshy client (async, concurrency-safe)
# ----------------------------------------------------------------------------

class MeshyClient:
    def __init__(self, api_key: str, test_mode: bool = False,
                 timeout: int = 300, poll_interval: float = 5.0,
                 max_retries: int = 4):
        self.api_key = MESHY_TEST_KEY if test_mode else api_key
        self.test_mode = test_mode
        self.timeout = timeout
        self.poll_interval = poll_interval
        self.max_retries = max_retries
        self.session = requests.Session()
        self.session.headers.update({
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        })
        self._lock = asyncio.Lock()  # serialize stdout, not network

    # --- low level ----------------------------------------------------------
    def _post(self, path: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        url = f"{MESHY_API_BASE}{path}"
        for attempt in range(self.max_retries):
            try:
                r = self.session.post(url, json=payload, timeout=60)
            except requests.RequestException as e:
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)
                    continue
                raise
            if r.status_code in (200, 201, 202):
                return r.json()
            if r.status_code in (429, 500, 503):
                wait = 2 ** attempt
                Log.warn(f"  Meshy {r.status_code} on {path}; retry in {wait}s")
                time.sleep(wait)
                continue
            # 400/401/402 -> do not retry
            raise RuntimeError(f"Meshy API error {r.status_code}: {r.text}")

    def _get(self, path: str) -> Dict[str, Any]:
        url = f"{MESHY_API_BASE}{path}"
        for attempt in range(self.max_retries):
            r = self.session.get(url, timeout=60)
            if r.status_code in (200, 201):
                return r.json()
            if r.status_code in (429, 500, 503):
                time.sleep(2 ** attempt)
                continue
            if r.status_code == 404:
                raise RuntimeError(f"Task not found (expired?): {path}")
            raise RuntimeError(f"Meshy API error {r.status_code}: {r.text}")

    # --- task model ---------------------------------------------------------
    def create_preview(self, prompt: str, art_style: str,
                       negative: str) -> str:
        payload = {
            "mode": "preview",
            "prompt": prompt,
            "art_style": art_style,
            "model_version": "latest",  # Meshy 6
        }
        if negative:
            payload["negative_prompt"] = negative
        return self._post("/text-to-3d", payload)["result"]

    def refine(self, preview_task_id: str) -> str:
        return self._post("/text-to-3d", {
            "mode": "refine",
            "preview_task_id": preview_task_id,
        })["result"]

    def remesh(self, model_url: str, target_polycount: int,
               surface_mode: str, topology: str) -> str:
        """Re-topologize a completed model to a target poly budget.
        Accepts either a model URL or a raw glb/fbx download URL."""
        return self._post("/remesh", {
            "input": {"model_url": model_url},
            "target_polycount": target_polycount,
            "surface_mode": surface_mode,
            "topology": topology,
        })["result"]

    def poll(self, task_id: str, endpoint: str = "text-to-3d") -> Dict[str, Any]:
        url = f"/{endpoint}/{task_id}"
        elapsed = 0
        while elapsed < self.timeout:
            data = self._get(url)
            status = data.get("status")
            if status == "SUCCEEDED":
                return data
            if status == "FAILED":
                msg = data.get("task_error", {}).get("message", "unknown")
                raise RuntimeError(f"Task {task_id} FAILED: {msg}")
            if status == "EXPIRED":
                raise RuntimeError(f"Task {task_id} EXPIRED")
            # PENDING / IN_PROGRESS
            prog = data.get("progress", 0)
            Log.info(f"  poll {task_id[:8]}: {status} {prog}% ({elapsed}s)")
            time.sleep(self.poll_interval)
            elapsed += self.poll_interval
        raise TimeoutError(f"Task {task_id} timed out after {self.timeout}s")

    # --- textures -----------------------------------------------------------
    def download_textures(self, task_data: Dict[str, Any], dest_dir: Path,
                          force: bool = False) -> Dict[str, Path]:
        """Download PBR texture maps into dest_dir.
        Meshy returns textures either as a `texture_urls` object keyed by role,
        or embedded inside the GLB. Returns mapping role -> local path."""
        downloaded: Dict[str, Path] = {}
        raw_tex = task_data.get("texture_urls") or {}
        if isinstance(raw_tex, list):
            # Some responses return a list of {role/url} dicts; best-effort map.
            tex_urls: Dict[str, str] = {}
            for item in raw_tex:
                if isinstance(item, dict):
                    url = item.get("url") or item.get("model_url")
                    role = item.get("role") or item.get("type") or item.get("key")
                    if url and role:
                        tex_urls[role] = url
        elif isinstance(raw_tex, dict):
            tex_urls = raw_tex
        else:
            tex_urls = {}
        for role, meta in PBR_MAP_ROLES.items():
            url = tex_urls.get(meta["meshy_key"]) or tex_urls.get(role)
            if not url:
                continue
            ext = meta["ext"]
            out = dest_dir / f"{ext}.png"
            if out.exists() and not force:
                downloaded[role] = out
                continue
            try:
                r = self.session.get(url, timeout=120)
                r.raise_for_status()
                out.write_bytes(r.content)
                downloaded[role] = out
                Log.ok(f"  texture {role} -> {out.name}")
            except Exception as e:  # pragma: no cover
                Log.warn(f"  texture {role} download failed: {e}")
        return downloaded


# ----------------------------------------------------------------------------
# Blender headless processing (Stage 4)
# ----------------------------------------------------------------------------

BLENDER_SCRIPT = r"""
import sys, json, os
from pathlib import Path

args = json.loads(sys.argv[-1])
glb_path    = args["glb_path"]
out_fbx     = args["out_fbx"]
tex_dir     = args["tex_dir"]
scale       = args["scale"]
axis        = args["axis"]            # "Z" (GLB already Y-up) or "Y"->"-Z"
apply_nanite_hint = args["nanite"]
remove_bg   = args["remove_bg"]

import bpy

# ---- clean scene ----
bpy.ops.wm.read_factory_settings(use_empty=True)

def _del(objs):
    for o in list(objs):
        if o.name != "Camera":
            bpy.data.objects.remove(o, do_unlink=True)

_del(bpy.data.objects)
for c in list(bpy.data.collections):
    bpy.data.collections.remove(c)

# ---- import GLB ----
ext = os.path.splitext(glb_path)[1].lower()
if ext == ".glb" or ext == ".gltf":
    bpy.ops.import_scene.gltf(filepath=glb_path)
elif ext == ".fbx":
    bpy.ops.import_scene.fbx(filepath=glb_path)
elif ext == ".obj":
    bpy.ops.wm.obj_import(filepath=glb_path)
else:
    raise SystemExit(f"Unsupported import format: {ext}")

# ---- normalize transform ----
for o in bpy.context.scene.objects:
    if o.type == "MESH":
        o.select_set(True)
        # avoid importing stray world transforms
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    else:
        o.select_set(False)

# ---- fix orientation (GLB is typically Y-up; UE5 wants -Y forward, Z up) ----
bpy.ops.object.select_all(action="DESELECT")
for o in bpy.context.scene.objects:
    if o.type == "MESH":
        o.select_set(True)
if axis == "Y":
    bpy.ops.object.transform_apply(rotation=False)
    # rotate -90 on X to go from Y-up to Z-up
    bpy.ops.transform.rotate(value=-1.570796, orient_axis='X')
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

# ---- optional: drop transparent background plane (alpha backdrop) ----
if remove_bg:
    for o in list(bpy.context.scene.objects):
        # crude heuristic: a single huge flat plane with no real geometry use
        if o.type == "MESH" and o.dimensions.z < 0.01 and o.dimensions.x > 50:
            bpy.data.objects.remove(o, do_unlink=True)

# ---- rename main mesh deterministically for UE5 ----
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
root = meshes[0] if meshes else None
if root is not None:
    root.name = "Mesh"

# ---- (re)bind PBR textures if we have a texture folder ----
def _load(role, slot_node_group=None):
    p = Path(tex_dir) / f"{role}.png"
    if p.exists():
        return bpy.data.images.load(str(p))
    return None

if Path(tex_dir).exists():
    for mat in bpy.data.materials:
        if not mat.use_nodes:
            mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        role_to_input = {
            "basecolor": "Base Color",
            "roughness": "Roughness",
            "metallic":  "Metallic",
            "normal":    "Normal",
        }
        for role, inp in role_to_input.items():
            img = _load(role)
            if img is None:
                continue
            tex_node = nodes.new("ShaderNodeTexImage")
            tex_node.image = img
            tex_node.location = (-400, 300 - list(role_to_input).index(role)*220)
            if role == "normal":
                # normal map node
                nm = nodes.new("ShaderNodeNormalMap")
                links.new(tex_node.outputs["Color"], nm.inputs["Color"])
                links.new(nm.outputs["Normal"], bsdf.inputs[inp])
            else:
                links.new(tex_node.outputs["Color"], bsdf.inputs[inp])
            # ensure color space correctness
            if role == "basecolor":
                img.colorspace_settings.name = "sRGB"
            else:
                img.colorspace_settings.name = "Non-Color"

# ---- export FBX for UE5 ----
bpy.ops.object.select_all(action="SELECT")
for o in bpy.context.scene.objects:
    o.select_set(o.type == "MESH")

os.makedirs(os.path.dirname(out_fbx), exist_ok=True)
bpy.ops.export_scene.fbx(
    filepath=out_fbx,
    use_selection=True,
    global_scale=scale,
    apply_scale_options='FBX_SCALE_ALL',
    object_types={'MESH'},
    use_mesh_modifiers=True,
    mesh_smooth_type='FACE',
    add_leaf_bones=False,
    primary_bone_axis='Y',
    secondary_bone_axis='X',
    use_armature_deform_only=True,
    bake_space_transform=True,
    embed_textures=False,
    path_mode='COPY',
    axis_forward='-Y',
    axis_up='Z',
)
print("BLENDER_EXPORT_OK", out_fbx)
"""


def resolve_model_urls(raw: Any) -> Dict[str, str]:
    """Meshy returns model_urls as a dict {glb, fbx, obj} in production, but
    some/test responses may nest it differently. Normalize to a dict."""
    if isinstance(raw, dict):
        if any(k in raw for k in ("glb", "fbx", "obj")):
            return raw
        # maybe nested under another key
        for v in raw.values():
            if isinstance(v, (dict, list)):
                nested = resolve_model_urls(v)
                if nested:
                    return nested
        return raw
    if isinstance(raw, list):
        # list of dicts -> first dict with a known key
        for item in raw:
            if isinstance(item, dict):
                return resolve_model_urls(item)
        # list of strings -> treat first as glb
        if raw and isinstance(raw[0], str):
            return {"glb": raw[0]}
    return {}


def glb_url_from(raw: Any) -> Optional[str]:
    urls = resolve_model_urls(raw)
    return (urls.get("glb") or urls.get("fbx") or urls.get("obj"))


def run_blender_cleanup(blender_exe: Path, glb_path: Path, out_fbx: Path,
                        tex_dir: Path, scale: float, axis: str,
                        nanite: bool, remove_bg: bool) -> bool:
    """Invoke Blender headless to convert GLB -> UE5 FBX with PBR materials."""
    payload = {
        "glb_path": str(glb_path),
        "out_fbx": str(out_fbx),
        "tex_dir": str(tex_dir),
        "scale": scale,
        "axis": axis,
        "nanite": nanite,
        "remove_bg": remove_bg,
    }
    # Write the script to a temp .py next to the fbx (avoids quoting issues)
    script_path = out_fbx.parent / "_blender_clean.py"
    script_path.write_text(BLENDER_SCRIPT, encoding="utf-8")
    # Pass payload as base64 to avoid Windows argv quoting hell
    payload_b64 = base64.b64encode(json.dumps(payload).encode()).decode()
    cmd = [
        str(blender_exe), "--background", "--factory-startup",
        "--python", str(script_path),
        "--", payload_b64,
    ]
    Log.step(f"  Blender: {glb_path.name} -> {out_fbx.name}")
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    except subprocess.TimeoutExpired:
        Log.err(f"  Blender timed out on {glb_path.name}")
        return False
    if proc.returncode != 0 or "BLENDER_EXPORT_OK" not in proc.stdout:
        Log.err(f"  Blender failed on {glb_path.name}")
        # Surface tail of stderr for debugging
        tail = (proc.stderr or "")[-800:]
        Log.err(tail)
        return False
    return True


# ----------------------------------------------------------------------------
# Single asset orchestration (runs inside an asyncio worker)
# ----------------------------------------------------------------------------

@dataclass
class RunPaths:
    work: Path
    glb: Path
    fbx: Path
    tex: Path
    manifest_entry: Dict[str, Any] = field(default_factory=dict)


def make_paths(output_root: Path, brief: AssetBrief) -> RunPaths:
    key = hashlib.md5(brief.concept.encode()).hexdigest()[:12]
    safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in brief.name)[:48]
    asset_dir = output_root / safe
    asset_dir.mkdir(parents=True, exist_ok=True)
    return RunPaths(
        work=asset_dir,
        glb=asset_dir / f"{safe}.glb",
        fbx=asset_dir / f"{safe}.fbx",
        tex=asset_dir / "textures",
    )


async def process_one(brief: AssetBrief, client: MeshyClient,
                      blender_exe: Path, output_root: Path,
                      axis: str, resume: bool) -> Dict[str, Any]:
    paths = make_paths(output_root, brief)
    entry: Dict[str, Any] = {
        "name": brief.name,
        "concept": brief.concept,
        "asset_class": brief.asset_class,
        "style": brief.style,
        "district": brief.district,
        "position": brief.position,
        "rotation_y": brief.rotation_y,
        "scale": brief.scale,
        "status": "pending",
    }

    # Resume support: skip if FBX already produced
    if resume and paths.fbx.exists():
        Log.ok(f"[skip] {brief.name} (fbx exists)")
        entry.update({
            "status": "done",
            "fbx": str(paths.fbx),
            "glb": str(paths.glb),
            "textures": str(paths.tex),
        })
        return entry

    prompt, negative = render_prompt(brief.concept, brief.style, brief.asset_class)
    if brief.custom_negative:
        negative = brief.custom_negative
    guide = STYLE_GUIDES.get(brief.style, STYLE_GUIDES["gtai_nyc_realistic"])

    Log.step(f"[gen] {brief.name}: {prompt[:90]}...")
    try:
        preview_id = client.create_preview(prompt, guide["art_style"], negative)
        client.poll(preview_id)
        refine_id = client.refine(preview_id)
        result = client.poll(refine_id)
    except Exception as e:
        Log.err(f"  generation failed for {brief.name}: {e}")
        entry["status"] = "failed"
        entry["error"] = str(e)
        return entry

    model_urls = resolve_model_urls(result.get("model_urls"))
    glb_url = glb_url_from(result.get("model_urls"))
    if not glb_url:
        Log.err(f"  no GLB url for {brief.name}")
        entry["status"] = "failed"
        entry["error"] = "no glb url"
        return entry

    # Optional remesh to target poly budget
    if brief.asset_class in ("building", "vehicle", "character", "npc", "prop"):
        try:
            remesh_id = client.remesh(glb_url, brief.poly_budget(),
                                      guide["surface_mode"], guide["topology"])
            rres = client.poll(remesh_id, endpoint="remesh")
            remeshed = (rres.get("model_urls") or {}).get("glb")
            if remeshed:
                glb_url = remeshed
                Log.ok(f"  remeshed -> {brief.poly_budget()} tris")
        except Exception as e:
            Log.warn(f"  remesh skipped ({e}); using original mesh")

    # Download GLB
    Log.step(f"[dl] {brief.name}")
    try:
        r = client.session.get(glb_url, timeout=300)
        r.raise_for_status()
        paths.glb.write_bytes(r.content)
        # Download PBR textures alongside
        paths.tex.mkdir(exist_ok=True)
        client.download_textures(result, paths.tex)
    except Exception as e:
        Log.err(f"  download failed for {brief.name}: {e}")
        entry["status"] = "failed"
        entry["error"] = str(e)
        return entry

    # Blender cleanup -> FBX
    nanite = brief.enable_nanite if brief.enable_nanite is not None \
        else (brief.asset_class in ("building", "prop"))
    ok = run_blender_cleanup(
        blender_exe, paths.glb, paths.fbx, paths.tex,
        scale=BLENDER_TO_UE_SCALE, axis=axis, nanite=nanite,
        remove_bg=(brief.asset_class not in ("character", "npc")),
    )
    if not ok:
        entry["status"] = "failed"
        entry["error"] = "blender export failed"
        return entry

    entry.update({
        "status": "done",
        "fbx": str(paths.fbx),
        "glb": str(paths.glb),
        "textures": str(paths.tex),
        "preview_task": preview_id,
        "refine_task": refine_id,
        "nanite": nanite,
        "poly_budget": brief.poly_budget(),
    })
    Log.ok(f"[done] {brief.name} -> {paths.fbx.name}")
    return entry


# ----------------------------------------------------------------------------
# Batch driver
# ----------------------------------------------------------------------------

def _run_one_sync(brief: AssetBrief, client: MeshyClient, blender_exe: Path,
                  output_root: Path, axis: str, resume: bool) -> Dict[str, Any]:
    """Run the async process_one to completion inside a dedicated event loop.
    Intended to be called from a thread-pool worker so multiple assets
    generate concurrently without blocking the asyncio scheduler."""
    return asyncio.new_event_loop().run_until_complete(
        process_one(brief, client, blender_exe, output_root, axis, resume)
    )


def run_batch(briefs: List[AssetBrief], client: MeshyClient,
              blender_exe: Path, output_root: Path,
              workers: int, axis: str, resume: bool) -> List[Dict[str, Any]]:
    """Process all briefs concurrently using a thread pool. Each worker runs
    its own event loop so the blocking API/Blender calls don't stall others.
    `workers` caps in-flight assets (honor your Meshy concurrency plan)."""
    import concurrent.futures
    sem = threading.Semaphore(workers)

    def worker(brief: AssetBrief) -> Dict[str, Any]:
        with sem:
            return _run_one_sync(brief, client, blender_exe,
                                 output_root, axis, resume)

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        results = list(ex.map(worker, briefs))
    return results


# ----------------------------------------------------------------------------
# Manifest + UE5 placement CSV
# ----------------------------------------------------------------------------

def write_manifest(entries: List[Dict[str, Any]], output_root: Path) -> Path:
    mpath = output_root / "manifest.json"
    mpath.write_text(json.dumps(entries, indent=2), encoding="utf-8")
    Log.ok(f"Manifest -> {mpath}")
    return mpath


def write_ue5_placement_csv(entries: List[Dict[str, Any]], output_root: Path) -> Path:
    """UE5 Datatable-friendly CSV: Asset,Path,District,X,Y,Z,RotY,Scale,Nanite"""
    cpath = output_root / "ue5_placement.csv"
    lines = ["Asset,FBXRelativePath,District,X,Y,Z,RotY,Scale,Nanite"]
    for e in entries:
        if e.get("status") != "done":
            continue
        rel = os.path.relpath(e["fbx"], output_root).replace("\\", "/")
        p = e.get("position", [0, 0, 0])
        lines.append(",".join(str(x) for x in [
            e["name"], rel, e.get("district", "Default"),
            p[0], p[1], p[2], e.get("rotation_y", 0), e.get("scale", 1),
            "true" if e.get("nanite") else "false",
        ]))
    cpath.write_text("\n".join(lines), encoding="utf-8")
    Log.ok(f"UE5 placement CSV -> {cpath}")
    return cpath


# ----------------------------------------------------------------------------
# Brief loading
# ----------------------------------------------------------------------------

def load_briefs(path: Path) -> List[AssetBrief]:
    text = path.read_text(encoding="utf-8")
    if path.suffix.lower() in (".yaml", ".yml"):
        try:
            import yaml
            data = yaml.safe_load(text)
        except ImportError:
            raise RuntimeError("PyYAML required for .yaml briefs: pip install pyyaml")
    else:
        data = json.loads(text)
    if isinstance(data, dict) and "assets" in data:
        data = data["assets"]
    return [AssetBrief.from_dict(d) for d in data]


def make_single_brief(prompt: str, name: Optional[str] = None) -> AssetBrief:
    concept = prompt
    # crude asset-class guess for sensible defaults
    cls = "prop"
    if any(w in prompt.lower() for w in ("building", "brownstone", "tower", "skyscraper")):
        cls = "building"
    elif any(w in prompt.lower() for w in ("car", "truck", "vehicle", "taxi", "bus")):
        cls = "vehicle"
    elif any(w in prompt.lower() for w in ("character", "person", "npc", "pedestrian")):
        cls = "npc"
    name = name or hashlib.md5(prompt.encode()).hexdigest()[:8]
    return AssetBrief(name=name, concept=concept, asset_class=cls)


# ----------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------

def parse_args(argv: List[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="GTAI Meshy -> UE5 asset pipeline")
    p.add_argument("--prompt", help="Single text prompt to generate")
    p.add_argument("--name", help="Asset name for single-prompt mode")
    p.add_argument("--input", help="JSON/YAML brief file (list of assets)")
    p.add_argument("--output", default=None,
                   help="Output root dir (default: ./GeneratedAssets)")
    p.add_argument("--api-key", default=None,
                   help="Meshy API key (else env MESHY_API_KEY)")
    p.add_argument("--test-mode", action="store_true",
                   help="Use Meshy test key (no credits; mock responses)")
    p.add_argument("--blender", default=DEFAULT_BLENDER,
                   help="Path to blender.exe")
    p.add_argument("--workers", type=int, default=4,
                   help="Concurrent generation tasks (respect your plan limit)")
    p.add_argument("--axis", default="Z",
                   help="GLB up-axis ('Z' = already Y-up GLB, 'Y' = rotate to Z-up)")
    p.add_argument("--style", default="gtai_nyc_realistic",
                   help="Default style guide key")
    p.add_argument("--resume", action="store_true",
                   help="Skip assets whose FBX already exists")
    p.add_argument("--dry-run", action="store_true",
                   help="Validate prompts/paths, do not call API")
    p.add_argument("--list-styles", action="store_true",
                   help="Print available style guides and exit")
    return p.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    if args.list_styles:
        for k, v in STYLE_GUIDES.items():
            print(f"  {k}: art_style={v['art_style']} surface={v['surface_mode']}")
        return 0

    if not args.prompt and not args.input:
        Log.err("Provide --prompt or --input <brief.json>")
        return 2

    # Resolve briefs
    if args.input:
        briefs = load_briefs(Path(args.input))
        # apply default style where missing
        for b in briefs:
            if not b.style:
                b.style = args.style
    else:
        b = make_single_brief(args.prompt, args.name)
        b.style = args.style
        briefs = [b]

    Log.info(f"Loaded {len(briefs)} asset brief(s)")

    output_root = Path(args.output or "GeneratedAssets").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    # Blender availability check
    blender_exe = Path(args.blender)
    if not blender_exe.exists():
        Log.err(f"Blender not found at {blender_exe}. Use --blender.")
        return 3

    if args.dry_run:
        for b in briefs:
            pr, ne = render_prompt(b.concept, b.style, b.asset_class)
            Log.info(f"  [{b.name}] class={b.asset_class} style={b.style}")
            Log.info(f"      prompt: {pr}")
            Log.info(f"      neg   : {ne}")
        Log.ok("Dry run complete — no API calls made.")
        return 0

    api_key = args.api_key or os.environ.get("MESHY_API_KEY", "")
    if not args.test_mode and not api_key:
        Log.err("No API key. Set MESHY_API_KEY env var, pass --api-key, or use --test-mode.")
        return 4

    client = MeshyClient(api_key=api_key, test_mode=args.test_mode)

    Log.step(f"Starting batch: {len(briefs)} assets, {args.workers} workers")
    entries = run_batch(
        briefs, client, blender_exe, output_root,
        workers=args.workers, axis=args.axis, resume=args.resume,
    )

    write_manifest(entries, output_root)
    write_ue5_placement_csv(entries, output_root)

    done = sum(1 for e in entries if e.get("status") == "done")
    failed = sum(1 for e in entries if e.get("status") == "failed")
    Log.ok(f"Pipeline complete: {done} done, {failed} failed, {len(entries)-done-failed} pending")
    return 0


if __name__ == "__main__":
    sys.exit(main())
