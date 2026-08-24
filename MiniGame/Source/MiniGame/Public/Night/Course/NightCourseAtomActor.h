#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Course/NightCourseAtomArtComponents.h"
#include "NightCourseAtomActor.generated.h"

class UArrowComponent;
class UBoxComponent;
class UChildActorComponent;
class USceneComponent;
class ANightRoadsideSegmentActor;

#pragma region K2 moonyfli
USTRUCT(BlueprintType)
struct MINIGAME_API FNightAtomVisualBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	int32 StoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	int32 BridgeIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	bool bIsBridge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	FString AtomKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	int32 AtomSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	TSubclassOf<AActor> VisualPrefabClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Visual")
	FTransform LocalTransform = FTransform::Identity;
};

/**
 * Reusable authored course segment.
 *
 * The arrays are authored in the atom's local space. When bUseWorldPose is
 * true on a local stone/bridge, WorldLocation is interpreted as an atom-local
 * location. When it is false, TrackDistance is interpreted along local +X.
 * EntryAnchor and ExitAnchor define how consecutive atoms are aligned.
 */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseAtomActor : public AActor
{
	GENERATED_BODY()

public:
	ANightCourseAtomActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Atom")
	TObjectPtr<USceneComponent> AtomRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Atom|Anchors")
	TObjectPtr<UArrowComponent> EntryAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Atom|Anchors")
	TObjectPtr<UArrowComponent> ExitAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Atom|Bounds")
	TObjectPtr<UBoxComponent> ArtBoundsPreview;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom")
	float AtomLengthCm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Rotation")
	float MinYawDeg = -5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Rotation")
	float MaxYawDeg = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Rotation")
	bool bAllowDeterministicRandomYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bounds")
	bool bUseLocalArtBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bounds", meta = (EditCondition = "bUseLocalArtBounds"))
	FVector LocalArtBoundsMin = FVector(-100.f, -250.f, -200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Bounds", meta = (EditCondition = "bUseLocalArtBounds"))
	FVector LocalArtBoundsMax = FVector(700.f, 250.f, 300.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Course")
	TArray<FNightStoneSpec> LocalStones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Course")
	TArray<FNightBeatSpec> LocalBeats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Course")
	TArray<FNightBridgeSpec> LocalBridges;

	/** Optional editor-only representative house for this Atom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Roadside Preview")
	TSubclassOf<ANightRoadsideSegmentActor> HouseRoadsidePreviewPrefab;

	/** Optional editor-only representative pole for this Atom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Roadside Preview")
	TSubclassOf<ANightRoadsideSegmentActor> PoleRoadsidePreviewPrefab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Roadside Preview", meta = (ClampMin = "0.0"))
	float RoadsidePreviewLeftOffsetCm = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Roadside Preview", meta = (ClampMin = "0.0"))
	float RoadsidePreviewRightOffsetCm = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Atom|Roadside Preview")
	float RoadsidePreviewZOffsetCm = 0.f;

	UFUNCTION(BlueprintPure, Category = "Night|Atom")
	FTransform GetEntryAnchorTransform() const;

	UFUNCTION(BlueprintPure, Category = "Night|Atom")
	FTransform GetExitAnchorTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Night|Atom")
	bool ValidateAtom(FString& OutError) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Atom")
	void GetLocalCourseSpecs(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges) const;

	/**
	 * Returns the artist-authored layout. If no landing components exist, the
	 * legacy LocalStones/LocalBridges arrays are returned for compatibility.
	 */
	void GetLocalArtSpecs(
		TArray<FNightStoneSpec>& OutStones,
		TArray<FNightBeatSpec>& OutBeats,
		TArray<FNightBridgeSpec>& OutBridges,
		TArray<FNightAtomVisualBinding>& OutVisualBindings) const;

	UFUNCTION(BlueprintPure, Category = "Night|Atom|Landing")
	int32 GetLandingPointCount() const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Atom|Editor")
	void RebuildAtomVisualPreview();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Atom|Editor", meta = (DisplayName = "ValidateAtom"))
	void ValidateAtomForEditor();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Atom|Editor")
	void SnapFirstLastLandingToAnchors();

	void GetLocalArtBounds(FVector& OutMin, FVector& OutMax) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UChildActorComponent>> PreviewVisualComponents;
};
#pragma endregion K2 moonyfli
