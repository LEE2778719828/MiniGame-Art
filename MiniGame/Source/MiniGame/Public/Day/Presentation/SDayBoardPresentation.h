#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "SDayBoardPresentation.generated.h"

class ASMergeBoard;
class UBillboardComponent;
class UBorder;
class UButton;
class UCheckBox; //add by K2
class UCurveFloat;
class UCanvasPanelSlot;
class USDaySettlementWidget;
class UCameraComponent;
class UDirectionalLightComponent;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class USkeletalMeshComponent; //add by K2
class USkyLightComponent;
class UFont;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextBlock;
class UTextRenderComponent;
class UTexture2D;
class USChefGameInstance;
class UScrollBox;
class USizeBox;
class UVerticalBox; //add by K2

#pragma region K2 moonyfli

USTRUCT(BlueprintType)
struct MINIGAME_API FSDayBoardLayoutRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	float VisualRadius = 85.0f;
};

/** Gameplay output of one authored ingredient bin. Array entries keep the existing bin hit areas. */
USTRUCT(BlueprintType)
struct MINIGAME_API FSDayIngredientBinOutput
{
	GENERATED_BODY()

	/** Existing art/hit-zone index: 0 maps to the first bin, 1 to the second, and so on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins", meta = (ClampMin = "0"))
	int32 BinIndex = 0;

	/** IngredientId consumed from inventory and placed into the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins")
	FName IngredientId = NAME_None;

	/** Per-axis scale applied to this bin's hidden click proxy after the global hit scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins",
		meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
	FVector HitScale = FVector::OneVector;

	/** Local-space offset in centimetres from the authored bin centre to its hidden click proxy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins")
	FVector HitOffset = FVector::ZeroVector;
};

/** DT_SDayDishIconTune single row: plated-food size and clearance. */
USTRUCT(BlueprintType)
struct MINIGAME_API FSDayDishIconTuneRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board", meta = (ClampMin = "1.0"))
	float WorldSize = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board", meta = (ClampMin = "0.0"))
	float ScalePerLevel = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board", meta = (ClampMin = "1.0"))
	float SelectedScale = 1.18f;

	/** Screen-space multiplier used only while the dish follows the mouse or finger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board",
		meta = (ClampMin = "0.1", UIMin = "0.25", UIMax = "2.0"))
	float DragPreviewScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	float Yaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FVector LocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board", meta = (ClampMin = "0.0"))
	float CameraPush = 104.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	float SelectedLift = 14.0f;
};

/** Explicit per-level artwork overrides for one ingredient chain. */
USTRUCT(BlueprintType)
struct MINIGAME_API FSDayDishIconSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	TArray<TSoftObjectPtr<UTexture2D>> LevelIcons;
};

