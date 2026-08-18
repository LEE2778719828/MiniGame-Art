# NightProcCourseParams JSON Schema（UE / HTML 共用）

对应 C++：`FNightProcCourseParams`、`UNightProcParamsAsset`。

设计器：`MiniGame/Tools/NightCourseDesigner.html`
导入：`Night.Course.ImportParams <path.json>`

## Params-only（推荐日常）

```json
{
  "totalNodes": 12,
  "maxYawDeltaDeg": 8.0,
  "forkNodeMin": 4,
  "forkNodeMax": 8,
  "forkEnv": "ClearAB",
  "forkPair": "AB",
  "seed": 1001,
  "keySwapEveryNNodes": 10,
  "keySwapCountPerPeriod": 1,
  "jumpGapCm": 420,
  "killGapCm": 160,
  "branchEntryGapCm": 280,
  "attackBias": 0.45,
  "maxSameActionStreak": 2,
  "branchANodes": 4,
  "branchBNodes": 5,
  "branchCNodes": 6,
  "forkTimeoutSeconds": 2.4,
  "keySwapWarningSeconds": 0.8,
  "keySwapSafetySeconds": 0.6,
  "bridgeMeshAWeight": 0.55,
  "startingSoul": 100,
  "wrongPenalty": 7,
  "enableProcGenerator": true,
  "previewOnly": false
}
```

| 字段 | 含义 |
|---|---|
| `totalNodes` | 基础段目标石数预算（岔口抽在区间内） |
| `maxYawDeltaDeg` | 每步最大 \|dYaw\|，0.1° 量化 |
| `forkNodeMin/Max` | 岔口石索引闭区间 |
| `forkEnv` | `ClearAB` / `FogAC` / `ReverseBC` / `Custom` |
| `forkPair` | Custom 或覆盖用 `AB`/`AC`/`BC` |
| `seed` | `0` = 运行时随机并写回 |
| `keySwapEveryNNodes` | 分支上每 N 拍安排换键 |
| `keySwapCountPerPeriod` | 每个周期安排几次 |
| `attackBias` | 砍拍概率 0–1 |
| `maxSameActionStreak` | 同类动作连续上限 |
| `branchA/B/CNodes` | 各路分支拍子/节点数 |

## Baked（验收 HTML↔UE）

在 Params 基础上附加：

- `stones[]`：`x,y,z,yawDeg,trackDistance,hasFoe,useWorldPose`
- `beats[]`：`from,to,action`（`Jump`|`Attack`）
- `bridges[]`：`from,to,yawDeg,lengthScale,meshVariant,x,y,z`

导入后 `bPreferBakedCourse=true`，跳过再随机。

## 岔路环境映射

| forkEnv | ForkPair |
|---|---|
| ClearAB | AB |
| FogAC | AC |
| ReverseBC | BC |
| Custom | 使用 `forkPair` |

## RNG

UE 与 HTML 均使用 **FRandomStream** LCG：

`Seed = Seed * 196314165 + 907633515`

同 Seed + 同 Params 生成同轨迹（params-only 模式）。
