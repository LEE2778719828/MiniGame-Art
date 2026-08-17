#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "SStandaloneSandbox.generated.h"

class UTextBlock;
class UUniformGridPanel;
class ASMergeBoard;
class USDebugPanel;

#pragma region K2 moonyfli
/**
 * Night → Day → NextStage loop. PrepareDay / DaySettlement / PrepareNextStage are
 * transient: they run their edge work and hand off inside the same call.
 */
UENUM(BlueprintType)
enum class ESGamePhase : uint8
{
	Boot,
	PrepareNight,
	NightRunning,
	NightSettlement,
	PrepareDay,
	DayRunning,
	DayQualified,
	DaySettlement,
	PrepareNextStage,
	Ending
};

/** Why the shop closed; only TimeUp carries leftover stock into the next stage. */
UENUM(BlueprintType)
enum class ESDayEndReason : uint8
{
	None,
	TimeUp,
	OutOfIngredients
};

/** Slot in the pre-generated day order queue. */
UENUM(BlueprintType)
enum class ESOrderSlotKind : uint8
{
	Guest,
	Npc
};
#pragma endregion K2 moonyfli

/** DT_Recipes 行：菜品售价由表驱动。 */
USTRUCT(BlueprintType)
struct FSRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName IngredientId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "4"))
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 SellValue = 10;
};

/** DT_GameStages 行：T0–L3 表驱动，不在多个 Widget 写死数值。 */
USTRUCT(BlueprintType)
struct FSGameStageRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LevelId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NightDuration = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ForkPair = TEXT("AB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ReviewSeed = 1001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DayDuration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RevenueTarget = 90;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CustomerConcurrentMax = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CustomerSpawnInterval = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CustomerConfigId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName GuaranteedNpcRules = TEXT("ALing_SangPo");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NextLevelId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEndingAfterDay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;
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

#pragma region K2 moonyfli
	/** Inventory units actually spent for this piece; debug-spawned pieces stay 0 so refunds cannot mint food. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PaidUnits = 0;
#pragma endregion K2 moonyfli

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

#pragma region K2 moonyfli
/** One entry in the day order appearance queue (guest or special NPC). */
USTRUCT(BlueprintType)
struct FSPlannedOrder
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESOrderSlotKind Kind = ESOrderSlotKind::Guest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NpcId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSOrderRequest Order;
};
#pragma endregion K2 moonyfli

USTRUCT(BlueprintType)
struct FSCustomerState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString CustomerId;

#pragma region K2 moonyfli
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;
#pragma endregion K2 moonyfli

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSOrderRequest Order;

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

/** S → R2 唯一开局包。 */
USTRUCT(BlueprintType)
struct FSNightBootstrap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LevelId = TEXT("T0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ForkPair = TEXT("AB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSGiftBuffState GiftBuffState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FoeWeightOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed = 1001;

	FString ToDebugString() const
	{
		return FString::Printf(
			TEXT("LevelId=%s Fork=%s Seed=%d FoeW=%.1f Gifts=%s"),
			*LevelId.ToString(),
			*ForkPair.ToString(),
			Seed,
			FoeWeightOverride,
			*GiftBuffState.ToDebugString());
	}
};

#pragma region K2 moonyfli
/** 夜初/日初快照：失败回档到本阶段开始时的库存、营业额与谢礼。 */
USTRUCT(BlueprintType)
struct FSRunSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName StageId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FName, int32> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Revenue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RevenueTarget = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FName> GiftIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSGiftBuffState GiftBuffState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FSPlannedOrder> PlannedDayOrders;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NextPlannedOrderIndex = 0;
};
#pragma endregion K2 moonyfli

/** 最小存档 SG_ChefProfile：强退后可恢复库存/缺口/谢礼/关卡与 Result 去重。 */
UCLASS()
class MINIGAME_API USChefSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// v3: day order queue + cursor for mid-day load / day-start rollback.
	static constexpr int32 CurrentSaveVersion = 3;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	int32 SaveVersion = CurrentSaveVersion;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FName CurrentStageId = TEXT("T0");

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	TMap<FName, int32> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	int32 RevenueProgress = 0;