UCLASS(BlueprintType)
class MINIGAME_API USDayBoardVisualConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> BoardFrameMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> CellMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> PieceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> IngredientBinMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> ChefMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> CustomerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TSoftObjectPtr<UStaticMesh> NpcMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> BoardMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> CellMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> PieceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TSoftObjectPtr<UDataTable> CellLayout;

	/** Maps the existing authored bin index to the ingredient it produces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient Bins", meta = (TitleProperty = "IngredientId"))
	TArray<FSDayIngredientBinOutput> IngredientBinOutputs;

	/** Uniform scale applied to the hidden Visibility-trace proxy around each authored ingredient bin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient Bins",
		meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
	float IngredientBinHitScale = 1.0f;

	/**
	 * Offline font used by every world-space label. TextRenderComponent cannot use runtime
	 * fonts, so the engine default only ships ASCII glyphs and renders CJK as boxes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	TSoftObjectPtr<UFont> LabelFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
	TSubclassOf<class ASDayCellVisual> CellVisualClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
	TSubclassOf<class ASDayIngredientBinVisual> IngredientBinClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
	TSubclassOf<class ASDayCharacterStandIn> CharacterStandInClass;

	/** Enable generated food artwork on occupied plate cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	bool bUseDishIcons = true;

	/** Plane used to display the plated-food texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	TSoftObjectPtr<UStaticMesh> DishIconMesh;

	/** Unlit texture material used by the plated-food plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	TSoftObjectPtr<UMaterialInterface> DishIconMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	FName DishIconTextureParameter = TEXT("Tex");

	/** Optional tuning table; row Default is used when present. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	TSoftObjectPtr<UDataTable> DishIconTune;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	bool bShowPieceLabelWithIcon = false;

	/** IngredientId to food-art stem, e.g. LingGu -> rice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	TMap<FName, FName> DishArtStemByIngredient;

	/** Explicit artwork per level; a set entry wins over the naming convention. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dish Icons")
	TMap<FName, FSDayDishIconSet> DishIconOverrides;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayCellVisual : public AActor
{
	GENERATED_BODY()

public:
	ASDayCellVisual();

	void Configure(int32 InCellIndex, float InRadius, ASMergeBoard* InBoard);

	void SetLabelFont(UFont* InFont);

#pragma region K2 moonyfli
	/**
	 * Seat dishes on the cell origin instead of lifting them clear of a whitebox disc. The art
	 * pan puts the origin at a well floor, and the well is tilted toward the camera, so any lift
	 * projects on screen as the dish sliding out of its hole.
	 */
	void SetSeatedInWell(bool bInSeated);
	/** Lets authored scene textures own the cell visuals without changing the hit surface. */
	void SetUseAuthoredVisuals(bool bInUse);
	void SetDishIconConfig(USDayBoardVisualConfig* InConfig);
	void SetDragIconWorldLocation(const FVector& InWorldLocation);
#pragma endregion K2 moonyfli

	UFUNCTION(BlueprintCallable, Category = "S Day Board")
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> CellMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> PieceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UTextRenderComponent> PieceLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> PieceIcon;

