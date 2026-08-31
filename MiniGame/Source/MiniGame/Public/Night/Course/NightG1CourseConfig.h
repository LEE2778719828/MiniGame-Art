#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightG1CourseConfig.generated.h"

class ANightCoursePawn;
class ANightCourseStoneActor;
class ANightCourseForkAtomActor;
class UNightCourseAtomRouteData;
class UNightCourseRuleData;
class UNightCourseQueueData;
class UNightRouteRulesAsset;
class ANightRoadsideSegmentActor;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct MINIGAME_API FNightLevelCourseRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level", meta = (DisplayName = "关卡ID", ToolTip = "仅用于本配置的关卡标识；运行时的路线和岔路组合由 Day 的 DT_GameStages 传入。"))
	ENightLevelId LevelId = ENightLevelId::T0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level", meta = (DisplayName = "使用KeySwap提示", ToolTip = "开启后可为该关卡覆盖 KeySwap 提示节奏；不会覆盖 Day 传入的路线和岔路组合。"))
	bool bUseKeySwapCues = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level", meta = (DisplayName = "关卡KeySwap提示", ToolTip = "仅在上方开关开启时使用的该关卡 KeySwap 提示列表。"))
	TArray<FNightKeySwapCue> KeySwapCues;
};

#pragma region K2 moonyfli
/**
 * Builds a 刃心 stone chain: stones + beats (Jump across gap / Attack into foe stone).
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightG1CourseConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout", meta = (DisplayName = "前进速度", ToolTip = "角色沿课程前进的速度，单位 cm/s。"))
	float AdvanceSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "岔口过渡速度", ToolTip = "分支入口推移（前缀石到分支首石）的速度，单位 cm/s；绕过动画驱动的距离压缩，避免瞬移。"))
	float ForkTransitionAdvanceSpeed = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Layout", meta = (DisplayName = "终点缓冲时间", ToolTip = "完成最后一个节拍后等待多久结算成功，单位秒。"))
	float ExitBufferSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Space", meta = (DisplayName = "轨道起点", ToolTip = "无 WorldPose 时课程轨道的起点；通常保持为 0。"))
	FVector TrackOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Space", meta = (DisplayName = "轨道方向", ToolTip = "无 WorldPose 时课程轨道的前进方向；必须是有效非零向量。"))
	FVector TrackForward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "错误惩罚", ToolTip = "输入错误时扣除的灵魂值。"))
	float WrongPenalty = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "漏判惩罚", ToolTip = "节拍超时或漏判时扣除的灵魂值。"))
	float MissPenalty = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "初始灵魂", ToolTip = "Night 开始时的灵魂值。"))
	float StartingSoul = 100.f;

	/** Multiplier applied to the authored continuous soul drain on a selected Fork route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (ClampMin = "0.0"))
	float ForkSoulDrainScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "默认掉落ID", ToolTip = "未命中敌人掉落映射时的兼容回退食材 ID。正式 M01-M05 掉落优先使用 FoeDropMap。"))
	EIngredientId DefaultDropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "默认掉落数量", ToolTip = "每次敌人击杀的基础掉落数量；命中 FoeDropMap 时固定使用此数量，再叠加路线规则倍率。"))
	int32 DefaultDropCount = 1;

	/**
	 * When enabled, every successful Enemy beat grants an ingredient.
	 * When disabled, branch DropRhythmEveryN/DropCycle rules may limit drops.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "每次击杀掉落", ToolTip = "开启后每个成功击杀都产生掉落；关闭后由 DA_RouteRules 的节奏字段限制。"))
	bool bDropIngredientOnEveryEnemyKill = true;

	/** Repeated ingredient IDs act as weights for random enemy drops. Empty uses all Day ingredients. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "兼容随机掉落池", ToolTip = "仅在对应 FoeDropMap 未配置时使用；列表中重复的食材 ID 会提高随机权重。"))
	TArray<EIngredientId> IngredientDropPool;

	/** When enabled, authored landing-point drop IDs are replaced by a deterministic random pool pick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "启用兼容随机掉落", ToolTip = "仅对未配置 FoeDropMap 的敌人启用随机掉落；M01-M05 正式映射不会被覆盖。"))
	bool bRandomizeEnemyDrops = true;

	/** Repeated IDs act as weights for procedural enemy selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "敌人权重池", ToolTip = "敌人类型随机池；重复的 EFoeId 会提高出现概率。为空时使用 DefaultFoeId。"))
	TArray<EFoeId> FoeWeightPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "默认敌人ID", ToolTip = "敌人权重池为空或无效时使用的敌人 ID，必须存在于 FoeActorMap。"))
	EFoeId DefaultFoeId = EFoeId::M01;

	/** Explicit runtime actor class for each logical foe ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Combat", meta = (DisplayName = "敌人Actor映射", ToolTip = "运行时生成敌人唯一使用的映射：M01-M05 → 对应敌人 Blueprint 类。不要把预览 Atom BP 填到这里。"))
	TMap<EFoeId, TSoftClassPtr<ANightCourseStoneActor>> FoeActorMap;

	/** Fixed ingredient awarded when each logical foe is defeated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Drops", meta = (DisplayName = "敌人掉落映射", ToolTip = "为 M01-M05 分别绑定一个食材 ID。命中后掉落 ID 固定由这里决定，不会被随机掉落池或路线 DropCycle 替换。"))
	TMap<EFoeId, EIngredientId> FoeDropMap;

	/** Optional complete visual/structural Atom for each AB/AC/BC fork pair. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork|Art", meta = (DisplayName = "特殊岔路Atom映射", ToolTip = "为 AB、AC、BC 分别绑定完整岔路 Blueprint；Blueprint 应包含入口、左右出口、桥、路牌和装饰。"))
	TMap<ENightForkPair, TSoftClassPtr<ANightCourseForkAtomActor>> ForkAtomMap;

	/** Pair shown by the editor preview when no runtime Bootstrap is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork|Art", meta = (DisplayName = "预览岔路组合", ToolTip = "编辑器预览没有 Day Bootstrap 时显示的特殊岔路组合；运行时由 DT_GameStages.ForkPair 决定。"))
	ENightForkPair PreviewForkPair = ENightForkPair::AB;

	/** Additional fixed world-Z offset applied to every generated house. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Roadside|House", meta = (DisplayName = "房屋固定Z偏移", ToolTip = "在房屋道路基准高度上增加的统一 Z 偏移，单位 cm；想让所有房屋同高时只调整这个值。"))
	float HouseInitialZOffsetCm = 0.f;

	/** Extra longitudinal tolerance outside the authored fork bounds where houses are not generated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Roadside|House", meta = (DisplayName = "岔路Bounds房屋容忍度", ToolTip = "以 Fork Atom 实际 ArtBounds 的前后边界为基准，向外额外停止生成房屋的距离，单位 cm；建议 200-400。", ClampMin = "0.0"))
	float ForkHouseExclusionCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Roadside|House", meta = (DisplayName = "房屋生成设置", ToolTip = "房屋独立生成设置，可配置多个 Blueprint、间距、左右偏移和随机种子。房屋沿固定世界 X 轴排布。"))
	FNightRoadsideGenerationSettings HouseRoadside;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Roadside|Pole", meta = (DisplayName = "杆子生成设置", ToolTip = "杆子独立生成设置，可配置多个 Blueprint、间距、左右偏移和随机性；杆子跟随道路/桥方向。"))
	FNightRoadsideGenerationSettings PoleRoadside;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G3.5", meta = (DisplayName = "启用岔路", ToolTip = "开启后在所选 A/B/C 主路线队列末端生成特殊岔路并等待玩家选择。"))
	bool bEnableFork = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "岔路选择超时", ToolTip = "玩家未选择时等待的秒数；超时后按 bForkTimeoutPickLeft 自动选边。"))
	float ForkTimeoutSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "超时选择左路", ToolTip = "岔路选择超时后是否自动选择左侧分支。"))
	bool bForkTimeoutPickLeft = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "进入分支缓冲时间", ToolTip = "选路后角色进入分支前的缓冲时间，单位秒。"))
	float BranchEnterBufferSeconds = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "分支入口跳跃间距", ToolTip = "特殊岔路出口到首个分支 Atom 的衔接跳跃距离，单位 cm。"))
	float BranchEntryGapCm = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Post Process", meta = (DisplayName = "默认跑酷后处理材质实例", ToolTip = "进入跑酷时使用的基础后处理材质实例；分支路线没有单独配置时也回退到这里。"))
	TObjectPtr<UMaterialInterface> DefaultPostProcessMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Post Process", meta = (DisplayName = "后处理体积Actor Tag", ToolTip = "非空时只修改带此 Actor Tag 的 PostProcessVolume；为空时使用关卡中的第一个 Unbound PostProcessVolume。"))
	FName PostProcessVolumeTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Post Process", meta = (DisplayName = "后处理材质权重", ToolTip = "默认和分支后处理材质加入 PostProcessVolume 时使用的混合权重。", ClampMin = "0.0"))
	float PostProcessMaterialWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "启用运行时动态装卸", ToolTip = "开启后只生成玩家附近课程 Actor，远离玩家且已经走过的 Actor 会 Destroy；关闭后恢复整条课程一次性生成。"))
	bool bEnableRuntimeActorStreaming = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "前方预生成距离", ToolTip = "主路运行时玩家前方预先生成的石块数量；数量越大越平滑但占用更多内存。", ClampMin = "1"))
	int32 RuntimeSpawnAheadStoneCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "兼容身后保留石块数", ToolTip = "旧版按石块数量卸载的兼容字段；运行时改用下面的身后 Actor 卸载距离。", ClampMin = "0"))
	int32 RuntimeUnloadBehindStoneCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "身后Actor卸载距离", ToolTip = "已经走过的课程 Actor 超出该纵向距离后立即 Destroy，单位 cm。", ClampMin = "0.0"))
	float RuntimeUnloadBehindDistanceCm = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "Roadside可见距离", ToolTip = "Roadside Actor 使用的独立前后可见和生成距离，单位 cm。", ClampMin = "1.0"))
	float RuntimeRoadsideVisibleDistanceCm = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Runtime|Streaming", meta = (DisplayName = "岔路逐批生成数量", ToolTip = "选择岔路后每次只向前生成这么多石块；1 表示逐个生成，用于降低选路瞬间卡顿。", ClampMin = "1"))
	int32 BranchSpawnBatchSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "岔路前Atom数量覆盖", ToolTip = "岔路前生成多少个 Atom；填 -1 使用规则中的默认路线模式队列完整数量。"))
	int32 ForkAfterBaseAtomIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Fork", meta = (DisplayName = "预览分支路线", ToolTip = "编辑器预览选定的分支路线；None 表示只预览默认主路线。运行时由玩家选择路线。"))
	ENightRouteId PreviewRoute = ENightRouteId::None;

	/** Route mode used for the preview's pre-fork main segment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Route", meta = (DisplayName = "预览默认路线模式", ToolTip = "编辑器预览使用的岔路前主段模式；运行时由 Day 的 DefaultRoute 决定，默认 A。"))
	ENightRouteId PreviewDefaultRoute = ENightRouteId::A;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "启用KeySwap", ToolTip = "是否启用 Night 中的按键交换提示。"))
	bool bEnableKeySwap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "仅C路触发KeySwap", ToolTip = "开启后只有选择 C 路时使用 KeySwap 提示。"))
	bool bKeySwapOnlyOnRouteC = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "默认KeySwap警告时间", ToolTip = "没有单独 Cue 时的警告持续时间，单位秒。"))
	float DefaultKeySwapWarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "默认KeySwap安全时间", ToolTip = "KeySwap 后保持安全状态的时间，单位秒。"))
	float DefaultKeySwapSafetySeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "KeySwap提示列表", ToolTip = "按分支节拍序号配置 KeySwap 的警告和安全时间。"))
	TArray<FNightKeySwapCue> KeySwapCues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|KeySwap", meta = (DisplayName = "进入C路立即反转左右", ToolTip = "选择 C 路后立刻交换左右区域的跳/砍。与下面的 KeySwap 节拍列表无关；列表为空时也靠这项生效。"))
	bool bSwapControlsOnEnterRouteC = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (DisplayName = "启用课程Tips", ToolTip = "关闭后不弹出岔路/C路 K易斯提示，也不因此暂停。自动化测试应关闭。"))
	bool bEnableCourseTips = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (DisplayName = "启用岔路Tips", ToolTip = "本局第一次进入岔路选择时弹出提示。"))
	bool bEnableForkTip = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (DisplayName = "启用C路反转Tips", ToolTip = "进入 C 路后弹出左右操作反转提示。"))
	bool bEnableRouteCTip = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (DisplayName = "Tips说话人", ToolTip = "K易斯提示上的名字。"))
	FText CourseTipSpeakerName = INVTEXT("K易斯");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (MultiLine = "true", DisplayName = "第一次岔路文字", ToolTip = "第一次触发岔路时显示的提示。"))
	FText ForkTipText = INVTEXT("注意，这里就是岔路了，按左边选择左路，按右边选择右路");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (MultiLine = "true", DisplayName = "C路反转文字", ToolTip = "进入 C 路后显示的提示。"))
	FText RouteCTipText = INVTEXT("注意！你吸入了毒孢子，现在左右区域对应的操作反转了！");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Tips", meta = (DisplayName = "Tips继续提示", ToolTip = "提示底部的点击继续说明。"))
	FText CourseTipContinueHint = INVTEXT("点击继续");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Gift", meta = (DisplayName = "饕餮箱敌人覆盖数量", ToolTip = "饕餮箱效果生效时，前多少个敌人使用 Gift 配置的敌人池；单位为敌人数。", ClampMin = "0"))
	int32 TaotieFoeOverrideCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Level", meta = (DisplayName = "关卡KeySwap覆盖", ToolTip = "按关卡 ID 配置可选的 KeySwap 覆盖；路线和岔路组合不要在这里配置。"))
	TArray<FNightLevelCourseRule> LevelRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art|Classes", meta = (DisplayName = "主角Blueprint类", ToolTip = "Night 运行时使用的主角 Blueprint 类；应包含可移动视觉和碰撞。"))
	TSubclassOf<ANightCoursePawn> HeroClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Route", meta = (DisplayName = "Atom库", ToolTip = "AtomKey 到 Atom Blueprint 的唯一映射；用于主路线和分支路线的实际布局。"))
	TObjectPtr<UNightCourseAtomRouteData> AtomRoute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Route", meta = (DisplayName = "路线规则DA", ToolTip = "配置 RouteModes、BranchRoutes 与 Atom 模板；随机种子由课程队列DA提供。"))
	TObjectPtr<UNightCourseRuleData> CourseRuleData;

	/** Ordered Day -> Night plan. Each entry controls route, fork, atom counts and the deterministic seed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Course Queue", meta = (DisplayName = "课程队列DA", ToolTip = "绑定 DA_Queue；每次成功完成 Night 后推进到下一条，失败重试保持当前条目。"))
	TObjectPtr<UNightCourseQueueData> CourseQueueData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Route Rules", meta = (DisplayName = "分支节奏规则DA", ToolTip = "配置 A/B/C 分支的可见区块、掉落节奏、倍率和结算加成。"))
	TObjectPtr<UNightRouteRulesAsset> RouteRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|G1|Debug", meta = (DisplayName = "调试覆盖", ToolTip = "仅调试时覆盖默认参数；正式玩法通常保持关闭。"))
	FNightG1DebugSettings Debug;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|G1|Editor")
	void MarkPackageDirtyForEditor();

	UClass* ResolveFoeActorClass(EFoeId FoeId, FString& OutError) const;
	bool ValidateFoeActorMap(FString& OutError) const;
	bool TryGetFoeDropId(EFoeId FoeId, EIngredientId& OutDropId) const;
	bool ValidateFoeDropMap(FString& OutError) const;
	UClass* ResolveForkAtomClass(ENightForkPair ForkPair, FString& OutError) const;
	bool ValidateForkAtomMap(FString& OutError) const;
	bool ValidateRoadsideConfiguration(FString& OutError) const;
};
#pragma endregion K2 moonyfli