#pragma region K2 moonyfli
	/** 本日已获得且已生效的谢礼；无背包、无勾选。 */
	UPROPERTY(VisibleAnywhere, Category = "S Save")
	TArray<FName> ActiveGiftIds;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	float DayTimeRemaining = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	int32 CarryOverTargetBonus = 0;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	ESDayEndReason LastDayEndReason = ESDayEndReason::None;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FSRunSnapshot NightStartSnapshot;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FSRunSnapshot DayStartSnapshot;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	TArray<FSPlannedOrder> PlannedDayOrders;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	int32 NextPlannedOrderIndex = 0;
#pragma endregion K2 moonyfli

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FSNightBootstrap PendingNightBootstrap;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FString LastConsumedNightResultId = TEXT("None");

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	TArray<FName> CompletedDayFlags;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	int32 ReviewSeedState = 1001;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	TArray<FString> ConsumedResultIds;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	ESGamePhase Phase = ESGamePhase::PrepareNight;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	bool bAwaitingNightRetry = false;

	UPROPERTY(VisibleAnywhere, Category = "S Save")
	FSGiftBuffState GiftBuffState;
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

#pragma region K2 moonyfli
	/**
	 * 永久入库：除了当前库存，还要补进夜初/日初快照。快照是回档与中途读档的唯一
	 * 依据，只写 Inventory 的话，存档后读回来这笔入库就会被回档丢掉。
	 */
	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool GrantPermanentStock(FName IngredientId, int32 Quantity);
