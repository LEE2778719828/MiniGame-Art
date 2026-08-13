#include "SStandaloneSandbox.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSSandbox, Log, All);

const TCHAR* USChefGameInstance::SaveSlotName = TEXT("SG_ChefProfile");
const int32 USChefGameInstance::SaveUserIndex = 0;

namespace
{
	const FName LingGuId(TEXT("LingGu"));
	const FName YinShanJunId(TEXT("YinShanJun"));
	const FName ChiYanJiaoId(TEXT("ChiYanJiao"));
	const FName YueLinYuId(TEXT("YueLinYu"));
	const FName XuanYuQinId(TEXT("XuanYuQin"));

	const FName GiftGuideKiteId(TEXT("GuideKite"));
	const FName GiftLifeLampId(TEXT("LifeLamp"));
	const FName GiftBeatCoinId(TEXT("BeatCoin"));
	const FName GiftGluttonBoxId(TEXT("GluttonBox"));

	const FName NpcALingId(TEXT("ALing"));
	const FName NpcSangPoId(TEXT("SangPo"));

	constexpr int32 MaxDishLevel = 4;
	// Gifts are optional: 0–2 may be carried into the night.
	constexpr int32 MaxGiftSelections = 2;

	const TArray<FName>& GetKnownIds()
	{
		static const TArray<FName> Ids = {LingGuId, YinShanJunId, ChiYanJiaoId, YueLinYuId, XuanYuQinId};
		return Ids;
	}

	bool IsKnownGiftId(const FName GiftId)
	{
		return GiftId == GiftGuideKiteId
			|| GiftId == GiftLifeLampId
			|| GiftId == GiftBeatCoinId
			|| GiftId == GiftGluttonBoxId;
	}

	FString IngredientDisplayName(const FName Id)
	{
		if (Id == LingGuId) return TEXT("灵谷");
		if (Id == YinShanJunId) return TEXT("阴山菌");
		if (Id == ChiYanJiaoId) return TEXT("赤焰椒");
		if (Id == YueLinYuId) return TEXT("月鳞鱼");
		if (Id == XuanYuQinId) return TEXT("玄羽禽");
		return Id.ToString();
	}

	FString PieceShortLabel(const FSDishPiece& Piece)
	{
		return FString::Printf(TEXT("%s%d"), *IngredientDisplayName(Piece.IngredientId).Left(1), Piece.Level);
	}
}

void USChefGameInstance::Init()
{
	Super::Init();
	if (StageTable.IsNull())
	{
		StageTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Game/Day/Data/DT_GameStages.DT_GameStages")));
	}

#pragma region K2 moonyfli
	// Prefer restored profile; fall back to the fixed review starter on miss/corrupt.
	if (!LoadChefProfile())
	{
		ResetSandbox();
		LastSaveFeedback = TEXT("无可用存档，已创建固定评审初始档。");
		NotifyStateChanged();
	}
#pragma endregion K2 moonyfli
}

void USChefGameInstance::InitializeIngredientMaps()
{
	Inventory.Empty();
	TemporaryBasket.Empty();
	for (const FName Id : GetKnownIds())
	{
		Inventory.Add(Id, 0);
		TemporaryBasket.Add(Id, 0);
	}
}

bool USChefGameInstance::IsKnownIngredient(const FName IngredientId) const
{
	return Inventory.Contains(IngredientId);
}

TArray<FName> USChefGameInstance::GetKnownIngredientIds() const
{
	return GetKnownIds();
}

void USChefGameInstance::NotifyStateChanged()
{
	OnSandboxStateChanged.Broadcast();
}

bool USChefGameInstance::AddIngredient(const FName IngredientId, const int32 Quantity)
{
	if (!IsKnownIngredient(IngredientId))
	{
		UE_LOG(LogSSandbox, Warning, TEXT("拒绝 AddIngredient：未知 ID %s"), *IngredientId.ToString());
		LastBoardFeedback = FString::Printf(TEXT("未知食材 ID：%s，已拒绝写入。"), *IngredientId.ToString());
		NotifyStateChanged();
		return false;
	}
	if (Quantity < 0)
	{
		UE_LOG(LogSSandbox, Warning, TEXT("拒绝 AddIngredient：数量不能为负 %s x%d"), *IngredientId.ToString(), Quantity);
		LastBoardFeedback = TEXT("数量不能为负，已拒绝写入。");
		NotifyStateChanged();
		return false;
	}
	if (Quantity == 0)
	{
		return true;
	}

	Inventory.FindOrAdd(IngredientId) += Quantity;
	NotifyStateChanged();
	return true;
}

int32 USChefGameInstance::GetQuantity(const FName IngredientId) const
{
	return Inventory.FindRef(IngredientId);
}

bool USChefGameInstance::TryConsume(const FName IngredientId, const int32 Quantity)
{
	if (!IsKnownIngredient(IngredientId))
	{
		UE_LOG(LogSSandbox, Warning, TEXT("拒绝 TryConsume：未知 ID %s"), *IngredientId.ToString());
		LastBoardFeedback = FString::Printf(TEXT("未知食材 ID：%s，已拒绝扣除。"), *IngredientId.ToString());
		NotifyStateChanged();
		return false;
	}
	if (Quantity <= 0)
	{
		UE_LOG(LogSSandbox, Warning, TEXT("拒绝 TryConsume：无效数量 %s x%d"), *IngredientId.ToString(), Quantity);
		return false;
	}

	const int32 Current = Inventory.FindRef(IngredientId);
	if (Current < Quantity)
	{
		UE_LOG(
			LogSSandbox,
			Warning,
			TEXT("库存不足，无法扣除 %s：需要 %d，现有 %d"),
			*IngredientId.ToString(),
			Quantity,
			Current);
		LastBoardFeedback = FString::Printf(
			TEXT("%s 库存不足（%d/%d），未生成棋子。"),
			*IngredientDisplayName(IngredientId),
			Current,
			Quantity);
		NotifyStateChanged();
		return false;
	}

	Inventory.FindOrAdd(IngredientId) = Current - Quantity;
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::ApplyBatch(const TArray<FSIngredientStack>& Changes)
{
	TMap<FName, int32> Projected = Inventory;
	for (const FSIngredientStack& Stack : Changes)
	{
		if (!IsKnownIngredient(Stack.IngredientId))
		{
			UE_LOG(LogSSandbox, Warning, TEXT("拒绝 ApplyBatch：未知 ID %s"), *Stack.IngredientId.ToString());
			LastBoardFeedback = FString::Printf(TEXT("未知食材 ID：%s，批次写入已拒绝。"), *Stack.IngredientId.ToString());
			NotifyStateChanged();
			return false;
		}

		const int32 Next = Projected.FindRef(Stack.IngredientId) + Stack.Quantity;
		if (Next < 0)
		{
			UE_LOG(
				LogSSandbox,
				Warning,
				TEXT("拒绝 ApplyBatch：%s 结果为负 %d"),
				*Stack.IngredientId.ToString(),
				Next);
			LastBoardFeedback = TEXT("批次写入会导致负库存，已拒绝。");
			NotifyStateChanged();
			return false;
		}
		Projected.FindOrAdd(Stack.IngredientId) = Next;
	}

	Inventory = MoveTemp(Projected);
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::ConsumeNightResult(const FSNightResult& Result)
{
	if (Result.ResultId.IsEmpty())
	{
		UE_LOG(LogSSandbox, Warning, TEXT("拒绝 NightResult：ResultId 为空。"));
		return false;
	}

	if (ConsumedResultIds.Contains(Result.ResultId))
	{
		UE_LOG(LogSSandbox, Warning, TEXT("忽略重复 NightResult：%s"), *Result.ResultId);
		NotifyStateChanged();
		return false;
	}

	for (const FSIngredientStack& Stack : Result.Ingredients)
	{
		if (!IsKnownIngredient(Stack.IngredientId) || Stack.Quantity < 0)
		{
			UE_LOG(
				LogSSandbox,
				Warning,
				TEXT("拒绝 NightResult %s：无效食材 %s x%d。"),
				*Result.ResultId,
				*Stack.IngredientId.ToString(),
				Stack.Quantity);
			return false;
		}
	}

	ConsumedResultIds.Add(Result.ResultId);
	LastConsumedNightResultId = Result.ResultId;
	Phase = ESGamePhase::NightSettlement;

#pragma region K2 moonyfli
	if (Result.bSuccess)
	{
		// Provisional merge rule until design freeze: retry success folds temp basket into inventory.
		if (bAwaitingNightRetry)
		{
			MergeTemporaryBasketIntoInventory();
			bAwaitingNightRetry = false;
		}

		for (const FSIngredientStack& Stack : Result.Ingredients)
		{
			Inventory.FindOrAdd(Stack.IngredientId) += Stack.Quantity;
		}

		// Keep SelectedGiftIds / GiftBuffState / Revenue gap across night success and retries.
		BeginNewDayGiftPool();
		BuildNightBootstrap();
		Phase = ESGamePhase::DayRunning;
		LastBoardFeedback = FString::Printf(
			TEXT("夜结果入库成功。营业额缺口 %d/%d。服务阿翎/桑婆拿谢礼，或接普通顾客。"),
			Revenue,
			RevenueTarget);
	}
	else
	{
		for (const FSIngredientStack& Stack : Result.Ingredients)
		{
			const int32 QuantityToAdd = FMath::FloorToInt(Stack.Quantity * 0.5f);
			TemporaryBasket.FindOrAdd(Stack.IngredientId) += QuantityToAdd;
		}

		// Failure: no day shop, no stage advance, keep inventory/revenue/selected gifts.
		bAwaitingNightRetry = true;
		Phase = ESGamePhase::PrepareNight;
		BuildNightBootstrap();
		LastBoardFeedback = FString::Printf(
			TEXT("夜失败：50%% 入临时食篮，当日不开店。关卡=%s 保留营业额 %d/%d，谢礼继续有效。请补跑当前夜。"),
			*StageId.ToString(),
			Revenue,
			RevenueTarget);
	}
#pragma endregion K2 moonyfli

	UE_LOG(
		LogSSandbox,
		Display,
		TEXT("已消费 NightResult %s：Success=%s，Phase=%s，Retry=%s，Revenue=%d/%d"),
		*Result.ResultId,
		Result.bSuccess ? TEXT("true") : TEXT("false"),
		*GetPhaseDisplayName(),
		bAwaitingNightRetry ? TEXT("true") : TEXT("false"),
		Revenue,
		RevenueTarget);
	NotifyStateChanged();
	AutoSaveChefProfile(Result.bSuccess ? TEXT("成功消费 NightResult") : TEXT("失败保留与补跑状态"));

	if (Result.bSuccess)
	{
		if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
		{
			Director->NotifyDayStarted();
		}
		if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
		{
			NpcDirector->NotifyDayStarted();
		}
	}
	return true;
}

int32 USChefGameInstance::GetInventoryQuantity(const FName IngredientId) const
{
	return GetQuantity(IngredientId);
}

int32 USChefGameInstance::GetTemporaryQuantity(const FName IngredientId) const
{
	return TemporaryBasket.FindRef(IngredientId);
}

FString USChefGameInstance::GetPhaseDisplayName() const
{
	if (const UEnum* Enum = StaticEnum<ESGamePhase>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Phase));
	}
	return TEXT("Unknown");
}

FName USChefGameInstance::MakeRecipeId(const FName IngredientId, const int32 Level)
{
	return FName(*FString::Printf(TEXT("%s_Lv%d"), *IngredientId.ToString(), Level));
}

int32 USChefGameInstance::GetRecipeSellValue(const FName RecipeId)
{
	const FString Id = RecipeId.ToString();
	int32 Level = 0;
	if (Id.EndsWith(TEXT("_Lv0"))) Level = 0;
	else if (Id.EndsWith(TEXT("_Lv1"))) Level = 1;
	else if (Id.EndsWith(TEXT("_Lv2"))) Level = 2;
	else if (Id.EndsWith(TEXT("_Lv3"))) Level = 3;
	else if (Id.EndsWith(TEXT("_Lv4"))) Level = 4;
	else return 0;

	// 第四步占位售价；后续改由 DT_Recipes 驱动。
	static const int32 Values[5] = {10, 22, 48, 100, 220};
	return Values[Level];
}

void USChefGameInstance::AddRevenue(const int32 Amount)
{
	if (Amount <= 0 || Phase != ESGamePhase::DayRunning)
	{
		return;
	}
	Revenue += Amount;
	NotifyStateChanged();
	if (Revenue >= RevenueTarget)
	{
		TryEnterGiftSelect(FString::Printf(TEXT("营业额达标 %d/%d，进入闭店选礼。"), Revenue, RevenueTarget));
	}
}

int32 USChefGameInstance::GetRevenueGap() const
{
	return FMath::Max(0, RevenueTarget - Revenue);
}

FString USChefGameInstance::GetGiftDisplayName(const FName GiftId)
{
	if (GiftId == GiftGuideKiteId) return TEXT("引路纸鸢");
	if (GiftId == GiftLifeLampId) return TEXT("借命纸灯");
	if (GiftId == GiftBeatCoinId) return TEXT("定键铜钱");
	if (GiftId == GiftGluttonBoxId) return TEXT("饕餮食盒");
	return GiftId.ToString();
}