private:
	TWeakObjectPtr<ASMergeBoard> Board;
	float VisualRadius = 85.0f;
	bool bUseAuthoredVisuals = false;
	TWeakObjectPtr<USDayBoardVisualConfig> IconConfig;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DishIconMaterial;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayIngredientBinVisual : public AActor
{
	GENERATED_BODY()

public:
	ASDayIngredientBinVisual();

	void Configure(int32 InBinIndex, FName InIngredientId, const FString& InDisplayName);

	void SetLabelFont(UFont* InFont);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FName IngredientId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	int32 BinIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> BinMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UTextRenderComponent> Label;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayCharacterStandIn : public AActor
{
	GENERATED_BODY()

public:
	ASDayCharacterStandIn();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void Configure(const FString& InLabel, const FLinearColor& InColor);

	/** Rewrites the floating name/order text without touching the mesh tint. */
	void SetHeadline(const FString& InText, const FLinearColor& InColor);

	void SetLabelFont(UFont* InFont);

#pragma region K2 moonyfli
	void SetPortrait(UTexture2D* InTexture);
	void SetSceneSeatEnabled(bool bEnabled);
	void NotifySeatOccupied(
		const FString& OccupantKey,
		bool bSpecialNpc,
		FName IngredientId,
		int32 Level,
		FName GiftId);
	void NotifySeatVacated();
	void NotifyServeSucceeded(bool bSpecialNpc);
	void BeginServeAttempt();
	void CancelServeAttempt();

	/**
	 * Called by the seat Blueprint when its authored departure animation is finished.
	 * Gameplay vacates the logical seat immediately; the portrait is retained until this handshake
	 * (or the fallback timeout) so eat/exit animation remains visible.
	 */
	UFUNCTION(BlueprintCallable, Category = "S Day Board|Seat")
	void CompletePresentationDeparture();

	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Seat")
	void OnSeatOccupied(
		const FString& OccupantKey,
		bool bSpecialNpc,
		FName IngredientId,
		int32 Level,
		FName GiftId);

	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Seat")
	void OnSeatVacated();

	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Seat")
	void OnServeSucceeded(bool bSpecialNpc);

	/**
	 * Unified animation trigger. Served customers normally play eat then exit; timeout/closure
	 * departures skip eating. Call CompletePresentationDeparture from the final timeline Finished pin.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Seat")
	void OnDepartureRequested(bool bServed, bool bSpecialNpc);

	/** Safety net for Blueprints that do not complete the departure handshake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat", meta = (ClampMin = "0.1"))
	float DepartureFallbackSeconds = 6.0f;

	/** Scene-authored seats are discovered instead of spawned and never repositioned at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat")
	bool bSceneAuthoredSeat = false;

	/** Stable logical slot, left-to-right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat", meta = (ClampMin = "0", ClampMax = "5"))
	int32 AuthoredSeatSlot = 0;

	/** Visible only in editor worlds so artists can frame an empty seat without PIE. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat")
	bool bShowEditorPreview = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat")
	TSoftObjectPtr<UTexture2D> EditorPreviewPortrait;

	/** Per-seat visual size; no code constant is needed for framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat", meta = (ClampMin = "1.0"))
	float PortraitWorldHeight = 330.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Seat")
	FVector PortraitLocalOffset = FVector::ZeroVector;
#pragma endregion K2 moonyfli

	/** None while empty or taken by a walk-in guest; set to ALing/SangPo when an NPC sits down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FName NpcId = NAME_None;

#pragma region K2 moonyfli
	/** Walk-in customer occupying this seat; empty for NPC and vacant seats. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	FString CustomerId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	int32 SeatIndex = INDEX_NONE;
#pragma endregion K2 moonyfli

	/** Seats accept a dragged piece; the chef stand-in does not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	bool bDeliveryTarget = false;

	/** Seats are shared: a guest or NPC takes one on arrival and frees it once served. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	bool bOccupied = false; //add by K2

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board|Seat")
	TObjectPtr<USceneComponent> VisualRoot;

	/** Blueprint timelines animate this node without moving the authored seat or its hit target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board|Seat")
	TObjectPtr<USceneComponent> PortraitMotionRoot;

	/** Artist-positioned world-space origin for revenue coins; projected by the HUD on delivery. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board|Revenue")
	TObjectPtr<USceneComponent> RevenueFlyAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board|Seat")
	TObjectPtr<USceneComponent> EatEffectAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board|Seat")
	TObjectPtr<USceneComponent> GiftEffectAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UTextRenderComponent> Label;

#pragma region K2 moonyfli
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UBillboardComponent> Portrait;

	UPROPERTY(Transient)
	FString PresentedOccupantKey;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "S Day Board|Seat", meta = (AllowPrivateAccess = "true"))
	bool bPresentationDepartureInProgress = false;

	UPROPERTY(Transient)
	bool bHoldPresentationForServeAttempt = false;

	UPROPERTY(Transient)
	bool bPresentedSpecialNpc = false;

	FTimerHandle PresentationDepartureFallbackHandle;

	/** Repairs stale Blueprint attachment overrides so authored timelines always drive the portrait. */
	void EnsurePortraitMotionAttachment();

	void BeginPresentationDeparture(bool bServed, bool bSpecialNpc);
#pragma endregion K2 moonyfli
};

#pragma region K2 moonyfli
/**
 * Owner of the composition camera and of the cookingUI layers hanging off it.
 * BP_DayCamera currently subclasses this. Layer transforms stay authored: do not solve
 * them from OnConstruction or BeginPlay, or the editor camera preview and PIE diverge.
 */
UCLASS(Blueprintable)
class MINIGAME_API ASDayCameraRig : public AActor
{
	GENERATED_BODY()

public:
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "S Day Board")
	void RefitDayArt();
};
#pragma endregion K2 moonyfli

/**
 * Screen-space dish shown while a board piece follows the pointer. Keeping the preview in
 * the viewport prevents the authored stove/pan meshes from occluding it on desktop or mobile.
 */
