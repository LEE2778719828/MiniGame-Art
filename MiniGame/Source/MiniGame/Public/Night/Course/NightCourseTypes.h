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
	Enemy UMETA(DisplayName = "Enemy"),
	Hazard UMETA(DisplayName = "Hazard")
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

	/** G1 soak test: auto Success when a window opens (no input needed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAutoSucceedWindows = false;
};
#pragma endregion K2 moonyfli