void USChefGameInstance::BeginNewDayGiftPool()
{
	ObtainedGiftIds.Empty();
	PendingGiftIds.Empty();
}

void USChefGameInstance::RebuildGiftBuffState()
{
	GiftBuffState = FSGiftBuffState();
	for (const FName GiftId : SelectedGiftIds)
	{
		if (GiftId == GiftGuideKiteId) GiftBuffState.bGuideKite = true;
		else if (GiftId == GiftLifeLampId) GiftBuffState.bLifeLamp = true;
		else if (GiftId == GiftBeatCoinId) GiftBuffState.bBeatCoin = true;
		else if (GiftId == GiftGluttonBoxId) GiftBuffState.bGluttonBox = true;
	}
}

bool USChefGameInstance::AddObtainedGift(const FName GiftId)
{
	if (!IsKnownGiftId(GiftId))
	{
		LastBoardFeedback = FString::Printf(TEXT("未知谢礼 ID：%s，已拒绝。"), *GiftId.ToString());
		NotifyStateChanged();
		return false;
	}
	if (ObtainedGiftIds.Contains(GiftId))
	{
		LastBoardFeedback = FString::Printf(TEXT("谢礼 %s 本局已获得，不再重复发放。"), *GetGiftDisplayName(GiftId));
		NotifyStateChanged();
		return false;
	}

	ObtainedGiftIds.Add(GiftId);
	LastBoardFeedback = FString::Printf(TEXT("获得谢礼卡：%s（%s）。"), *GetGiftDisplayName(GiftId), *GiftId.ToString());
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::TryEnterGiftSelect(const FString& Reason)
{
	if (Phase == ESGamePhase::GiftSelect)
	{
		return true;
	}
	if (Phase != ESGamePhase::DayRunning)
	{
		LastBoardFeedback = TEXT("当前阶段不能闭店选礼。");
		NotifyStateChanged();
		return false;
	}
	Phase = ESGamePhase::GiftSelect;
	PendingGiftIds.Empty();
	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->ClearActiveDrag();
	}
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
	}
	LastBoardFeedback = Reason.IsEmpty()
		? FString::Printf(TEXT("闭店：谢礼卡 %d 张，最多勾选 %d 件（可不选）后确认进入夜晚。"), ObtainedGiftIds.Num(), MaxGiftSelections)
		: Reason;
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("闭店选礼"));
	return true;
}

bool USChefGameInstance::ForceCloseShopForDebug()
{
	if (Phase != ESGamePhase::DayRunning && Phase != ESGamePhase::GiftSelect)
	{
		LastBoardFeedback = TEXT("仅白天营业中可强制闭店。");
		NotifyStateChanged();
		return false;
	}
	return TryEnterGiftSelect(FString::Printf(
		TEXT("调试强制闭店，进入选礼。谢礼卡 %d 张，可选 0–%d 件。"),
		ObtainedGiftIds.Num(),
		MaxGiftSelections));
}

bool USChefGameInstance::TogglePendingGiftSelection(const FName GiftId)
{
	if (Phase != ESGamePhase::GiftSelect)
	{
		LastBoardFeedback = TEXT("尚未闭店，不能选礼。");
		NotifyStateChanged();
		return false;
	}
	if (!ObtainedGiftIds.Contains(GiftId))
	{
		LastBoardFeedback = FString::Printf(TEXT("%s 不在本次获得列表中。"), *GetGiftDisplayName(GiftId));
		NotifyStateChanged();
		return false;
	}

	if (PendingGiftIds.Contains(GiftId))
	{
		PendingGiftIds.Remove(GiftId);
		LastBoardFeedback = FString::Printf(TEXT("取消勾选 %s。"), *GetGiftDisplayName(GiftId));
		NotifyStateChanged();
		return true;
	}

	if (PendingGiftIds.Num() >= MaxGiftSelections)
	{
		LastBoardFeedback = TEXT("最多勾选两件谢礼；请先取消一件再选。");
		NotifyStateChanged();
		return false;
	}

	PendingGiftIds.Add(GiftId);
	LastBoardFeedback = FString::Printf(
		TEXT("已勾选 %s（%d/%d）。"),
		*GetGiftDisplayName(GiftId),
		PendingGiftIds.Num(),
		MaxGiftSelections);
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::ConfirmGiftSelection()
{
	if (Phase != ESGamePhase::GiftSelect)
	{
		LastBoardFeedback = TEXT("当前不在选礼阶段。");
		NotifyStateChanged();
		return false;
	}
	if (PendingGiftIds.Num() > MaxGiftSelections)
	{
		LastBoardFeedback = FString::Printf(
			TEXT("最多带 %d 件谢礼入夜（当前 %d）。"),
			MaxGiftSelections,
			PendingGiftIds.Num());
		NotifyStateChanged();
		return false;
	}

	for (const FName GiftId : PendingGiftIds)
	{
		if (!ObtainedGiftIds.Contains(GiftId) || !IsKnownGiftId(GiftId))
		{
			LastBoardFeedback = TEXT("选礼校验失败，请重选。");
			NotifyStateChanged();
			return false;
		}
	}

	SelectedGiftIds = PendingGiftIds;
	PendingGiftIds.Empty();
	RebuildGiftBuffState();
	return AdvanceAfterGiftConfirm();
}

FSGameStageRow USChefGameInstance::MakeBuiltInStageRow(const FName InStageId)
{
	FSGameStageRow Row;
	Row.LevelId = InStageId;
	Row.GuaranteedNpcRules = TEXT("ALing_SangPo");

	if (InStageId == TEXT("T0"))
	{
		Row.DisplayName = TEXT("教程日");
		Row.NightDuration = 90.0f;
		Row.ForkPair = TEXT("AB");
		Row.ReviewSeed = 1001;
		Row.DayDuration = 60.0f;
		Row.RevenueTarget = 90;
		Row.CustomerConcurrentMax = 2;
		Row.CustomerSpawnInterval = 7.0f;
		Row.CustomerPatience = 32.0f;
		Row.CustomerConfigId = TEXT("Wave_T0");
		Row.NextLevelId = TEXT("L1");
	}
	else if (InStageId == TEXT("L1"))
	{
		Row.DisplayName = TEXT("第一夜");
		Row.NightDuration = 110.0f;
		Row.ForkPair = TEXT("AB");
		Row.ReviewSeed = 2101;
		Row.DayDuration = 90.0f;
		Row.RevenueTarget = 220;
		Row.CustomerConcurrentMax = 3;
		Row.CustomerSpawnInterval = 5.8f;
		Row.CustomerPatience = 28.0f;
		Row.CustomerConfigId = TEXT("Wave_L1");
		Row.NextLevelId = TEXT("L2");
	}
	else if (InStageId == TEXT("L2"))
	{
		Row.DisplayName = TEXT("第二夜");
		Row.NightDuration = 130.0f;
		Row.ForkPair = TEXT("AC");
		Row.ReviewSeed = 3201;
		Row.DayDuration = 120.0f;
		Row.RevenueTarget = 360;
		Row.CustomerConcurrentMax = 4;
		Row.CustomerSpawnInterval = 5.0f;
		Row.CustomerPatience = 25.0f;
		Row.CustomerConfigId = TEXT("Wave_L2");
		Row.NextLevelId = TEXT("L3");
	}
	else if (InStageId == TEXT("L3"))
	{
		Row.DisplayName = TEXT("第三夜");
		Row.NightDuration = 150.0f;
		Row.ForkPair = TEXT("BC");
		Row.ReviewSeed = 4301;
		Row.DayDuration = 120.0f;
		Row.RevenueTarget = 520;
		Row.CustomerConcurrentMax = 5;
		Row.CustomerSpawnInterval = 4.5f;
		Row.CustomerPatience = 22.0f;
		Row.CustomerConfigId = TEXT("Wave_L3");
		Row.NextLevelId = NAME_None;
		Row.bEndingAfterDay = true;
	}
	else
	{
		Row.LevelId = TEXT("T0");
		return MakeBuiltInStageRow(TEXT("T0"));
	}
	return Row;
}

bool USChefGameInstance::TryGetStageRow(const FName InStageId, FSGameStageRow& OutRow) const
{
	if (InStageId.IsNone())
	{
		return false;
	}

	if (UDataTable* Table = StageTable.LoadSynchronous())
	{
		const FString Context = TEXT("USChefGameInstance::TryGetStageRow");
		if (const FSGameStageRow* Found = Table->FindRow<FSGameStageRow>(InStageId, Context, false))
		{
			OutRow = *Found;
			if (OutRow.LevelId.IsNone())
			{
				OutRow.LevelId = InStageId;
			}
			return true;
		}
	}

	OutRow = MakeBuiltInStageRow(InStageId);
	return OutRow.LevelId == InStageId || InStageId == TEXT("T0");
}

bool USChefGameInstance::ApplyStage(const FName InStageId)
{
	FSGameStageRow Row;
	if (!TryGetStageRow(InStageId, Row))
	{
		LastBoardFeedback = FString::Printf(TEXT("未知关卡 %s，保持当前 Stage。"), *InStageId.ToString());
		NotifyStateChanged();
		return false;
	}

	StageId = Row.LevelId;
	ActiveStageRow = Row;
	ReviewSeed = Row.ReviewSeed;
	ForkPair = Row.ForkPair;
	DayDurationSeconds = Row.DayDuration;
	NightDurationSeconds = Row.NightDuration;
	RevenueTarget = Row.RevenueTarget;
	CustomerSpawnIntervalSeconds = Row.CustomerSpawnInterval;
	CustomerPatienceSeconds = Row.CustomerPatience;
	CustomerConcurrentMax = Row.CustomerConcurrentMax;
	BuildNightBootstrap();
	return true;
}

FSNightBootstrap USChefGameInstance::BuildNightBootstrap()
{
	PendingNightBootstrap = FSNightBootstrap();
	PendingNightBootstrap.LevelId = StageId;
	PendingNightBootstrap.ForkPair = ForkPair;
	PendingNightBootstrap.GiftBuffState = GiftBuffState;
	PendingNightBootstrap.Seed = ReviewSeed;
	PendingNightBootstrap.FoeWeightOverride = GiftBuffState.bGluttonBox ? 1.25f : -1.0f;
	return PendingNightBootstrap;
}

FString USChefGameInstance::FormatBootstrapDebug() const
{
	return PendingNightBootstrap.ToDebugString();
}

bool USChefGameInstance::JumpToStageForDebug(const FName InStageId)
{
	if (!ApplyStage(InStageId))
	{
		return false;
	}

	ObtainedGiftIds.Empty();
	PendingGiftIds.Empty();
	Revenue = 0;
	Phase = ESGamePhase::PrepareNight;
	const int32 ReclaimedUnits = ReclaimBoardPiecesOnClose(); //add by K2
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
	}
	if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		NpcDirector->ResetDirector();
	}

	LastBoardFeedback = FString::Printf(
		TEXT("调试跳关 → %s（目标%d / Seed%d / Fork%s）。%sBootstrap: %s"),
		*StageId.ToString(),
		RevenueTarget,
		ReviewSeed,
		*ForkPair.ToString(),
		*FormatReclaimSuffix(ReclaimedUnits),
		*FormatBootstrapDebug());
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::AdvanceAfterGiftConfirm()
{
	const FName FinishedStage = StageId;
	const bool bEnding = ActiveStageRow.bEndingAfterDay || ActiveStageRow.NextLevelId.IsNone();

#pragma region K2 moonyfli
	CompletedDayFlags.AddUnique(FinishedStage);
	bAwaitingNightRetry = false;
	for (const FName Id : GetKnownIds())
	{
		TemporaryBasket.FindOrAdd(Id) = 0;
	}
#pragma endregion K2 moonyfli

	if (bEnding)
	{
		Phase = ESGamePhase::Ending;
		BuildNightBootstrap();
		LastBoardFeedback = FString::Printf(
			TEXT("L3 日结完成，进入尾声。已选谢礼=%s。不再开启下一夜。"),
			*GiftBuffState.ToDebugString());
		NotifyStateChanged();
		AutoSaveChefProfile(TEXT("L3 尾声选礼确认"));
		return true;
	}

	const FName NextId = ActiveStageRow.NextLevelId;
	Revenue = 0;
	ObtainedGiftIds.Empty();
	const int32 ReclaimedUnits = ReclaimBoardPiecesOnClose(); //add by K2
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
	}
	if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		NpcDirector->ResetDirector();
	}

	if (!ApplyStage(NextId))
	{
		Phase = ESGamePhase::PrepareNight;
		LastBoardFeedback = FString::Printf(TEXT("选礼成功，但推进到 %s 失败。"), *NextId.ToString());
		NotifyStateChanged();
		return false;
	}

	Phase = ESGamePhase::PrepareNight;
	LastBoardFeedback = FString::Printf(
		TEXT("已确认谢礼 %d 件。%s 日结 → 下一夜 %s。%sBootstrap: %s"),
		SelectedGiftIds.Num(),
		*FinishedStage.ToString(),
		*StageId.ToString(),
		*FormatReclaimSuffix(ReclaimedUnits),
		*FormatBootstrapDebug());
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("选礼确认并进入下一夜"));
	return true;
}

