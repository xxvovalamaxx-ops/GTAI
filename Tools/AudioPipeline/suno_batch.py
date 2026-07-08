#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
suno_batch.py
===================================================================
GTAI — AI Radio Music Generation Pipeline (Suno API -> UE5-ready WAV)
===================================================================

A production-grade, Windows-friendly pipeline that turns a list of radio
track specs (per station) into UE5-importable music assets: MP3 + WAV
(44.1k/48k), with optional separated stems (vocals + instrumental) so the
Dynamic Music System (Docs/audio_system.md §3.1, §9) can mute/solo layers.

This is the audio mirror of `Tools/AssetPipeline/meshy_batch_gen.py`. Same
orchestration philosophy: dataclass brief -> async-capable client with
retry/poll -> download -> manifest + UE5 metadata CSV -> resume / dry-run /
workers / test-mode. Build-time only — never at game runtime (Suno latency,
cost, and licensing control; see skill constraint).

Pipeline stages
---------------
  1. PROMPT    Build a consistent genre-aware prompt from a style/station guide.
  2. GENERATE  Call Suno API v1/generate (async) + poll record-info (or webhook).
  3. DOWNLOAD  Fetch the .mp3 from Suno's CDN; convert to .wav via convert_to_wav.
  4. STEMS     Optionally separate_vocals -> vocals + instrumental stems.
  5. MANIFEST  Emit a JSON manifest (one entry per track) + a UE5 CSV datatable
               (Station, Track, Style, BPM, Duration, VocalOrInstr, WavPath).