UCLASS()
class MINIGAME_API USDayDragPreview : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowPreview(
		UTexture2D* Texture,
		const FVector2D& ScreenPosition,
		const FVector2D& PreviewSize);
	void MovePreview(const FVector2D& ScreenPosition);
	void HidePreview();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> DragImage;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayBoardPresenter : public AActor
{
	GENERATED_BODY()

public:
	ASDayBoardPresenter();

	/** Button-like Blueprint hook fired whenever an authored ingredient bin is clicked. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Ingredient Bins",
		meta = (DisplayName = "Ingredient Bin Clicked"))
	void BP_OnIngredientBinClicked(int32 BinIndex, FName IngredientId, bool bSpawnSucceeded);

	/** Fired after two player-controlled pieces have successfully finished merging. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S Day Board|Merge",
		meta = (DisplayName = "Ingredient Merge Completed"))
	void BP_OnIngredientMergeCompleted(FName IngredientId, int32 ResultLevel, int32 TargetCellIndex);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "S Day Board")
	void RefreshFromLogic();

	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASMergeBoard* GetLogicBoard() const { return LogicBoard.Get(); }

	/** Feeds a synthetic screen position through the same handlers the pointer tick uses. */
	UFUNCTION(BlueprintCallable, Category = "S Day Board")
	void SimulatePointerEvent(FVector2D ScreenPosition, bool bPressed);

	/** Updates the held drag visual without changing the board's drop state. */
	void SimulatePointerMove(FVector2D ScreenPosition);

	void SetUseExternalPointerDriver(bool bEnabled); //add by K2
	void CancelPointerInteraction(); //add by K2

	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASDayCellVisual* GetCellVisual(int32 CellIndex) const;

	/** NAME_None returns the seat the walk-in guest currently occupies. */
	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASDayCharacterStandIn* GetSeat(FName InNpcId) const;

	/** Shared seats in the top row; the chef stand-in is not one of them. Count comes from GI. */
	UFUNCTION(BlueprintPure, Category = "S Day Board")
	int32 GetDeliverySeatCount() const; //add by K2

	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASDayIngredientBinVisual* GetIngredientBin(FName IngredientId) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TSoftObjectPtr<USDayBoardVisualConfig> VisualConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	bool bTakeCameraOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	float PortraitOrthoWidth = 1100.0f;

#pragma region K2 moonyfli
	/**
	 * Seconds the bin stays fully open before it starts closing.
	 * Playback speed is not configured here: it comes from the BoxAnim component PlayRate
	 * in BP_SDayCanguan multiplied by the RateScale of the box*_Anim sequence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Bin Animation", meta = (ClampMin = "0.0"))
	float BinAnimHoldSeconds = 0.15f;

	/** When false the bin lid holds the last frame instead of closing itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Bin Animation")
	bool bBinAnimAutoClose = true;

	/** Niagara systems used by a successful ingredient-bin click. Defaults point at /Game/VFX_Merge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	TSoftObjectPtr<UNiagaraSystem> IngredientBinFogSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	TSoftObjectPtr<UNiagaraSystem> IngredientBinFoodBurstSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	TSoftObjectPtr<UNiagaraSystem> IngredientBinTrailSystem;

	/** Bin-local offset for Fog, initial FoodBurst, Trail, and the temporary food icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinVfxStartOffset = FVector(0.0f, 0.0f, 90.0f);

	/** Target-cell local offset used when PieceIcon is unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinVfxTargetOffset = FVector(0.0f, 0.0f, 55.0f);

	/**
	 * World-space draw offset shared by Fog, FoodBurst, Trail and the temporary food icon.
	 * The Day camera looks toward world -Y, so the default pulls feedback in front of the models.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinVfxViewOffset = FVector(0.0f, -80.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinFogScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinFoodBurstScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	FVector IngredientBinTrailScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	bool bSpawnFoodBurstAtBin = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX")
	bool bSpawnFoodBurstAtTarget = true;

	/** Niagara user-parameter names written after spawning the Trail component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Trail")
	FName IngredientTrailStartParameter = TEXT("User.StartPosition");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Trail")
	FName IngredientTrailTargetParameter = TEXT("User.TargetPosition");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Trail")
	FName IngredientTrailDurationParameter = TEXT("User.DurationTime");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Trail",
		meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float IngredientTrailDuration = 0.35f;

	/** Extra local bounds around the complete start-to-target path; prevents long trails being culled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Trail",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1000.0"))
	float IngredientTrailBoundsPadding = 180.0f;

	/** Screen-space food icon layered over the Niagara trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Food Icon",
		meta = (ClampMin = "1.0", UIMin = "32.0", UIMax = "256.0"))
	float IngredientFlyIconWidth = 118.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Food Icon",
		meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float IngredientFlyDuration = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board|Ingredient Bins|VFX|Food Icon",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
	float IngredientFlyArcHeight = 105.0f;
#pragma endregion K2 moonyfli

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> BoardFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> Counter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UDirectionalLightComponent> WhiteboxKeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<USkyLightComponent> WhiteboxFillLight;

private:
	void BuildWhitebox();
	void EnsureDragPreview();
	void HideDragPreview();
	void BuildCells();
	void BuildBins();
	void BuildCharacters();
	bool TryBindSceneAuthoredSeats(int32 DesiredSeatCount);
	void RefreshCharacters();
	UFont* ResolveLabelFont() const;
	bool TryDeliverToCharacter(ASDayCharacterStandIn* Character, ASMergeBoard* Board);
	bool TryDropPieceAndNotify(ASMergeBoard* Board, int32 FromCellIndex, int32 ToCellIndex);
#pragma region K2 moonyfli
	void PlayIngredientBinAnimation(int32 BinIndex);
	void PlayIngredientSpawnFeedback(
		ASDayIngredientBinVisual* Bin,
		FName IngredientId,
		int32 SpawnedCellIndex);
	void CloseIngredientBinAnimation(int32 BinIndex);
	void RestIngredientBinAnimation(int32 BinIndex);
	USkeletalMeshComponent* FindIngredientBinAnimComponent(int32 BinIndex) const;
#pragma endregion K2 moonyfli
	bool IsInIngredientDropZone(const FVector2D& ScreenPosition) const;
	bool TryDecomposeInIngredientArea(const FVector2D& ScreenPosition, ASMergeBoard* Board);
	void HandlePointerPressed(const FVector2D& ScreenPosition);
	void HandlePointerReleased(const FVector2D& ScreenPosition);
	void UpdateDraggedIcon(const FVector2D& ScreenPosition);
	bool GetPointerState(FVector2D& OutScreenPosition) const;
	AActor* HitTest(const FVector2D& ScreenPosition) const;
	TArray<FSDayBoardLayoutRow> GetLayoutRows() const;

	TWeakObjectPtr<ASMergeBoard> LogicBoard;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASDayCellVisual>> CellVisuals;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASDayIngredientBinVisual>> IngredientBins;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASDayCharacterStandIn>> CharacterStandIns;

	UPROPERTY(Transient)
	TObjectPtr<USDayDragPreview> DragPreview;

	TMap<int32, FTimerHandle> BinAnimTimers; //add by K2
	/** Presenter-owned timer handles keep flight callbacks from owning and destroying their own handles. */
	TMap<uint64, FTimerHandle> IngredientFlightTimers;
	/** Number of in-flight bin items targeting each cell; their resting visuals stay hidden until arrival. */
	TMap<int32, int32> PendingIngredientArrivalCounts;
	uint64 NextIngredientFlightId = 0;

	bool bUsingSceneAuthoredSeats = false; //add by K2
	bool bPointerWasDown = false;
	bool bUseExternalPointerDriver = false; //add by K2
	bool bDropHandledOnPress = false;
	bool bDragIconTuneResolved = false;
	float SeatRefreshCountdown = 0.0f;
	FVector2D LastPointerPosition = FVector2D::ZeroVector;
	FSDayDishIconTuneRow DragIconTune;
};