#pragma region K2 moonyfli
void USChefGameInstance::MergeTemporaryBasketIntoInventory()
{
	for (const FName Id : GetKnownIds())
	{
		const int32 TempQty = TemporaryBasket.FindRef(Id);
		if (TempQty > 0)
		{
			Inventory.FindOrAdd(Id) += TempQty;
		}
		TemporaryBasket.FindOrAdd(Id) = 0;
	}
}

int32 USChefGameInstance::ReclaimBoardPiecesOnClose()
{
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!Board)
	{
		return 0;
	}
	Board->ClearActiveDrag();
	return Board->ReclaimPiecesToInventory();
}

FString USChefGameInstance::FormatReclaimSuffix(const int32 ReclaimedUnits)
{
	if (ReclaimedUnits <= 0)
	{
		return FString();
	}
	return FString::Printf(TEXT("合成格未用食材 %d 份已退回库存。"), ReclaimedUnits);
}

bool USChefGameInstance::CloseDayKeepGapForDebug()
{
	if (Phase != ESGamePhase::DayRunning)
	{
		LastBoardFeedback = TEXT("仅白天营业中可「保留缺口闭店」。");
		NotifyStateChanged();
		return false;
	}
	if (Revenue >= RevenueTarget)
	{
		LastBoardFeedback = TEXT("已达标请走正常闭店选礼；保留缺口仅用于未达标补跑。");
		NotifyStateChanged();
		return false;
	}

	Phase = ESGamePhase::PrepareNight;
	PendingGiftIds.Empty();
	const int32 ReclaimedUnits = ReclaimBoardPiecesOnClose();
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
	}
	if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		NpcDirector->ResetDirector();
	}

	BuildNightBootstrap();
	LastBoardFeedback = FString::Printf(
		TEXT("未达标闭店：保留库存与营业额缺口 %d（进度 %d/%d）。%s关卡=%s 不前进，下次成功夜继续补缺口。"),
		GetRevenueGap(),
		Revenue,
		RevenueTarget,
		*FormatReclaimSuffix(ReclaimedUnits),
		*StageId.ToString());
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("未达标保留缺口"));
	return true;
}

void USChefGameInstance::CaptureProfileToSave(USChefSaveGame& SaveObject) const
{
	SaveObject.SaveVersion = USChefSaveGame::CurrentSaveVersion;
	SaveObject.CurrentStageId = StageId;
	SaveObject.Inventory = Inventory;

	// The board is not persisted, so fold unspent board pieces back into the saved inventory.
	if (const ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		TMap<FName, int32> BoardUnits;
		if (Board->GetPendingReclaimUnits(BoardUnits) > 0)
		{
			for (const TPair<FName, int32>& Pair : BoardUnits)
			{
				SaveObject.Inventory.FindOrAdd(Pair.Key) += Pair.Value;
			}
		}
	}

	SaveObject.TemporaryBasket = TemporaryBasket;
	SaveObject.RevenueProgress = Revenue;
	SaveObject.SelectedGiftIds = SelectedGiftIds;
	SaveObject.PendingNightBootstrap = PendingNightBootstrap;
	SaveObject.LastConsumedNightResultId = LastConsumedNightResultId;
	SaveObject.CompletedDayFlags = CompletedDayFlags;
	SaveObject.ReviewSeedState = ReviewSeed;
	SaveObject.ConsumedResultIds = ConsumedResultIds.Array();
	SaveObject.Phase = Phase;
	SaveObject.bAwaitingNightRetry = bAwaitingNightRetry;
	SaveObject.GiftBuffState = GiftBuffState;
}

bool USChefGameInstance::ApplyProfileFromSave(const USChefSaveGame& SaveObject)
{
	if (SaveObject.SaveVersion != USChefSaveGame::CurrentSaveVersion)
	{
		UE_LOG(
			LogSSandbox,
			Warning,
			TEXT("存档版本不兼容：file=%d current=%d，回退默认档。"),
			SaveObject.SaveVersion,
			USChefSaveGame::CurrentSaveVersion);
		return false;
	}

	InitializeIngredientMaps();
	for (const TPair<FName, int32>& Pair : SaveObject.Inventory)
	{
		if (IsKnownIngredient(Pair.Key) && Pair.Value >= 0)
		{
			Inventory.FindOrAdd(Pair.Key) = Pair.Value;
		}
	}
	for (const TPair<FName, int32>& Pair : SaveObject.TemporaryBasket)
	{
		if (IsKnownIngredient(Pair.Key) && Pair.Value >= 0)
		{
			TemporaryBasket.FindOrAdd(Pair.Key) = Pair.Value;
		}
	}

	if (!ApplyStage(SaveObject.CurrentStageId.IsNone() ? FName(TEXT("T0")) : SaveObject.CurrentStageId))
	{
		return false;
	}

	Revenue = FMath::Max(0, SaveObject.RevenueProgress);
	SelectedGiftIds = SaveObject.SelectedGiftIds;
	RebuildGiftBuffState();
	if (SaveObject.GiftBuffState.bGuideKite || SaveObject.GiftBuffState.bLifeLamp
		|| SaveObject.GiftBuffState.bBeatCoin || SaveObject.GiftBuffState.bGluttonBox)
	{
		GiftBuffState = SaveObject.GiftBuffState;
	}
	PendingNightBootstrap = SaveObject.PendingNightBootstrap;
	LastConsumedNightResultId = SaveObject.LastConsumedNightResultId.IsEmpty()
		? TEXT("None")
		: SaveObject.LastConsumedNightResultId;
	CompletedDayFlags = SaveObject.CompletedDayFlags;
	ReviewSeed = SaveObject.ReviewSeedState;
	ConsumedResultIds.Empty();
	for (const FString& Id : SaveObject.ConsumedResultIds)
	{
		if (!Id.IsEmpty())
		{
			ConsumedResultIds.Add(Id);
		}
	}
	bAwaitingNightRetry = SaveObject.bAwaitingNightRetry;
	ObtainedGiftIds.Empty();
	PendingGiftIds.Empty();

	// Mid-day exit policy for this sandbox: reopen day start, keep inventory/revenue.
	if (SaveObject.Phase == ESGamePhase::DayRunning
		|| SaveObject.Phase == ESGamePhase::GiftSelect
		|| SaveObject.Phase == ESGamePhase::DayOpening
		|| SaveObject.Phase == ESGamePhase::NightSettlement)
	{
		Phase = bAwaitingNightRetry ? ESGamePhase::PrepareNight : ESGamePhase::DayRunning;
	}
	else if (SaveObject.Phase == ESGamePhase::Ending)
	{
		Phase = ESGamePhase::Ending;
	}
	else
	{
		Phase = ESGamePhase::PrepareNight;
	}

	BuildNightBootstrap();

	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->ClearActiveDrag();
		Board->ClearBoard();
	}
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
		if (Phase == ESGamePhase::DayRunning)
		{
			Director->NotifyDayStarted();
		}
	}
	if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		NpcDirector->ResetDirector();
		if (Phase == ESGamePhase::DayRunning)
		{
			NpcDirector->NotifyDayStarted();
		}
	}

	return true;
}

bool USChefGameInstance::SaveChefProfile()
{
	USChefSaveGame* SaveObject = Cast<USChefSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USChefSaveGame::StaticClass()));
	if (!SaveObject)
	{
		LastSaveFeedback = TEXT("创建存档对象失败。");
		NotifyStateChanged();
		return false;
	}

	CaptureProfileToSave(*SaveObject);
	const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, SaveUserIndex);
	LastSaveFeedback = bOk
		? FString::Printf(
			TEXT("已保存 %s：Stage=%s Revenue=%d/%d Result=%s Retry=%s"),
			SaveSlotName,
			*StageId.ToString(),
			Revenue,
			RevenueTarget,
			*LastConsumedNightResultId,
			bAwaitingNightRetry ? TEXT("true") : TEXT("false"))
		: TEXT("SaveGameToSlot 失败。");
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *LastSaveFeedback);
	NotifyStateChanged();
	return bOk;
}

bool USChefGameInstance::AutoSaveChefProfile(const FString& Reason)
{
	const bool bOk = SaveChefProfile();
	if (bOk)
	{
		LastSaveFeedback = FString::Printf(TEXT("%s｜自动存档成功。"), *Reason);
		NotifyStateChanged();
	}
	return bOk;
}

bool USChefGameInstance::LoadChefProfile()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		LastSaveFeedback = TEXT("存档槽为空。");
		return false;
	}

	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex);
	USChefSaveGame* SaveObject = Cast<USChefSaveGame>(Loaded);
	if (!SaveObject)
	{
		UE_LOG(LogSSandbox, Warning, TEXT("坏档：无法反序列化为 USChefSaveGame，回退默认档。"));
		LastSaveFeedback = TEXT("坏档或类型不匹配，已回退默认档。");
		ResetSandbox();
		return true;
	}

	if (!ApplyProfileFromSave(*SaveObject))
	{
		UE_LOG(LogSSandbox, Warning, TEXT("坏档：ApplyProfileFromSave 失败，回退默认档。"));
		ResetSandbox();
		LastSaveFeedback = TEXT("坏档/版本不兼容，已回退默认档。");
		LastBoardFeedback = LastSaveFeedback;
		NotifyStateChanged();
		return true;
	}

	LastBoardFeedback = FString::Printf(
		TEXT("已读档：Stage=%s Phase=%s Revenue=%d/%d Result=%s"),
		*StageId.ToString(),
		*GetPhaseDisplayName(),
		Revenue,
		RevenueTarget,
		*LastConsumedNightResultId);
	LastSaveFeedback = LastBoardFeedback;
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *LastSaveFeedback);
	NotifyStateChanged();
	return true;
}

bool USChefGameInstance::DeleteChefProfile()
{
	const bool bOk = !UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex)
		|| UGameplayStatics::DeleteGameInSlot(SaveSlotName, SaveUserIndex);
	LastSaveFeedback = bOk ? TEXT("存档槽已清空。") : TEXT("删除存档失败。");
	NotifyStateChanged();
	return bOk;
}

bool USChefGameInstance::SimulateCorruptSaveForDebug()
{
	USChefSaveGame* SaveObject = Cast<USChefSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USChefSaveGame::StaticClass()));
	if (!SaveObject)
	{
		LastSaveFeedback = TEXT("无法创建坏档对象。");
		NotifyStateChanged();
		return false;
	}

	CaptureProfileToSave(*SaveObject);
	SaveObject->SaveVersion = -999;
	SaveObject->CurrentStageId = NAME_None;
	const bool bOk = UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, SaveUserIndex);
	LastSaveFeedback = bOk
		? TEXT("已写入坏档（SaveVersion=-999）。下次读档应回退默认档。")
		: TEXT("写入坏档失败。");
	NotifyStateChanged();
	return bOk;
}
#pragma endregion K2 moonyfli

void USChefGameInstance::ResetSandbox()
{
	InitializeIngredientMaps();
	ConsumedResultIds.Empty();
	ObtainedGiftIds.Empty();
	PendingGiftIds.Empty();
	SelectedGiftIds.Empty();
	GiftBuffState = FSGiftBuffState();
	Revenue = 0;
	bAwaitingNightRetry = false;
	CompletedDayFlags.Empty();
	LastConsumedNightResultId = TEXT("None");
	LastSaveFeedback = TEXT("沙盒内存已重置（未自动删档）。");
	ApplyStage(TEXT("T0"));
	LastBoardFeedback = TEXT("沙盒已重置到 T0。");
	Phase = ESGamePhase::PrepareNight;
	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->ClearActiveDrag();
		Board->ClearBoard();
	}
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ResetDirector();
	}
	if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		NpcDirector->ResetDirector();
	}
	NotifyStateChanged();
	UE_LOG(LogSSandbox, Display, TEXT("S 独立沙盒已重置。"));
}

ASMergeBoard::ASMergeBoard()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASMergeBoard::BeginPlay()
{
	Super::BeginPlay();
	if (Cells.Num() == 0)
	{
		BuildDefaultIrregularBoard();
	}
}

void ASMergeBoard::BuildDefaultIrregularBoard()
{
	CancelPieceDrag();
	GridWidth = 4;
	GridHeight = 4;
	Cells.SetNum(GridWidth * GridHeight);

	auto IsCorner = [this](const int32 Index)
	{
		const int32 X = Index % GridWidth;
		const int32 Y = Index / GridWidth;
		const bool bLeft = X == 0;
		const bool bRight = X == GridWidth - 1;
		const bool bTop = Y == 0;
		const bool bBottom = Y == GridHeight - 1;
		return (bLeft || bRight) && (bTop || bBottom);
	};

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FSMergeCell& Cell = Cells[Index];
		Cell = FSMergeCell();
		Cell.bEnabled = !IsCorner(Index);
		Cell.bOccupied = false;
	}

	UE_LOG(
		LogSSandbox,
		Display,
		TEXT("不规则棋盘已构建：%dx%d，启用格 %d"),
		GridWidth,
		GridHeight,
		GetEnabledCellCount());
	SetFeedback(FString::Printf(TEXT("棋盘就绪：启用格 %d，空格 %d。拖同链同级棋子可合成。"), GetEnabledCellCount(), GetEmptyCellCount()));
}

