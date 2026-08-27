# GameMode 接入 BGM（背景音乐）指南

适用：`MiniGame`（UE 5.8）。目标：让**白天关卡**和**夜晚关卡**各自播放自己的背景音乐，切关时自动停止。

---

## 0. 先搞懂四个名词

| 名词                       | 是什么                                    | 干什么用                                         |
| ------------------------ | -------------------------------------- | -------------------------------------------- |
| **SoundWave（声波）**        | 一个音频文件导进 Content 后变成的**资产**            | 存音频数据本身。你项目里已有 `/Game/Day/Music/canting_bgm` |
| **SoundBase**            | SoundWave、SoundCue、MetaSound 的**父类类型** | 代码里写 `USoundBase*`，三种资产都能塞进去，以后换类型不用改代码      |
| **AudioComponent（音频组件）** | 一个"播放器"                                | 播/停/调音量/淡出，都要通过它                             |
| **Play Sound 2D**        | 一个播放函数/蓝图节点                            | "2D" = 不挂在场景某个位置上，全图均匀响，BGM 就该用它             |

**一句话原理**：GameMode 只负责"开局时播一下、结束时停掉"，音频数据放资产里，两者靠一个引用连起来。

---

## 1. 准备音频资产（只需做一次）

### 导入

1. 把 `xxx.wav` 直接**拖进 Content Browser**（或 Import 按钮）
2. 建议放 `Content/Day/Music/`、`Content/Night/Music/`
3. 格式优先级：**wav（16bit / 44.1kHz）最稳** > ogg > mp3

### 必须改的三个设置（双击资产打开）

| 位置                       | 设置                   | 值            | 为什么                                   |
| ------------------------ | -------------------- | ------------ | ------------------------------------- |
| Details → Sound 分类       | **Looping**          | **勾选**       | 不勾就播一遍就停。你现在 `canting_bgm` 是**未勾选**状态 |
| Details → Loading 分类     | **Streaming**        | 长音乐（>10 秒）勾选 | 边播边读，不一次性全塞进内存                        |
| Details → Attenuation 分类 | Attenuation Settings | 留空           | 留空 = 2D 声音，不会有"离远了变小"的效果              |

改完点 **Save**。

> 想让循环无缝：音频首尾要能接得上（淡入淡出的音乐会有明显断点）。有明显断点时，改用 MetaSound / Sound Cue 里加 Looping 节点，或在音频软件里做无缝版。

---

## 2. 方案 A：纯蓝图，零代码（推荐先试这个）

适合：只想快点听到声音、不想动 C++。

1. 打开 `Content/Day/Blueprints/BP_DayGameMode`（父类是 `ASDayWhiteboxGameMode`）
   - 夜晚用 `Content/Night/Course/Blueprints/BP_NightCourseGameMode`
2. 事件图表 → **Event BeginPlay**
3. 拖出节点 **Play Sound 2D**（搜 "Play Sound 2D"）
   - `Sound`：选 `canting_bgm`
   - `Volume Multiplier`：`0.6`（1.0 就是原始音量，BGM 一般压到 0.4~0.7）
4. 节点返回值 `AudioComponent` → 右键 **Promote to variable**，命名 `BGMComponent`
   - 这个变量后面要用来 Stop / 淡出，别丢
5. 需要停止时：`BGMComponent` → **Fade Out**（`Fade Time = 0.8`，`Volume Level = 0`）或 **Stop**
6. **Compile + Save**

验收：打开白天关卡 → PIE → 立刻听到音乐，且**循环不停**。

---

## 3. 方案 B：C++（可调音量、可淡出、可复用，推荐长期）

在 GameMode 上加一个 `USoundBase*` 属性，音乐就变成**可在面板里换的配置项**，不用改代码。

下面以白天基类 `ASChefGameMode` 为例（`Source/MiniGame/SStandaloneSandbox.h` / `.cpp`）。夜晚 `ANightCourseGameMode` 完全同样写法。

### 3.1 头文件 — `SStandaloneSandbox.h`

类声明前补前向声明：

```cpp
class USoundBase;
class UAudioComponent;
```

`ASChefGameMode` 类内 `public:` 区域加：

```cpp
	/** 背景音乐；SoundWave / SoundCue / MetaSound 都能填。留空则不播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day|Audio")
	TObjectPtr<USoundBase> BGM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BGMVolume = 0.6f;

	/** 切关时淡出时长；0 = 立刻停。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day|Audio", meta = (ClampMin = "0.0"))
	float BGMFadeOutSeconds = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "Day|Audio", Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	UFUNCTION(BlueprintCallable, Category = "Day|Audio")
	void StartBGM();

	UFUNCTION(BlueprintCallable, Category = "Day|Audio")
	void StopBGM(bool bFadeOut = true);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
```