UCLASS(Blueprintable)
class MINIGAME_API USDayHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Spawn the authored coin-flight feedback from a served customer to the top-left revenue readout. */
	void PlayRevenueFlyFromWorld(const FVector& SourceWorldLocation, int32 RevenueAmount);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshSettlement(const USChefGameInstance& GameInstance);
	void RestoreDayInputMode();

	UFUNCTION()
	void Refresh();

	UFUNCTION()
	void HandleCustomer();

	UFUNCTION()
	void HandleALing();

	UFUNCTION()
	void HandleSangPo();

	UFUNCTION()
	void HandleLingGu();

	UFUNCTION()
	void HandleYinShanJun();

	UFUNCTION()
	void HandleChiYanJiao();

	UFUNCTION()
	void HandleYueLinYu();

	UFUNCTION()
	void HandleXuanYuQin();

	UFUNCTION()
	void HandleFlowButton();

#pragma region K2 moonyfli
	UFUNCTION()
	void HandleChromeVisibilityChanged(bool bIsChecked);
	void ApplyChromeVisibility(bool bShow);

	/**
	 * Diegetic readouts live in the art foreground page, not in this debug HUD, so they survive
	 * the chrome toggle. Only the numbers come from C++; size, position and font stay in UMG.
	 */
	void RefreshForegroundReadouts(const USChefGameInstance& GameInstance);
	void ResolveForegroundReadouts();