void ASMergeBoard::ClearBoard()
{
	CancelPieceDrag();
	for (FSMergeCell& Cell : Cells)
	{
		Cell.bOccupied = false;
		Cell.Piece = FSDishPiece();
	}
	SetFeedback(TEXT("棋盘已清空。"));
}

int32 ASMergeBoard::GetEnabledCellCount() const
{
	int32 Count = 0;
	for (const FSMergeCell& Cell : Cells)
	{
		if (Cell.bEnabled)
		{
			++Count;
		}
	}
	return Count;
}

int32 ASMergeBoard::GetOccupiedCellCount() const
{
	int32 Count = 0;
	for (const FSMergeCell& Cell : Cells)
	{
		if (Cell.bEnabled && Cell.bOccupied)
		{
			++Count;
		}
	}
	return Count;
}

int32 ASMergeBoard::GetEmptyCellCount() const
{
	return GetEnabledCellCount() - GetOccupiedCellCount();
}

int32 ASMergeBoard::FindFirstEmptyCell() const
{
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FSMergeCell& Cell = Cells[Index];
		if (Cell.bEnabled && !Cell.bOccupied)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool ASMergeBoard::IsFull() const
{
	return FindFirstEmptyCell() == INDEX_NONE;
}

bool ASMergeBoard::IsValidCellIndex(const int32 CellIndex) const
{
	return Cells.IsValidIndex(CellIndex) && Cells[CellIndex].bEnabled;
}

bool ASMergeBoard::TryGetPiece(const int32 CellIndex, FSDishPiece& OutPiece) const
{
	if (!IsValidCellIndex(CellIndex) || !Cells[CellIndex].bOccupied)
	{
		return false;
	}
	OutPiece = Cells[CellIndex].Piece;
	return true;
}

int32 ASMergeBoard::CountPiecesAtLevel(const FName IngredientId, const int32 Level) const
{
	int32 Count = 0;
	for (const FSMergeCell& Cell : Cells)
	{
		if (Cell.bEnabled && Cell.bOccupied && Cell.Piece.IngredientId == IngredientId && Cell.Piece.Level == Level)
		{
			++Count;
		}
	}
	return Count;
}

int32 ASMergeBoard::GetHighestLevel(const FName IngredientId) const
{
	int32 Highest = -1;
	for (const FSMergeCell& Cell : Cells)
	{
		if (Cell.bEnabled && Cell.bOccupied && Cell.Piece.IngredientId == IngredientId)
		{
			Highest = FMath::Max(Highest, Cell.Piece.Level);
		}
	}
	return Highest;
}

FName ASMergeBoard::MakeRecipeId(const FName IngredientId, const int32 Level)
{
	return USChefGameInstance::MakeRecipeId(IngredientId, Level);
}

bool ASMergeBoard::RemovePieceAt(const int32 CellIndex)
{
	if (!IsValidCellIndex(CellIndex) || !Cells[CellIndex].bOccupied)
	{
		return false;
	}
	if (ActiveDragCellIndex == CellIndex)
	{
		ClearActiveDrag();
	}
	ClearCell(CellIndex);
	return true;
}

USChefGameInstance* ASMergeBoard::GetChefGameInstance() const
{
	return GetGameInstance<USChefGameInstance>();
}

void ASMergeBoard::SetFeedback(const FString& Message)
{
	if (USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		GameInstance->LastBoardFeedback = Message;
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *Message);
}

void ASMergeBoard::ClearCell(const int32 CellIndex)
{
	if (!Cells.IsValidIndex(CellIndex))
	{
		return;
	}
	Cells[CellIndex].bOccupied = false;
	Cells[CellIndex].Piece = FSDishPiece();
}

void ASMergeBoard::PlacePiece(const int32 CellIndex, const FName IngredientId, const int32 Level, const int32 PaidUnits)
{
	FSMergeCell& Cell = Cells[CellIndex];
	Cell.bOccupied = true;
	Cell.Piece.IngredientId = IngredientId;
	Cell.Piece.Level = Level;
	Cell.Piece.RecipeId = MakeRecipeId(IngredientId, Level);
	Cell.Piece.CellIndex = CellIndex;
	Cell.Piece.PaidUnits = FMath::Max(0, PaidUnits); //add by K2
}

#pragma region K2 moonyfli
int32 ASMergeBoard::GetPendingReclaimUnits(TMap<FName, int32>& OutUnits) const
{
	OutUnits.Empty();
	int32 Total = 0;
	for (const FSMergeCell& Cell : Cells)
	{
		if (!Cell.bOccupied || Cell.Piece.PaidUnits <= 0 || Cell.Piece.IngredientId.IsNone())
		{
			continue;
		}
		OutUnits.FindOrAdd(Cell.Piece.IngredientId) += Cell.Piece.PaidUnits;
		Total += Cell.Piece.PaidUnits;
	}
	return Total;
}

int32 ASMergeBoard::ReclaimPiecesToInventory()
{
	TMap<FName, int32> Refunds;
	const int32 Total = GetPendingReclaimUnits(Refunds);

	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		// No inventory owner: keep pieces so nothing is silently destroyed.
		return 0;
	}

	TArray<FString> Report;
	for (const TPair<FName, int32>& Pair : Refunds)
	{
		if (GameInstance->AddIngredient(Pair.Key, Pair.Value))
		{
			Report.Add(FString::Printf(TEXT("%s+%d"), *IngredientDisplayName(Pair.Key), Pair.Value));
		}
	}

	ClearActiveDrag();
	ClearBoard();

	if (Total > 0)
	{
		UE_LOG(LogSSandbox, Display, TEXT("闭店回收合成格：退回 %d 份（%s）"), Total, *FString::Join(Report, TEXT("、")));
	}
	return Total;
}
#pragma endregion K2 moonyfli

bool ASMergeBoard::CanMergePieces(const FSDishPiece& A, const FSDishPiece& B, FString& OutReason) const
{
	if (A.IngredientId != B.IngredientId)
	{
		OutReason = TEXT("不同食材链，不能合成，已回弹。");
		return false;
	}
	if (A.Level != B.Level)
	{
		OutReason = TEXT("等级不同，不能合成，已回弹。");
		return false;
	}
	if (A.Level >= MaxDishLevel)
	{
		OutReason = TEXT("Lv4 已封顶，不能继续合成，已回弹。");
		return false;
	}
	return true;
}

bool ASMergeBoard::TrySpawnFromMotherPiece(const FName IngredientId)
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	if (GameInstance->Phase != ESGamePhase::DayRunning)
	{
		SetFeedback(TEXT("尚未开店（非 DayRunning），无法取材。"));
		return false;
	}

	if (Cells.Num() == 0)
	{
		BuildDefaultIrregularBoard();
	}

	const int32 EmptyIndex = FindFirstEmptyCell();
	if (EmptyIndex == INDEX_NONE)
	{
		SetFeedback(TEXT("棋盘已满，拒绝生成，库存未扣除。"));
		return false;
	}

	const int32 QuantityBefore = GameInstance->GetQuantity(IngredientId);
	if (!GameInstance->TryConsume(IngredientId, 1))
	{
		return false;
	}

	PlacePiece(EmptyIndex, IngredientId, 0, 1); //add by K2
	const int32 QuantityAfter = GameInstance->GetQuantity(IngredientId);
	SetFeedback(FString::Printf(
		TEXT("母棋子 %s：空格 #%d 生成 Lv0，库存 %d→%d，占用 %d/%d。"),
		*IngredientDisplayName(IngredientId),
		EmptyIndex,
		QuantityBefore,
		QuantityAfter,
		GetOccupiedCellCount(),
		GetEnabledCellCount()));
	return true;
}

bool ASMergeBoard::BeginPieceDrag(const int32 CellIndex, const int32 PointerId)
{
	FSDishPiece Piece;
	if (!TryGetPiece(CellIndex, Piece))
	{
		return false;
	}

	if (ActiveDragCellIndex != INDEX_NONE)
	{
		if (ActiveDragCellIndex == CellIndex && ActiveDragPointerId == PointerId)
		{
			return true;
		}
		SetFeedback(TEXT("已有棋子在拖拽中，同一时间只允许一个 Pointer 持有。"));
		return false;
	}

	ActiveDragCellIndex = CellIndex;
	ActiveDragPointerId = PointerId;
	SetFeedback(FString::Printf(TEXT("开始拖拽 #%d %s（Pointer %d）。"), CellIndex, *PieceShortLabel(Piece), PointerId));
	return true;
}

void ASMergeBoard::ClearActiveDrag()
{
	ActiveDragCellIndex = INDEX_NONE;
	ActiveDragPointerId = INDEX_NONE;
}

void ASMergeBoard::CancelPieceDrag(const int32 PointerId)
{
	if (ActiveDragCellIndex == INDEX_NONE)
	{
		return;
	}
	if (PointerId != INDEX_NONE && ActiveDragPointerId != PointerId)
	{
		return;
	}

	const int32 Released = ActiveDragCellIndex;
	ClearActiveDrag();
	SetFeedback(FString::Printf(TEXT("拖拽取消，棋子已安全留在原格 #%d。"), Released));
}

bool ASMergeBoard::TryDropPiece(const int32 FromCellIndex, const int32 ToCellIndex)
{
	const int32 LockedFrom = ActiveDragCellIndex;
	const int32 LockedPointer = ActiveDragPointerId;
	ActiveDragCellIndex = INDEX_NONE;
	ActiveDragPointerId = INDEX_NONE;

	if (LockedFrom != INDEX_NONE && LockedFrom != FromCellIndex)
	{
		SetFeedback(TEXT("拖拽来源与锁定格不一致，已回弹。"));
		return false;
	}

	if (FromCellIndex == ToCellIndex)
	{
		SetFeedback(TEXT("拖回原格，无变化。"));
		return false;
	}

	FSDishPiece FromPiece;
	if (!TryGetPiece(FromCellIndex, FromPiece))
	{
		SetFeedback(TEXT("来源格没有棋子，已回弹。"));
		return false;
	}

	if (!IsValidCellIndex(ToCellIndex))
	{
		SetFeedback(TEXT("目标格无效，已回弹。"));
		return false;
	}

	FSMergeCell& ToCell = Cells[ToCellIndex];
	if (!ToCell.bOccupied)
	{
		ClearCell(FromCellIndex);
		PlacePiece(ToCellIndex, FromPiece.IngredientId, FromPiece.Level, FromPiece.PaidUnits);
		SetFeedback(FString::Printf(
			TEXT("移动：%s #%d → #%d。"),
			*PieceShortLabel(FromPiece),
			FromCellIndex,
			ToCellIndex));
		return true;
	}

	FSDishPiece ToPiece = ToCell.Piece;
	FString Reason;
	if (!CanMergePieces(FromPiece, ToPiece, Reason))
	{
		SetFeedback(Reason);
		return false;
	}

	const int32 NextLevel = ToPiece.Level + 1;
	ClearCell(FromCellIndex);
	// Merged piece carries the summed cost of both inputs so a later close refunds the full amount.
	PlacePiece(ToCellIndex, ToPiece.IngredientId, NextLevel, FromPiece.PaidUnits + ToPiece.PaidUnits);
	SetFeedback(FString::Printf(
		TEXT("合成成功：%s + %s → %s%d（格 #%d）。Pointer锁定已释放。"),
		*PieceShortLabel(FromPiece),
		*PieceShortLabel(ToPiece),
		*IngredientDisplayName(ToPiece.IngredientId).Left(1),
		NextLevel,
		ToCellIndex));
	(void)LockedPointer;
	return true;
}

void ASMergeBoard::ForceFillBoardForDebug()
{
	if (Cells.Num() == 0)
	{
		BuildDefaultIrregularBoard();
	}

	CancelPieceDrag();
	int32 Filled = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FSMergeCell& Cell = Cells[Index];
		if (!Cell.bEnabled || Cell.bOccupied)
		{
			continue;
		}
		// Debug fill does not touch inventory, so PaidUnits stays 0 and cannot be refunded later.
		PlacePiece(Index, LingGuId, 0, 0);
		++Filled;
	}

	SetFeedback(FString::Printf(TEXT("调试强制满盘：新占 %d 格，现占用 %d/%d。"), Filled, GetOccupiedCellCount(), GetEnabledCellCount()));
}

int32 ASMergeBoard::FindPairForMerge(const FName IngredientId, const int32 Level, const int32 PreferKeepIndex) const
{
	int32 First = INDEX_NONE;
	int32 Second = INDEX_NONE;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FSMergeCell& Cell = Cells[Index];
		if (!(Cell.bEnabled && Cell.bOccupied && Cell.Piece.IngredientId == IngredientId && Cell.Piece.Level == Level))
		{
			continue;
		}
		if (First == INDEX_NONE)
		{
			First = Index;
		}
		else
		{
			Second = Index;
			break;
		}
	}

	if (First == INDEX_NONE || Second == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PreferKeepIndex == First || PreferKeepIndex == Second)
	{
		return PreferKeepIndex;
	}
	return Second;
}

