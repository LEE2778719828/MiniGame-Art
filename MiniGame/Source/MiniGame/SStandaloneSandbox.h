#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "SStandaloneSandbox.generated.h"

class UTextBlock;
class UUniformGridPanel;
class ASMergeBoard;
class USDebugPanel;

UENUM(BlueprintType)
enum class ESGamePhase : uint8
{
	Boot,
	PrepareNight,
	NightSettlement,
	DayOpening,
	DayRunning,
	GiftSelect
};

USTRUCT(BlueprintType)
struct FSIngredientStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName IngredientId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FSNightResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ResultId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RouteTaken = TEXT("AB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSIngredientStack> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SoulLeft = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFailedMidway = false;
};

USTRUCT(BlueprintType)
struct FSDishPiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName IngredientId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "4"))
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CellIndex = INDEX_NONE;

	bool IsValid() const
	{
		return IngredientId != NAME_None && CellIndex != INDEX_NONE;
	}
};

USTRUCT(BlueprintType)
struct FSMergeCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOccupied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSDishPiece Piece;
};

USTRUCT(BlueprintType)
struct FSOrderRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName IngredientId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SellValue = 0;
};

USTRUCT(BlueprintType)
struct FSCustomerState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CustomerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSOrderRequest Order;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PatienceRemaining = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PatienceMax = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActive = false;
};

/** 下一夜可读的谢礼 Buff；代码认稳定 GiftId，不认卡面顺序。 */
USTRUCT(BlueprintType)
struct FSGiftBuffState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGuideKite = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLifeLamp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bBeatCoin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGluttonBox = false;

	FString ToDebugString() const
	{
		TArray<FString> Parts;
		if (bGuideKite) Parts.Add(TEXT("GuideKite"));
		if (bLifeLamp) Parts.Add(TEXT("LifeLamp"));
		if (bBeatCoin) Parts.Add(TEXT("BeatCoin"));
		if (bGluttonBox) Parts.Add(TEXT("GluttonBox"));
		return Parts.IsEmpty() ? TEXT("None") : FString::Join(Parts, TEXT(", "));
	}
};

USTRUCT(BlueprintType)
struct FSSpecialNpcState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NpcId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSOrderRequest Order;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName GiftId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPresent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bServed = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSSandboxStateChanged);

