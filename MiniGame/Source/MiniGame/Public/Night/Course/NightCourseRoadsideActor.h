#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightCourseRoadsideActor.generated.h"

class UArrowComponent;
class USceneComponent;

/**
 * Base actor contract for a modular roadside segment.
 *
 * Blueprint children own their meshes, animations, materials and scale. The
 * Director only uses the two marker locations to place consecutive segments.
 */
UCLASS(Blueprintable)
class MINIGAME_API ANightRoadsideSegmentActor : public AActor
{
	GENERATED_BODY()

public:
	ANightRoadsideSegmentActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Roadside")
	TObjectPtr<USceneComponent> RoadsideRoot;

	/** Start of the segment along the roadside chain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Roadside|Markers")
	TObjectPtr<UArrowComponent> StartMarker;

	/** End of the segment along the roadside chain. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Roadside|Markers")
	TObjectPtr<UArrowComponent> EndMarker;

	UFUNCTION(BlueprintPure, Category = "Night|Roadside")
	bool GetRoadsideMarkerLocations(FVector& OutStart, FVector& OutEnd) const;

	UFUNCTION(BlueprintPure, Category = "Night|Roadside")
	float GetRoadsideSpanCm() const;

	/** Roadside decorations are presentation-only and never block gameplay. */
	void ApplyRoadsideCollisionPolicy();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
};