bool ASMergeBoard::DebugPromoteChainToLv4(const FName IngredientId)
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	if (GameInstance->Phase != ESGamePhase::DayRunning)
	{
		GameInstance->Phase = ESGamePhase::DayRunning;
	}

	ReclaimPiecesToInventory(); //add by K2

	// Lv4 需要 16 个 Lv0。
	constexpr int32 NeedLv0 = 16;
	while (GameInstance->GetQuantity(IngredientId) < NeedLv0)
	{
		if (!GameInstance->AddIngredient(IngredientId, NeedLv0))
		{
			SetFeedback(FString::Printf(TEXT("无法补充 %s 库存。"), *IngredientDisplayName(IngredientId)));
			return false;
		}
	}

	auto TryMergeAny = [this, IngredientId](const int32 Level) -> bool
	{
		int32 Keep = INDEX_NONE;
		int32 Consume = INDEX_NONE;
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			const FSMergeCell& Cell = Cells[Index];
			if (!(Cell.bEnabled && Cell.bOccupied && Cell.Piece.IngredientId == IngredientId && Cell.Piece.Level == Level))
			{
				continue;
			}
			if (Keep == INDEX_NONE)
			{
				Keep = Index;
			}
			else
			{
				Consume = Index;
				break;
			}
		}
		if (Keep == INDEX_NONE || Consume == INDEX_NONE)
		{
			return false;
		}
		return TryDropPiece(Consume, Keep);
	};

	int32 Guard = 0;
	while (GetHighestLevel(IngredientId) < MaxDishLevel && Guard++ < 256)
	{
		bool bProgress = false;
		for (int32 Level = MaxDishLevel - 1; Level >= 0; --Level)
		{
			while (CountPiecesAtLevel(IngredientId, Level) >= 2)
			{
				if (!TryMergeAny(Level))
				{
					break;
				}
				bProgress = true;
			}
		}

		if (GetHighestLevel(IngredientId) >= MaxDishLevel)
		{
			break;
		}

		if (FindFirstEmptyCell() != INDEX_NONE && GameInstance->GetQuantity(IngredientId) > 0)
		{
			if (TrySpawnFromMotherPiece(IngredientId))
			{
				bProgress = true;
			}
		}

		if (!bProgress)
		{
			break;
		}
	}

	const bool bOk = GetHighestLevel(IngredientId) >= MaxDishLevel;
	SetFeedback(bOk
		? FString::Printf(TEXT("%s 已合到 Lv4。棋盘占用 %d/%d。"), *IngredientDisplayName(IngredientId), GetOccupiedCellCount(), GetEnabledCellCount())
		: FString::Printf(TEXT("%s 未能合到 Lv4，最高 Lv%d。"), *IngredientDisplayName(IngredientId), GetHighestLevel(IngredientId)));
	return bOk;
}

bool ASMergeBoard::DebugPromoteAllChainsToLv4()
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	GameInstance->Phase = ESGamePhase::DayRunning;
	CancelPieceDrag();

	TArray<FString> Report;
	bool bAllOk = true;
	for (const FName Id : GetKnownIds())
	{
		const bool bOk = DebugPromoteChainToLv4(Id);
		bAllOk &= bOk;
		Report.Add(FString::Printf(TEXT("%s:Lv%d"), *IngredientDisplayName(Id).Left(1), GetHighestLevel(Id)));
		// 保留该链 Lv4 棋子会占格；下一条链前清空（食材退回库存），只验证“能合到”，结果写反馈。
		if (Id != GetKnownIds().Last())
		{
			ReclaimPiecesToInventory();
		}
	}

	SetFeedback(FString::Printf(
		TEXT("五链验收 %s｜%s"),
		bAllOk ? TEXT("通过") : TEXT("失败"),
		*FString::Join(Report, TEXT(" "))));
	return bAllOk;
}

ASMergeBoard* ASMergeBoard::FindBoard(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return Cast<ASMergeBoard>(UGameplayStatics::GetActorOfClass(World, ASMergeBoard::StaticClass()));
}

ASCustomerDirector::ASCustomerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASCustomerDirector::BeginPlay()
{
	Super::BeginPlay();
	ResetDirector();
}

void ASCustomerDirector::ResetDirector()
{
	ActiveCustomer = FSCustomerState();
	SpawnCooldownRemaining = 0.0f;
	bDayServiceActive = false;
	NextCustomerNumber = 1;
}

void ASCustomerDirector::NotifyDayStarted()
{
	bDayServiceActive = true;
	SpawnCooldownRemaining = 0.0f;
	if (const USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		FixedPatienceSeconds = GameInstance->CustomerPatienceSeconds;
		SpawnIntervalSeconds = GameInstance->CustomerSpawnIntervalSeconds;
	}
	SpawnFixedCustomer();
}

USChefGameInstance* ASCustomerDirector::GetChefGameInstance() const
{
	return GetGameInstance<USChefGameInstance>();
}

void ASCustomerDirector::SetFeedback(const FString& Message)
{
	if (USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		GameInstance->LastBoardFeedback = Message;
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *Message);
}

FSOrderRequest ASCustomerDirector::MakeFixedOrder() const
{
	FSOrderRequest Order;
	Order.IngredientId = TEXT("LingGu");
	Order.Level = 0;
	Order.RecipeId = USChefGameInstance::MakeRecipeId(Order.IngredientId, Order.Level);
	Order.SellValue = USChefGameInstance::GetRecipeSellValue(Order.RecipeId);
	return Order;
}

bool ASCustomerDirector::SpawnFixedCustomer()
{
	if (ActiveCustomer.bActive)
	{
		SetFeedback(TEXT("已有顾客在场，不再重复生成。"));
		return false;
	}

	ActiveCustomer = FSCustomerState();
	ActiveCustomer.bActive = true;
	ActiveCustomer.CustomerId = FString::Printf(TEXT("Guest-%02d"), NextCustomerNumber++);
	ActiveCustomer.Order = MakeFixedOrder();
	ActiveCustomer.PatienceMax = FixedPatienceSeconds;
	ActiveCustomer.PatienceRemaining = FixedPatienceSeconds;
	SpawnCooldownRemaining = 0.0f;

	SetFeedback(FString::Printf(
		TEXT("顾客 %s 入座，固定订单 %s（售价 %d），耐心 %.0fs。选中对应棋子后点顾客交付。"),
		*ActiveCustomer.CustomerId,
		*ActiveCustomer.Order.RecipeId.ToString(),
		ActiveCustomer.Order.SellValue,
		ActiveCustomer.PatienceRemaining));
	return true;
}

void ASCustomerDirector::ClearActiveCustomer(const FString& Reason)
{
	ActiveCustomer = FSCustomerState();
	SpawnCooldownRemaining = SpawnIntervalSeconds;
	SetFeedback(Reason);
}

void ASCustomerDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance || GameInstance->Phase != ESGamePhase::DayRunning || !bDayServiceActive)
	{
		return;
	}

	bool bDirty = false;
	if (ActiveCustomer.bActive)
	{
		ActiveCustomer.PatienceRemaining = FMath::Max(0.0f, ActiveCustomer.PatienceRemaining - DeltaSeconds);
		bDirty = true;
		if (ActiveCustomer.PatienceRemaining <= 0.0f)
		{
			ClearActiveCustomer(FString::Printf(
				TEXT("顾客 %s 耐心耗尽离场，未扣棋子。下一位约 %.0fs 后出现。"),
				*ActiveCustomer.CustomerId,
				SpawnIntervalSeconds));
			return;
		}
	}
	else
	{
		SpawnCooldownRemaining = FMath::Max(0.0f, SpawnCooldownRemaining - DeltaSeconds);
		bDirty = true;
		if (SpawnCooldownRemaining <= 0.0f)
		{
			SpawnFixedCustomer();
			return;
		}
	}

	// 节流刷新 HUD 耐心条。
	HudRefreshAccum += DeltaSeconds;
	if (bDirty && HudRefreshAccum >= 0.25f)
	{
		HudRefreshAccum = 0.0f;
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
}

bool ASCustomerDirector::TryDeliverSelectedPiece()
{
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!Board || !Board->IsDragging())
	{
		SetFeedback(TEXT("请先点选棋盘上的棋子，再点顾客交付。"));
		return false;
	}
	return TryDeliverFromCell(Board->GetActiveDragCellIndex());
}

bool ASCustomerDirector::TryDeliverFromCell(const int32 CellIndex)
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!GameInstance || !Board)
	{
		return false;
	}

	if (GameInstance->Phase != ESGamePhase::DayRunning)
	{
		SetFeedback(TEXT("尚未开店，无法交付。"));
		return false;
	}

	if (!ActiveCustomer.bActive)
	{
		SetFeedback(TEXT("当前没有等候顾客。"));
		Board->ClearActiveDrag();
		return false;
	}

	FSDishPiece Piece;
	if (!Board->TryGetPiece(CellIndex, Piece))
	{
		SetFeedback(TEXT("来源格没有棋子，交付取消。"));
		Board->ClearActiveDrag();
		return false;
	}

	if (Piece.RecipeId != ActiveCustomer.Order.RecipeId)
	{
		SetFeedback(FString::Printf(
			TEXT("订单需要 %s，当前是 %s，已回弹，未扣棋子/营业额。"),
			*ActiveCustomer.Order.RecipeId.ToString(),
			*Piece.RecipeId.ToString()));
		Board->ClearActiveDrag();
		return false;
	}

	const int32 SellValue = ActiveCustomer.Order.SellValue > 0
		? ActiveCustomer.Order.SellValue
		: USChefGameInstance::GetRecipeSellValue(Piece.RecipeId);
	const int32 RevenueBefore = GameInstance->Revenue;

	if (!Board->RemovePieceAt(CellIndex))
	{
		SetFeedback(TEXT("移除棋子失败，交付中止。"));
		return false;
	}

	GameInstance->AddRevenue(SellValue);
	const FString ServedId = ActiveCustomer.CustomerId;
	ClearActiveCustomer(FString::Printf(
		TEXT("交付成功：%s 收到 %s，营业额 %d→%d。下一位约 %.0fs 后出现。"),
		*ServedId,
		*Piece.RecipeId.ToString(),
		RevenueBefore,
		GameInstance->Revenue,
		SpawnIntervalSeconds));
	return true;
}

ASCustomerDirector* ASCustomerDirector::FindDirector(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return Cast<ASCustomerDirector>(UGameplayStatics::GetActorOfClass(World, ASCustomerDirector::StaticClass()));
}

ASSpecialNpcDirector::ASSpecialNpcDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASSpecialNpcDirector::BeginPlay()
{
	Super::BeginPlay();
	ResetDirector();
}

void ASSpecialNpcDirector::ResetDirector()
{
	bDayServiceActive = false;
	Npcs.Empty();
}

void ASSpecialNpcDirector::NotifyDayStarted()
{
	bDayServiceActive = true;
	BuildGuaranteedNpcs();
	SetFeedback(TEXT("保底 NPC 阿翎、桑婆已入店（无耐心倒计时）。选中对应棋子后点 NPC 交付拿谢礼。"));
}

USChefGameInstance* ASSpecialNpcDirector::GetChefGameInstance() const
{
	return GetGameInstance<USChefGameInstance>();
}

void ASSpecialNpcDirector::SetFeedback(const FString& Message)
{
	if (USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		GameInstance->LastBoardFeedback = Message;
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *Message);
}

FSOrderRequest ASSpecialNpcDirector::MakeOrder(const FName IngredientId, const int32 Level) const
{
	FSOrderRequest Order;
	Order.IngredientId = IngredientId;
	Order.Level = Level;
	Order.RecipeId = USChefGameInstance::MakeRecipeId(IngredientId, Level);
	Order.SellValue = USChefGameInstance::GetRecipeSellValue(Order.RecipeId);
	return Order;
}

void ASSpecialNpcDirector::BuildGuaranteedNpcs()
{
	Npcs.Reset();

	FSSpecialNpcState ALing;
	ALing.NpcId = NpcALingId;
	ALing.DisplayName = TEXT("阿翎");
	ALing.Order = MakeOrder(LingGuId, 0);
	ALing.GiftId = GiftGuideKiteId;
	ALing.bPresent = true;
	ALing.bServed = false;
	Npcs.Add(ALing);

	FSSpecialNpcState SangPo;
	SangPo.NpcId = NpcSangPoId;
	SangPo.DisplayName = TEXT("桑婆");
	SangPo.Order = MakeOrder(YinShanJunId, 0);
	SangPo.GiftId = GiftLifeLampId;
	SangPo.bPresent = true;
	SangPo.bServed = false;
	Npcs.Add(SangPo);
}

bool ASSpecialNpcDirector::TryGetNpc(const FName NpcId, FSSpecialNpcState& OutNpc) const
{
	for (const FSSpecialNpcState& Npc : Npcs)
	{
		if (Npc.NpcId == NpcId)
		{
			OutNpc = Npc;
			return true;
		}
	}
	return false;
}

int32 ASSpecialNpcDirector::CountServed() const
{
	int32 Count = 0;
	for (const FSSpecialNpcState& Npc : Npcs)
	{
		if (Npc.bServed)
		{
			++Count;
		}
	}
	return Count;
}