#pragma endregion K2 moonyfli

	UFUNCTION(BlueprintPure, Category = "S Inventory")
	int32 GetQuantity(FName IngredientId) const;

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool TryConsume(FName IngredientId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "S Inventory")
	bool ApplyBatch(const TArray<FSIngredientStack>& Changes);

	UFUNCTION(BlueprintPure, Category = "S Sandbox")
	int32 GetInventoryQuantity(FName IngredientId) const;

	UFUNCTION(BlueprintPure, Category = "S Sandbox")
	FString GetPhaseDisplayName() const;

	UFUNCTION(BlueprintCallable, Category = "S Revenue")
	void AddRevenue(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	int32 GetRecipeSellValue(FName RecipeId) const;

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	static int32 GetBuiltInRecipeSellValue(int32 Level);

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	static FName MakeRecipeId(FName IngredientId, int32 Level);

#pragma region K2 moonyfli
	/** 开店前预生成当日订单出现顺序（含特殊 NPC 槽）。 */
	UFUNCTION(BlueprintCallable, Category = "S Orders")
	bool BuildPlannedDayOrders();

	UFUNCTION(BlueprintPure, Category = "S Orders")
	TArray<FSPlannedOrder> GetPlannedDayOrders() const { return PlannedDayOrders; }

	UFUNCTION(BlueprintPure, Category = "S Orders")
	int32 GetNextPlannedOrderIndex() const { return NextPlannedOrderIndex; }

	UFUNCTION(BlueprintPure, Category = "S Orders")
	int32 GetPlannedOrderTotalValue() const;

	UFUNCTION(BlueprintPure, Category = "S Orders")
	FString GetPlannedOrderSummary() const;

	/** Reveal leading NPC slots, then return the next guest order without advancing past it. */
	UFUNCTION(BlueprintCallable, Category = "S Orders")
	bool TryPrepareNextGuestOrder(FSOrderRequest& OutOrder);

	/** Advance past the current guest slot after a successful delivery. */
	UFUNCTION(BlueprintCallable, Category = "S Orders")
	void AdvancePastCurrentGuestOrder();

	/** Reveal every NPC slot still sitting at the front of the cursor. */
	UFUNCTION(BlueprintCallable, Category = "S Orders")
	void RevealLeadingNpcOrders();
#pragma endregion K2 moonyfli

#pragma region K2 moonyfli
	/** 完成订单即发放并立即生效；没有选礼、没有谢礼背包。 */
	UFUNCTION(BlueprintCallable, Category = "S Gifts")
	bool GrantGift(FName GiftId);

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	TArray<FName> GetActiveGiftIds() const { return ActiveGiftIds; }

	/** 入夜前礼品卡页签文案（只读展示，不可操作）。 */
	UFUNCTION(BlueprintPure, Category = "S Gifts")
	FString GetGiftTabSummary() const;

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	static FString GetGiftEffectText(FName GiftId);

	/** PrepareNight → NightRunning：建立夜初快照。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool StartNight();

	/** 白天时钟由 ASCustomerDirector::Tick 驱动，DayRunning/DayQualified 都要走。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void TickDayClock(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "S Flow")
	float GetDayTimeRemaining() const { return DayTimeRemaining; }

	UFUNCTION(BlueprintPure, Category = "S Flow")
	bool IsShopOpen() const { return Phase == ESGamePhase::DayRunning || Phase == ESGamePhase::DayQualified; }

	/** 立即闭店：达标走日结，未达标回档日初。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool CloseShopNow(ESDayEndReason Reason);

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool ForceCloseShopForDebug();

	/** 调试直接开店：补上倒计时与日初快照，避免时钟一上来就闭店。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void OpenShopForDebug();

	UFUNCTION(BlueprintPure, Category = "S Flow")
	bool CanFulfillOrder(const FSOrderRequest& Order) const;

	/** 当前订单可完成，或仍有食材可做菜；两者皆无即「食材耗尽」。 */
	UFUNCTION(BlueprintPure, Category = "S Flow")
	bool HasCompletableOrder() const;
#pragma endregion K2 moonyfli

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	FSGiftBuffState GetGiftBuffState() const { return GiftBuffState; }

	UFUNCTION(BlueprintPure, Category = "S Gifts")
	static FString GetGiftDisplayName(FName GiftId);

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool ApplyStage(FName InStageId);

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool JumpToStageForDebug(FName InStageId);

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	FSNightBootstrap BuildNightBootstrap();

	UFUNCTION(BlueprintPure, Category = "S Flow")
	FSNightBootstrap GetPendingNightBootstrap() const { return PendingNightBootstrap; }

	UFUNCTION(BlueprintPure, Category = "S Flow")
	FSGameStageRow GetActiveStageRow() const { return ActiveStageRow; }

	UFUNCTION(BlueprintPure, Category = "S Flow")
	FString FormatBootstrapDebug() const;

	UFUNCTION(BlueprintPure, Category = "S Revenue")
	int32 GetRevenueGap() const;

#pragma region K2 moonyfli
	/** 调试：白天判失败，回档日初快照并重开当日。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	bool FailDayForDebug();
#pragma endregion K2 moonyfli

	UFUNCTION(BlueprintCallable, Category = "S Save")
	bool SaveChefProfile();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	bool LoadChefProfile();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	bool DeleteChefProfile();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	bool SimulateCorruptSaveForDebug();

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

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	FName ForkPair = TEXT("AB");

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	float DayDurationSeconds = 60.0f;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	float NightDurationSeconds = 120.0f;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	float CustomerSpawnIntervalSeconds = 7.0f;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	int32 CustomerConcurrentMax = 2;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	int32 Revenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	int32 RevenueTarget = 90;

#pragma region K2 moonyfli
	/** 本日完成订单拿到的谢礼：即时生效，直接带进下一夜。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Gifts")
	TArray<FName> ActiveGiftIds;

	/** 开店剩余时长；归零即闭店。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	float DayTimeRemaining = 0.0f;

	/** 时间结束闭店时，按剩余食材折算并加到下一关营业额目标上。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	int32 CarryOverTargetBonus = 0;

	/** 结转食材每份折算多少营业额目标；Lv0 售价 10，取半价避免囤货直接抵消目标。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S Flow")
	int32 CarryOverTargetBonusPerUnit = 5;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	ESDayEndReason LastDayEndReason = ESDayEndReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	FSRunSnapshot NightStartSnapshot;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	FSRunSnapshot DayStartSnapshot;

	/** 当日预生成订单队列（含 NPC 槽）；日初快照也会备份一份。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Orders")
	TArray<FSPlannedOrder> PlannedDayOrders;

	UPROPERTY(BlueprintReadOnly, Category = "S Orders")
	int32 NextPlannedOrderIndex = 0;
#pragma endregion K2 moonyfli

	UPROPERTY(BlueprintReadOnly, Category = "S Gifts")
	FSGiftBuffState GiftBuffState;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	FSNightBootstrap PendingNightBootstrap;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	FSGameStageRow ActiveStageRow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S Flow")
	TSoftObjectPtr<UDataTable> StageTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S Flow")
	TSoftObjectPtr<UDataTable> RecipeTable;

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	FString LastConsumedNightResultId = TEXT("None");

	UPROPERTY(BlueprintReadOnly, Category = "S Sandbox")
	FString LastBoardFeedback = TEXT("等待操作。");

	/** 夜失败后等待补跑；重跑成功前关卡不前进、白天不开门。 */
	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	bool bAwaitingNightRetry = false;

	UPROPERTY(BlueprintReadOnly, Category = "S Flow")
	TArray<FName> CompletedDayFlags;

	UPROPERTY(BlueprintReadOnly, Category = "S Save")
	FString LastSaveFeedback = TEXT("尚未存档。");

private:
	static const TCHAR* SaveSlotName;
	static const int32 SaveUserIndex;

	UPROPERTY()
	TMap<FName, int32> Inventory;

	UPROPERTY()
	TSet<FString> ConsumedResultIds;

	bool IsKnownIngredient(FName IngredientId) const;
	void InitializeIngredientMaps();
	void NotifyStateChanged();
	void RebuildGiftBuffState();
	bool TryGetStageRow(FName InStageId, FSGameStageRow& OutRow) const;
	static FSGameStageRow MakeBuiltInStageRow(FName InStageId);
	int32 ReclaimBoardPiecesOnClose();

#pragma region K2 moonyfli
	FSRunSnapshot CaptureSnapshot() const;
	void RestoreSnapshot(const FSRunSnapshot& Snapshot);
	/** PrepareDay 边：清空谢礼、营业额归零、建立日初快照，然后直接开店。 */
	void EnterPrepareDay(const FString& Reason);
	void FailDay(ESDayEndReason Reason);
	void EnterDaySettlement(ESDayEndReason Reason);
	void AdvanceToNextStage();
	void ResetDayDirectors(bool bStartService);
	int32 CountPantryUnits() const;
	int32 CountChainUnitsAvailable(FName IngredientId) const;
	bool HasDayResourcesLeft() const;
	static const TCHAR* DayEndReasonText(ESDayEndReason Reason);

	float DayStuckCheckAccum = 0.0f;
	/** 只有本日曾经有过食材，才允许用「食材耗尽」结束当天，避免空档 0.5s 循环回档。 */
	bool bDayHadResources = false;
#pragma endregion K2 moonyfli

	static FString FormatReclaimSuffix(int32 ReclaimedUnits);
	void CaptureProfileToSave(USChefSaveGame& SaveObject) const;
	bool ApplyProfileFromSave(const USChefSaveGame& SaveObject);
	bool AutoSaveChefProfile(const FString& Reason);
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

#pragma region K2 moonyfli
	/** Picks uniformly among enabled empty cells; INDEX_NONE when the board is full. */
	UFUNCTION(BlueprintPure, Category = "S Merge")
	int32 FindRandomEmptyCell() const;
#pragma endregion K2 moonyfli

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

	/** 在空格中随机落点，再扣永久库存，再生成 Lv0；任一步失败都不改库存与棋盘。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool TrySpawnFromMotherPiece(FName IngredientId);

	/** 拖放到目标格：空格则移动；同链同级则合成；否则回弹且棋盘不变。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool TryDropPiece(int32 FromCellIndex, int32 ToCellIndex);

	/** 将高级食材拖回食材区：撤销全部合成并按付费基础单位退回对应库存。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	bool TryDecomposePieceToInventory(int32 CellIndex);

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

#pragma region K2 moonyfli
	/** 统计盘上棋子折算回库存的食材数（不改状态），闭店/存档前用。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	int32 GetPendingReclaimUnits(TMap<FName, int32>& OutUnits) const;

	/** 闭店等清盘场景：先把棋子按付费单位退回库存，再清空棋盘。返回退回总数。 */
	UFUNCTION(BlueprintCallable, Category = "S Merge")
	int32 ReclaimPiecesToInventory();
#pragma endregion K2 moonyfli

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
	void PlacePiece(int32 CellIndex, FName IngredientId, int32 Level, int32 PaidUnits);
	bool CanMergePieces(const FSDishPiece& A, const FSDishPiece& B, FString& OutReason) const;
	int32 FindPairForMerge(FName IngredientId, int32 Level, int32 PreferKeepIndex) const;
};

	/** 第四步：按预生成队列刷客与交付。 */
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
	bool SpawnNextPlannedCustomer();

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
	float SpawnIntervalSeconds = 7.0f;

	float SpawnCooldownRemaining = 0.0f;
	bool bDayServiceActive = false;
	int32 NextCustomerNumber = 1;

	USChefGameInstance* GetChefGameInstance() const;
	void SetFeedback(const FString& Message);
	void ClearActiveCustomer(const FString& Reason);
};

