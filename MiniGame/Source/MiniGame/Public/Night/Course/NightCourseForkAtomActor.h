#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseForkAtomActor.generated.h"

class UArrowComponent;
class UBoxComponent;
class USceneComponent;
struct FNightAtomVisualBinding;
struct FNightBeatSpec;
struct FNightBridgeSpec;
struct FNightStoneSpec;

#pragma region K2 moonyfli
/**
 * Runtime/editor visual connector for the unique Night course fork.
 *
 * A Blueprint derived from this class is a complete fork presentation asset:
 * it may contain the authored fork geometry, signs and other child actors.
 * The Director owns placement and branch hand-off, and reads optional
 * LandingPoint/BridgeVisual authoring for the playable fork interior. Sign
 * and arbitrary mesh data remains owned by the Blueprint.
 */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseForkAtomActor : public AActor
{
	GENERATED_BODY()

public:
	ANightCourseForkAtomActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Fork Atom")
	TObjectPtr<USceneComponent> ForkRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Fork Atom|Anchors")
	TObjectPtr<UArrowComponent> EntryAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Fork Atom|Anchors")
	TObjectPtr<UArrowComponent> LeftExitAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Fork Atom|Anchors")
	TObjectPtr<UArrowComponent> RightExitAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Fork Atom|Bounds")
	TObjectPtr<UBoxComponent> ArtBoundsPreview;

	/**
	 * When enabled, the Blueprint-authored bounds are used for LayoutBounds
	 * checks. Composite fork assets should leave this enabled and size the
	 * bounds to include their bridge and sign geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork Atom|Bounds")
	bool bUseLocalArtBounds = true;

	/**
	 * When enabled, LandingPoint.ForkLane selects the main-road points or
	 * the branch points that belong to the active route.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork Atom|Landing")
	bool bUseLandingPointLanes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork Atom|Bounds", meta = (EditCondition = "bUseLocalArtBounds"))
	FVector LocalArtBoundsMin = FVector(-150.f, -700.f, -250.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Fork Atom|Bounds", meta = (EditCondition = "bUseLocalArtBounds"))
	FVector LocalArtBoundsMax = FVector(1300.f, 700.f, 500.f);

	UFUNCTION(BlueprintPure, Category = "Night|Fork Atom")
	FTransform GetEntryAnchorTransform() const;

	UFUNCTION(BlueprintPure, Category = "Night|Fork Atom")
	FTransform GetLeftExitAnchorTransform() const;

	UFUNCTION(BlueprintPure, Category = "Night|Fork Atom")
	FTransform GetRightExitAnchorTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Night|Fork Atom")
	bool ValidateForkAtom(FString& OutError) const;

	/**
	 * Reads LandingPoint/BridgeVisual components authored inside this fork BP.
	 * Landing points become the playable stones inside the fork; bridge exits
	 * remain the connectors for the selected external branch.
	 */
	void GetLocalCourseSpecs(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges,
		TArray<FNightAtomVisualBinding>& OutVisualBindings) const;
	void GetLocalCourseSpecs(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges,
		TArray<FNightAtomVisualBinding>& OutVisualBindings,
		ENightRouteId ActiveRoute,
		ENightRouteId LeftRoute,
		ENightRouteId RightRoute) const;

	UFUNCTION(BlueprintPure, Category = "Night|Fork Atom|Landing")
	int32 GetLandingPointCount() const;

	void GetLocalArtBounds(FVector& OutMin, FVector& OutMax) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
};
#pragma endregion K2 moonyfli