bool ASSpecialNpcDirector::TryDeliverToNpc(const FName NpcId)
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!GameInstance || !Board)
	{
		return false;
	}

	if (GameInstance->Phase != ESGamePhase::DayRunning || !bDayServiceActive)
	{
		SetFeedback(TEXT("尚未开店，无法服务特殊 NPC。"));
		return false;
	}

	FSSpecialNpcState* Target = nullptr;
	for (FSSpecialNpcState& Npc : Npcs)
	{
		if (Npc.NpcId == NpcId)
		{
			Target = &Npc;
			break;
		}
	}
	if (!Target || !Target->bPresent)
	{
		SetFeedback(TEXT("找不到该特殊 NPC。"));
		Board->ClearActiveDrag();
		return false;
	}
	if (Target->bServed)
	{
		SetFeedback(FString::Printf(TEXT("%s 本局已服务过，不再重复。"), *Target->DisplayName));
		Board->ClearActiveDrag();
		return false;
	}

	if (!Board->IsDragging())
	{
		SetFeedback(FString::Printf(TEXT("请先点选棋子，再交付给 %s。"), *Target->DisplayName));
		return false;
	}

	const int32 CellIndex = Board->GetActiveDragCellIndex();
	FSDishPiece Piece;
	if (!Board->TryGetPiece(CellIndex, Piece))
	{
		SetFeedback(TEXT("来源格没有棋子，交付取消。"));
		Board->ClearActiveDrag();
		return false;
	}

	if (Piece.RecipeId != Target->Order.RecipeId)
	{
		SetFeedback(FString::Printf(
			TEXT("%s 需要 %s，当前是 %s，已回弹，未扣棋子/谢礼。"),
			*Target->DisplayName,
			*Target->Order.RecipeId.ToString(),
			*Piece.RecipeId.ToString()));
		Board->ClearActiveDrag();
		return false;
	}

	const int32 SellValue = Target->Order.SellValue > 0
		? Target->Order.SellValue
		: USChefGameInstance::GetRecipeSellValue(Piece.RecipeId);
	const int32 RevenueBefore = GameInstance->Revenue;

	if (!Board->RemovePieceAt(CellIndex))
	{
		SetFeedback(TEXT("移除棋子失败，NPC 交付中止。"));
		return false;
	}

	Target->bServed = true;
	GameInstance->AddObtainedGift(Target->GiftId);
	GameInstance->AddRevenue(SellValue);

	SetFeedback(FString::Printf(
		TEXT("服务成功：%s 收到 %s，营业额 %d→%d，留下谢礼 %s。"),
		*Target->DisplayName,
		*Piece.RecipeId.ToString(),
		RevenueBefore,
		GameInstance->Revenue,
		*USChefGameInstance::GetGiftDisplayName(Target->GiftId)));
	return true;
}

ASSpecialNpcDirector* ASSpecialNpcDirector::FindDirector(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return Cast<ASSpecialNpcDirector>(UGameplayStatics::GetActorOfClass(World, ASSpecialNpcDirector::StaticClass()));
}

void USMergeCellButton::Setup(const int32 InCellIndex, ASMergeBoard* InBoard, USDebugPanel* InOwnerPanel)
{
	CellIndex = InCellIndex;
	Board = InBoard;
	OwnerPanel = InOwnerPanel;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);

	if (!Label)
	{
		Label = NewObject<UTextBlock>(this, UTextBlock::StaticClass(), TEXT("CellLabel"));
		Label->SetJustification(ETextJustify::Center);
		AddChild(Label);
	}

	if (!bClickBound)
	{
		OnClicked.AddDynamic(this, &USMergeCellButton::HandleClicked);
		bClickBound = true;
	}

	RefreshFromBoard();
}

void USMergeCellButton::RefreshFromBoard()
{
	if (!Label)
	{
		return;
	}

	ASMergeBoard* MergeBoard = Board.Get();
	if (!MergeBoard || !MergeBoard->GetCells().IsValidIndex(CellIndex))
	{
		Label->SetText(FText::FromString(TEXT("?")));
		return;
	}

	const FSMergeCell& Cell = MergeBoard->GetCells()[CellIndex];
	if (!Cell.bEnabled)
	{
		Label->SetText(FText::FromString(TEXT("×")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.25f, 0.25f, 0.25f)));
		SetIsEnabled(false);
		return;
	}

	SetIsEnabled(true);
	if (!Cell.bOccupied)
	{
		Label->SetText(FText::FromString(TEXT("□")));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
		return;
	}

	Label->SetText(FText::FromString(PieceShortLabel(Cell.Piece)));
	Label->SetColorAndOpacity(FSlateColor(
		MergeBoard->GetActiveDragCellIndex() == CellIndex
			? FLinearColor(0.4f, 0.8f, 1.0f)
			: FLinearColor(0.95f, 0.8f, 0.2f)));
}

void USMergeCellButton::HandleClicked()
{
	ASMergeBoard* MergeBoard = Board.Get();
	if (!MergeBoard)
	{
		return;
	}

	if (MergeBoard->IsDragging())
	{
		const int32 FromIndex = MergeBoard->GetActiveDragCellIndex();
		MergeBoard->TryDropPiece(FromIndex, CellIndex);
		if (USDebugPanel* Panel = OwnerPanel.Get())
		{
			Panel->NotifyBoardChanged();
		}
		return;
	}

	FSDishPiece Piece;
	if (!MergeBoard->TryGetPiece(CellIndex, Piece))
	{
		return;
	}

	MergeBoard->BeginPieceDrag(CellIndex, 0);
	if (USDebugPanel* Panel = OwnerPanel.Get())
	{
		Panel->NotifyBoardChanged();
	}
}

ASFakeNightGateway::ASFakeNightGateway()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASFakeNightGateway::BeginPlay()
{
	Super::BeginPlay();

#pragma region K2 moonyfli
	// The sandbox level can use a non-sandbox GameMode, so the placed gateway
	// must guarantee that the gameplay actors exist before creating the panel.
	if (!ASMergeBoard::FindBoard(this))
	{
		GetWorld()->SpawnActor<ASMergeBoard>();
	}
	if (!ASCustomerDirector::FindDirector(this))
	{
		GetWorld()->SpawnActor<ASCustomerDirector>();
	}
	if (!ASSpecialNpcDirector::FindDirector(this))
	{
		GetWorld()->SpawnActor<ASSpecialNpcDirector>();
	}

	// GameInstance::Init may restore DayRunning before actors exist; re-open shop now.
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		if (GameInstance->Phase == ESGamePhase::DayRunning)
		{
			if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
			{
				Director->NotifyDayStarted();
			}
			if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
			{
				NpcDirector->NotifyDayStarted();
			}
		}
	}
#pragma endregion K2 moonyfli

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		UClass* PanelClass = USDebugPanel::StaticClass();
		DebugPanel = CreateWidget<USDebugPanel>(PlayerController, PanelClass);
		if (DebugPanel)
		{
			DebugPanel->AddToViewport(100);
			UE_LOG(LogSSandbox, Display, TEXT("调试面板已加入视口：%s"), *PanelClass->GetName());
		}
		else
		{
			UE_LOG(LogSSandbox, Warning, TEXT("创建调试面板失败。"));
		}

		PlayerController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

FSNightResult ASFakeNightGateway::MakeResult(const bool bSuccess)
{
	FSNightResult Result;
	Result.ResultId = FString::Printf(TEXT("S-SANDBOX-%04d"), NextResultNumber++);
	Result.bSuccess = bSuccess;
	Result.bFailedMidway = !bSuccess;
	Result.RouteTaken = TEXT("AB");
	Result.SoulLeft = bSuccess ? 75.0f : 0.0f;
	Result.Ingredients =
	{
		{LingGuId, 8},
		{YinShanJunId, 4}
	};
	return Result;
}

void ASFakeNightGateway::Submit(const FSNightResult& Result)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->ConsumeNightResult(Result);
	}
}

void ASFakeNightGateway::SubmitNewSuccessResult()
{
	LastGeneratedResult = MakeResult(true);
	Submit(LastGeneratedResult);
}

void ASFakeNightGateway::SubmitNewFailureResult()
{
	LastGeneratedResult = MakeResult(false);
	Submit(LastGeneratedResult);
}

void ASFakeNightGateway::RepeatLastResult()
{
	if (LastGeneratedResult.ResultId.IsEmpty())
	{
		UE_LOG(LogSSandbox, Warning, TEXT("没有可重复提交的 NightResult。"));
		return;
	}
	Submit(LastGeneratedResult);
}

void ASFakeNightGateway::ResetSandbox()
{
	LastGeneratedResult = FSNightResult();
	NextResultNumber = 1;
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->ResetSandbox();
	}
}

void ASFakeNightGateway::DebugAddTenEach()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		for (const FName Id : GetKnownIds())
		{
			GameInstance->AddIngredient(Id, 10);
		}
		GameInstance->LastBoardFeedback = TEXT("五类食材各 +10。");
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
}

void ASFakeNightGateway::DebugTryUnknownId()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->AddIngredient(TEXT("UnknownHerb"), 1);
	}
}

void ASFakeNightGateway::DebugPromoteAllChains()
{
	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->DebugPromoteAllChainsToLv4();
	}
}

void ASFakeNightGateway::DebugSpawnFixedCustomer()
{
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
		{
			if (GameInstance->Phase != ESGamePhase::DayRunning)
			{
				GameInstance->Phase = ESGamePhase::DayRunning;
			}
		}
		Director->NotifyDayStarted();
	}
}

void ASFakeNightGateway::DebugForceCloseShop()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->ForceCloseShopForDebug();
	}
}

void ASFakeNightGateway::DebugJumpToStage(const FName InStageId)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->JumpToStageForDebug(InStageId);
	}
}

void ASFakeNightGateway::DebugPrintBootstrap()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->BuildNightBootstrap();
		GameInstance->LastBoardFeedback = FString::Printf(
			TEXT("PendingNightBootstrap: %s"),
			*GameInstance->FormatBootstrapDebug());
		GameInstance->OnSandboxStateChanged.Broadcast();
		UE_LOG(LogSSandbox, Display, TEXT("%s"), *GameInstance->LastBoardFeedback);
	}
}

void ASFakeNightGateway::DebugCloseDayKeepGap()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->CloseDayKeepGapForDebug();
	}
}

void ASFakeNightGateway::DebugSaveProfile()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->SaveChefProfile();
	}
}

void ASFakeNightGateway::DebugLoadProfile()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		if (!GameInstance->LoadChefProfile())
		{
			GameInstance->LastBoardFeedback = TEXT("读档失败：无存档或无法恢复。");
			GameInstance->OnSandboxStateChanged.Broadcast();
		}
	}
}

void ASFakeNightGateway::DebugCorruptSave()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->SimulateCorruptSaveForDebug();
	}
}

void ASFakeNightGateway::DebugDeleteSave()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->DeleteChefProfile();
	}
}

TSharedRef<SWidget> USDebugPanel::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void USDebugPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &USDebugPanel::Refresh);
	}

	if (ASMergeBoard* Board = GetBoard())
	{
		for (int32 Index = 0; Index < BoardCells.Num(); ++Index)
		{
			if (BoardCells[Index])
			{
				BoardCells[Index]->Setup(Index, Board, this);
			}
		}
	}
	Refresh();
}

void USDebugPanel::NativeDestruct()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &USDebugPanel::Refresh);
	}
	Super::NativeDestruct();
}