#pragma endregion K2 moonyfli

	void SpawnIngredient(FName IngredientId);
	void DeliverNpc(FName NpcId);

	UPROPERTY()
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY()
	TObjectPtr<UTextBlock> InventoryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> OrderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY()
	TObjectPtr<UButton> CustomerButton;

	UPROPERTY()
	TObjectPtr<UButton> ALingButton;

	UPROPERTY()
	TObjectPtr<UButton> SangPoButton;

	/** 入夜前礼品卡页签：只读展示，谢礼拿到即生效，玩家不需要挑选。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> GiftTabText;

	UPROPERTY()
	TObjectPtr<UButton> FlowButton;

	/** Assign /Game/Day/UI/Settlement/WBP_DaySettlement in WBP_SDayHUD defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Day|Settlement", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<USDaySettlementWidget> SettlementWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<USDaySettlementWidget> SettlementWidget;

	bool bSettlementClassWarningLogged = false;

#pragma region K2 moonyfli
	UPROPERTY()
	TObjectPtr<UCheckBox> ChromeToggle;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ControlsHost;

	/** Text blocks authored inside the foreground art page; absent until that page is loaded. */
	TWeakObjectPtr<UTextBlock> CoinAmountText;
	TWeakObjectPtr<UTextBlock> RevenueCurrentText;
	TWeakObjectPtr<UTextBlock> RevenueTargetText;

	/** Single flying item authored in UMG; one sale creates a short staggered burst of instances. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback")
	TSubclassOf<UUserWidget> RevenueFlyingItemClass;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback")
	TObjectPtr<UCurveFloat> RevenueFlyPathCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback")
	TObjectPtr<UCurveFloat> RevenueFlyScaleCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback")
	TObjectPtr<UMaterialInterface> RevenueFlyingItemMaterial;

	/** Pixel offset applied after projecting the served customer's portrait into player-screen space. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Placement")
	FVector2D RevenueFlySourceScreenOffset = FVector2D::ZeroVector;

	/** Normalized player-viewport destination. (0,0) is top-left and (1,1) is bottom-right. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	FVector2D RevenueFlyTargetViewportRatio = FVector2D(0.08f, 0.07f);

	/** Fine adjustment in pixels after applying RevenueFlyTargetViewportRatio. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Placement")
	FVector2D RevenueFlyTargetScreenOffset = FVector2D::ZeroVector;

	/** Keep configured source and destination positions inside the owning player's viewport. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Placement")
	bool bClampRevenueFlyToViewport = true;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "1", UIMin = "1"))
	int32 RevenueFlyMinItemCount = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "1", UIMin = "1"))
	int32 RevenueFlyMaxItemCount = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.01", UIMin = "0.1", Units = "s"))
	float RevenueFlyMinDuration = 0.58f;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.01", UIMin = "0.1", Units = "s"))
	float RevenueFlyMaxDuration = 0.74f;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float RevenueFlyItemInterval = 0.055f;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RevenueFlyMinScale = 0.82f;

	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RevenueFlyMaxScale = 1.08f;

	/** Arc height as a fraction of viewport height; 0.10 means ten percent. */
	UPROPERTY(EditDefaultsOnly, Category = "Day|Revenue Feedback|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.3"))
	float RevenueFlyArcHeightRatio = 0.10f;
#pragma endregion K2 moonyfli

	/** Shop clock and spawn cooldown move without state events, so poll the logic. */
	FTimerHandle RefreshTimerHandle; //add by K2

