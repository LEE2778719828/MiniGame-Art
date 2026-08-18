# NightProcCourseParams JSON 字段说明

这是 HTML 设计器和 UE 导入器共用的 JSON 字段说明。

## Params JSON 示例

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

## 字段解释

| 字段 | 类型 | 含义 |
|---|---|---|
| `totalNodes` | 整数 | 基础段规模；分支节点数另计 |
| `maxYawDeltaDeg` | 小数 | 每一步允许的最大 Yaw 变化，精度 0.1° |
| `forkNodeMin` | 整数 | 岔口抽取区间下限，包含该节点 |
| `forkNodeMax` | 整数 | 岔口抽取区间上限，包含该节点 |
| `forkEnv` | 字符串 | `ClearAB`、`FogAC`、`ReverseBC` 或 `Custom` |
| `forkPair` | 字符串 | `AB`、`AC` 或 `BC`；Custom 时使用 |
| `seed` | 整数 | 0 表示随机；固定数字保证复现 |
| `keySwapEveryNNodes` | 整数 | 分支每 N 个节点触发换键计划 |
| `keySwapCountPerPeriod` | 整数 | 每个周期触发的换键次数 |
| `jumpGapCm` | 小数 | Jump 空档间距，单位 cm |
| `killGapCm` | 小数 | Attack 目标石间距，单位 cm |
| `branchEntryGapCm` | 小数 | 岔口进入分支的间距，单位 cm |
| `attackBias` | 小数 | Attack 概率，范围 0–1 |
| `maxSameActionStreak` | 整数 | 连续 Jump 或 Attack 的最大次数 |
| `branchANodes` | 整数 | A 路节点数 |
| `branchBNodes` | 整数 | B 路节点数 |
| `branchCNodes` | 整数 | C 路节点数 |
| `forkTimeoutSeconds` | 小数 | 岔口选择超时时间 |
| `keySwapWarningSeconds` | 小数 | 换键预警时长 |
| `keySwapSafetySeconds` | 小数 | 换键安全停顿时长 |
| `bridgeMeshAWeight` | 小数 | 桥模型 A 的随机使用权重 |
| `startingSoul` | 小数 | 初始灶魂 |
| `wrongPenalty` | 小数 | 错键扣魂 |
| `enableProcGenerator` | 布尔 | 是否启用程序化生成 |
| `previewOnly` | 布尔 | 是否只预览 |

## Baked JSON 附加字段

导出 Baked Course JSON 时，在上述字段基础上增加：

- `stones[]`：每块石头的坐标、Yaw、弧长和是否带怪；
- `beats[]`：每个节点要求 Jump 还是 Attack；
- `bridges[]`：每段桥的起点、终点、Yaw、长度比例和模型变体；
- `forkAfterStoneIndex`：实际岔口石索引；
- `resolvedSeed`：实际使用的 Seed。

## 路线映射

| `forkEnv` | `forkPair` | 说明 |
|---|---|---|
| `ClearAB` | AB | 清途 / 瘴途 |
| `FogAC` | AC | 清途 / 倒途 |
| `ReverseBC` | BC | 瘴途 / 倒途 |
| `Custom` | 使用填写的 `forkPair` | 自定义组合 |

## 复现规则

同一组 Params、同一个 Seed、同一个预览分支，应得到同一条预览轨迹。

如果 Seed 填 0 或留空，工具会生成随机 Seed；提交时必须把页面显示的实际 Seed 一并记录。
