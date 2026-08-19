#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "NightCourseAtomArtComponents.generated.h"

class AActor;

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
	 * Editor-only character preview for this landing point. This is not a
	 * bridge and is not spawned by the runtime course composer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing|Preview", meta = (DisplayName = "Character Preview Prefab"))
	TSubclassOf<AActor> LandingVisualPrefab;

	/** Enemy visual used at runtime only when the planner action is Kill/Enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing|Enemy", meta = (DisplayName = "Enemy Visual Prefab"))
	TSubclassOf<AActor> FoeVisualPrefab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	FName SurfaceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing")
	bool bEnabled = true;

	/** Editor-only display choice; planner rules still own the runtime action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Landing|Preview", meta = (DisplayName = "Preview As Enemy"))
	bool bPreviewAsFoe = false;
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
