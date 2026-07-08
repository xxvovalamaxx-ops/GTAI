# GTAI Asset Pipeline — Design Doc

> Agent: FORGE (Asset Pipeline Engineer)
> Date: 2026-07-08
> Status: Phase 1 complete (script delivered, doc written by orchestrator)

## Overview

Automated pipeline: text prompt → Meshy 6 API → GLB download → Blender headless cleanup → UE5-ready FBX with PBR textures.

## Pipeline Architecture

```
Style Guide Templates (prompt consistency)
  ↓
meshy_batch_gen.py (async, batch, resume)
  ├── 1. PROMPT    → Build consistent prompt from template
  ├── 2. GENERATE  → Meshy 6 REST API (preview → refine → remesh)
  ├── 3. DOWNLOAD  → Fetch GLB + texture maps from Meshy CDN
  ├── 4. CLEANUP   → Blender headless (import GLB, fix orientation/scale, clean materials, export FBX)
  └── 5. MANIFEST  → JSON manifest + UE5 CSV datatable for placement
```

## Key Technical Decisions

### Meshy 6 API Integration
- REST API at `https://api.meshy.ai/openapi/v2`
- Workflow: text-to-3D preview → refine → optional remesh for topology optimization
- Test mode with dummy key (no credits consumed)
- Async with configurable worker count (default 6 concurrent)

### Blender Headless Processing
- Blender 5.1 at `C:\Program Files\Blender Foundation\Blender 5.1\blender.exe`
- Runs in `--background` mode with Python script
- Fixes: orientation (Z-up for UE5), scale normalization, material cleanup
- Exports: FBX with embedded PBR textures (albedo/basecolor, normal, roughness, metallic)

### Style Guide System
- Prompt templates enforce visual consistency across all generated assets
- Templates define: art style keywords, material references, quality modifiers, negative prompts
- Example template: `"NYC {building_type}, {stories} stories, {facade_material} facade, photorealistic, PBR textures, game-ready asset"`

### Batch Management
- Input: JSON/YAML brief file with list of asset specifications
- Resume capability: interrupted runs can resume from last successful asset
- Manifest tracking: every generated asset logged with hash, prompt, status, file paths
- Concurrency: asyncio with worker pool (default 6, configurable)
- Progress tracking: per-asset status in manifest JSON

### UE5 Integration
- Output: FBX files + CSV datatable for UE5 import
- CSV format: `asset_name,fbx_path,category,scale,rotation,placement_tag`
- Importable via UE5 Asset Manager or custom C++ importer
- Nanite-enabled: meshes can be Nanite-enabled on import for automatic LOD

## Output Structure

```
AssetPipeline/
├── meshy_batch_gen.py          # Main pipeline script
├── output/
│   ├── manifest.json           # All generated assets with metadata
│   ├── ue5_import.csv          # UE5 datatable for batch import
│   ├── fbx/                    # UE5-ready FBX files
│   │   ├── nyc_brownstone_001.fbx
│   │   └── ...
│   ├── textures/               # PBR texture maps
│   └── raw_glb/                # Raw Meshy downloads (pre-Blender)
└── briefs/                     # Asset specification files
    └── nyc_buildings.json      # Example batch brief
```

## Style Guide Templates

### NYC Buildings
- Brownstone: `"NYC brownstone building, {stories} stories, red brick facade, fire escape, photorealistic, PBR"`
- Glass Tower: `"NYC glass skyscraper, {stories} stories, curtain wall facade, reflective glass, photorealistic, PBR"`
- Walk-up: `"NYC walk-up apartment, {stories} stories, brick facade, storefront ground level, photorealistic, PBR"`

### Props
- Street furniture: `"NYC {prop_type}, weathered metal, urban environment, photorealistic, PBR"`
- Vehicles: `"NYC {vehicle_type}, {color}, {era} style, game-ready, PBR"`

## Cost Estimation

| Asset Type | Meshy Credits | Time per Asset |
|------------|--------------|----------------|
| Building (text-to-3D) | ~5-10 credits | 2-5 min |
| Prop (text-to-3D) | ~3-5 credits | 1-3 min |
| Vehicle (text-to-3D) | ~5-10 credits | 2-5 min |
| Character (text-to-3D + rig) | ~10-20 credits | 5-10 min |

Free tier: 100 credits/month. Pro (~$20/mo): ~1000 credits.
One city block (~30 buildings): ~150-300 credits = 1-3 Pro months.

## Verified Artifacts

| File | Size | Status |
|------|------|--------|
| `Tools/AssetPipeline/meshy_batch_gen.py` | 911 lines, 35KB | Complete — async batch, Meshy API, Blender headless, resume, style templates, UE5 datatable output |

## Next Steps

1. Set up Meshy API key (user needs to create account)
2. Create batch brief for Phase 1 assets (30 buildings, 20 props, 3 vehicles)
3. Run first batch generation
4. Test UE5 import pipeline with generated FBX files
5. Integrate with ATLAS city data — place assets at building footprint locations
6. Set up automated import script in UE5 C++