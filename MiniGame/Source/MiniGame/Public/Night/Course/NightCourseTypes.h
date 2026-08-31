#pragma once

#include "CoreMinimal.h"
#include "Night/Shared/NightSharedTypes.h"
#include "Night/Course/NightCourseForkAtomActor.h"
#include "NightCourseTypes.generated.h"

class AActor;
class ANightRoadsideSegmentActor;

#pragma region K2 moonyfli
UENUM(BlueprintType)
enum class ENightNodeKind : uint8
{
	None UMETA(DisplayName = "None"),
	/** Attack beat (next stone has foe). */
	Enemy UMETA(DisplayName = "Attack"),
	/** Jump beat (gap to next stone). */
	Hazard UMETA(DisplayName = "Jump")
};

UENUM(BlueprintType)
enum class ENightJudgeOutcome : uint8
{
	None UMETA(DisplayName = "None"),
	Success UMETA(DisplayName = "Success"),
	WrongButton UMETA(DisplayName = "WrongButton"),
	Miss UMETA(DisplayName = "Miss")
};

UENUM(BlueprintType)
enum class ENightCoursePhase : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	BaseSegment UMETA(DisplayName = "BaseSegment"),
	ForkChoice UMETA(DisplayName = "ForkChoice"),
	BranchEnterBuffer UMETA(DisplayName = "BranchEnterBuffer"),
	BranchSegment UMETA(DisplayName = "BranchSegment"),
	KeySwapWarning UMETA(DisplayName = "KeySwapWarning"),
	KeySwapSafetyHold UMETA(DisplayName = "KeySwapSafetyHold"),
	ExitBuffer UMETA(DisplayName = "ExitBuffer"),
	Failed UMETA(DisplayName = "Failed"),
	Finished UMETA(DisplayName = "Finished")
};

UENUM(BlueprintType)
enum class ENightCourseTipId : uint8
{
	None UMETA(DisplayName = "无"),
	Fork UMETA(DisplayName = "第一次岔路"),
	RouteC UMETA(DisplayName = "C路反转")
};

/** One stepping stone on the track. */
USTRUCT(BlueprintType)
struct FNightStoneSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float TrackDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	bool bUseWorldPose = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float YawDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	bool bHasFoe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EFoeId FoeId = EFoeId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EIngredientId DropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 DropCount = 1;

	/**
	 * Runtime fork preview connector only. These stones are rendered on both
	 * fork exits before route selection, but are not part of the active beat
	 * chain and are replaced by the selected branch during hand-off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Fork")
	bool bForkConnectorVisualOnly = false;
};

/**
 * Action while standing on FromStone to reach ToStone.
 * Jump if gap; Attack if ToStone has foe.
 */
USTRUCT(BlueprintType)
struct FNightBeatSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 FromStoneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 ToStoneIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	ENightNodeKind Action = ENightNodeKind::Hazard;
};

USTRUCT(BlueprintType)
struct FNightBridgeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	int32 FromStoneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	int32 ToStoneIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	int32 MeshVariant = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	float YawDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge")
	float LengthScale = 1.f;

	/** Bridge belonging to a pre-choice fork connector Atom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Bridge|Fork")
	bool bForkConnectorVisualOnly = false;
};

/** One resolved special fork Atom placed at the end of the base route. */
USTRUCT(BlueprintType)
struct MINIGAME_API FNightForkAtomSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork")
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork")
	TSubclassOf<ANightCourseForkAtomActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork")
	FTransform LeftExitTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork")
	FTransform RightExitTransform = FTransform::Identity;
};

UENUM(BlueprintType)
enum class ENightRoadsideKind : uint8
{
	House UMETA(DisplayName = "House"),
	Pole UMETA(DisplayName = "Pole")
};

/** One weighted Blueprint candidate for a roadside decoration category. */
USTRUCT(BlueprintType)
struct FNightRoadsideBlueprintEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside", meta = (DisplayName = "装饰Blueprint", ToolTip = "道路装饰 Blueprint；必须继承 ANightRoadsideSegmentActor。"))
	TSoftClassPtr<ANightRoadsideSegmentActor> Blueprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside", meta = (DisplayName = "选择权重", ToolTip = "多个装饰 Blueprint 的相对选择概率；重复配置效果等同于提高权重。", ClampMin = "0.0"))
	float Weight = 1.f;
};

/**
 * Generation settings shared by one roadside category.
 *
 * SpacingCm is the gap after a segment's End marker. A zero value makes a
 * house row continuous when the adjacent markers are aligned. All roadside
 * placement samples the Bounds-translated composed path. Houses retain fixed
 * world-X orientation and world-Y side offsets; poles use sampled direction
 * and height. Houses near a fork are excluded by the configured tolerance.
 */