Constraints honored
-------------------
  * Python 3.11+ (git-bash/MSYS path style supported).
  * Suno REST API (https://api.sunoapi.org/api/v1).
  * Bearer token auth (env SUNO_API_KEY or --api-key).
  * Test-mode uses a dummy key + mocked responses (no credits consumed).
  * Batch generation (100+ tracks) with concurrency control + resume.
  * Style-guide system: per-station prompt templates enforce sonic consistency.

SUNO API FACTS (from docs.sunoapi.org, July 2026)
-------------------------------------------------
  * Base URL: https://api.sunoapi.org
  * Auth:     Authorization: Bearer <token>
  * Generate: POST /api/v1/generate
      body: { customMode, instrumental, model, prompt, style, title,
              callBackUrl, personaId, personaModel, negativeTags,
              vocalGender, styleWeight, weirdnessConstraint, audioWeight }
      -> { code, msg, data: { taskId } }
    Models: V4 (<=4min, best vocals), V4_5 (<=8min, smart prompts),
            V4_5PLUS (richer tones), V4_5ALL (best structure), V5, V5_5.
    Watermark-free commercial output. ~20s streaming latency.
  * Poll:   GET /api/v1/generate/record-info?taskId=...
      -> data.status: PENDING / TEXT_SUCCESS / FIRST_SUCCESS / SUCCESS /
                      CREATE_TASK_FAILED / GENERATE_AUDIO_FAILED /
                      CALLBACK_EXCEPTION / SENSITIVE_WORD_ERROR
      -> data.response.sunoData[]: { id, audioUrl, streamAudioUrl, imageUrl,
                                      prompt, modelName, title, tags,
                                      createTime, duration }
  * WAV:    POST /api/v1/convert-to-wav (separate endpoint) -> wav URL.
  * Stems:  POST /api/v1/separate-vocals (separate endpoint) -> stems.
  * Credits: GET /api/v1/get-remaining-credits.
  * All endpoints support webhook `callBackUrl` (recommended for large batches).
  * This pipeline uses POLLING (record-info) so it works without a public
    callback URL — set --callback-url to switch to webhook mode.

USAGE
-----
  # Single track
  python suno_batch.py --prompt "Driving synthwave, neon city, 120 BPM" --title "Neon Drive 01"

  # Batch from a JSON/YAML brief (per station)
  python suno_batch.py --input tracks/brief.json --workers 6

  # Dry run (validate prompts/paths, no credits)
  python suno_batch.py --input tracks/brief.json --dry-run

  # Test mode (mock responses, no credits, exercises full code path)
  python suno_batch.py --input tracks/brief.json --test-mode --dry-run

  # Resume an interrupted run
  python suno_batch.py --input tracks/brief.json --resume

  # Webhook mode (recommended for 100+ tracks)
  python suno_batch.py --input tracks/brief.json --callback-url https://my.server/hook

Author: SIREN (GTAI Audio & Radio Specialist)
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    import requests
except ImportError:  # pragma: no cover
    sys.stderr.write("ERROR: 'requests' is required.  pip install requests\n")
    raise


# ----------------------------------------------------------------------------
# Configuration / Constants
# ----------------------------------------------------------------------------

SUNO_API_BASE = "https://api.sunoapi.org/api/v1"
SUNO_TEST_KEY = "suno_dummy_api_key_for_test_mode_12345678"  # no credits consumed
SUNO_TEST_CALLBACK = None  # test mode does not need a real callback

# Per-model max duration (seconds) — used to validate briefs.
MODEL_MAX_DURATION = {
    "V4": 240,
    "V4_5": 480,
    "V4_5PLUS": 480,
    "V4_5ALL": 480,
    "V5": 480,
    "V5_5": 480,
}

# Default output sample rate for UE5 import (48k is the engine's native master).
DEFAULT_WAV_RATE = "48000"

# ffmpeg is required for local MP3->WAV fallback if the convert_to_wav endpoint
# is unavailable. Override with --ffmpeg. If missing, we still keep the MP3 and
# flag the track; UE5 can import MP3 via the Media Framework / SoundWave importer.
DEFAULT_FFMPEG = "ffmpeg"


# ----------------------------------------------------------------------------
# Logging helper (mirrors meshy_batch_gen.py)
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
# Station / style guide system (prompt templates => sonic consistency)
# ----------------------------------------------------------------------------
# Each guide maps a GTAI station to a Suno `style` + `prompt` framing so every
# track on a station feels like part of the same broadcast. Mirrors the
# STYLE_GUIDES pattern from the asset pipeline.
#
# Keys mirror Docs/audio_system.md §2.2 station roster:
#   PulseFM, NeonDrive, TheForum, NYCNow, Airbrands, ClassicNY, Latido
STYLE_GUIDES: Dict[str, Dict[str, Any]] = {
    "PulseFM": {
        # Hip-Hop / Rap — DJ Kano
        "style": "East Coast hip-hop, trap hi-hats, 808 bass, boom bap soul sample",
        "model": "V4_5ALL",
        "instrumental_bias": False,  # mostly vocal tracks
        "prompt_suffix": "radio-ready mix, punchy master, New York energy",
        "negative": "country, metal, lo-fi, distorted vocals",
        "default_bpm": 92,
    },
    "NeonDrive": {
        # Electronic / Synthwave — VERA (AI host)
        "style": "synthwave, retrowave, arpeggiated analog synths, gated reverb drums, "
                 "driving bassline, neon city night drive",
        "model": "V4_5",
        "instrumental_bias": True,  # mostly instrumental beds
        "prompt_suffix": "club-ready, wide stereo, 80s analog warmth",
        "negative": "acoustic, folk, vocals, lo-fi",
        "default_bpm": 118,
    },
    "TheForum": {
        # Talk Radio — Marcus Webb (music is incidental bumpers only)
        "style": "spoken-word news broadcast bed, subtle ambient piano, serious tone",
        "model": "V4",
        "instrumental_bias": True,
        "prompt_suffix": "understated, non-distracting, studio broadcast",
        "negative": "energetic, dance, loud",
        "default_bpm": 0,
    },
    "NYCNow": {
        # News — Anchors Chen & Okafor (news stinger beds)
        "style": "urgent broadcast news theme, orchestral strings, tense percussion",
        "model": "V4",
        "instrumental_bias": True,
        "prompt_suffix": "cinematic, immediate, television news grade",
        "negative": "comedy, relaxed, vocals",
        "default_bpm": 0,
    },
    "Airbrands": {
        # Commercials-only — rotating VO (jingle bumpers)
        "style": "catchy advertising jingle, bright pop production, upbeat",
        "model": "V4_5",
        "instrumental_bias": False,
        "prompt_suffix": "memorable hook, brand-safe, 15-30s feel",
        "negative": "dark, aggressive, dissonant",
        "default_bpm": 110,
    },
    "ClassicNY": {
        # Jazz / Soul — Smooth Cole
        "style": "smooth jazz, soul, warm upright bass, brushed drums, "
                 "rhodes piano, saxophone",
        "model": "V4_5ALL",
        "instrumental_bias": True,
        "prompt_suffix": "late-night lounge, intimate, vintage analog",
        "negative": "EDM, metal, distortion",
        "default_bpm": 88,
    },
    "Latido": {
        # Latin / Reggaeton — Sofi Reyes
        "style": "reggaeton, latin pop, dembow rhythm, brass, tropical melody",
        "model": "V4_5ALL",
        "instrumental_bias": False,
        "prompt_suffix": "bilingual energy, festival-ready, percussion-forward",
        "negative": "rock, emo, ballad",
        "default_bpm": 95,
    },
    # Generic fallback for ad-hoc tracks
    "generic": {
        "style": "cinematic instrumental, balanced mix",
        "model": "V4_5ALL",
        "instrumental_bias": True,
        "prompt_suffix": "radio-ready",
        "negative": "harsh, clipping",
        "default_bpm": 100,
    },
}


def render_prompt(concept: str, style: str, station: str,
                  bpm: Optional[int], instrumental: Optional[bool]) -> Tuple[str, str]:
    """Apply a station guide to a raw concept and return (prompt, negative)."""
    guide = STYLE_GUIDES.get(station, STYLE_GUIDES["generic"])
    concept = concept.strip().strip(",").strip()

    # Decide vocal vs instrumental
    if instrumental is None:
        instrumental = guide["instrumental_bias"]

    parts = [concept, guide["style"]]
    if bpm and bpm > 0:
        parts.append(f"{bpm} BPM")
    parts.append(guide["prompt_suffix"])
    if instrumental:
        parts.append("instrumental, no vocals")
    else:
        parts.append("clear lead vocal")
    prompt = ", ".join(p for p in parts if p)
    prompt = " ".join(prompt.split())
    return prompt, guide["negative"]


# ----------------------------------------------------------------------------
# Track brief model
# ----------------------------------------------------------------------------

@dataclass
class TrackBrief:
    name: str
    station: str = "generic"
    concept: str = ""
    title: Optional[str] = None
    model: Optional[str] = None          # override guide default
    instrumental: Optional[bool] = None   # None = station default
    bpm: Optional[int] = None
    duration_sec: int = 180
    vocal_gender: Optional[str] = None    # 'm' | 'f'
    seed: Optional[int] = None
    persona_id: Optional[str] = None
    persona_model: Optional[str] = None   # style_persona | voice_persona
    negative_tags: Optional[str] = None
    separate_stems: bool = False
    convert_wav: bool = True              # produce a .wav (default True)
    # Output slot overrides (which subfolder it lands in under the station)
    district: str = "Default"

    @staticmethod
    def from_dict(d: Dict[str, Any]) -> "TrackBrief":
        valid = {k: d[k] for k in d if k in TrackBrief.__dataclass_fields__}
        return TrackBrief(**valid)

    def resolved_model(self) -> str:
        if self.model:
            return self.model
        return STYLE_GUIDES.get(self.station, STYLE_GUIDES["generic"])["model"]

    def resolved_neg(self) -> str:
        if self.negative_tags:
            return self.negative_tags
        return STYLE_GUIDES.get(self.station, STYLE_GUIDES["generic"])["negative"]


# ----------------------------------------------------------------------------
# Suno client (async-capable, concurrency-safe, polling-based)
# ----------------------------------------------------------------------------

class SunoClient:
    def __init__(self, api_key: str, test_mode: bool = False,
                 timeout: int = 900, poll_interval: float = 8.0,
                 max_retries: int = 5, callback_url: Optional[str] = None):
        self.api_key = SUNO_TEST_KEY if test_mode else api_key
        self.test_mode = test_mode
        self.timeout = timeout
        self.poll_interval = poll_interval
        self.max_retries = max_retries
        self.callback_url = callback_url
        self.session = requests.Session()
        self.session.headers.update({
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        })
        self._lock = threading.Lock()

    # --- low level ----------------------------------------------------------
    def _post(self, path: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        url = f"{SUNO_API_BASE}{path}"
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
                Log.warn(f"  Suno {r.status_code} on {path}; retry in {wait}s")
                time.sleep(wait)
                continue
            # 400/401/402/413/430/455 -> do not retry
            raise RuntimeError(f"Suno API error {r.status_code}: {r.text}")

    def _get(self, path: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        url = f"{SUNO_API_BASE}{path}"
        for attempt in range(self.max_retries):
            try:
                r = self.session.get(url, params=params, timeout=60)
            except requests.RequestException as e:
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)
                    continue
                raise
            if r.status_code in (200, 201):
                return r.json()
            if r.status_code in (429, 500, 503):
                wait = 2 ** attempt
                Log.warn(f"  Suno {r.status_code} on {path}; retry in {wait}s")
                time.sleep(wait)
                continue
            if r.status_code == 404:
                raise RuntimeError(f"Task not found (expired?): {path}")
            raise RuntimeError(f"Suno API error {r.status_code}: {r.text}")
        raise RuntimeError("Suno client exhausted retries")

    # --- task model ---------------------------------------------------------
    def generate(self, brief: TrackBrief, prompt: str, negative: str) -> str:
        """Submit a generate task. Returns taskId (or a fake id in test mode)."""
        if self.test_mode:
            fake = "test_" + hashlib.md5((brief.name + prompt).encode()).hexdigest()[:12]
            Log.info(f"  [test] generate {brief.name} -> {fake}")
            return fake

        payload: Dict[str, Any] = {
            "customMode": True,
            "instrumental": bool(brief.instrumental
                                 if brief.instrumental is not None
                                 else STYLE_GUIDES.get(brief.station,
                                                      STYLE_GUIDES["generic"])["instrumental_bias"]),
            "model": brief.resolved_model(),
            "prompt": prompt,
            "style": STYLE_GUIDES.get(brief.station, STYLE_GUIDES["generic"])["style"],
            "title": brief.title or brief.name,
        }
        if self.callback_url:
            payload["callBackUrl"] = self.callback_url
        if brief.vocal_gender:
            payload["vocalGender"] = brief.vocal_gender
        if brief.persona_id:
            payload["personaId"] = brief.persona_id
            payload["personaModel"] = brief.persona_model or "style_persona"
        if negative:
            payload["negativeTags"] = negative
        # Seed is not a documented generate param; we keep it for reproducible
        # re-runs at the pipeline level (filename + manifest), mirroring ElevenLabs.
        resp = self._post("/generate", payload)
        if resp.get("code") != 200:
            raise RuntimeError(f"generate failed: {resp.get('msg')} ({resp})")
        task_id = resp["data"]["taskId"]
        return task_id

    def poll(self, task_id: str) -> Dict[str, Any]:
        """Poll record-info until SUCCESS or terminal failure."""
        elapsed = 0
        last_status = None
        while elapsed < self.timeout:
            if self.test_mode:
                # Simulate a successful generation with a deterministic fake.
                return self._test_fake_record(task_id)
            data = self._get("/generate/record-info", params={"taskId": task_id})
            if data.get("code") != 200:
                raise RuntimeError(f"record-info failed: {data.get('msg')}")
            inner = data.get("data", {})
            status = inner.get("status")
            if status == "SUCCESS":
                return inner
            if status in ("CREATE_TASK_FAILED", "GENERATE_AUDIO_FAILED",
                          "CALLBACK_EXCEPTION", "SENSITIVE_WORD_ERROR"):
                msg = inner.get("errorMessage") or status
                raise RuntimeError(f"Task {task_id} FAILED: {msg}")
            # PENDING / TEXT_SUCCESS / FIRST_SUCCESS -> keep waiting
            if status != last_status:
                Log.info(f"  poll {task_id[:8]}: {status} ({elapsed}s)")
                last_status = status
            time.sleep(self.poll_interval)
            elapsed += self.poll_interval
        raise TimeoutError(f"Task {task_id} timed out after {self.timeout}s")

    def _test_fake_record(self, task_id: str) -> Dict[str, Any]:
        """Return a plausible SUCCESS record for test mode (no network)."""
        return {
            "taskId": task_id,
            "status": "SUCCESS",
            "type": "GENERATE",
            "response": {
                "sunoData": [{
                    "id": task_id + "_a",
                    "audioUrl": f"https://example.invalid/{task_id}.mp3",
                    "streamAudioUrl": f"https://example.invalid/{task_id}_s",
                    "imageUrl": f"https://example.invalid/{task_id}.jpeg",
                    "prompt": "[test] synthetic track",
                    "modelName": "chirp-v4-5",
                    "title": "Test Track",
                    "tags": "test",
                    "createTime": "2026-01-01 00:00:00",
                    "duration": 180.0,
                }]
            },
        }

    def convert_to_wav(self, audio_url: str) -> Optional[str]:
        """Best-effort WAV conversion via the endpoint; falls back to ffmpeg."""
        if self.test_mode:
            return audio_url.replace(".mp3", ".wav")  # pretend
        try:
            resp = self._post("/convert-to-wav", {"audioUrl": audio_url})
            if resp.get("code") == 200 and resp.get("data", {}).get("wavUrl"):
                return resp["data"]["wavUrl"]
        except Exception as e:  # pragma: no cover
            Log.warn(f"  convert_to_wav endpoint failed ({e}); trying ffmpeg")
        return None  # caller falls back to ffmpeg if available

    def separate_vocals(self, audio_url: str) -> Optional[Dict[str, str]]:
        """Best-effort stem separation. Returns {vocals, instrumental} or None."""
        if self.test_mode:
            return {"vocals": audio_url, "instrumental": audio_url}
        try:
            resp = self._post("/separate-vocals", {"audioUrl": audio_url})
            if resp.get("code") == 200:
                d = resp.get("data", {})
                return {
                    "vocals": d.get("vocalUrl"),
                    "instrumental": d.get("instrumentalUrl"),
                }
        except Exception as e:  # pragma: no cover
            Log.warn(f"  separate_vocals failed ({e})")
        return None

    # --- download helpers ---------------------------------------------------
    def download(self, url: str, dest: Path, force: bool = False) -> Optional[Path]:
        if dest.exists() and not force:
            return dest
        try:
            r = self.session.get(url, timeout=300, stream=True) if not self.test_mode \
                else None
        except requests.RequestException as e:
            Log.warn(f"  download failed {url}: {e}")
            return None
        if self.test_mode:
            # Write a tiny silent placeholder so downstream code has a file.
            dest.write_bytes(b"\x00\x00")  # minimal placeholder
            return dest
        if r is None or r.status_code != 200:
            Log.warn(f"  download HTTP {getattr(r, 'status_code', '?')} for {url}")
            return None
        dest.write_bytes(r.content)
        return dest

    def ffmpeg_mp3_to_wav(self, mp3: Path, wav: Path, ffmpeg_exe: str,
                          rate: str = DEFAULT_WAV_RATE) -> bool:
        if not shutil.which(ffmpeg_exe) and not Path(ffmpeg_exe).exists():
            return False
        if self.test_mode:
            # In test mode we only have the placeholder; skip real transcode.
            return False
        cmd = [ffmpeg_exe, "-y", "-i", str(mp3),
               "-ar", rate, "-ac", "2", str(wav)]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            return proc.returncode == 0 and wav.exists()
        except Exception as e:  # pragma: no cover
            Log.warn(f"  ffmpeg failed: {e}")
            return False


# ----------------------------------------------------------------------------
# Single-track orchestration
# ----------------------------------------------------------------------------

@dataclass
class RunPaths:
    work: Path
    mp3: Path
    wav: Path
    vocals: Path
    instrumental: Path
    manifest_entry: Dict[str, Any] = field(default_factory=dict)


def make_paths(output_root: Path, brief: TrackBrief) -> RunPaths:
    safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in brief.name)[:48]
    station_dir = output_root / brief.station
    station_dir.mkdir(parents=True, exist_ok=True)
    return RunPaths(
        work=station_dir,
        mp3=station_dir / f"{safe}.mp3",
        wav=station_dir / f"{safe}.wav",
        vocals=station_dir / f"{safe}_vocals.wav",
        instrumental=station_dir / f"{safe}_instr.wav",
    )


def process_one(brief: TrackBrief, client: SunoClient,
                output_root: Path, ffmpeg_exe: str,
                resume: bool) -> Dict[str, Any]:
    paths = make_paths(output_root, brief)
    entry: Dict[str, Any] = {
        "name": brief.name,
        "station": brief.station,
        "title": brief.title or brief.name,
        "model": brief.resolved_model(),
        "district": brief.district,
        "status": "pending",
    }

    # Resume support: skip if both mp3 + (wav if requested) exist
    have_mp3 = paths.mp3.exists()
    have_wav = (not brief.convert_wav) or paths.wav.exists()
    if resume and have_mp3 and have_wav:
        Log.ok(f"[skip] {brief.name} (assets exist)")
        entry.update({
            "status": "done",
            "mp3": str(paths.mp3),
            "wav": str(paths.wav) if brief.convert_wav else None,
        })
        return entry

    prompt, negative = render_prompt(
        brief.concept, STYLE_GUIDES.get(brief.station,
                                        STYLE_GUIDES["generic"])["style"],
        brief.station, brief.bpm, brief.instrumental)
    Log.step(f"[gen] {brief.name}: {prompt[:90]}...")
    try:
        task_id = client.generate(brief, prompt, negative)
        record = client.poll(task_id)
    except Exception as e:
        Log.err(f"  generation failed for {brief.name}: {e}")
        entry["status"] = "failed"
        entry["error"] = str(e)
        return entry

    suno_data = (record.get("response", {}) or {}).get("sunoData", []) or []
    if not suno_data:
        Log.err(f"  no sunoData for {brief.name}")
        entry["status"] = "failed"
        entry["error"] = "empty sunoData"
        return entry

    track = suno_data[0]
    audio_url = track.get("audioUrl") or track.get("streamAudioUrl")
    if not audio_url:
        Log.err(f"  no audio url for {brief.name}")
        entry["status"] = "failed"
        entry["error"] = "no audio url"
        return entry

    # Download MP3
    Log.step(f"[dl] {brief.name}")
    mp3 = client.download(audio_url, paths.mp3)
    if mp3 is None:
        entry["status"] = "failed"
        entry["error"] = "mp3 download failed"
        return entry

    wav_path: Optional[Path] = None
    if brief.convert_wav:
        # Try official endpoint first
        wav_url = client.convert_to_wav(audio_url)
        if wav_url:
            wav_path = client.download(wav_url, paths.wav)
        # Fallback to ffmpeg if endpoint missing/failed
        if wav_path is None:
            ok = client.ffmpeg_mp3_to_wav(paths.mp3, paths.wav, ffmpeg_exe)
            wav_path = paths.wav if ok else None

    # Optional stems
    stem_vocals = stem_instr = None
    if brief.separate_stems:
        stems = client.separate_vocals(audio_url)
        if stems:
            if stems.get("vocals"):
                stem_vocals = client.download(stems["vocals"], paths.vocals)
            if stems.get("instrumental"):
                stem_instr = client.download(stems["instrumental"], paths.instrumental)

    entry.update({
        "status": "done",
        "mp3": str(paths.mp3),
        "wav": str(paths.wav) if (brief.convert_wav and wav_path) else None,
        "wav_via_ffmpeg": (brief.convert_wav and wav_path is not None
                           and not client.test_mode),
        "stems_vocals": str(stem_vocals) if stem_vocals else None,
        "stems_instrumental": str(stem_instr) if stem_instr else None,
        "duration": track.get("duration"),
        "tags": track.get("tags"),
        "suno_model": track.get("modelName"),
        "task_id": task_id,
        "seed": brief.seed,
    })
    Log.ok(f"[done] {brief.name} -> {paths.mp3.name}"
           + (f" (+wav)" if entry.get("wav") else ""))
    return entry


# ----------------------------------------------------------------------------
# Batch driver (mirrors meshy_batch_gen.py exactly)
# ----------------------------------------------------------------------------

def _run_one_sync(brief: TrackBrief, client: SunoClient, output_root: Path,
                  ffmpeg_exe: str, resume: bool) -> Dict[str, Any]:
    return process_one(brief, client, output_root, ffmpeg_exe, resume)


def run_batch(briefs: List[TrackBrief], client: SunoClient,
              output_root: Path, workers: int, ffmpeg_exe: str,
              resume: bool) -> List[Dict[str, Any]]:
    import concurrent.futures
    sem = threading.Semaphore(workers)

    def worker(brief: TrackBrief) -> Dict[str, Any]:
        with sem:
            return _run_one_sync(brief, client, output_root, ffmpeg_exe, resume)

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        results = list(ex.map(worker, briefs))
    return results


# ----------------------------------------------------------------------------
# Manifest + UE5 metadata CSV
# ----------------------------------------------------------------------------

def write_manifest(entries: List[Dict[str, Any]], output_root: Path) -> Path:
    mpath = output_root / "manifest.json"
    mpath.write_text(json.dumps(entries, indent=2), encoding="utf-8")
    Log.ok(f"Manifest -> {mpath}")
    return mpath


def write_ue5_csv(entries: List[Dict[str, Any]], output_root: Path) -> Path:
    """UE5 Datatable-friendly CSV: Station,Track,Style,Model,BPM,Duration,
    VocalOrInstr,WavRelativePath,Mp3RelativePath,Stems."""
    cpath = output_root / "ue5_tracks.csv"
    lines = ["Station,Track,Style,Model,BPM,Duration,VocalOrInstr,"
             "WavRelativePath,Mp3RelativePath,StemsVocals,StemsInstrumental"]
    for e in entries:
        if e.get("status") != "done":
            continue
        rel_wav = os.path.relpath(e["wav"], output_root).replace("\\", "/") \
            if e.get("wav") else ""
        rel_mp3 = os.path.relpath(e["mp3"], output_root).replace("\\", "/")
        rel_v = os.path.relpath(e["stems_vocals"], output_root).replace("\\", "/") \
            if e.get("stems_vocals") else ""
        rel_i = os.path.relpath(e["stems_instrumental"], output_root).replace("\\", "/") \
            if e.get("stems_instrumental") else ""
        vocal = "vocal" if not e.get("instrumental", False) else "instrumental"
        lines.append(",".join(str(x) for x in [
            e.get("station", ""), e.get("name", ""),
            e.get("tags", ""), e.get("suno_model", e.get("model", "")),
            e.get("bpm", ""), e.get("duration", ""), vocal,
            rel_wav, rel_mp3, rel_v, rel_i,
        ]))
    cpath.write_text("\n".join(lines), encoding="utf-8")
    Log.ok(f"UE5 tracks CSV -> {cpath}")
    return cpath


# ----------------------------------------------------------------------------
# Brief loading
# ----------------------------------------------------------------------------

def load_briefs(path: Path) -> List[TrackBrief]:
    text = path.read_text(encoding="utf-8")
    if path.suffix.lower() in (".yaml", ".yml"):
        try:
            import yaml
            data = yaml.safe_load(text)
        except ImportError:
            raise RuntimeError("PyYAML required for .yaml briefs: pip install pyyaml")
    else:
        data = json.loads(text)
    if isinstance(data, dict) and "tracks" in data:
        data = data["tracks"]
    return [TrackBrief.from_dict(d) for d in data]


def make_single_brief(prompt: str, title: Optional[str],
                      station: str) -> TrackBrief:
    name = title or ("track_" + hashlib.md5(prompt.encode()).hexdigest()[:8])
    return TrackBrief(name=name, station=station, concept=prompt, title=title)


# ----------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------

def parse_args(argv: List[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="GTAI Suno -> UE5 radio music pipeline")
    p.add_argument("--prompt", help="Single text prompt to generate")
    p.add_argument("--title", help="Track title for single-prompt mode")
    p.add_argument("--station", default="generic",
                   help="Station key (PulseFM, NeonDrive, TheForum, NYCNow, "
                        "Airbrands, ClassicNY, Latido, generic)")
    p.add_argument("--input", help="JSON/YAML brief file (list of tracks)")
    p.add_argument("--output", default=None,
                   help="Output root dir (default: ./GeneratedMusic)")
    p.add_argument("--api-key", default=None,
                   help="Suno API key (else env SUNO_API_KEY)")
    p.add_argument("--test-mode", action="store_true",
                   help="Use Suno dummy key (no credits; mock responses)")
    p.add_argument("--callback-url", default=None,
                   help="Webhook URL for async completion (else polling)")
    p.add_argument("--ffmpeg", default=DEFAULT_FFMPEG,
                   help="Path to ffmpeg.exe (MP3->WAV fallback)")
    p.add_argument("--workers", type=int, default=4,
                   help="Concurrent generation tasks (respect your plan limit)")
    p.add_argument("--style", default="generic",
                   help="Default station/style guide key for --input briefs")
    p.add_argument("--resume", action="store_true",
                   help="Skip tracks whose MP3 (+WAV) already exist")
    p.add_argument("--dry-run", action="store_true",
                   help="Validate prompts/paths, do not call API")
    p.add_argument("--list-stations", action="store_true",
                   help="Print available station guides and exit")
    p.add_argument("--no-wav", action="store_true",
                   help="Skip WAV conversion (keep MP3 only)")
    return p.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    if args.list_stations:
        for k, v in STYLE_GUIDES.items():
            print(f"  {k}: model={v['model']} instrumental_bias={v['instrumental_bias']}")
        return 0

    if not args.prompt and not args.input:
        Log.err("Provide --prompt or --input <brief.json>")
        return 2

    # Resolve briefs
    if args.input:
        briefs = load_briefs(Path(args.input))
        for b in briefs:
            if not b.station or b.station == "generic":
                b.station = args.style
    else:
        b = make_single_brief(args.prompt, args.title, args.station)
        briefs = [b]

    if args.no_wav:
        for b in briefs:
            b.convert_wav = False

    Log.info(f"Loaded {len(briefs)} track brief(s)")

    output_root = Path(args.output or "GeneratedMusic").resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    if args.dry_run:
        for b in briefs:
            pr, ne = render_prompt(
                b.concept, STYLE_GUIDES.get(b.station,
                                            STYLE_GUIDES["generic"])["style"],
                b.station, b.bpm, b.instrumental)
            Log.info(f"  [{b.name}] station={b.station} model={b.resolved_model()}")
            Log.info(f"      prompt: {pr}")
            Log.info(f"      neg   : {ne}")
        Log.ok("Dry run complete — no API calls made.")
        return 0

    api_key = args.api_key or os.environ.get("SUNO_API_KEY", "")
    if not args.test_mode and not api_key:
        Log.err("No API key. Set SUNO_API_KEY env var, pass --api-key, or use --test-mode.")
        return 4

    client = SunoClient(api_key=api_key, test_mode=args.test_mode,
                        callback_url=args.callback_url)

    Log.step(f"Starting batch: {len(briefs)} tracks, {args.workers} workers"
             + (" [TEST MODE]" if args.test_mode else ""))
    entries = run_batch(
        briefs, client, output_root,
        workers=args.workers, ffmpeg_exe=args.ffmpeg, resume=args.resume,
    )

    write_manifest(entries, output_root)
    write_ue5_csv(entries, output_root)

    done = sum(1 for e in entries if e.get("status") == "done")
    failed = sum(1 for e in entries if e.get("status") == "failed")
    Log.ok(f"Pipeline complete: {done} done, {failed} failed, "
           f"{len(entries)-done-failed} pending")
    return 0


if __name__ == "__main__":
    sys.exit(main())
