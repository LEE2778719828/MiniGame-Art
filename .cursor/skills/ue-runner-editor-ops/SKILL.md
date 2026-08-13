---
name: ue-runner-editor-ops
description: >-
  UE editor click-paths for MiniGame night runner. Prefer R2 Course paths under
  /Game/Night/Course. Use for Input/BP/DataAsset setup and Nexus asset edits.
---

# MiniGame Editor Ops

## Split reminder

- R1 ≈ 3C (`/Game/Night/Feel/`)
- R2 ≈ Gameplay (`/Game/Night/Course/`) — primary for this skill when editing night course
- S ≈ Shop (`/Game/Day/`)

## Specs

- `Docs/R2_Gameplay_Plan.md`
- `Docs/FullFeature_ThreeDev_Split.md`
- `Docs/G1_Course_Usage.md` — **刃心石头链**（落脚石 / 空→跳 / 带怪→砍）
- `.cursor/rules/blade-heart-stone-chain.mdc`

## Stone-chain whitebox（do not regress）

- Course = sequence of **stones**, not per-beat dual-pad gadgets.
- Example: stone1 — gap — stone2 — stone3+foe → Jump on 1, Attack on 2, foe clears on 3.
- No continuous road mesh; only stepping pads + optional foe capsule on target stone.

## Communication

Numbered menu paths; exact `/Game/Night/Course/...` asset names; Nexus for property edits when connected.

## Forbidden

- Stick locomotion as core loop
- Mid-platform Meta APIs beyond Bootstrap/Result