void USDebugPanel::BuildWidgetTree()
{
	USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("RootSafeZone"));
	WidgetTree->RootWidget = SafeZone;

	// 状态文本行数会随阶段变化，用 ScrollBox 承载，避免把棋盘和按钮挤出可视区。
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DebugScroll"));
	SafeZone->AddChild(Scroll);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugLayout"));
	Scroll->AddChild(Layout);

	auto SetFontSize = [](UTextBlock* Text, const int32 Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	};

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("S 独立沙盒｜第七步：失败、补跑与存档")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.72f, 0.12f)));
	SetFontSize(Title, 18);
	Layout->AddChildToVerticalBox(Title)->SetPadding(FMargin(24.0f, 10.0f, 24.0f, 4.0f));

	// 单行摘要放顶部；完整状态文本挪到棋盘下方，避免文本变长时把交互区顶出屏幕。
	StageSummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StageSummary"));
	StageSummaryText->SetAutoWrapText(true);
	StageSummaryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.95f, 0.75f)));
	SetFontSize(StageSummaryText, 14);
	Layout->AddChildToVerticalBox(StageSummaryText)->SetPadding(FMargin(24.0f, 2.0f));

	// WrapBox：按钮多于一行宽度时自动换行，不再互相压缩。
	auto AddButtonRow = [this, Layout](const TCHAR* RowName) -> UWrapBox*
	{
		UWrapBox* Row = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), RowName);
		Row->SetInnerSlotPadding(FVector2D(6.0f, 4.0f));
		Layout->AddChildToVerticalBox(Row)->SetPadding(FMargin(20.0f, 2.0f));
		return Row;
	};

	auto AddButton = [this, &SetFontSize](UWrapBox* Row, const TCHAR* Name, const TCHAR* LabelTextValue) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetText(FText::FromString(LabelTextValue));
		SetFontSize(LabelText, 14);
		Button->AddChild(LabelText);
		Row->AddChildToWrapBox(Button);
		return Button;
	};

	// 详情文本限宽后自动换行，避免单行把整行撑出屏幕。
	auto AddDetailText = [this, &SetFontSize](UWrapBox* Row, const TCHAR* Name) -> UTextBlock*
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(520.0f);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetAutoWrapText(true);
		SetFontSize(Text, 14);
		Box->AddChild(Text);
		Row->AddChildToWrapBox(Box);
		return Text;
	};

	UWrapBox* NightButtons = AddButtonRow(TEXT("NightButtons"));
	AddButton(NightButtons, TEXT("SuccessButton"), TEXT("成功结果"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleSuccessClicked);
	AddButton(NightButtons, TEXT("FailureButton"), TEXT("失败结果"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleFailureClicked);
	AddButton(NightButtons, TEXT("RepeatButton"), TEXT("重复提交"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleRepeatClicked);
	AddButton(NightButtons, TEXT("ResetButton"), TEXT("重置"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleResetClicked);

	UWrapBox* MotherButtons = AddButtonRow(TEXT("MotherButtons"));
	AddButton(MotherButtons, TEXT("MotherLingGu"), TEXT("母·灵谷"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleMotherLingGu);
	AddButton(MotherButtons, TEXT("MotherYinShanJun"), TEXT("母·阴山菌"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleMotherYinShanJun);
	AddButton(MotherButtons, TEXT("MotherChiYanJiao"), TEXT("母·赤焰椒"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleMotherChiYanJiao);
	AddButton(MotherButtons, TEXT("MotherYueLinYu"), TEXT("母·月鳞鱼"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleMotherYueLinYu);
	AddButton(MotherButtons, TEXT("MotherXuanYuQin"), TEXT("母·玄羽禽"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleMotherXuanYuQin);

	UWrapBox* DebugButtons = AddButtonRow(TEXT("DebugButtons"));
	AddButton(DebugButtons, TEXT("AddTenButton"), TEXT("五类各+10"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleAddTenEach);
	AddButton(DebugButtons, TEXT("ForceFillButton"), TEXT("强制满盘"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleForceFillBoard);
	AddButton(DebugButtons, TEXT("UnknownIdButton"), TEXT("未知ID写入"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleUnknownId);
	AddButton(DebugButtons, TEXT("PromoteAllButton"), TEXT("五链升Lv4"))->OnClicked.AddDynamic(this, &USDebugPanel::HandlePromoteAllChains);
	AddButton(DebugButtons, TEXT("SpawnCustomerButton"), TEXT("刷新顾客"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleSpawnCustomer);
	AddButton(DebugButtons, TEXT("ForceCloseButton"), TEXT("强制闭店"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleForceCloseShop);

	UWrapBox* CustomerRow = AddButtonRow(TEXT("CustomerRow"));
	CustomerButton = AddButton(CustomerRow, TEXT("CustomerButton"), TEXT("顾客：无"));
	CustomerButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleCustomerClicked);
	CustomerLabel = AddDetailText(CustomerRow, TEXT("CustomerDetail"));

	UWrapBox* NpcRow = AddButtonRow(TEXT("NpcRow"));
	NpcALingButton = AddButton(NpcRow, TEXT("NpcALingButton"), TEXT("阿翎"));
	NpcALingButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleNpcALingClicked);
	NpcSangPoButton = AddButton(NpcRow, TEXT("NpcSangPoButton"), TEXT("桑婆"));
	NpcSangPoButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleNpcSangPoClicked);
	NpcDetailLabel = AddDetailText(NpcRow, TEXT("NpcDetail"));

	UWrapBox* GiftRow = AddButtonRow(TEXT("GiftRow"));
	GiftGuideKiteButton = AddButton(GiftRow, TEXT("GiftGuideKiteButton"), TEXT("选·纸鸢"));
	GiftGuideKiteButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleToggleGiftGuideKite);
	GiftLifeLampButton = AddButton(GiftRow, TEXT("GiftLifeLampButton"), TEXT("选·纸灯"));
	GiftLifeLampButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleToggleGiftLifeLamp);
	ConfirmGiftsButton = AddButton(GiftRow, TEXT("ConfirmGiftsButton"), TEXT("确认入夜(可不选礼)"));
	ConfirmGiftsButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleConfirmGifts);

	UWrapBox* StageRow = AddButtonRow(TEXT("StageRow"));
	AddButton(StageRow, TEXT("JumpT0Button"), TEXT("跳T0"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpT0);
	AddButton(StageRow, TEXT("JumpL1Button"), TEXT("跳L1"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL1);
	AddButton(StageRow, TEXT("JumpL2Button"), TEXT("跳L2"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL2);
	AddButton(StageRow, TEXT("JumpL3Button"), TEXT("跳L3"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL3);
	AddButton(StageRow, TEXT("PrintBootstrapButton"), TEXT("打印Bootstrap"))->OnClicked.AddDynamic(this, &USDebugPanel::HandlePrintBootstrap);
	AddButton(StageRow, TEXT("KeepGapButton"), TEXT("保留缺口闭店"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleCloseDayKeepGap);

	UWrapBox* SaveRow = AddButtonRow(TEXT("SaveRow"));
	AddButton(SaveRow, TEXT("SaveProfileButton"), TEXT("存档"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleSaveProfile);
	AddButton(SaveRow, TEXT("LoadProfileButton"), TEXT("读档"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleLoadProfile);
	AddButton(SaveRow, TEXT("CorruptSaveButton"), TEXT("写坏档"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleCorruptSave);
	AddButton(SaveRow, TEXT("DeleteSaveButton"), TEXT("删档"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleDeleteSave);

	// 固定棋盘尺寸并给每格最小边长，避免 4x4 被压扁或裁掉。
	USizeBox* BoardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BoardSizeBox"));
	BoardBox->SetWidthOverride(300.0f);
	BoardBox->SetHeightOverride(300.0f);
	UVerticalBoxSlot* BoardSlot = Layout->AddChildToVerticalBox(BoardBox);
	BoardSlot->SetPadding(FMargin(24.0f, 6.0f));
	BoardSlot->SetHorizontalAlignment(HAlign_Left);

	BoardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BoardGrid"));
	BoardGrid->SetMinDesiredSlotWidth(66.0f);
	BoardGrid->SetMinDesiredSlotHeight(66.0f);
	BoardGrid->SetSlotPadding(FMargin(3.0f));
	BoardBox->AddChild(BoardGrid);

	BoardCells.Reset();
	for (int32 Y = 0; Y < 4; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			const int32 Index = Y * 4 + X;
			USMergeCellButton* CellButton = WidgetTree->ConstructWidget<USMergeCellButton>(
				USMergeCellButton::StaticClass(),
				*FString::Printf(TEXT("MC%02d"), Index));
			UUniformGridSlot* CellSlot = BoardGrid->AddChildToUniformGrid(CellButton, Y, X);
			CellSlot->SetHorizontalAlignment(HAlign_Fill);
			CellSlot->SetVerticalAlignment(VAlign_Fill);
			BoardCells.Add(CellButton);
		}
	}

	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FeedbackText"));
	FeedbackText->SetAutoWrapText(true);
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.9f, 1.0f)));
	SetFontSize(FeedbackText, 14);
	Layout->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(24.0f, 6.0f, 24.0f, 4.0f));

	StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StateText"));
	StateText->SetAutoWrapText(true);
	SetFontSize(StateText, 13);
	Layout->AddChildToVerticalBox(StateText)->SetPadding(FMargin(24.0f, 4.0f, 24.0f, 24.0f));
}

ASFakeNightGateway* USDebugPanel::GetGateway() const
{
	return Cast<ASFakeNightGateway>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFakeNightGateway::StaticClass()));
}

ASMergeBoard* USDebugPanel::GetBoard() const
{
	return ASMergeBoard::FindBoard(this);
}

ASCustomerDirector* USDebugPanel::GetDirector() const
{
	return ASCustomerDirector::FindDirector(this);
}

ASSpecialNpcDirector* USDebugPanel::GetNpcDirector() const
{
	return ASSpecialNpcDirector::FindDirector(this);
}

void USDebugPanel::NotifyBoardChanged()
{
	Refresh();
}

void USDebugPanel::HandleSuccessClicked()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->SubmitNewSuccessResult();
		SetFeedback(TEXT("已提交：灵谷×8、阴山菌×4。取材后服务 NPC / 顾客。"));
	}
}

void USDebugPanel::HandleFailureClicked()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->SubmitNewFailureResult();
		SetFeedback(TEXT("已提交失败结果：50% 进入临时食篮，阶段保持 PrepareNight。"));
	}
}

void USDebugPanel::HandleRepeatClicked()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->RepeatLastResult();
		SetFeedback(TEXT("已重复提交当前 ResultId；库存与临时食篮应保持不变。"));
	}
}

void USDebugPanel::HandleResetClicked()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->ResetSandbox();
		SetFeedback(TEXT("沙盒已重置。"));
	}
}

void USDebugPanel::TryMother(const FName IngredientId, const FString& DisplayName)
{
	ASMergeBoard* Board = GetBoard();
	if (!Board)
	{
		SetFeedback(TEXT("未找到 MergeBoard。"));
		return;
	}

	Board->TrySpawnFromMotherPiece(IngredientId);
	if (const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		SetFeedback(GameInstance->LastBoardFeedback);
	}
	Refresh();
}

void USDebugPanel::HandleMotherLingGu() { TryMother(LingGuId, TEXT("灵谷")); }
void USDebugPanel::HandleMotherYinShanJun() { TryMother(YinShanJunId, TEXT("阴山菌")); }
void USDebugPanel::HandleMotherChiYanJiao() { TryMother(ChiYanJiaoId, TEXT("赤焰椒")); }
void USDebugPanel::HandleMotherYueLinYu() { TryMother(YueLinYuId, TEXT("月鳞鱼")); }
void USDebugPanel::HandleMotherXuanYuQin() { TryMother(XuanYuQinId, TEXT("玄羽禽")); }

void USDebugPanel::HandleAddTenEach()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugAddTenEach();
		SetFeedback(TEXT("五类食材各 +10。"));
	}
}

void USDebugPanel::HandleForceFillBoard()
{
	if (ASMergeBoard* Board = GetBoard())
	{
		Board->ForceFillBoardForDebug();
		SetFeedback(TEXT("已强制满盘（不扣库存），再点母棋子应拒绝且库存不变。"));
		Refresh();
	}
}

void USDebugPanel::HandleUnknownId()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugTryUnknownId();
		SetFeedback(TEXT("已尝试写入未知 ID，库存应保持不变。"));
	}
}

void USDebugPanel::HandlePromoteAllChains()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugPromoteAllChains();
		if (const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
		{
			SetFeedback(GameInstance->LastBoardFeedback);
		}
		Refresh();
	}
}

void USDebugPanel::HandleCustomerClicked()
{
	if (ASCustomerDirector* Director = GetDirector())
	{
		Director->TryDeliverSelectedPiece();
		Refresh();
	}
}

void USDebugPanel::HandleSpawnCustomer()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugSpawnFixedCustomer();
		Refresh();
	}
}

void USDebugPanel::DeliverToNpc(const FName NpcId)
{
	if (ASSpecialNpcDirector* NpcDirector = GetNpcDirector())
	{
		NpcDirector->TryDeliverToNpc(NpcId);
		Refresh();
	}
}

void USDebugPanel::HandleNpcALingClicked()
{
	DeliverToNpc(NpcALingId);
}

void USDebugPanel::HandleNpcSangPoClicked()
{
	DeliverToNpc(NpcSangPoId);
}

void USDebugPanel::HandleForceCloseShop()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugForceCloseShop();
		Refresh();
	}
}

void USDebugPanel::ToggleGift(const FName GiftId)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->TogglePendingGiftSelection(GiftId);
		SetFeedback(GameInstance->LastBoardFeedback);
		Refresh();
	}
}

void USDebugPanel::HandleToggleGiftGuideKite()
{
	ToggleGift(GiftGuideKiteId);
}

void USDebugPanel::HandleToggleGiftLifeLamp()
{
	ToggleGift(GiftLifeLampId);
}

void USDebugPanel::HandleConfirmGifts()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->ConfirmGiftSelection();
		SetFeedback(GameInstance->LastBoardFeedback);
		Refresh();
	}
}

void USDebugPanel::JumpStage(const FName InStageId)
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugJumpToStage(InStageId);
		Refresh();
	}
}

void USDebugPanel::HandleJumpT0() { JumpStage(TEXT("T0")); }
void USDebugPanel::HandleJumpL1() { JumpStage(TEXT("L1")); }
void USDebugPanel::HandleJumpL2() { JumpStage(TEXT("L2")); }
void USDebugPanel::HandleJumpL3() { JumpStage(TEXT("L3")); }

void USDebugPanel::HandlePrintBootstrap()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugPrintBootstrap();
		Refresh();
	}
}