USTRUCT(BlueprintType)
struct FNightRoadsideGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside", meta = (DisplayName = "启用生成", ToolTip = "是否生成此类道路装饰。"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside", meta = (DisplayName = "Blueprint候选池", ToolTip = "可配置多个道路装饰 Blueprint；运行时按权重和 Seed 选择。"))
	TArray<FNightRoadsideBlueprintEntry> BlueprintPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside", meta = (DisplayName = "装饰间距", ToolTip = "一个模块 End 到下一个模块 Start 的间距，单位 cm；房屋填 0 可连续拼接。", ClampMin = "0.0"))
	float SpacingCm = 0.f;

	/**
	 * Positive lateral magnitudes. Houses and poles use the sampled
	 * Bounds-translated bridge/track centerline as their lateral baseline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside|Offset", meta = (DisplayName = "左侧道路偏移", ToolTip = "左侧装饰相对道路/桥基准线的距离，单位 cm。", ClampMin = "0.0"))
	float LeftBridgeOffsetCm = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside|Offset", meta = (DisplayName = "右侧道路偏移", ToolTip = "右侧装饰相对道路/桥基准线的距离，单位 cm。", ClampMin = "0.0"))
	float RightBridgeOffsetCm = 350.f;

	/** Applied on top of the category's fixed/sampled world Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside|Offset", meta = (DisplayName = "Z偏移", ToolTip = "叠加到房屋固定高度或杆子采样高度上的偏移，单位 cm。"))
	float ZOffsetCm = 0.f;

	/**
	 * Used for poles and other non-continuous decorations. House rows keep
	 * their marker chain aligned so this does not create seams.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside|Random", meta = (DisplayName = "随机Yaw范围", ToolTip = "装饰随机旋转范围，单位度；房屋连续排布不改变首尾对齐。", ClampMin = "0.0"))
	float RandomYawRangeDeg = 0.f;

	/** Keeps roadside random streams independent from course/foe/drop random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside|Random", meta = (DisplayName = "装饰随机种子偏移", ToolTip = "只影响道路装饰随机选择，不影响 Atom、敌人和掉落。"))
	int32 RandomSeedOffset = 0;
};

/** One resolved roadside actor placement for the current composed route. */
USTRUCT(BlueprintType)
struct FNightRoadsidePropSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	ENightRoadsideKind Kind = ENightRoadsideKind::House;

	/** -1 = left side, +1 = right side; right-side actors are mirrored on Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	int32 Side = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	int32 PathSegmentIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	int32 FromStoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	int32 ToStoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	float DistanceAlongPath = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	TSubclassOf<ANightRoadsideSegmentActor> PropClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Roadside")
	FTransform WorldTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FNightProcCourseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 TotalNodes = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float MaxYawDeltaDeg = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 ForkNodeMin = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 ForkNodeMax = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	ENightForkEnv ForkEnv = ENightForkEnv::ClearAB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 Seed = 1001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 KeySwapEveryNNodes = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 KeySwapCountPerPeriod = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float JumpGapCm = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float KillGapCm = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float BranchEntryGapCm = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float AttackBias = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 MaxSameActionStreak = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 BranchANodes = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 BranchBNodes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 BranchCNodes = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float ForkTimeoutSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float KeySwapWarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float KeySwapSafetySeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float BridgeMeshAWeight = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float StartingSoul = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float WrongPenalty = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	bool bEnableProcGenerator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	bool bPreviewOnly = false;
};

/** @deprecated Prefer FNightStoneSpec / FNightBeatSpec. Kept for transitional Feel payloads. */
USTRUCT(BlueprintType)
struct FNightTrackNodeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	ENightNodeKind Kind = ENightNodeKind::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float JudgeTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float TrackDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EFoeId FoeId = EFoeId::M01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EIngredientId DropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 DropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	FName ArtTag = NAME_None;
};

USTRUCT(BlueprintType)
struct FNightJudgeRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 NodeIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	ENightNodeKind Kind = ENightNodeKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float WindowOpenTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float WindowCloseTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EFoeId FoeId = EFoeId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TObjectPtr<AActor> NodeActor = nullptr;
};

USTRUCT(BlueprintType)
struct FNightG1DebugSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bLogEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bUseBuiltInFeelStub = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAlwaysConsumeNode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAutoSucceedWindows = false;
};
#pragma endregion K2 moonyfli