UCLASS()
class MINIGAME_API USChefGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	bool ConsumeNightResult(const FSNightResult& Result);

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool AddIngredient(FName IngredientId, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "S Inventory")
	int32 GetQuantity(FName IngredientId) const;

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool TryConsume(FName IngredientId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool ApplyBatch(const TArray<FSIngredientStack>& Changes);

	UFUNCTION(BlueprintPure, Category = "S Sandbox")
	int32 GetInventoryQuantity(FName IngredientId) const;

	UFUNCTION(BlueprintPure, Category = "S Sandbox")
	int32 GetTemporaryQuantity(FName IngredientId) const;

	UFUNCTION(BlueprintPure, Category = "S Sandbox")
	FString GetPhaseDisplayName() const;

	UFUNCTION(BlueprintCallable, Category = "S Revenue")
	void AddRevenue(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	static int32 GetRecipeSellValue(FName RecipeId);

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	static FName MakeRecipeId(FName IngredientId, int32 Level);

	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool AddObtainedGift(FName GiftId);

	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool TogglePendingGiftSelection(FName GiftId);

	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool ConfirmGiftSelection();

	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool TryEnterGiftSelect(const FString& Reason);

	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool ForceCloseShopForDebug();

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	FSGiftBuffState GetGiftBuffState() const { return GiftBuffState; }

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	TArray<FName> GetObtainedGiftIds() const { return ObtainedGiftIds; }

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	TArray<FName> GetPendingGiftIds() const { return PendingGiftIds; }

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	static FString GetGiftDisplayName(FName GiftId);

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	void ResetSandbox();

	UFUNCTION(BlueprintPure, Category = "S Inventory")
	TArray<FName> GetKnownIngredientIds() const;

	UPROPERTY(BlueprintAssignable, Category = "S Sandbox")
	FSSandboxStateChanged OnSandboxStateChanged;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	ESGamePhase Phase = ESGamePhase::Boot;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	FName StageId = TEXT("T0");

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	int32 ReviewSeed = 1001;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	int32 Revenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	int32 RevenueTarget = 90;

	/** 本局营业已获得的谢礼卡（闭店候选）。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Gifts")
	TArray<FName> ObtainedGiftIds;

	/** 闭店选礼时的临时勾选（最多 2）。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Gifts")
	TArray<FName> PendingGiftIds;

	/** 已确认并带入下一夜的两件谢礼。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	TArray<FName> SelectedGiftIds;

	UPROPERTY(BlueprintReadOnly, Category = "S Gifts")
	FSGiftBuffState GiftBuffState;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	FString LastConsumedNightResultId = TEXT("None");

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	FString LastBoardFeedback = TEXT("等待操作。");

private:
	UPROPERTY()
	TMap<FName, int32> Inventory;

	UPROPERTY()
	TMap<FName, int32> TemporaryBasket;

	UPROPERTY()
	TSet<FString> ConsumedResultIds;

	bool IsKnownIngredient(FName IngredientId) const;
	void InitializeIngredientMaps();
	void NotifyStateChanged();
	void RebuildGiftBuffState();
	void BeginNewDayGiftPool();
};

UCLASS()
class MINIGAME_API USMergeDragOp : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "S Merge")
	int32 SourceCellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "S Merge")
	FSDishPiece Piece;

	UPROPERTY(BlueprintReadOnly, Category = "S Merge")
	int32 PointerId = INDEX_NONE;
};

/** 不规则 Merge 棋盘：格子掩码可调，不把 5x5 写死为逻辑前提。 */
UCLASS(Blueprintable)
class MINIGAME_API ASMergeBoard : public AActor
{
	GENERATED_BODY()

public:
	ASMergeBoard();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void BuildDefaultIrregularBoard();

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void ClearBoard();

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetEnabledCellCount() const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetOccupiedCellCount() const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetEmptyCellCount() const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 FindFirstEmptyCell() const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	bool IsFull() const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetGridWidth() const { return GridWidth; }

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetGridHeight() const { return GridHeight; }

	UFUNCTION(BlueprintPure, Category = "S Merge")
	const TArray<FSMergeCell>& GetCells() const { return Cells; }

	UFUNCTION(BlueprintPure, Category = "S Merge")
	bool TryGetPiece(int32 CellIndex, FSDishPiece& OutPiece) const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 CountPiecesAtLevel(FName IngredientId, int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetHighestLevel(FName IngredientId) const;

	/** 先找空格，再扣永久库存，再生成 Lv0；任一步失败都不改库存与棋盘。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool TrySpawnFromMotherPiece(FName IngredientId);

	/** 拖放到目标格：空格则移动；同链同级则合成；否则回弹且棋盘不变。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool TryDropPiece(int32 FromCellIndex, int32 ToCellIndex);

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool BeginPieceDrag(int32 CellIndex, int32 PointerId);

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void CancelPieceDrag(int32 PointerId = -1);

	/** 仅清除拖拽锁定，不改反馈文案（交付回弹时用）。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void ClearActiveDrag();

	UFUNCTION(BlueprintPure, Category = "S Merge")
	bool IsDragging() const { return ActiveDragCellIndex != INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetActiveDragCellIndex() const { return ActiveDragCellIndex; }

	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 GetActiveDragPointerId() const { return ActiveDragPointerId; }

	/** 调试：不扣库存强制占满所有启用格。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void ForceFillBoardForDebug();

	/** 调试：用库存把指定链自动合到 Lv4。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool DebugPromoteChainToLv4(FName IngredientId);

	/** 调试：五条链各合到至少一枚 Lv4。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool DebugPromoteAllChainsToLv4();

	/** 交付成功后移除棋子；失败则不改棋盘。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool RemovePieceAt(int32 CellIndex);

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	static ASMergeBoard* FindBoard(const UObject* WorldContextObject);

private:
	UPROPERTY(EditAnywhere, Category = "S Merge")
	int32 GridWidth = 4;

	UPROPERTY(EditAnywhere, Category = "S Merge")
	int32 GridHeight = 4;

	UPROPERTY(VisibleAnywhere, Category = "S Merge")
	TArray<FSMergeCell> Cells;

	int32 ActiveDragCellIndex = INDEX_NONE;
	int32 ActiveDragPointerId = INDEX_NONE;

	static FName MakeRecipeId(FName IngredientId, int32 Level);
	USChefGameInstance* GetChefGameInstance() const;
	void SetFeedback(const FString& Message);
	bool IsValidCellIndex(int32 CellIndex) const;
	void ClearCell(int32 CellIndex);
	void PlacePiece(int32 CellIndex, FName IngredientId, int32 Level);
	bool CanMergePieces(const FSDishPiece& A, const FSDishPiece& B, FString& OutReason) const;
	int32 FindPairForMerge(FName IngredientId, int32 Level, int32 PreferKeepIndex) const;
};

/** 第四步：固定订单顾客与交付。 */
UCLASS(Blueprintable)
class MINIGAME_API ASCustomerDirector : public AActor
{
	GENERATED_BODY()

public:
	ASCustomerDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	void ResetDirector();

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	void NotifyDayStarted();

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	bool SpawnFixedCustomer();

	/** 用当前选中棋子尝试交付；配方不对则回弹，不扣棋子/营业额。 */
	UFUNCTION(BlueprintCallable, Category = "S Customers")
	bool TryDeliverSelectedPiece();

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	bool TryDeliverFromCell(int32 CellIndex);

	UFUNCTION(BlueprintPure, Category = "S Customers")
	bool HasActiveCustomer() const { return ActiveCustomer.bActive; }

	UFUNCTION(BlueprintPure, Category = "S Customers")
	FSCustomerState GetActiveCustomer() const { return ActiveCustomer; }

	UFUNCTION(BlueprintPure, Category = "S Customers")
	float GetSpawnCooldownRemaining() const { return SpawnCooldownRemaining; }

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	static ASCustomerDirector* FindDirector(const UObject* WorldContextObject);

private:
	UPROPERTY(VisibleAnywhere, Category = "S Customers")
	FSCustomerState ActiveCustomer;

	UPROPERTY(EditAnywhere, Category = "S Customers")
	float FixedPatienceSeconds = 32.0f;

	UPROPERTY(EditAnywhere, Category = "S Customers")
	float SpawnIntervalSeconds = 7.0f;

	float SpawnCooldownRemaining = 0.0f;
	bool bDayServiceActive = false;
	int32 NextCustomerNumber = 1;
	float HudRefreshAccum = 0.0f;

	USChefGameInstance* GetChefGameInstance() const;
	void SetFeedback(const FString& Message);
	void ClearActiveCustomer(const FString& Reason);
	FSOrderRequest MakeFixedOrder() const;
};

/** 第五步：两名保底特殊 NPC；服务成功留下谢礼卡，无耐心倒计时。 */
UCLASS(Blueprintable)
class MINIGAME_API ASSpecialNpcDirector : public AActor
{
	GENERATED_BODY()

public:
	ASSpecialNpcDirector();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	void ResetDirector();

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	void NotifyDayStarted();

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	bool TryDeliverToNpc(FName NpcId);

	UFUNCTION(BlueprintPure, Category = "S NPC")
	TArray<FSSpecialNpcState> GetNpcs() const { return Npcs; }

	UFUNCTION(BlueprintPure, Category = "S NPC")
	bool TryGetNpc(FName NpcId, FSSpecialNpcState& OutNpc) const;

	UFUNCTION(BlueprintPure, Category = "S NPC")
	int32 CountServed() const;

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	static ASSpecialNpcDirector* FindDirector(const UObject* WorldContextObject);

private:
	UPROPERTY(VisibleAnywhere, Category = "S NPC")
	TArray<FSSpecialNpcState> Npcs;

	bool bDayServiceActive = false;

	USChefGameInstance* GetChefGameInstance() const;
	void SetFeedback(const FString& Message);
	void BuildGuaranteedNpcs();
	FSOrderRequest MakeOrder(FName IngredientId, int32 Level) const;
};

/** 棋盘格按钮：点选来源再点目标即可移动/合成；同一时间只允许一个来源锁定。 */
UCLASS()
class MINIGAME_API USMergeCellButton : public UButton
{
	GENERATED_BODY()

public:
	void Setup(int32 InCellIndex, ASMergeBoard* InBoard, USDebugPanel* InOwnerPanel);
	void RefreshFromBoard();

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	UPROPERTY()
	TWeakObjectPtr<ASMergeBoard> Board;

	UPROPERTY()
	TWeakObjectPtr<USDebugPanel> OwnerPanel;

	int32 CellIndex = INDEX_NONE;
	bool bClickBound = false;

	UFUNCTION()
	void HandleClicked();
};

UCLASS(Blueprintable)
class MINIGAME_API ASFakeNightGateway : public AActor
{
	GENERATED_BODY()

public:
	ASFakeNightGateway();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	void SubmitNewSuccessResult();

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	void SubmitNewFailureResult();

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	void RepeatLastResult();

	UFUNCTION(BlueprintCallable, Category = "S Sandbox")
	void ResetSandbox();

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	void DebugAddTenEach();

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	void DebugTryUnknownId();

	UFUNCTION(BlueprintCallable, Category = "S Merge")
	void DebugPromoteAllChains();

	UFUNCTION(BlueprintCallable, Category = "S Customers")
	void DebugSpawnFixedCustomer();

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	void DebugForceCloseShop();

private:
	UPROPERTY()
	FSNightResult LastGeneratedResult;

	UPROPERTY()
	TObjectPtr<class USDebugPanel> DebugPanel;

	int32 NextResultNumber = 1;
	FSNightResult MakeResult(bool bSuccess);
	void Submit(const FSNightResult& Result);
};

UCLASS(Blueprintable)
class MINIGAME_API USDebugPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void NotifyBoardChanged();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BoardGrid;

	UPROPERTY()
	TArray<TObjectPtr<USMergeCellButton>> BoardCells;

	UPROPERTY()
	TObjectPtr<UButton> CustomerButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> CustomerLabel;

	UPROPERTY()
	TObjectPtr<UButton> NpcALingButton;

	UPROPERTY()
	TObjectPtr<UButton> NpcSangPoButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> NpcDetailLabel;

	UPROPERTY()
	TObjectPtr<UButton> GiftGuideKiteButton;

	UPROPERTY()
	TObjectPtr<UButton> GiftLifeLampButton;

	UPROPERTY()
	TObjectPtr<UButton> ConfirmGiftsButton;

	UFUNCTION()
	void HandleSuccessClicked();

	UFUNCTION()
	void HandleFailureClicked();

	UFUNCTION()
	void HandleRepeatClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleMotherLingGu();

	UFUNCTION()
	void HandleMotherYinShanJun();

	UFUNCTION()
	void HandleMotherChiYanJiao();

	UFUNCTION()
	void HandleMotherYueLinYu();

	UFUNCTION()
	void HandleMotherXuanYuQin();

	UFUNCTION()
	void HandleAddTenEach();

	UFUNCTION()
	void HandleForceFillBoard();

	UFUNCTION()
	void HandleUnknownId();

	UFUNCTION()
	void HandlePromoteAllChains();

	UFUNCTION()
	void HandleCustomerClicked();

	UFUNCTION()
	void HandleSpawnCustomer();

	UFUNCTION()
	void HandleNpcALingClicked();

	UFUNCTION()
	void HandleNpcSangPoClicked();

	UFUNCTION()
	void HandleForceCloseShop();

	UFUNCTION()
	void HandleToggleGiftGuideKite();

	UFUNCTION()
	void HandleToggleGiftLifeLamp();

	UFUNCTION()
	void HandleConfirmGifts();

	UFUNCTION()
	void Refresh();

	ASFakeNightGateway* GetGateway() const;
	ASMergeBoard* GetBoard() const;
	ASCustomerDirector* GetDirector() const;
	ASSpecialNpcDirector* GetNpcDirector() const;
	void BuildWidgetTree();
	void SetFeedback(const FString& Message);
	void TryMother(FName IngredientId, const FString& DisplayName);
	void RefreshBoardVisual();
	void RefreshCustomerVisual();
	void RefreshNpcVisual();
	void RefreshGiftVisual();
	void DeliverToNpc(FName NpcId);
	void ToggleGift(FName GiftId);
};

UCLASS()
class MINIGAME_API ASChefGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASChefGameMode();

protected:
	virtual void BeginPlay() override;
};