void USDebugPanel::HandleCloseDayKeepGap()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugCloseDayKeepGap();
		Refresh();
	}
}

void USDebugPanel::HandleSaveProfile()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugSaveProfile();
		Refresh();
	}
}

void USDebugPanel::HandleLoadProfile()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugLoadProfile();
		Refresh();
	}
}

void USDebugPanel::HandleCorruptSave()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugCorruptSave();
		Refresh();
	}
}

void USDebugPanel::HandleDeleteSave()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugDeleteSave();
		Refresh();
	}
}

void USDebugPanel::RefreshBoardVisual()
{
	ASMergeBoard* Board = GetBoard();
	for (int32 Index = 0; Index < BoardCells.Num(); ++Index)
	{
		if (USMergeCellButton* CellButton = BoardCells[Index])
		{
			if (Board)
			{
				CellButton->Setup(Index, Board, this);
			}
			CellButton->RefreshFromBoard();
		}
	}
}

void USDebugPanel::RefreshCustomerVisual()
{
	const ASCustomerDirector* Director = GetDirector();
	FString ButtonText = TEXT("顾客：无");
	FString Detail = TEXT("开店后会刷固定订单 LingGu_Lv0。");

	if (Director && Director->HasActiveCustomer())
	{
		const FSCustomerState Customer = Director->GetActiveCustomer();
		ButtonText = FString::Printf(TEXT("交付→%s"), *Customer.Order.RecipeId.ToString());
		Detail = FString::Printf(
			TEXT("%s 要 %s｜耐心 %.0f/%.0fs｜售价 %d"),
			*Customer.CustomerId,
			*Customer.Order.RecipeId.ToString(),
			Customer.PatienceRemaining,
			Customer.PatienceMax,
			Customer.Order.SellValue);
	}
	else if (Director)
	{
		Detail = FString::Printf(TEXT("下一位倒计时 %.1fs"), Director->GetSpawnCooldownRemaining());
	}

	if (CustomerButton && CustomerButton->GetChildrenCount() > 0)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(CustomerButton->GetChildAt(0)))
		{
			Label->SetText(FText::FromString(ButtonText));
		}
	}
	if (CustomerLabel)
	{
		CustomerLabel->SetText(FText::FromString(Detail));
	}
}

void USDebugPanel::RefreshNpcVisual()
{
	const ASSpecialNpcDirector* NpcDirector = GetNpcDirector();
	auto SetNpcButtonText = [](UButton* Button, const FString& Text)
	{
		if (Button && Button->GetChildrenCount() > 0)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
			{
				Label->SetText(FText::FromString(Text));
			}
		}
	};

	FString Detail = TEXT("开店后出现保底阿翎/桑婆。");
	if (NpcDirector)
	{
		TArray<FString> Parts;
		for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
		{
			Parts.Add(FString::Printf(
				TEXT("%s%s要%s→%s"),
				*Npc.DisplayName,
				Npc.bServed ? TEXT("[已服务]") : TEXT(""),
				*Npc.Order.RecipeId.ToString(),
				*USChefGameInstance::GetGiftDisplayName(Npc.GiftId)));
			if (Npc.NpcId == NpcALingId)
			{
				SetNpcButtonText(NpcALingButton, Npc.bServed ? TEXT("阿翎✓") : TEXT("阿翎←灵0"));
			}
			else if (Npc.NpcId == NpcSangPoId)
			{
				SetNpcButtonText(NpcSangPoButton, Npc.bServed ? TEXT("桑婆✓") : TEXT("桑婆←阴0"));
			}
		}
		if (Parts.Num() > 0)
		{
			Detail = FString::Join(Parts, TEXT(" ｜ "));
		}
	}
	if (NpcDetailLabel)
	{
		NpcDetailLabel->SetText(FText::FromString(Detail));
	}
}

void USDebugPanel::RefreshGiftVisual()
{
	const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	auto SetGiftButtonText = [](UButton* Button, const FString& Text)
	{
		if (Button && Button->GetChildrenCount() > 0)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
			{
				Label->SetText(FText::FromString(Text));
			}
		}
	};

	const bool bSelecting = GameInstance && GameInstance->Phase == ESGamePhase::GiftSelect;
	const bool bHasKite = GameInstance && GameInstance->ObtainedGiftIds.Contains(GiftGuideKiteId);
	const bool bHasLamp = GameInstance && GameInstance->ObtainedGiftIds.Contains(GiftLifeLampId);
	const bool bPendKite = GameInstance && GameInstance->PendingGiftIds.Contains(GiftGuideKiteId);
	const bool bPendLamp = GameInstance && GameInstance->PendingGiftIds.Contains(GiftLifeLampId);

	SetGiftButtonText(
		GiftGuideKiteButton,
		!bHasKite ? TEXT("纸鸢(未获得)") : (bPendKite ? TEXT("纸鸢✓") : (bSelecting ? TEXT("选·纸鸢") : TEXT("纸鸢已得"))));
	SetGiftButtonText(
		GiftLifeLampButton,
		!bHasLamp ? TEXT("纸灯(未获得)") : (bPendLamp ? TEXT("纸灯✓") : (bSelecting ? TEXT("选·纸灯") : TEXT("纸灯已得"))));
	SetGiftButtonText(
		ConfirmGiftsButton,
		bSelecting ? TEXT("确认入夜(可不选礼)") : TEXT("闭店后确认"));
}

void USDebugPanel::Refresh()
{
	const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance || !StateText)
	{
		return;
	}

	const ASMergeBoard* Board = GetBoard();
	const int32 Enabled = Board ? Board->GetEnabledCellCount() : 0;
	const int32 Occupied = Board ? Board->GetOccupiedCellCount() : 0;
	const int32 Empty = Board ? Board->GetEmptyCellCount() : 0;
	const int32 DragCell = Board ? Board->GetActiveDragCellIndex() : INDEX_NONE;
	TMap<FName, int32> PendingReclaimUnits;
	const int32 PendingReclaim = Board ? Board->GetPendingReclaimUnits(PendingReclaimUnits) : 0; //add by K2

	const FString SelectedGifts = GameInstance->SelectedGiftIds.IsEmpty()
		? TEXT("None")
		: FString::JoinBy(GameInstance->SelectedGiftIds, TEXT(", "), [](const FName Id) { return Id.ToString(); });
	const FString ObtainedGifts = GameInstance->ObtainedGiftIds.IsEmpty()
		? TEXT("None")
		: FString::JoinBy(GameInstance->ObtainedGiftIds, TEXT(", "), [](const FName Id)
			{
				return FString::Printf(TEXT("%s(%s)"), *USChefGameInstance::GetGiftDisplayName(Id), *Id.ToString());
			});
	const FString PendingGifts = GameInstance->PendingGiftIds.IsEmpty()
		? TEXT("None")
		: FString::JoinBy(GameInstance->PendingGiftIds, TEXT(", "), [](const FName Id) { return Id.ToString(); });

	const ASCustomerDirector* Director = GetDirector();
	FString CustomerLine = TEXT("顾客: 无");
	if (Director && Director->HasActiveCustomer())
	{
		const FSCustomerState Customer = Director->GetActiveCustomer();
		CustomerLine = FString::Printf(
			TEXT("顾客: %s 订单=%s 耐心=%.0f/%.0f"),
			*Customer.CustomerId,
			*Customer.Order.RecipeId.ToString(),
			Customer.PatienceRemaining,
			Customer.PatienceMax);
	}
	else if (Director)
	{
		CustomerLine = FString::Printf(TEXT("顾客: 冷却 %.1fs"), Director->GetSpawnCooldownRemaining());
	}

	const ASSpecialNpcDirector* NpcDirector = GetNpcDirector();
	const int32 NpcServed = NpcDirector ? NpcDirector->CountServed() : 0;
	const int32 NpcTotal = NpcDirector ? NpcDirector->GetNpcs().Num() : 0;

	if (StageSummaryText)
	{
		StageSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s ｜ %s(%s) ｜ 营业额 %d/%d 缺口%d ｜ Retry=%s ｜ %s ｜ NPC %d/%d"),
			*GameInstance->GetPhaseDisplayName(),
			*GameInstance->StageId.ToString(),
			*GameInstance->ActiveStageRow.DisplayName,
			GameInstance->Revenue,
			GameInstance->RevenueTarget,
			GameInstance->GetRevenueGap(),
			GameInstance->bAwaitingNightRetry ? TEXT("Y") : TEXT("N"),
			*CustomerLine,
			NpcServed,
			NpcTotal)));
	}

	StateText->SetText(FText::FromString(FString::Printf(
		TEXT("永久库存  灵谷:%d 阴山菌:%d 赤焰椒:%d 月鳞鱼:%d 玄羽禽:%d ｜ 临时食篮  灵谷:%d 阴山菌:%d 赤:%d 月:%d 玄:%d\n")
		TEXT("棋盘  启用:%d 占用:%d 空格:%d 拖拽格:%s 待退回:%d ｜ 最高等级 灵:%d 阴:%d 赤:%d 月:%d 玄:%d\n")
		TEXT("谢礼卡: %s ｜ 勾选中: %s ｜ 带入夜: %s\n")
		TEXT("GiftBuff: %s ｜ CompletedDays: %s\n")
		TEXT("Stage  Seed:%d Fork:%s 昼:%.0fs 夜:%.0fs Next:%s%s\n")
		TEXT("Bootstrap: %s ｜ LastResult: %s\n")
		TEXT("Save: %s"),
		GameInstance->GetInventoryQuantity(LingGuId),
		GameInstance->GetInventoryQuantity(YinShanJunId),
		GameInstance->GetInventoryQuantity(ChiYanJiaoId),
		GameInstance->GetInventoryQuantity(YueLinYuId),
		GameInstance->GetInventoryQuantity(XuanYuQinId),
		GameInstance->GetTemporaryQuantity(LingGuId),
		GameInstance->GetTemporaryQuantity(YinShanJunId),
		GameInstance->GetTemporaryQuantity(ChiYanJiaoId),
		GameInstance->GetTemporaryQuantity(YueLinYuId),
		GameInstance->GetTemporaryQuantity(XuanYuQinId),
		Enabled,
		Occupied,
		Empty,
		DragCell == INDEX_NONE ? TEXT("-") : *FString::FromInt(DragCell),
		PendingReclaim,
		Board ? Board->GetHighestLevel(LingGuId) : -1,
		Board ? Board->GetHighestLevel(YinShanJunId) : -1,
		Board ? Board->GetHighestLevel(ChiYanJiaoId) : -1,
		Board ? Board->GetHighestLevel(YueLinYuId) : -1,
		Board ? Board->GetHighestLevel(XuanYuQinId) : -1,
		*ObtainedGifts,
		*PendingGifts,
		*SelectedGifts,
		*GameInstance->GiftBuffState.ToDebugString(),
		GameInstance->CompletedDayFlags.IsEmpty()
			? TEXT("None")
			: *FString::JoinBy(GameInstance->CompletedDayFlags, TEXT(","), [](const FName Id) { return Id.ToString(); }),
		GameInstance->ReviewSeed,
		*GameInstance->ForkPair.ToString(),
		GameInstance->DayDurationSeconds,
		GameInstance->NightDurationSeconds,
		GameInstance->ActiveStageRow.NextLevelId.IsNone() ? TEXT("None") : *GameInstance->ActiveStageRow.NextLevelId.ToString(),
		GameInstance->ActiveStageRow.bEndingAfterDay ? TEXT("(Ending)") : TEXT(""),
		*GameInstance->FormatBootstrapDebug(),
		*GameInstance->LastConsumedNightResultId,
		*GameInstance->LastSaveFeedback)));

	if (FeedbackText && !GameInstance->LastBoardFeedback.IsEmpty())
	{
		FeedbackText->SetText(FText::FromString(GameInstance->LastBoardFeedback));
	}

	RefreshBoardVisual();
	RefreshCustomerVisual();
	RefreshNpcVisual();
	RefreshGiftVisual();
}

void USDebugPanel::SetFeedback(const FString& Message)
{
	if (FeedbackText)
	{
		FeedbackText->SetText(FText::FromString(Message));
	}
}

ASChefGameMode::ASChefGameMode()
{
	DefaultPawnClass = nullptr;
}

void ASChefGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!ASMergeBoard::FindBoard(this))
	{
		GetWorld()->SpawnActor<ASMergeBoard>();
	}

	if (!ASCustomerDirector::FindDirector(this))
	{
		GetWorld()->SpawnActor<ASCustomerDirector>();
	}

	if (!ASSpecialNpcDirector::FindDirector(this))
	{
		GetWorld()->SpawnActor<ASSpecialNpcDirector>();
	}

	for (TActorIterator<ASFakeNightGateway> It(GetWorld()); It; ++It)
	{
		return;
	}

	UClass* GatewayClass = LoadClass<ASFakeNightGateway>(
		nullptr,
		TEXT("/Game/Game/Day/Test/BP_FakeNightGateway.BP_FakeNightGateway_C"));
	if (!GatewayClass)
	{
		GatewayClass = ASFakeNightGateway::StaticClass();
	}
	GetWorld()->SpawnActor<ASFakeNightGateway>(GatewayClass);
}