#pragma region K2 moonyfli
	UFUNCTION()
	void HandleToggleCheatPanel();

	UPROPERTY()
	TObjectPtr<UButton> CheatToggleButton;

	UPROPERTY()
	TObjectPtr<class USDayCheatPanel> CheatPanel;
#pragma endregion K2 moonyfli
};

/** Dev-only daytime modifier overlay; HUD entry is omitted in Shipping builds. */
UCLASS()
class MINIGAME_API USDayCheatPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPanelVisible(bool bVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	/** Keeps the frame inside the viewport across resolutions and UI scales. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Title bar acts as the drag handle; body clicks stay with the buttons. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	void BuildWidgetTree();
	UFUNCTION()
	void RefreshStatus();
	USChefGameInstance* GetChef() const;
	void SetSelectedIngredient(FName IngredientId);
	void AdjustStock(int32 Delta);
	void SetStock(int32 Quantity);
	/** Frame lives in a full-screen canvas, so all placement math stays in widget-local space. */
	void ApplyFramePosition(const FVector2D& DesiredPosition, const FGeometry& CanvasGeometry);
	/** Scroll body needs a definite height; collapsed body falls back to auto height. */
	void ApplyBodyHeight();

	UFUNCTION() void HandleToggleBody();
	UFUNCTION() void HandleClose();
	UFUNCTION() void HandleSelectLingGu();
	UFUNCTION() void HandleSelectYin();
	UFUNCTION() void HandleSelectChi();
	UFUNCTION() void HandleSelectYue();
	UFUNCTION() void HandleSelectXuan();
	UFUNCTION() void HandleStockPlus1();
	UFUNCTION() void HandleStockPlus10();
	UFUNCTION() void HandleStockClear();
	UFUNCTION() void HandleStockSet0();
	UFUNCTION() void HandleStockSet10();
	UFUNCTION() void HandleStockSet20();
	UFUNCTION() void HandleStockSet50();
	UFUNCTION() void HandleStockSet99();
	UFUNCTION() void HandleForceNextCustomer();
	UFUNCTION() void HandleRevenuePlus10();
	UFUNCTION() void HandleRevenuePlus50();
	UFUNCTION() void HandleRevenueQualify();
	UFUNCTION() void HandleTimePlus30();
	UFUNCTION() void HandleTimeMinus30();
	UFUNCTION() void HandleTimeSet60();
	UFUNCTION() void HandleTimeSet10();
	UFUNCTION() void HandleOpenShop();
	UFUNCTION() void HandleForceClose();
	UFUNCTION() void HandleFailDay();
	UFUNCTION() void HandleGiftKite();
	UFUNCTION() void HandleGiftLamp();
	UFUNCTION() void HandleGiftCoin();
	UFUNCTION() void HandleGiftBox();
	UFUNCTION() void HandleClearGifts();
	UFUNCTION() void HandleJumpT0();
	UFUNCTION() void HandleJumpL1();
	UFUNCTION() void HandleJumpL2();
	UFUNCTION() void HandleJumpL3();
	UFUNCTION() void HandleSave();
	UFUNCTION() void HandleLoad();
	UFUNCTION() void HandleDeleteSave();
	UFUNCTION() void HandleCorruptSave();

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> OrderQueueText;

	UPROPERTY()
	TObjectPtr<UBorder> DragHandle;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> FrameSlot;

	UPROPERTY()
	TObjectPtr<USizeBox> FrameSizeBox;

	UPROPERTY()
	TObjectPtr<USizeBox> BodySizeBox;

	UPROPERTY()
	TObjectPtr<UScrollBox> BodyScroll;

	UPROPERTY()
	TObjectPtr<UTextBlock> CollapseLabel;

	FName SelectedIngredientId = TEXT("LingGu");

	FVector2D FramePosition = FVector2D(24.0f, 140.0f);
	/** Cursor offset inside the drag handle when the grab started. */
	FVector2D DragGrabOffset = FVector2D::ZeroVector;
	float AppliedFrameWidth = 0.0f;
	float AppliedFrameHeight = 0.0f;
	bool bDraggingPanel = false;
};

#pragma endregion K2 moonyfli
