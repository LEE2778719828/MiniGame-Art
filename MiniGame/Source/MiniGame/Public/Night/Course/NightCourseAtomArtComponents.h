#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "NightCourseAtomArtComponents.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ENightForkLandingLane : uint8
{
	MainRoad UMETA(DisplayName = "Main Road"),
	LeftBranch UMETA(DisplayName = "Left Branch"),
	RightBranch UMETA(DisplayName = "Right Branch")
};

#pragma region K2 moonyfli
/**
 * Persistent visual authoring slot inside an Atom BP.
 *
 * The OrderIndex is the only contract shared with the planner: planner
 * actions are applied in this ordered list and do not contain art data.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightAtomLandingPointComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	int32 OrderIndex = 0;

	/**
	 * Optional editor-only preview for aligning this landing point. This is
	 * never used by the runtime course composer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing|Preview", meta = (DisplayName = "Temporary Preview Prefab"))
	TSubclassOf<AActor> LandingVisualPrefab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	FName SurfaceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing|Fork")
	ENightForkLandingLane ForkLane = ENightForkLandingLane::MainRoad;

	/**
	 * Fork Atom only: when enabled, this landing point becomes an Enemy
	 * target and the Director resolves its foe from DA_Course.FoeActorMap.
	 * Regular Atoms continue to derive enemy targets from DA_Rules actions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	bool bSpawnFoe = false;
};

/**
 * Persistent bridge visual authoring slot inside an Atom BP.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightAtomBridgeVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bridge")
	int32 FromLandingIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bridge")
	int32 ToLandingIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bridge")
	TSubclassOf<AActor> VisualPrefab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bridge")
	float LengthScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bridge")
	bool bEnabled = true;
};
#pragma endregion K2 moonyfli