/** 第五步：特殊 NPC 按订单队列揭示；服务成功留下谢礼卡。 */
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

	UFUNCTION(BlueprintCallable, Category = "S NPC")
	bool RevealNpc(FName NpcId);

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
	void BuildNpcsFromPlan();
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

#pragma region K2 moonyfli
	/** Keep the old diagnostic panel available without covering the playable day presentation by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "S Presentation")
	bool bShowDebugPanel = false;
#pragma endregion K2 moonyfli

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

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void DebugJumpToStage(FName InStageId);

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void DebugPrintBootstrap();

	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void DebugFailDay();

#pragma region K2 moonyfli
	/** 单键推进主流程：入夜 → 模拟夜成功 → 闭店结算。 */
	UFUNCTION(BlueprintCallable, Category = "S Flow")
	void AdvanceFlow();
#pragma endregion K2 moonyfli

	UFUNCTION(BlueprintCallable, Category = "S Save")
	void DebugSaveProfile();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	void DebugLoadProfile();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	void DebugCorruptSave();

	UFUNCTION(BlueprintCallable, Category = "S Save")
	void DebugDeleteSave();

#pragma region K2 moonyfli
	/** Also reachable in PIE via the S.Day.RunSmoke console command. */
	UFUNCTION(BlueprintCallable, Category = "S Debug")
	void RunDayWhiteboxSmokeTest();
