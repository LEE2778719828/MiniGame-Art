#pragma once

#include "CoreMinimal.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseTypes.generated.h"

class AActor;

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
	ExitBuffer UMETA(DisplayName = "ExitBuffer"),
	Finished UMETA(DisplayName = "Finished")
};

/** One stepping stone on the track. */
USTRUCT(BlueprintType)
struct FNightStoneSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float TrackDistance = 0.f;

	/** When true, WorldLocation/YawDeg drive spawn pose instead of 1D TrackDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	bool bUseWorldPose = false; //add by K2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	FVector WorldLocation = FVector::ZeroVector; //add by K2

	/** Yaw in degrees (0.1 precision from generator). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float YawDeg = 0.f; //add by K2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	bool bHasFoe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EFoeId FoeId = EFoeId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	EIngredientId DropId = EIngredientId::F01_LingGu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 DropCount = 1;
};

/** Bridge board between two stones (art splice). */
USTRUCT(BlueprintType)
struct FNightBridgeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 FromStoneIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 ToStoneIndex = 1;

	/** 0 = BridgeMeshA, 1 = BridgeMeshB. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	int32 MeshVariant = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float YawDeg = 0.f;

	/** Scale along forward to span the stone gap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	float LengthScale = 1.f;
};

/** Fork environment flavor (maps to ForkPair + optional fog tag). */
UENUM(BlueprintType)
enum class ENightForkEnv : uint8
{
	ClearAB UMETA(DisplayName = "ClearAB"),
	FogAC UMETA(DisplayName = "FogAC"),
	ReverseBC UMETA(DisplayName = "ReverseBC"),
	Custom UMETA(DisplayName = "Custom")
};

/**
 * Authoring / HTML / JSON params for Seed-driven procedural course (G3.5).
 * Seed 0 = roll random at StartNight and write back for replay.
 */
USTRUCT(BlueprintType)
struct FNightProcCourseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 TotalNodes = 12;

	/** Max |dYaw| per step in degrees (quantized to 0.1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxYawDeltaDeg = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 ForkNodeMin = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 ForkNodeMax = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	ENightForkEnv ForkEnv = ENightForkEnv::ClearAB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	ENightForkPair ForkPair = ENightForkPair::AB;

	/** 0 = randomize at generate time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 Seed = 0;

	/** Every N nodes along the path, schedule KeySwapCountPerPeriod swaps on branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0"))
	int32 KeySwapEveryNNodes = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0"))
	int32 KeySwapCountPerPeriod = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1.0"))
	float JumpGapCm = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1.0"))
	float KillGapCm = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1.0"))
	float BranchEntryGapCm = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackBias = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 MaxSameActionStreak = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 BranchANodes = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 BranchBNodes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "1"))
	int32 BranchCNodes = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.1"))
	float ForkTimeoutSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.0"))
	float KeySwapWarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.0"))
	float KeySwapSafetySeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
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

	/** Deprecated: WrongButton never advances; Success/Miss still consume the beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAlwaysConsumeNode = false; //add by K2

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAutoSucceedWindows = false;
};
#pragma endregion K2 moonyfli
