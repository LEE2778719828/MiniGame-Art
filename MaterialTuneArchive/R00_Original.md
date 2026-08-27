# Night post-process material tuning archive

## Round 00 — original state

- Date: 2026-08-27 (Asia/Shanghai)
- Scope: read-only baseline; no material or DA values changed.
- Baseline screenshot: `MaterialTuneArchive_R00_Baseline.png`
- MCP snapshots:
  - `NightMaterialTune_R00_Default`
  - `NightMaterialTune_R00_ForkA`
  - `NightMaterialTune_R00_ForkB`
  - `NightMaterialTune_R00_ForkC`

## Route mapping

- Default: reference image 1, cyan daylight.
- ForkA: reference image 2, deep-blue night.
- ForkB: reference image 3, red atmosphere.
- ForkC: reference image 4, gray-white fog.

## Initial observation

The four material instances currently have the same scalar and vector overrides. They are separate assets, so they can be tuned independently in later rounds.
