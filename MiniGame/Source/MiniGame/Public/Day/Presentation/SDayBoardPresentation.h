#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "SDayBoardPresentation.generated.h"

class ASMergeBoard;
class UButton;
class UCameraComponent;
class UDirectionalLightComponent;
class UMaterialInterface;
class USkyLightComponent;
class UFont;
class UStaticMesh;
class UStaticMeshComponent;
class UTextBlock;
class UTextRenderComponent;

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
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayCellVisual : public AActor
{
	GENERATED_BODY()

public:
	ASDayCellVisual();

	void Configure(int32 InCellIndex, float InRadius, ASMergeBoard* InBoard);

	void SetLabelFont(UFont* InFont);

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

private:
	TWeakObjectPtr<ASMergeBoard> Board;
	float VisualRadius = 85.0f;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayIngredientBinVisual : public AActor
{
	GENERATED_BODY()

public:
	ASDayIngredientBinVisual();

	void Configure(FName InIngredientId, const FString& InDisplayName);

	void SetLabelFont(UFont* InFont);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FName IngredientId = NAME_None;

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

	void Configure(const FString& InLabel, const FLinearColor& InColor);

	/** Rewrites the floating name/order text without touching the mesh tint. */
	void SetHeadline(const FString& InText, const FLinearColor& InColor);

	void SetLabelFont(UFont* InFont);

	/** None on the walk-in customer seat; set to ALing/SangPo on the guaranteed NPC seats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	FName NpcId = NAME_None;

	/** Seats accept a dragged piece; the chef stand-in does not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	bool bDeliveryTarget = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UStaticMeshComponent> CharacterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TObjectPtr<UTextRenderComponent> Label;
};

UCLASS(Blueprintable)
class MINIGAME_API ASDayBoardPresenter : public AActor
{
	GENERATED_BODY()

public:
	ASDayBoardPresenter();
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

	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASDayCellVisual* GetCellVisual(int32 CellIndex) const;

	/** NAME_None returns the walk-in customer seat. */
	UFUNCTION(BlueprintPure, Category = "S Day Board")
	ASDayCharacterStandIn* GetSeat(FName InNpcId) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S Day Board")
	TSoftObjectPtr<USDayBoardVisualConfig> VisualConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	bool bTakeCameraOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Day Board")
	float PortraitOrthoWidth = 1100.0f;

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
	void BuildCells();
	void BuildBins();
	void BuildCharacters();
	void RefreshCharacters();
	UFont* ResolveLabelFont() const;
	bool TryDeliverToCharacter(ASDayCharacterStandIn* Character, ASMergeBoard* Board);
	void HandlePointerPressed(const FVector2D& ScreenPosition);
	void HandlePointerReleased(const FVector2D& ScreenPosition);
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

	bool bPointerWasDown = false;
	bool bDropHandledOnPress = false;
	float SeatRefreshCountdown = 0.0f;
	FVector2D LastPointerPosition = FVector2D::ZeroVector;
};

UCLASS(Blueprintable)
class MINIGAME_API USDayHUD : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();

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
	void HandleGuideKite();

	UFUNCTION()
	void HandleLifeLamp();

	UFUNCTION()
	void HandleConfirmNight();

	void SpawnIngredient(FName IngredientId);
	void DeliverNpc(FName NpcId);
	void ToggleGift(FName GiftId);

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

	UPROPERTY()
	TObjectPtr<UButton> GuideKiteButton;

	UPROPERTY()
	TObjectPtr<UButton> LifeLampButton;

	UPROPERTY()
	TObjectPtr<UButton> ConfirmNightButton;
};

#pragma endregion K2 moonyfli