### 3.2 实现 — `SStandaloneSandbox.cpp`

顶部加：

```cpp
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
```

> `Kismet/GameplayStatics.h` 和 `Engine` 模块项目里已经有了，`MiniGame.Build.cs` **不需要加新模块**。

**⚠️ 关键坑**：`ASChefGameMode::BeginPlay()` 里有一个 `for (TActorIterator<ASFakeNightGateway>...)` 循环，找到已存在的 Gateway 时会 **`return` 提前退出**。`StartBGM()` 必须放在这个 return 之前，否则音乐不响。

```cpp
void ASChefGameMode::BeginPlay()
{
	Super::BeginPlay();

	StartBGM();   // ← 放在最前面，别放在函数末尾

	// ...原有的 Board / CustomerDirector / Gateway 逻辑...
}

void ASChefGameMode::StartBGM()
{
	if (!BGM)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[DayAudio] No BGM configured on '%s'; skipped."), *GetNameSafe(this));
		return;
	}

	BGMComponent = UGameplayStatics::PlaySound2D(this, BGM, BGMVolume, 1.f, 0.f);

	if (BGMComponent)
	{
		BGMComponent->bIsUISound = true;   // 不受暂停/静音外的游戏逻辑影响，切关也不拖尾
		UE_LOG(LogTemp, Display, TEXT("[DayAudio] Playing BGM='%s' volume=%.2f."), *BGM->GetPathName(), BGMVolume);
	}
}

void ASChefGameMode::StopBGM(bool bFadeOut)
{
	if (!BGMComponent)
	{
		return;
	}

	if (bFadeOut && BGMFadeOutSeconds > 0.f)
	{
		BGMComponent->FadeOut(BGMFadeOutSeconds, 0.f);
	}
	else
	{
		BGMComponent->Stop();
	}
	BGMComponent = nullptr;
}

void ASChefGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBGM(false);
	Super::EndPlay(EndPlayReason);
}
```

切夜时先淡出，在 `TravelToNight()` 里 `OpenLevel` 之前加一行：

```cpp
	StopBGM(true);   // 淡出，避免硬切爆音
	UGameplayStatics::OpenLevel(this, FName(*NightLevelPackage), true, Options);
```

### 3.3 在编辑器里配（改完代码后）

1. 编译（Live Coding `Ctrl+Alt+F11` 或重启编辑器）
2. 打开 `BP_DayGameMode` → 上方 **Class Defaults**
3. 右侧 Details → **Day | Audio** → `BGM` 选 `/Game/Day/Music/canting_bgm`
4. `BGM Volume` 设 `0.6`，`BGM Fade Out Seconds` 设 `0.8`
5. **Compile + Save**

---

## 4. 昼夜切换时的行为

| 场景                 | 结果                | 说明                                                                  |
| ------------------ | ----------------- | ------------------------------------------------------------------- |
| 白天 → 夜晚（OpenLevel） | 白天 BGM 停，夜晚 BGM 起 | 旧世界销毁，GameMode 重建，两个 GameMode 各配一首                                  |
| 想让音乐**跨关不中断**      | 现在做不到             | GameMode 会随关卡销毁。要连续就得把播放器放进 `USChefGameInstance`（GameInstance 跨关存活） |

---

## 5. 音量统一控制（以后做设置界面时用）

- **简单**：`BGMComponent->SetVolumeMultiplier(0.3f)`
- **规范**：建一个 Sound Class `SC_Music` → BGM 资产的 **Sound Class** 填它 → 设置界面里调 Sound Mix / Sound Class 音量，所有音乐一起生效

---

## 6. 常见坑与验收清单

| 症状        | 原因                                                                              |
| --------- | ------------------------------------------------------------------------------- |
| 完全没声音     | ① 忘了在 GameMode 细节面板选 BGM；② `StartBGM()` 写在 BeginPlay 的 `return` 之后；③ 系统音量/编辑器静音 |
| 播一遍就停     | SoundWave 没勾 **Looping**                                                        |
| 切关后两层音乐重叠 | 用了 `Play Sound 2D` 但没保存/停止旧 Component；用 `StopBGM` 或 `bIsUISound`                |
| 打包后没声音    | 音频没被 Cook（确认资产在 Content 内且被引用）                                                  |
| 走远了音乐变小   | 误填了 Attenuation Settings；BGM 应留空                                                |

验收：

- [ ] 白天关卡 PIE 立即出声，且循环无断点
- [ ] 结算进夜晚：白天音乐淡出，夜晚音乐起
- [ ] 夜晚回白天：同样正常
- [ ] 日志里有 `[DayAudio] Playing BGM='/Game/Day/Music/canting_bgm'`