#pragma endregion K2 moonyfli

private:
	UPROPERTY()
	FSNightResult LastGeneratedResult;

	UPROPERTY()
	TObjectPtr<class USDebugPanel> DebugPanel;

#pragma region K2 moonyfli
	UPROPERTY()
	TObjectPtr<class USDayHUD> DayHUD;

	UPROPERTY()
	TObjectPtr<class ASDayBoardPresenter> DayBoardPresenter;
#pragma endregion K2 moonyfli

	int32 NextResultNumber = 1;
	FSNightResult MakeResult(bool bSuccess);
	void Submit(const FSNightResult& Result);

#pragma region K2 moonyfli
	void FinishDayWhiteboxSmokeTest();
	void CleanupNightPresentation();
	bool bDayWhiteboxSmokePassed = false;
	int32 NightCleanupPassesRemaining = 0;
	FTimerHandle NightCleanupTimerHandle;
#pragma endregion K2 moonyfli
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
	TObjectPtr<UTextBlock> StageSummaryText;

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
	void HandleGrantGiftGuideKite();

	UFUNCTION()
	void HandleGrantGiftLifeLamp();

	UFUNCTION()
	void HandleAdvanceFlow();

	UFUNCTION()
	void HandleJumpT0();

	UFUNCTION()
	void HandleJumpL1();

	UFUNCTION()
	void HandleJumpL2();

	UFUNCTION()
	void HandleJumpL3();

	UFUNCTION()
	void HandlePrintBootstrap();

	UFUNCTION()
	void HandleFailDay();

	UFUNCTION()
	void HandleSaveProfile();

	UFUNCTION()
	void HandleLoadProfile();

	UFUNCTION()
	void HandleCorruptSave();

	UFUNCTION()
	void HandleDeleteSave();

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
	void GrantGift(FName GiftId);
	void JumpStage(FName InStageId);

	/** 开店倒计时不发状态广播，用定时器轮询刷新面板。 */
	FTimerHandle RefreshTimerHandle; //add by K2
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

#pragma region K2 moonyfli
/**
 * Day whitebox entry: same actors as the debug sandbox plus the 3D board presenter.
 * Set as a level's GameMode override so the legacy debug sandbox level stays untouched.
 */
UCLASS()
class MINIGAME_API ASDayWhiteboxGameMode : public ASChefGameMode
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
#pragma endregion K2 moonyfli
