#include "SStandaloneSandbox.h"

#include "Day/Presentation/SDayBoardPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#include "Templates/Function.h"
#include "TimerManager.h"
#include "UnrealClient.h"

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

#pragma region K2 moonyfli
	const TArray<FString>& GetCustomerNames()
	{
		static const TArray<FString> Names =
		{
			TEXT("小满"),
			TEXT("阿桃"),
			TEXT("青禾"),
			TEXT("石榴"),
			TEXT("团团"),
			TEXT("霜叶")
		};
		return Names;
	}
#pragma endregion K2 moonyfli
}

void USChefGameInstance::Init()
{
	Super::Init();
	if (StageTable.IsNull())
	{
		StageTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_GameStages.DT_GameStages")));
	}
	if (RecipeTable.IsNull())
	{
		RecipeTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_Recipes.DT_Recipes")));
	}
#pragma region K2 moonyfli
	if (DayBalanceTable.IsNull())
	{
		DayBalanceTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_SDayBalance.DT_SDayBalance")));
	}
	if (IngredientTable.IsNull())
	{
		IngredientTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_Ingredients.DT_Ingredients")));
	}
	if (SpecialNpcTable.IsNull())
	{
		SpecialNpcTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_SpecialNpcs.DT_SpecialNpcs")));
	}
	if (GiftTable.IsNull())
	{
		GiftTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_Gifts.DT_Gifts")));
	}
	if (CustomerNameTable.IsNull())
	{
		CustomerNameTable = TSoftObjectPtr<UDataTable>(
			FSoftObjectPath(TEXT("/Game/Shared/Data/DT_CustomerNames.DT_CustomerNames")));
	}

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
	for (const FName Id : GetKnownIds())
	{
		Inventory.Add(Id, 0);
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

#pragma region K2 moonyfli
bool USChefGameInstance::GrantPermanentStock(const FName IngredientId, const int32 Quantity)
{
	if (!AddIngredient(IngredientId, Quantity))
	{
		return false;
	}
	if (Quantity == 0)
	{
		return true;
	}

	// Spending during a stage is meant to roll back, but a grant is permanent, so it has to
	// land in the snapshots as well or the next rollback (including a mid-stage load) drops it.
	if (NightStartSnapshot.bValid)
	{
		NightStartSnapshot.Inventory.FindOrAdd(IngredientId) += Quantity;
	}
	if (DayStartSnapshot.bValid)
	{
		DayStartSnapshot.Inventory.FindOrAdd(IngredientId) += Quantity;
	}
	return true;
}
#pragma endregion K2 moonyfli

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

#pragma region K2 moonyfli
	if (Phase == ESGamePhase::PrepareNight || Phase == ESGamePhase::Boot)
	{
		// The sandbox submits results without a playable night; open the run so the
		// snapshot exists before we decide success or failure.
		StartNight();
	}
	if (Phase != ESGamePhase::NightRunning)
	{
		LastBoardFeedback = FString::Printf(TEXT("当前阶段 %s 不接收夜结果。"), *GetPhaseDisplayName());
		NotifyStateChanged();
		return false;
	}
#pragma endregion K2 moonyfli

	ConsumedResultIds.Add(Result.ResultId);
	LastConsumedNightResultId = Result.ResultId;

#pragma region K2 moonyfli
	if (Result.bSuccess)
	{
		Phase = ESGamePhase::NightSettlement;
		bAwaitingNightRetry = false;

		// NightSettlement → PrepareDay：提交夜间食材。
		for (const FSIngredientStack& Stack : Result.Ingredients)
		{
			Inventory.FindOrAdd(Stack.IngredientId) += Stack.Quantity;
		}
		EnterPrepareDay(TEXT("夜间食材已入库｜"));
	}
	else
	{
		// 夜败：回档夜初快照，清除本次收获，留在 PrepareNight 等补跑。
		RestoreSnapshot(NightStartSnapshot);
		bAwaitingNightRetry = true;
		Phase = ESGamePhase::PrepareNight;
		ResetDayDirectors(false);
		LastBoardFeedback = FString::Printf(
			TEXT("夜失败（%s）：已回档夜初，本次收获清除。关卡=%s 不前进，请补跑当前夜。%s"),
			Result.bFailedMidway ? TEXT("中途死亡") : TEXT("时间耗尽"),
			*StageId.ToString(),
			*GetGiftTabSummary());
		NotifyStateChanged();
		AutoSaveChefProfile(TEXT("夜失败回档夜初"));
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
	return true;
}

int32 USChefGameInstance::GetInventoryQuantity(const FName IngredientId) const
{
	return GetQuantity(IngredientId);
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

int32 USChefGameInstance::GetBuiltInRecipeSellValue(const int32 Level)
{
	static const int32 Values[5] = {10, 22, 48, 100, 220};
	const int32 Clamped = FMath::Clamp(Level, 0, MaxDishLevel);
	return Values[Clamped];
}

int32 USChefGameInstance::GetRecipeSellValue(const FName RecipeId) const
{
	if (UDataTable* Table = RecipeTable.LoadSynchronous())
	{
		if (const FSRecipeRow* Row = Table->FindRow<FSRecipeRow>(RecipeId, TEXT("GetRecipeSellValue"), false))
		{
			if (Row->SellValue > 0)
			{
				return Row->SellValue;
			}
		}
	}

	const FString Id = RecipeId.ToString();
	int32 Level = 0;
	if (Id.EndsWith(TEXT("_Lv0"))) Level = 0;
	else if (Id.EndsWith(TEXT("_Lv1"))) Level = 1;
	else if (Id.EndsWith(TEXT("_Lv2"))) Level = 2;
	else if (Id.EndsWith(TEXT("_Lv3"))) Level = 3;
	else if (Id.EndsWith(TEXT("_Lv4"))) Level = 4;
	else return 0;

	UE_LOG(LogSSandbox, Warning, TEXT("DT_Recipes missing %s, using built-in sell value."), *Id);
	return GetBuiltInRecipeSellValue(Level);
}

#pragma region K2 moonyfli
namespace
{
	int32 OrderUnitCost(const int32 Level)
	{
		return 1 << FMath::Clamp(Level, 0, MaxDishLevel);
	}

	TArray<FName> ParseGuaranteedNpcIds(const FName Rules)
	{
		TArray<FName> Out;
		const FString Text = Rules.ToString();
		if (Text.IsEmpty() || Text.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return Out;
		}
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("_"), true);
		for (const FString& Part : Parts)
		{
			if (Part.Equals(TEXT("ALing"), ESearchCase::IgnoreCase))
			{
				Out.AddUnique(NpcALingId);
			}
			else if (Part.Equals(TEXT("SangPo"), ESearchCase::IgnoreCase))
			{
				Out.AddUnique(NpcSangPoId);
			}
		}
		return Out;
	}

	FName DefaultNpcIngredient(const USChefGameInstance* GameInstance, const FName NpcId)
	{
		FSSpecialNpcDefRow Def;
		if (GameInstance && GameInstance->TryGetSpecialNpcDef(NpcId, Def) && !Def.DefaultIngredientId.IsNone())
		{
			return Def.DefaultIngredientId;
		}
		if (NpcId == NpcALingId) return LingGuId;
		if (NpcId == NpcSangPoId) return YinShanJunId;
		return LingGuId;
	}

	FName DefaultNpcGift(const USChefGameInstance* GameInstance, const FName NpcId)
	{
		FSSpecialNpcDefRow Def;
		if (GameInstance && GameInstance->TryGetSpecialNpcDef(NpcId, Def) && !Def.GiftId.IsNone())
		{
			return Def.GiftId;
		}
		if (NpcId == NpcALingId) return GiftGuideKiteId;
		if (NpcId == NpcSangPoId) return GiftLifeLampId;
		return NAME_None;
	}

	FString DefaultNpcDisplayName(const USChefGameInstance* GameInstance, const FName NpcId)
	{
		FSSpecialNpcDefRow Def;
		if (GameInstance && GameInstance->TryGetSpecialNpcDef(NpcId, Def) && !Def.DisplayName.IsEmpty())
		{
			return Def.DisplayName;
		}
		if (NpcId == NpcALingId) return TEXT("阿翎");
		if (NpcId == NpcSangPoId) return TEXT("桑婆");
		return NpcId.ToString();
	}

	int32 MaxSellableValueForStock(
		const TMap<FName, int32>& Stock,
		const TFunctionRef<int32(FName, int32)>& SellOf)
	{
		int32 Total = 0;
		for (const TPair<FName, int32>& Pair : Stock)
		{
			int32 Units = FMath::Max(0, Pair.Value);
			for (int32 Level = MaxDishLevel; Level >= 0; --Level)
			{
				const int32 Cost = OrderUnitCost(Level);
				const int32 Count = Units / Cost;
				if (Count > 0)
				{
					Total += Count * SellOf(Pair.Key, Level);
					Units -= Count * Cost;
				}
			}
		}
		return Total;
	}

	bool CanMixLevelsWithStock(const TMap<FName, int32>& Stock)
	{
		int32 TotalUnits = 0;
		for (const TPair<FName, int32>& Pair : Stock)
		{
			TotalUnits += FMath::Max(0, Pair.Value);
		}
		return TotalUnits >= 3;
	}

	FSOrderRequest MakePlannedRequest(
		const USChefGameInstance& GameInstance,
		const FName IngredientId,
		const int32 Level)
	{
		FSOrderRequest Order;
		Order.IngredientId = IngredientId;
		Order.Level = FMath::Clamp(Level, 0, MaxDishLevel);
		Order.RecipeId = USChefGameInstance::MakeRecipeId(IngredientId, Order.Level);
		Order.SellValue = GameInstance.GetRecipeSellValue(Order.RecipeId);
		return Order;
	}

	bool ValidatePlannedOrders(
		const TArray<FSPlannedOrder>& Orders,
		const TMap<FName, int32>& StartingStock,
		const int32 EffectiveTarget,
		FString& OutReason)
	{
		TMap<FName, int32> Remaining = StartingStock;
		int32 TotalValue = 0;
		TSet<int32> Levels;
		const int32 HalfExclusive = FMath::DivideAndRoundUp(Orders.Num(), 2);
		for (int32 Index = 0; Index < Orders.Num(); ++Index)
		{
			const FSPlannedOrder& Slot = Orders[Index];
			if (Slot.Kind == ESOrderSlotKind::Npc)
			{
				if (Index >= HalfExclusive)
				{
					OutReason = TEXT("NPC slot outside first half");
					return false;
				}
				if (Slot.NpcId.IsNone())
				{
					OutReason = TEXT("NPC slot missing NpcId");
					return false;
				}
			}
			else if (!Slot.NpcId.IsNone())
			{
				OutReason = TEXT("Guest slot has NpcId");
				return false;
			}

			const int32 Cost = OrderUnitCost(Slot.Order.Level);
			const int32 Have = Remaining.FindRef(Slot.Order.IngredientId);
			if (Slot.Order.IngredientId.IsNone() || Have < Cost || Slot.Order.SellValue <= 0)
			{
				OutReason = TEXT("order not feasible against stock");
				return false;
			}
			Remaining.FindOrAdd(Slot.Order.IngredientId) = Have - Cost;
			TotalValue += Slot.Order.SellValue;
			Levels.Add(Slot.Order.Level);
		}

		if (TotalValue < EffectiveTarget)
		{
			OutReason = FString::Printf(TEXT("value %d < target %d"), TotalValue, EffectiveTarget);
			return false;
		}
		if (Orders.Num() >= 3 && CanMixLevelsWithStock(StartingStock) && Levels.Num() < 2)
		{
			OutReason = TEXT("all orders share one level");
			return false;
		}
		OutReason.Reset();
		return true;
	}
}

bool USChefGameInstance::BuildPlannedDayOrders()
{
	PlannedDayOrders.Reset();
	NextPlannedOrderIndex = 0;

	const TMap<FName, int32> StartingStock = Inventory;
	auto SellOf = [this](const FName IngredientId, const int32 Level) -> int32
	{
		return GetRecipeSellValue(MakeRecipeId(IngredientId, Level));
	};

	const int32 MaxValue = MaxSellableValueForStock(StartingStock, SellOf);
	int32 EffectiveTarget = RevenueTarget;
	if (MaxValue < RevenueTarget)
	{
		UE_LOG(
			LogSSandbox,
			Warning,
			TEXT("Day order plan: max sellable %d < target %d, clamping effective target."),
			MaxValue,
			RevenueTarget);
		EffectiveTarget = MaxValue;
	}

	const FSDayBalanceRow Balance = GetDayBalance();
	const int32 DishCap = FMath::Clamp(Balance.MaxDishLevel, 0, MaxDishLevel);
	const int32 MinimumAppearanceSlots = FMath::Max(1, Balance.MinPlannedOrderSlots);
	const TArray<FName> NpcIds = ParseGuaranteedNpcIds(ActiveStageRow.GuaranteedNpcRules);
	const uint32 StageHash = GetTypeHash(StageId) ^ GetTypeHash(ActiveStageRow.CustomerConfigId);
	bool bAccepted = false;
	FString FailReason;

	for (int32 Attempt = 0; Attempt < 24; ++Attempt)
	{
		FRandomStream Stream(ReviewSeed ^ StageHash ^ (Attempt * 9973));
		TMap<FName, int32> Remaining = StartingStock;
		TArray<FSPlannedOrder> NpcSlots;
		TArray<FSPlannedOrder> GuestSlots;
		int32 SumValue = 0;
		TMap<int32, int32> LevelCounts;

		auto TryPickLevel = [&](const FName IngredientId, const bool bPreferMid) -> int32
		{
			TArray<int32> Candidates;
			TArray<float> Weights;
			float WeightSum = 0.0f;
			const int32 Have = Remaining.FindRef(IngredientId);
			for (int32 Level = 0; Level <= DishCap; ++Level)
			{
				const int32 Cost = OrderUnitCost(Level);
				if (Have < Cost)
				{
					continue;
				}
				float Weight = 1.0f + static_cast<float>(Have - Cost);
				if (bPreferMid)
				{
					Weight *= (Level == 0 || Level == DishCap)
						? Balance.OrderEdgeLevelWeight
						: Balance.OrderMidLevelWeight;
				}
				else
				{
					// Early stages lean low; later stages open mid tiers.
					const float StageBias = StageId == TEXT("T0")
						? (Level <= 1 ? Balance.T0LowLevelBias : Balance.T0HighLevelBias)
						: (Level >= 1 && Level <= 3 ? Balance.LaterMidLevelBias : Balance.LaterEdgeLevelBias);
					Weight *= StageBias;
				}
				const int32 Seen = LevelCounts.FindRef(Level);
				Weight *= 1.0f / (1.0f + static_cast<float>(Seen));
				Candidates.Add(Level);
				Weights.Add(Weight);
				WeightSum += Weight;
			}
			if (Candidates.IsEmpty() || WeightSum <= 0.0f)
			{
				return INDEX_NONE;
			}
			float Roll = Stream.FRandRange(0.0f, WeightSum);
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				Roll -= Weights[Index];
				if (Roll <= 0.0f)
				{
					return Candidates[Index];
				}
			}
			return Candidates.Last();
		};

		for (const FName NpcId : NpcIds)
		{
			const FName IngredientId = DefaultNpcIngredient(this, NpcId);
			const int32 Level = TryPickLevel(IngredientId, true);
			if (Level == INDEX_NONE)
			{
				FailReason = TEXT("NPC order infeasible");
				break;
			}
			FSPlannedOrder Slot;
			Slot.Kind = ESOrderSlotKind::Npc;
			Slot.NpcId = NpcId;
			Slot.Order = MakePlannedRequest(*this, IngredientId, Level);
			Remaining.FindOrAdd(IngredientId) -= OrderUnitCost(Level);
			SumValue += Slot.Order.SellValue;
			LevelCounts.FindOrAdd(Level)++;
			NpcSlots.Add(Slot);
		}
		if (!FailReason.IsEmpty() && NpcSlots.Num() < NpcIds.Num())
		{
			FailReason.Reset();
			continue;
		}

		// Keep a small reserve behind the visible seats so an independently freed
		// seat can demonstrate automatic replenishment without waiting for a new batch.
		int32 Guard = 0;
		while ((SumValue < EffectiveTarget
			|| GuestSlots.Num() + NpcSlots.Num() < MinimumAppearanceSlots)
			&& Guard++ < 64)
		{
			TArray<FName> IngredientChoices;
			TArray<float> IngredientWeights;
			float IngredientWeightSum = 0.0f;
			for (const FName Id : GetKnownIds())
			{
				if (Remaining.FindRef(Id) <= 0)
				{
					continue;
				}
				const float Weight = static_cast<float>(Remaining.FindRef(Id));
				IngredientChoices.Add(Id);
				IngredientWeights.Add(Weight);
				IngredientWeightSum += Weight;
			}
			if (IngredientChoices.IsEmpty() || IngredientWeightSum <= 0.0f)
			{
				break;
			}

			float Roll = Stream.FRandRange(0.0f, IngredientWeightSum);
			FName PickedIngredient = IngredientChoices.Last();
			for (int32 Index = 0; Index < IngredientChoices.Num(); ++Index)
			{
				Roll -= IngredientWeights[Index];
				if (Roll <= 0.0f)
				{
					PickedIngredient = IngredientChoices[Index];
					break;
				}
			}

			const int32 Level = TryPickLevel(PickedIngredient, false);
			if (Level == INDEX_NONE)
			{
				break;
			}

			FSPlannedOrder Slot;
			Slot.Kind = ESOrderSlotKind::Guest;
			Slot.Order = MakePlannedRequest(*this, PickedIngredient, Level);
			Remaining.FindOrAdd(PickedIngredient) -= OrderUnitCost(Level);
			SumValue += Slot.Order.SellValue;
			LevelCounts.FindOrAdd(Level)++;
			GuestSlots.Add(Slot);
		}

		// Shuffle guests, then insert NPC slots into unique first-half indices.
		for (int32 Index = GuestSlots.Num() - 1; Index > 0; --Index)
		{
			GuestSlots.Swap(Index, Stream.RandRange(0, Index));
		}

		const int32 TotalCount = GuestSlots.Num() + NpcSlots.Num();
		if (TotalCount == 0)
		{
			FailReason = TEXT("empty plan");
			continue;
		}

		const int32 HalfExclusive = FMath::Max(1, FMath::DivideAndRoundUp(TotalCount, 2));
		TArray<int32> InsertSlots;
		for (int32 Index = 0; Index < HalfExclusive; ++Index)
		{
			InsertSlots.Add(Index);
		}
		for (int32 Index = InsertSlots.Num() - 1; Index > 0; --Index)
		{
			InsertSlots.Swap(Index, Stream.RandRange(0, Index));
		}

		// Reserve final indices for the NPCs first, then stream the guests into what is left;
		// inserting one by one would shift the already placed NPCs out of the first half.
		TMap<int32, FSPlannedOrder> NpcByFinalIndex;
		bool bSeatedAllNpcs = true;
		for (const FSPlannedOrder& Npc : NpcSlots)
		{
			int32 Pick = INDEX_NONE;
			for (const int32 Candidate : InsertSlots)
			{
				if (!NpcByFinalIndex.Contains(Candidate))
				{
					Pick = Candidate;
					break;
				}
			}
			if (Pick == INDEX_NONE)
			{
				bSeatedAllNpcs = false;
				break;
			}
			NpcByFinalIndex.Add(Pick, Npc);
		}
		if (!bSeatedAllNpcs)
		{
			FailReason = TEXT("not enough first-half slots for NPCs");
			continue;
		}

		TArray<FSPlannedOrder> Assembled;
		Assembled.Reserve(TotalCount);
		int32 GuestCursor = 0;
		for (int32 Index = 0; Index < TotalCount; ++Index)
		{
			if (const FSPlannedOrder* Npc = NpcByFinalIndex.Find(Index))
			{
				Assembled.Add(*Npc);
			}
			else if (GuestSlots.IsValidIndex(GuestCursor))
			{
				Assembled.Add(GuestSlots[GuestCursor++]);
			}
		}
		while (GuestSlots.IsValidIndex(GuestCursor))
		{
			Assembled.Add(GuestSlots[GuestCursor++]);
		}

		if (ValidatePlannedOrders(Assembled, StartingStock, EffectiveTarget, FailReason))
		{
			PlannedDayOrders = MoveTemp(Assembled);
			NextPlannedOrderIndex = 0;
			bAccepted = true;
			break;
		}
	}

	if (!bAccepted)
	{
		// Last-resort greedy Lv0 fill so the day still has a feasible queue.
		TMap<FName, int32> Remaining = StartingStock;
		int32 SumValue = 0;
		for (const FName NpcId : NpcIds)
		{
			const FName IngredientId = DefaultNpcIngredient(this, NpcId);
			if (Remaining.FindRef(IngredientId) <= 0)
			{
				continue;
			}
			FSPlannedOrder Slot;
			Slot.Kind = ESOrderSlotKind::Npc;
			Slot.NpcId = NpcId;
			Slot.Order = MakePlannedRequest(*this, IngredientId, 0);
			Remaining.FindOrAdd(IngredientId) -= 1;
			SumValue += Slot.Order.SellValue;
			PlannedDayOrders.Add(Slot);
		}
		// 每隔几单抬一次等级，兜底队列也不会退化成清一色 Lv0。
		int32 GuestCounter = 0;
		for (const FName Id : GetKnownIds())
		{
			while (Remaining.FindRef(Id) > 0
				&& (SumValue < EffectiveTarget || PlannedDayOrders.Num() < MinimumAppearanceSlots))
			{
				const bool bPromote = (GuestCounter % 3) == 2 && Remaining.FindRef(Id) >= OrderUnitCost(1);
				const int32 Level = bPromote ? 1 : 0;
				FSPlannedOrder Slot;
				Slot.Kind = ESOrderSlotKind::Guest;
				Slot.Order = MakePlannedRequest(*this, Id, Level);
				Remaining.FindOrAdd(Id) -= OrderUnitCost(Level);
				SumValue += Slot.Order.SellValue;
				PlannedDayOrders.Add(Slot);
				++GuestCounter;
			}
		}
		UE_LOG(
			LogSSandbox,
			Warning,
			TEXT("Day order plan fell back to greedy Lv0 fill (%s)."),
			FailReason.IsEmpty() ? TEXT("retries exhausted") : *FailReason);
	}

	NextPlannedOrderIndex = 0;
	UE_LOG(
		LogSSandbox,
		Display,
		TEXT("Day order plan ready: %d slots, total=%d, target=%d/%d"),
		PlannedDayOrders.Num(),
		GetPlannedOrderTotalValue(),
		EffectiveTarget,
		RevenueTarget);
	return PlannedDayOrders.Num() > 0 || EffectiveTarget <= 0;
}

int32 USChefGameInstance::GetPlannedOrderTotalValue() const
{
	int32 Total = 0;
	for (const FSPlannedOrder& Slot : PlannedDayOrders)
	{
		Total += Slot.Order.SellValue;
	}
	return Total;
}

FString USChefGameInstance::GetPlannedOrderSummary() const
{
	if (PlannedDayOrders.IsEmpty())
	{
		return TEXT("订单队列：空");
	}

	TArray<FString> Parts;
	for (int32 Index = 0; Index < PlannedDayOrders.Num(); ++Index)
	{
		const FSPlannedOrder& Slot = PlannedDayOrders[Index];
		const TCHAR Marker = Index < NextPlannedOrderIndex ? TEXT('✓') : (Index == NextPlannedOrderIndex ? TEXT('>') : TEXT('·'));
		if (Slot.Kind == ESOrderSlotKind::Npc)
		{
			Parts.Add(FString::Printf(
				TEXT("%c%s:%s%d"),
				Marker,
				*Slot.NpcId.ToString(),
				*IngredientDisplayName(Slot.Order.IngredientId).Left(1),
				Slot.Order.Level));
		}
		else
		{
			Parts.Add(FString::Printf(
				TEXT("%c客:%s%d"),
				Marker,
				*IngredientDisplayName(Slot.Order.IngredientId).Left(1),
				Slot.Order.Level));
		}
	}
	return FString::Printf(
		TEXT("订单队列 %d/%d 总价%d｜%s"),
		FMath::Clamp(NextPlannedOrderIndex, 0, PlannedDayOrders.Num()),
		PlannedDayOrders.Num(),
		GetPlannedOrderTotalValue(),
		*FString::Join(Parts, TEXT(" ")));
}

bool USChefGameInstance::TryDequeueNextPlannedOrder(FSPlannedOrder& OutOrder)
{
	if (!PlannedDayOrders.IsValidIndex(NextPlannedOrderIndex))
	{
		return false;
	}

	OutOrder = PlannedDayOrders[NextPlannedOrderIndex++];
	NotifyStateChanged();
	return true;
}

void USChefGameInstance::RevealLeadingNpcOrders()
{
	ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this);
	while (PlannedDayOrders.IsValidIndex(NextPlannedOrderIndex)
		&& PlannedDayOrders[NextPlannedOrderIndex].Kind == ESOrderSlotKind::Npc)
	{
		const FName NpcId = PlannedDayOrders[NextPlannedOrderIndex].NpcId;
		if (NpcDirector)
		{
			NpcDirector->RevealNpc(NpcId, INDEX_NONE);
		}
		++NextPlannedOrderIndex;
	}
	NotifyStateChanged();
}

bool USChefGameInstance::TryPrepareNextGuestOrder(FSOrderRequest& OutOrder)
{
	RevealLeadingNpcOrders();
	if (!PlannedDayOrders.IsValidIndex(NextPlannedOrderIndex)
		|| PlannedDayOrders[NextPlannedOrderIndex].Kind != ESOrderSlotKind::Guest)
	{
		return false;
	}
	OutOrder = PlannedDayOrders[NextPlannedOrderIndex].Order;
	return true;
}

void USChefGameInstance::AdvancePastCurrentGuestOrder()
{
	if (PlannedDayOrders.IsValidIndex(NextPlannedOrderIndex)
		&& PlannedDayOrders[NextPlannedOrderIndex].Kind == ESOrderSlotKind::Guest)
	{
		++NextPlannedOrderIndex;
	}
	RevealLeadingNpcOrders();
}
#pragma endregion K2 moonyfli

void USChefGameInstance::AddRevenue(const int32 Amount)
{
	if (Amount <= 0 || !IsShopOpen())
	{
		return;
	}
	Revenue += Amount;
#pragma region K2 moonyfli
	// 达标只是解锁日结，营业照旧继续，直到时间结束或食材耗尽。
	if (Phase == ESGamePhase::DayRunning && Revenue >= RevenueTarget)
	{
		Phase = ESGamePhase::DayQualified;
		LastBoardFeedback = FString::Printf(
			TEXT("营业额达标 %d/%d，本关已过。继续营业到时间结束或食材耗尽即日结。"),
			Revenue,
			RevenueTarget);
		NotifyStateChanged();
		AutoSaveChefProfile(TEXT("营业额达标"));
		return;
	}
#pragma endregion K2 moonyfli
	NotifyStateChanged();
}

int32 USChefGameInstance::GetRevenueGap() const
{
	return FMath::Max(0, RevenueTarget - Revenue);
}

#pragma region K2 moonyfli
FSDayBalanceRow USChefGameInstance::GetDayBalance() const
{
	FSDayBalanceRow BuiltIn;
	if (UDataTable* Table = DayBalanceTable.LoadSynchronous())
	{
		if (const FSDayBalanceRow* Row = Table->FindRow<FSDayBalanceRow>(TEXT("Default"), TEXT("GetDayBalance"), false))
		{
			return *Row;
		}
		TArray<FSDayBalanceRow*> Rows;
		Table->GetAllRows(TEXT("GetDayBalance"), Rows);
		if (Rows.Num() > 0 && Rows[0])
		{
			return *Rows[0];
		}
	}
	return BuiltIn;
}

int32 USChefGameInstance::GetConfiguredMaxDishLevel() const
{
	return FMath::Clamp(GetDayBalance().MaxDishLevel, 0, 4);
}

int32 USChefGameInstance::GetServiceSeatCount() const
{
	const FSDayBalanceRow Balance = GetDayBalance();
	const int32 MaxSeats = FMath::Clamp(Balance.MaxServiceSeats, 1, 6);
	return FMath::Clamp(CustomerConcurrentMax, 1, MaxSeats);
}

FString USChefGameInstance::ResolveIngredientDisplayName(const FName IngredientId) const
{
	if (UDataTable* Table = IngredientTable.LoadSynchronous())
	{
		if (const FSIngredientDefRow* Row = Table->FindRow<FSIngredientDefRow>(IngredientId, TEXT("ResolveIngredientDisplayName"), false))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
		TArray<FSIngredientDefRow*> Rows;
		Table->GetAllRows(TEXT("ResolveIngredientDisplayName"), Rows);
		for (const FSIngredientDefRow* Row : Rows)
		{
			if (Row && Row->IngredientId == IngredientId && !Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
	}
	return IngredientDisplayName(IngredientId);
}

FString USChefGameInstance::ResolveIngredientShortName(const FName IngredientId) const
{
	if (UDataTable* Table = IngredientTable.LoadSynchronous())
	{
		if (const FSIngredientDefRow* Row = Table->FindRow<FSIngredientDefRow>(IngredientId, TEXT("ResolveIngredientShortName"), false))
		{
			if (!Row->ShortName.IsEmpty())
			{
				return Row->ShortName;
			}
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName.Left(1);
			}
		}
		TArray<FSIngredientDefRow*> Rows;
		Table->GetAllRows(TEXT("ResolveIngredientShortName"), Rows);
		for (const FSIngredientDefRow* Row : Rows)
		{
			if (Row && Row->IngredientId == IngredientId)
			{
				if (!Row->ShortName.IsEmpty())
				{
					return Row->ShortName;
				}
				if (!Row->DisplayName.IsEmpty())
				{
					return Row->DisplayName.Left(1);
				}
			}
		}
	}
	return IngredientDisplayName(IngredientId).Left(1);
}

bool USChefGameInstance::TryGetSpecialNpcDef(const FName NpcId, FSSpecialNpcDefRow& OutRow) const
{
	if (NpcId.IsNone())
	{
		return false;
	}
	if (UDataTable* Table = SpecialNpcTable.LoadSynchronous())
	{
		if (const FSSpecialNpcDefRow* Row = Table->FindRow<FSSpecialNpcDefRow>(NpcId, TEXT("TryGetSpecialNpcDef"), false))
		{
			OutRow = *Row;
			return true;
		}
		TArray<FSSpecialNpcDefRow*> Rows;
		Table->GetAllRows(TEXT("TryGetSpecialNpcDef"), Rows);
		for (const FSSpecialNpcDefRow* Row : Rows)
		{
			if (Row && Row->NpcId == NpcId)
			{
				OutRow = *Row;
				return true;
			}
		}
	}
	return false;
}

bool USChefGameInstance::TryGetGiftDef(const FName GiftId, FSGiftDefRow& OutRow) const
{
	if (GiftId.IsNone())
	{
		return false;
	}
	if (UDataTable* Table = GiftTable.LoadSynchronous())
	{
		if (const FSGiftDefRow* Row = Table->FindRow<FSGiftDefRow>(GiftId, TEXT("TryGetGiftDef"), false))
		{
			OutRow = *Row;
			return true;
		}
		TArray<FSGiftDefRow*> Rows;
		Table->GetAllRows(TEXT("TryGetGiftDef"), Rows);
		for (const FSGiftDefRow* Row : Rows)
		{
			if (Row && Row->GiftId == GiftId)
			{
				OutRow = *Row;
				return true;
			}
		}
	}
	return false;
}

TArray<FString> USChefGameInstance::GetCustomerNamePool() const
{
	TArray<FString> Names;
	if (UDataTable* Table = CustomerNameTable.LoadSynchronous())
	{
		TArray<FSCustomerNameRow*> Rows;
		Table->GetAllRows(TEXT("GetCustomerNamePool"), Rows);
		for (const FSCustomerNameRow* Row : Rows)
		{
			if (Row && !Row->DisplayName.IsEmpty())
			{
				Names.Add(Row->DisplayName);
			}
		}
	}
	if (Names.IsEmpty())
	{
		Names = GetCustomerNames();
	}
	return Names;
}
#pragma endregion K2 moonyfli

FString USChefGameInstance::GetGiftDisplayName(const FName GiftId)
{
	UDataTable* Table = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/Shared/Data/DT_Gifts.DT_Gifts"));
	if (Table)
	{
		if (const FSGiftDefRow* Row = Table->FindRow<FSGiftDefRow>(GiftId, TEXT("GetGiftDisplayName"), false))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
		TArray<FSGiftDefRow*> Rows;
		Table->GetAllRows(TEXT("GetGiftDisplayName"), Rows);
		for (const FSGiftDefRow* Row : Rows)
		{
			if (Row && Row->GiftId == GiftId && !Row->DisplayName.IsEmpty())
			{
				return Row->DisplayName;
			}
		}
	}
	if (GiftId == GiftGuideKiteId) return TEXT("引路纸鸢");
	if (GiftId == GiftLifeLampId) return TEXT("借命纸灯");
	if (GiftId == GiftBeatCoinId) return TEXT("定键铜钱");
	if (GiftId == GiftGluttonBoxId) return TEXT("饕餮食盒");
	return GiftId.ToString();
}

void USChefGameInstance::RebuildGiftBuffState()
{
	GiftBuffState = FSGiftBuffState();
	for (const FName GiftId : ActiveGiftIds)
	{
		FSGiftDefRow Def;
		if (TryGetGiftDef(GiftId, Def))
		{
			GiftBuffState.bGuideKite |= Def.bGuideKite;
			GiftBuffState.bLifeLamp |= Def.bLifeLamp;
			GiftBuffState.bBeatCoin |= Def.bBeatCoin;
			GiftBuffState.bGluttonBox |= Def.bGluttonBox;

			const FString Trigger = Def.EffectTrigger.ToString();
			if (Trigger.Equals(TEXT("BeforeFork"), ESearchCase::IgnoreCase))
			{
				GiftBuffState.PreForkGatherRhythmBonus = FMath::Max(
					GiftBuffState.PreForkGatherRhythmBonus, Def.EffectValue);
			}
			else if (Trigger.Equals(TEXT("AfterFork"), ESearchCase::IgnoreCase))
			{
				GiftBuffState.PostForkInvulnDashSeconds = FMath::Max(
					GiftBuffState.PostForkInvulnDashSeconds, Def.EffectValue);
			}
			else if (Trigger.Equals(TEXT("NearDeath"), ESearchCase::IgnoreCase))
			{
				GiftBuffState.NearDeathHeal = FMath::Max(
					GiftBuffState.NearDeathHeal, Def.EffectValue);
			}
			else
			{
				// Live Coding may not yet expose EffectTrigger on the DataTable row;
				// fall back to stable GiftId mapping from the design sheet.
				if (GiftId == TEXT("WindfallWealth"))
				{
					GiftBuffState.PreForkGatherRhythmBonus = FMath::Max(
						GiftBuffState.PreForkGatherRhythmBonus, 0.3f);
				}
				else if (GiftId == TEXT("BossPie"))
				{
					GiftBuffState.PostForkInvulnDashSeconds = FMath::Max(
						GiftBuffState.PostForkInvulnDashSeconds, 2.5f);
				}
				else if (GiftId == TEXT("WildMilk"))
				{
					GiftBuffState.NearDeathHeal = FMath::Max(
						GiftBuffState.NearDeathHeal, 40.0f);
				}
			}
			continue;
		}
		if (GiftId == GiftGuideKiteId) GiftBuffState.bGuideKite = true;
		else if (GiftId == GiftLifeLampId) GiftBuffState.bLifeLamp = true;
		else if (GiftId == GiftBeatCoinId) GiftBuffState.bBeatCoin = true;
		else if (GiftId == GiftGluttonBoxId) GiftBuffState.bGluttonBox = true;
	}
}

#pragma region K2 moonyfli
FString USChefGameInstance::GetGiftEffectText(const FName GiftId)
{
	UDataTable* Table = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/Shared/Data/DT_Gifts.DT_Gifts"));
	if (Table)
	{
		if (const FSGiftDefRow* Row = Table->FindRow<FSGiftDefRow>(GiftId, TEXT("GetGiftEffectText"), false))
		{
			if (!Row->EffectText.IsEmpty())
			{
				return Row->EffectText;
			}
		}
		TArray<FSGiftDefRow*> Rows;
		Table->GetAllRows(TEXT("GetGiftEffectText"), Rows);
		for (const FSGiftDefRow* Row : Rows)
		{
			if (Row && Row->GiftId == GiftId && !Row->EffectText.IsEmpty())
			{
				return Row->EffectText;
			}
		}
	}
	if (GiftId == GiftGuideKiteId) return TEXT("夜路提前显影");
	if (GiftId == GiftLifeLampId) return TEXT("多一次容错");
	if (GiftId == GiftBeatCoinId) return TEXT("判定窗口放宽");
	if (GiftId == GiftGluttonBoxId) return TEXT("怪物权重提高，收获更多");
	return TEXT("未知效果");
}

bool USChefGameInstance::GrantGift(const FName GiftId)
{
	if (!IsKnownGiftId(GiftId))
	{
		LastBoardFeedback = FString::Printf(TEXT("未知谢礼 ID：%s，已拒绝。"), *GiftId.ToString());
		NotifyStateChanged();
		return false;
	}
	if (ActiveGiftIds.Contains(GiftId))
	{
		LastBoardFeedback = FString::Printf(TEXT("谢礼 %s 本日已生效，不再重复发放。"), *GetGiftDisplayName(GiftId));
		NotifyStateChanged();
		return false;
	}

	ActiveGiftIds.Add(GiftId);
	RebuildGiftBuffState();
	BuildNightBootstrap();
	LastBoardFeedback = FString::Printf(
		TEXT("获得谢礼：%s（%s）——已立即生效，今夜可用。当前谢礼 %d 件。"),
		*GetGiftDisplayName(GiftId),
		*GetGiftEffectText(GiftId),
		ActiveGiftIds.Num());
	NotifyStateChanged();
	return true;
}

FString USChefGameInstance::GetGiftTabSummary() const
{
	if (ActiveGiftIds.IsEmpty())
	{
		return TEXT("谢礼页签：本次无谢礼（完成阿翎/桑婆的委托即可获得，拿到即生效）。");
	}

	TArray<FString> Cards;
	for (const FName GiftId : ActiveGiftIds)
	{
		Cards.Add(FString::Printf(
			TEXT("【%s】%s"),
			*GetGiftDisplayName(GiftId),
			*GetGiftEffectText(GiftId)));
	}
	return FString::Printf(
		TEXT("谢礼页签（%d 件，已生效，入夜自动带上）：%s"),
		ActiveGiftIds.Num(),
		*FString::Join(Cards, TEXT("　")));
}

FSRunSnapshot USChefGameInstance::CaptureSnapshot() const
{
	FSRunSnapshot Snapshot;
	Snapshot.bValid = true;
	Snapshot.StageId = StageId;
	Snapshot.Inventory = Inventory;
	Snapshot.Revenue = Revenue;
	Snapshot.RevenueTarget = RevenueTarget;
	Snapshot.GiftIds = ActiveGiftIds;
	Snapshot.GiftBuffState = GiftBuffState;
	Snapshot.PlannedDayOrders = PlannedDayOrders;
	Snapshot.NextPlannedOrderIndex = NextPlannedOrderIndex;

	// The board is not part of the snapshot, so pieces already paid for must fold back in.
	if (const ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		TMap<FName, int32> BoardUnits;
		if (Board->GetPendingReclaimUnits(BoardUnits) > 0)
		{
			for (const TPair<FName, int32>& Pair : BoardUnits)
			{
				Snapshot.Inventory.FindOrAdd(Pair.Key) += Pair.Value;
			}
		}
	}
	return Snapshot;
}

void USChefGameInstance::RestoreSnapshot(const FSRunSnapshot& Snapshot)
{
	if (!Snapshot.bValid)
	{
		return;
	}

	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		// Snapshot inventory already accounts for board pieces; drop them without refunding.
		Board->ClearActiveDrag();
		Board->ClearBoard();
	}

	Inventory = Snapshot.Inventory;
	Revenue = Snapshot.Revenue;
	ActiveGiftIds = Snapshot.GiftIds;
	GiftBuffState = Snapshot.GiftBuffState;
	PlannedDayOrders = Snapshot.PlannedDayOrders;
	NextPlannedOrderIndex = Snapshot.NextPlannedOrderIndex;
	if (!Snapshot.StageId.IsNone() && Snapshot.StageId != StageId)
	{
		ApplyStage(Snapshot.StageId);
	}
	RevenueTarget = Snapshot.RevenueTarget > 0 ? Snapshot.RevenueTarget : RevenueTarget;
	BuildNightBootstrap();
}

void USChefGameInstance::ResetDayDirectors(const bool bStartService)
{
	ASCustomerDirector* CustomerDirector = ASCustomerDirector::FindDirector(this);
	ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this);
	if (CustomerDirector)
	{
		CustomerDirector->ResetDirector();
	}
	if (NpcDirector)
	{
		NpcDirector->ResetDirector();
	}
	if (!bStartService)
	{
		return;
	}
	// NPC roster must exist before the customer director reveals leading slots / spawns guests.
	if (NpcDirector)
	{
		NpcDirector->NotifyDayStarted();
	}
	if (CustomerDirector)
	{
		CustomerDirector->NotifyDayStarted();
	}
}

bool USChefGameInstance::StartNight()
{
	if (Phase == ESGamePhase::NightRunning)
	{
		return true;
	}
	if (Phase != ESGamePhase::PrepareNight && Phase != ESGamePhase::Boot)
	{
		LastBoardFeedback = FString::Printf(TEXT("当前阶段 %s 不能入夜。"), *GetPhaseDisplayName());
		NotifyStateChanged();
		return false;
	}

	NightStartSnapshot = CaptureSnapshot();
	Phase = ESGamePhase::NightRunning;
	BuildNightBootstrap();
	LastBoardFeedback = FString::Printf(
		TEXT("已入夜（%s）：夜初快照已建立，夜败将清除本次收获。%s"),
		*StageId.ToString(),
		*GetGiftTabSummary());
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("入夜建立夜初快照"));
	return true;
}

void USChefGameInstance::EnterPrepareDay(const FString& Reason)
{
	Phase = ESGamePhase::PrepareDay;
	Revenue = 0;
	// 谢礼按天结算：上一日的谢礼已被刚结束的夜晚用掉。
	ActiveGiftIds.Empty();
	RebuildGiftBuffState();
	BuildNightBootstrap();

	DayTimeRemaining = FMath::Max(1.0f, DayDurationSeconds);
	DayStuckCheckAccum = 0.0f;
	bDayHadResources = false;
	BuildPlannedDayOrders();
	DayStartSnapshot = CaptureSnapshot();

	Phase = ESGamePhase::DayRunning;
	ResetDayDirectors(true);
	LastBoardFeedback = FString::Printf(
		TEXT("%s开店 %s：营业 %.0fs，目标 %d（含结转 +%d）。%s"),
		*Reason,
		*StageId.ToString(),
		DayTimeRemaining,
		RevenueTarget,
		CarryOverTargetBonus,
		*GetPlannedOrderSummary());
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("开店并建立日初快照"));
}

void USChefGameInstance::FailDay(const ESDayEndReason Reason)
{
	const int32 RevenueAtFail = Revenue;
	LastDayEndReason = Reason;
	RestoreSnapshot(DayStartSnapshot);
	ResetDayDirectors(false);
	LastBoardFeedback = FString::Printf(
		TEXT("白天失败（%s）：营业额 %d/%d 未达标，已回档日初，重开当日。"),
		DayEndReasonText(Reason),
		RevenueAtFail,
		RevenueTarget);
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("白天失败回档日初"));
	EnterPrepareDay(TEXT("回档重开｜"));
}

void USChefGameInstance::EnterDaySettlement(const ESDayEndReason Reason)
{
	Phase = ESGamePhase::DaySettlement;
	LastDayEndReason = Reason;

	// 保留剩余库存：盘上棋子先按付费单位退回。
	const int32 ReclaimedUnits = ReclaimBoardPiecesOnClose();
	const int32 LeftoverUnits = CountPantryUnits();
	CarryOverTargetBonus = Reason == ESDayEndReason::TimeUp
		? LeftoverUnits * FMath::Max(0, GetDayBalance().CarryOverTargetBonusPerUnit)
		: 0;
	CompletedDayFlags.AddUnique(StageId);
	DayTimeRemaining = 0.0f;
	ResetDayDirectors(false);

	LastBoardFeedback = FString::Printf(
		TEXT("日结（%s）：营业额 %d/%d 达标。%s结转食材 %d 份，下一关目标 +%d。"),
		DayEndReasonText(Reason),
		Revenue,
		RevenueTarget,
		*FormatReclaimSuffix(ReclaimedUnits),
		LeftoverUnits,
		CarryOverTargetBonus);
	NotifyStateChanged();
	AdvanceToNextStage();
}

void USChefGameInstance::AdvanceToNextStage()
{
	Phase = ESGamePhase::PrepareNextStage;
	const FName FinishedStage = StageId;
	const bool bEnding = ActiveStageRow.bEndingAfterDay || ActiveStageRow.NextLevelId.IsNone();
	bAwaitingNightRetry = false;

	if (bEnding)
	{
		Phase = ESGamePhase::Ending;
		BuildNightBootstrap();
		LastBoardFeedback = FString::Printf(TEXT("%s 日结完成，进入尾声，不再开启下一夜。"), *FinishedStage.ToString());
		NotifyStateChanged();
		AutoSaveChefProfile(TEXT("最终关日结进入尾声"));
		return;
	}

	const FName NextId = ActiveStageRow.NextLevelId;
	Revenue = 0;
	if (!ApplyStage(NextId))
	{
		Phase = ESGamePhase::PrepareNight;
		LastBoardFeedback = FString::Printf(TEXT("日结完成，但推进到 %s 失败，停在 PrepareNight。"), *NextId.ToString());
		NotifyStateChanged();
		return;
	}

	Phase = ESGamePhase::PrepareNight;
	LastBoardFeedback = FString::Printf(
		TEXT("%s 日结 → 下一关 %s：目标 %d（基础 %d + 结转 %d）。%s"),
		*FinishedStage.ToString(),
		*StageId.ToString(),
		RevenueTarget,
		ActiveStageRow.RevenueTarget,
		CarryOverTargetBonus,
		*GetGiftTabSummary());
	NotifyStateChanged();
	AutoSaveChefProfile(TEXT("进入下一关夜晚"));
}

const TCHAR* USChefGameInstance::DayEndReasonText(const ESDayEndReason Reason)
{
	switch (Reason)
	{
	case ESDayEndReason::TimeUp: return TEXT("营业时间结束");
	case ESDayEndReason::OutOfIngredients: return TEXT("食材耗尽");
	default: return TEXT("未知");
	}
}

int32 USChefGameInstance::CountPantryUnits() const
{
	int32 Units = 0;
	for (const TPair<FName, int32>& Pair : Inventory)
	{
		Units += FMath::Max(0, Pair.Value);
	}
	return Units;
}

int32 USChefGameInstance::CountChainUnitsAvailable(const FName IngredientId) const
{
	int32 Units = GetQuantity(IngredientId);
	if (const ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		for (const FSMergeCell& Cell : Board->GetCells())
		{
			if (Cell.bOccupied && Cell.Piece.IngredientId == IngredientId)
			{
				Units += 1 << FMath::Clamp(Cell.Piece.Level, 0, MaxDishLevel);
			}
		}
	}
	return Units;
}

bool USChefGameInstance::CanFulfillOrder(const FSOrderRequest& Order) const
{
	if (Order.IngredientId.IsNone())
	{
		return false;
	}

	const int32 Level = FMath::Clamp(Order.Level, 0, MaxDishLevel);
	if (CountChainUnitsAvailable(Order.IngredientId) < (1 << Level))
	{
		return false;
	}

	const ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!Board)
	{
		return true;
	}
	// The exact level is servable as-is; anything else still needs room to spawn or merge.
	return Board->CountPiecesAtLevel(Order.IngredientId, Level) > 0 || Board->GetEmptyCellCount() > 0;
}

bool USChefGameInstance::HasDayResourcesLeft() const
{
	const ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (Board && Board->GetOccupiedCellCount() > 0)
	{
		// A finished dish can always be served to a later guest.
		return true;
	}
	return CountPantryUnits() > 0 && (!Board || Board->GetEmptyCellCount() > 0);
}

bool USChefGameInstance::HasCompletableOrder() const
{
	if (const ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		for (const FSCustomerState& Customer : Director->GetActiveCustomers())
		{
			if (Customer.bActive && CanFulfillOrder(Customer.Order))
			{
				return true;
			}
		}
	}
	if (const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
		{
			if (Npc.bPresent && !Npc.bServed && CanFulfillOrder(Npc.Order))
			{
				return true;
			}
		}
	}
	// 座位可能正在轮换，只有连一份菜都做不出来才算「无可完成订单」。
	return HasDayResourcesLeft();
}

void USChefGameInstance::TickDayClock(const float DeltaSeconds)
{
	if (!IsShopOpen() || DeltaSeconds <= 0.0f)
	{
		return;
	}

	DayTimeRemaining = FMath::Max(0.0f, DayTimeRemaining - DeltaSeconds);
	if (DayTimeRemaining <= 0.0f)
	{
		CloseShopNow(ESDayEndReason::TimeUp);
		return;
	}

	DayStuckCheckAccum += DeltaSeconds;
	if (DayStuckCheckAccum >= 0.5f)
	{
		DayStuckCheckAccum = 0.0f;
		bDayHadResources |= HasDayResourcesLeft();
		if (bDayHadResources && !HasCompletableOrder())
		{
			CloseShopNow(ESDayEndReason::OutOfIngredients);
		}
	}
}

bool USChefGameInstance::CloseShopNow(const ESDayEndReason Reason)
{
	if (!IsShopOpen())
	{
		LastBoardFeedback = TEXT("当前不在营业中，无法闭店。");
		NotifyStateChanged();
		return false;
	}

	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->ClearActiveDrag();
	}

	if (Phase == ESGamePhase::DayQualified)
	{
		EnterDaySettlement(Reason);
	}
	else
	{
		FailDay(Reason);
	}
	return true;
}

bool USChefGameInstance::ForceCloseShopForDebug()
{
	return CloseShopNow(ESDayEndReason::TimeUp);
}

void USChefGameInstance::OpenShopForDebug()
{
	if (IsShopOpen())
	{
		return;
	}
	Phase = ESGamePhase::DayRunning;
	DayTimeRemaining = FMath::Max(1.0f, DayDurationSeconds);
	DayStuckCheckAccum = 0.0f;
	bDayHadResources = false;
	if (PlannedDayOrders.IsEmpty())
	{
		BuildPlannedDayOrders();
	}
	DayStartSnapshot = CaptureSnapshot();
	ResetDayDirectors(true);
	LastBoardFeedback = TEXT("调试开店：倒计时已启动。");
	NotifyStateChanged();
}
#pragma endregion K2 moonyfli

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
	// 上一关时间到闭店时的剩余食材已结转，目标相应抬高。
	RevenueTarget = Row.RevenueTarget + FMath::Max(0, CarryOverTargetBonus); //add by K2
	CustomerSpawnIntervalSeconds = Row.CustomerSpawnInterval;
	CustomerConcurrentMax = Row.CustomerConcurrentMax;
	CarryOverTargetBonusPerUnit = GetDayBalance().CarryOverTargetBonusPerUnit;
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
	PendingNightBootstrap.FoeWeightOverride = GiftBuffState.bGluttonBox
		? GetDayBalance().GluttonBoxFoeWeight
		: -1.0f;
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

#pragma region K2 moonyfli
	ActiveGiftIds.Empty();
	RebuildGiftBuffState();
	Revenue = 0;
	DayTimeRemaining = 0.0f;
	NightStartSnapshot = FSRunSnapshot();
	DayStartSnapshot = FSRunSnapshot();
	Phase = ESGamePhase::PrepareNight;
#pragma endregion K2 moonyfli
	const int32 ReclaimedUnits = ReclaimBoardPiecesOnClose(); //add by K2
	ResetDayDirectors(false); //add by K2

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

#pragma region K2 moonyfli
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

bool USChefGameInstance::FailDayForDebug()
{
	if (!IsShopOpen())
	{
		LastBoardFeedback = TEXT("仅白天营业中可判失败回档。");
		NotifyStateChanged();
		return false;
	}

	// 达标后强制判失败也走回档，用于验证日初快照。
	Phase = ESGamePhase::DayRunning;
	FailDay(ESDayEndReason::TimeUp);
	return true;
}

#pragma region K2 moonyfli
bool USChefGameInstance::SetInventoryQuantityForDebug(const FName IngredientId, const int32 Quantity)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!IsKnownIngredient(IngredientId))
	{
		LastBoardFeedback = FString::Printf(TEXT("未知食材 ID：%s"), *IngredientId.ToString());
		NotifyStateChanged();
		return false;
	}
	const int32 Clamped = FMath::Max(0, Quantity);
	Inventory.FindOrAdd(IngredientId) = Clamped;
	LastBoardFeedback = FString::Printf(
		TEXT("修改器：%s 库存设为 %d。"),
		*ResolveIngredientDisplayName(IngredientId),
		Clamped);
	NotifyStateChanged();
	return true;
#endif
}

void USChefGameInstance::ClearActiveGiftsForDebug()
{
#if UE_BUILD_SHIPPING
	return;
#else
	ActiveGiftIds.Empty();
	RebuildGiftBuffState();
	BuildNightBootstrap();
	LastBoardFeedback = TEXT("修改器：已清空本日谢礼。");
	NotifyStateChanged();
#endif
}

void USChefGameInstance::SetDayTimeRemainingForDebug(const float Seconds)
{
#if UE_BUILD_SHIPPING
	return;
#else
	DayTimeRemaining = FMath::Max(0.0f, Seconds);
	LastBoardFeedback = FString::Printf(TEXT("修改器：营业剩余时间设为 %.1fs。"), DayTimeRemaining);
	NotifyStateChanged();
#endif
}

void USChefGameInstance::ForceQualifyRevenueForDebug()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsShopOpen())
	{
		LastBoardFeedback = TEXT("修改器：仅营业中可直接达标。");
		NotifyStateChanged();
		return;
	}
	const int32 Gap = FMath::Max(0, RevenueTarget - Revenue);
	if (Gap > 0)
	{
		AddRevenue(Gap);
	}
	else
	{
		LastBoardFeedback = FString::Printf(TEXT("修改器：营业额已达标 %d/%d。"), Revenue, RevenueTarget);
		NotifyStateChanged();
	}
#endif
}
#pragma endregion K2 moonyfli

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

#pragma region K2 moonyfli
	SaveObject.RevenueProgress = Revenue;
	SaveObject.ActiveGiftIds = ActiveGiftIds;
	SaveObject.DayTimeRemaining = DayTimeRemaining;
	SaveObject.CarryOverTargetBonus = CarryOverTargetBonus;
	SaveObject.LastDayEndReason = LastDayEndReason;
	SaveObject.NightStartSnapshot = NightStartSnapshot;
	SaveObject.DayStartSnapshot = DayStartSnapshot;
	SaveObject.PlannedDayOrders = PlannedDayOrders;
	SaveObject.NextPlannedOrderIndex = NextPlannedOrderIndex;
#pragma endregion K2 moonyfli
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

#pragma region K2 moonyfli
	CarryOverTargetBonus = FMath::Max(0, SaveObject.CarryOverTargetBonus);
#pragma endregion K2 moonyfli
	if (!ApplyStage(SaveObject.CurrentStageId.IsNone() ? FName(TEXT("T0")) : SaveObject.CurrentStageId))
	{
		return false;
	}

	Revenue = FMath::Max(0, SaveObject.RevenueProgress);
#pragma region K2 moonyfli
	ActiveGiftIds = SaveObject.ActiveGiftIds;
	RebuildGiftBuffState();
	LastDayEndReason = SaveObject.LastDayEndReason;
	NightStartSnapshot = SaveObject.NightStartSnapshot;
	DayStartSnapshot = SaveObject.DayStartSnapshot;
	PlannedDayOrders = SaveObject.PlannedDayOrders;
	NextPlannedOrderIndex = SaveObject.NextPlannedOrderIndex;
	DayTimeRemaining = FMath::Max(0.0f, SaveObject.DayTimeRemaining);
	DayStuckCheckAccum = 0.0f;
#pragma endregion K2 moonyfli
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

#pragma region K2 moonyfli
	// 中途强退按「回档到该阶段开始」处理，和失败回档同一套语义。
	const bool bWasInDay = SaveObject.Phase == ESGamePhase::PrepareDay
		|| SaveObject.Phase == ESGamePhase::DayRunning
		|| SaveObject.Phase == ESGamePhase::DayQualified
		|| SaveObject.Phase == ESGamePhase::DaySettlement
		|| SaveObject.Phase == ESGamePhase::NightSettlement;

	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->ClearActiveDrag();
		Board->ClearBoard();
	}

	if (SaveObject.Phase == ESGamePhase::Ending)
	{
		Phase = ESGamePhase::Ending;
		BuildNightBootstrap();
		ResetDayDirectors(false);
		return true;
	}

	if (bWasInDay && !bAwaitingNightRetry)
	{
		if (DayStartSnapshot.bValid)
		{
			RestoreSnapshot(DayStartSnapshot);
		}
		Phase = ESGamePhase::DayRunning;
		DayTimeRemaining = DayTimeRemaining > 0.0f ? DayTimeRemaining : FMath::Max(1.0f, DayDurationSeconds);
		BuildNightBootstrap();
		ResetDayDirectors(true);
		return true;
	}

	Phase = ESGamePhase::PrepareNight;
	BuildNightBootstrap();
	ResetDayDirectors(false);
#pragma endregion K2 moonyfli

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
#pragma region K2 moonyfli
	ActiveGiftIds.Empty();
	GiftBuffState = FSGiftBuffState();
	Revenue = 0;
	DayTimeRemaining = 0.0f;
	DayStuckCheckAccum = 0.0f;
	CarryOverTargetBonus = 0;
	LastDayEndReason = ESDayEndReason::None;
	NightStartSnapshot = FSRunSnapshot();
	DayStartSnapshot = FSRunSnapshot();
	PlannedDayOrders.Reset();
	NextPlannedOrderIndex = 0;
#pragma endregion K2 moonyfli
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
	ResetDayDirectors(false); //add by K2
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

#pragma region K2 moonyfli
int32 ASMergeBoard::FindRandomEmptyCell() const
{
	TArray<int32> EmptyIndices;
	EmptyIndices.Reserve(Cells.Num());
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FSMergeCell& Cell = Cells[Index];
		if (Cell.bEnabled && !Cell.bOccupied)
		{
			EmptyIndices.Add(Index);
		}
	}
	if (EmptyIndices.Num() == 0)
	{
		return INDEX_NONE;
	}
	return EmptyIndices[FMath::RandRange(0, EmptyIndices.Num() - 1)];
}
#pragma endregion K2 moonyfli

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

	if (!GameInstance->IsShopOpen())
	{
		SetFeedback(TEXT("尚未开店（非营业阶段），无法取材。"));
		return false;
	}

	if (Cells.Num() == 0)
	{
		BuildDefaultIrregularBoard();
	}

#pragma region K2 moonyfli
	const int32 EmptyIndex = FindRandomEmptyCell();
#pragma endregion K2 moonyfli
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
		TEXT("母棋子 %s：随机空格 #%d 生成 Lv0，库存 %d→%d，占用 %d/%d。"),
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

#pragma region K2 moonyfli
bool ASMergeBoard::TryDecomposePieceToInventory(const int32 CellIndex)
{
	FSDishPiece Piece;
	if (!TryGetPiece(CellIndex, Piece))
	{
		SetFeedback(TEXT("分解来源格没有食材，已回弹。"));
		return false;
	}
	if (Piece.Level <= 0)
	{
		SetFeedback(TEXT("基础食材无需分解，已留在原格。"));
		return false;
	}

	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance || Piece.PaidUnits <= 0
		|| !GameInstance->AddIngredient(Piece.IngredientId, Piece.PaidUnits))
	{
		SetFeedback(TEXT("分解入库失败，食材已安全留在原格。"));
		return false;
	}

	ClearCell(CellIndex);
	ClearActiveDrag();
	SetFeedback(FString::Printf(
		TEXT("撤销合成：%s Lv%d 已分解为 %d 份基础食材并退回库存。"),
		*IngredientDisplayName(Piece.IngredientId),
		Piece.Level,
		Piece.PaidUnits));
	return true;
}
#pragma endregion K2 moonyfli

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

	GameInstance->OpenShopForDebug(); //add by K2

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

	GameInstance->OpenShopForDebug(); //add by K2
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
#pragma region K2 moonyfli
	ActiveCustomers.Reset();
	SeatCooldowns.Init(0.0f, GetConfiguredSeatCount());
	bOrderQueueExhausted = false;
#pragma endregion K2 moonyfli
	bDayServiceActive = false;
	NextCustomerNumber = 1;
}

void ASCustomerDirector::NotifyDayStarted()
{
	bDayServiceActive = true;
#pragma region K2 moonyfli
	ActiveCustomers.Reset();
	const int32 SeatCount = GetConfiguredSeatCount();
	SeatCooldowns.Init(0.0f, SeatCount);
	bOrderQueueExhausted = false;
#pragma endregion K2 moonyfli
	if (const USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		SpawnIntervalSeconds = GameInstance->CustomerSpawnIntervalSeconds;
	}
#pragma region K2 moonyfli
	// Every seat owns its own arrival clock. All start ready and independently
	// consume the unified appearance queue.
	for (int32 SeatIndex = 0; SeatIndex < SeatCount; ++SeatIndex)
	{
		TryFillSeat(SeatIndex);
	}
#pragma endregion K2 moonyfli
}

USChefGameInstance* ASCustomerDirector::GetChefGameInstance() const
{
	return GetGameInstance<USChefGameInstance>();
}

#pragma region K2 moonyfli
int32 ASCustomerDirector::GetConfiguredSeatCount() const
{
	if (const USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		return GameInstance->GetServiceSeatCount();
	}
	return 2;
}
#pragma endregion K2 moonyfli

void ASCustomerDirector::SetFeedback(const FString& Message)
{
	if (USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		GameInstance->LastBoardFeedback = Message;
		GameInstance->OnSandboxStateChanged.Broadcast();
	}
	UE_LOG(LogSSandbox, Display, TEXT("%s"), *Message);
}

bool ASCustomerDirector::SpawnNextPlannedCustomer()
{
	const int32 SeatCount = GetConfiguredSeatCount();
	for (int32 SeatIndex = 0; SeatIndex < SeatCount; ++SeatIndex)
	{
		if (!IsSeatOccupied(SeatIndex) && GetSeatCooldownRemaining(SeatIndex) <= 0.0f)
		{
			return TryFillSeat(SeatIndex);
		}
	}
	return false;
}

#pragma region K2 moonyfli
bool ASCustomerDirector::TryFillSeat(const int32 SeatIndex)
{
	if (!SeatCooldowns.IsValidIndex(SeatIndex) || IsSeatOccupied(SeatIndex) || bOrderQueueExhausted)
	{
		return false;
	}

	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	FSPlannedOrder Slot;
	if (!GameInstance->TryDequeueNextPlannedOrder(Slot))
	{
		bOrderQueueExhausted = true;
		return false;
	}

	if (Slot.Kind == ESOrderSlotKind::Npc)
	{
		if (ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
		{
			if (NpcDirector->RevealNpc(Slot.NpcId, SeatIndex))
			{
				SeatCooldowns[SeatIndex] = 0.0f;
				return true;
			}
		}

		// A malformed NPC slot must not block this seat forever.
		SeatCooldowns[SeatIndex] = SpawnIntervalSeconds;
		return false;
	}

	FSCustomerState Customer;
	Customer.bActive = true;
	Customer.SeatIndex = SeatIndex;
	const int32 CustomerNumber = NextCustomerNumber++;
	const TArray<FString> CustomerNames = GameInstance->GetCustomerNamePool();
	Customer.CustomerId = FString::Printf(TEXT("Guest-%02d"), CustomerNumber);
	Customer.DisplayName = CustomerNames[(CustomerNumber - 1) % CustomerNames.Num()];
	Customer.Order = Slot.Order;
	ActiveCustomers.Add(Customer);
	SeatCooldowns[SeatIndex] = 0.0f;

	SetFeedback(FString::Printf(
		TEXT("座位 %d：顾客 %s（%s）入座，订单 %s（售价 %d），会一直等待。%s"),
		SeatIndex + 1,
		*Customer.DisplayName,
		*Customer.CustomerId,
		*Customer.Order.RecipeId.ToString(),
		Customer.Order.SellValue,
		*GameInstance->GetPlannedOrderSummary()));
	return true;
}

bool ASCustomerDirector::IsSeatOccupied(const int32 SeatIndex) const
{
	if (ActiveCustomers.ContainsByPredicate(
		[SeatIndex](const FSCustomerState& Customer) { return Customer.bActive && Customer.SeatIndex == SeatIndex; }))
	{
		return true;
	}

	if (const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
	{
		for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
		{
			if (Npc.bPresent && !Npc.bServed && Npc.SeatIndex == SeatIndex)
			{
				return true;
			}
		}
	}
	return false;
}

void ASCustomerDirector::ClearCustomer(const FString& CustomerId, const FString& Reason)
{
	const int32 CustomerIndex = ActiveCustomers.IndexOfByPredicate(
		[&CustomerId](const FSCustomerState& Customer) { return Customer.CustomerId == CustomerId; });
	if (!ActiveCustomers.IsValidIndex(CustomerIndex))
	{
		return;
	}

	const int32 SeatIndex = ActiveCustomers[CustomerIndex].SeatIndex;
	ActiveCustomers.RemoveAt(CustomerIndex);
	NotifySeatVacated(SeatIndex);
	SetFeedback(Reason);
}

void ASCustomerDirector::NotifySeatVacated(const int32 SeatIndex)
{
	if (SeatCooldowns.IsValidIndex(SeatIndex))
	{
		SeatCooldowns[SeatIndex] = SpawnIntervalSeconds;
		bOrderQueueExhausted = false;
	}
}

bool ASCustomerDirector::TryGetCustomerAtSeat(const int32 SeatIndex, FSCustomerState& OutCustomer) const
{
	for (const FSCustomerState& Customer : ActiveCustomers)
	{
		if (Customer.bActive && Customer.SeatIndex == SeatIndex)
		{
			OutCustomer = Customer;
			return true;
		}
	}
	return false;
}

float ASCustomerDirector::GetSeatCooldownRemaining(const int32 SeatIndex) const
{
	return SeatCooldowns.IsValidIndex(SeatIndex) ? SeatCooldowns[SeatIndex] : 0.0f;
}

#pragma region K2 moonyfli
int32 ASCustomerDirector::ForceNextCustomersNow()
{
#if UE_BUILD_SHIPPING
	return 0;
#else
	if (!bDayServiceActive)
	{
		SetFeedback(TEXT("修改器：店未开，无法提前到客。"));
		return 0;
	}

	bOrderQueueExhausted = false;
	int32 Spawned = 0;
	const int32 SeatCount = GetConfiguredSeatCount();
	for (int32 SeatIndex = 0; SeatIndex < SeatCount; ++SeatIndex)
	{
		if (IsSeatOccupied(SeatIndex))
		{
			continue;
		}
		if (SeatCooldowns.IsValidIndex(SeatIndex))
		{
			SeatCooldowns[SeatIndex] = 0.0f;
		}
		if (TryFillSeat(SeatIndex))
		{
			++Spawned;
		}
	}
	SetFeedback(FString::Printf(TEXT("修改器：提前到客 %d 位。"), Spawned));
	return Spawned;
#endif
}
#pragma endregion K2 moonyfli

float ASCustomerDirector::GetSpawnCooldownRemaining() const
{
	float Minimum = TNumericLimits<float>::Max();
	for (int32 SeatIndex = 0; SeatIndex < SeatCooldowns.Num(); ++SeatIndex)
	{
		if (!IsSeatOccupied(SeatIndex))
		{
			Minimum = FMath::Min(Minimum, SeatCooldowns[SeatIndex]);
		}
	}
	return Minimum == TNumericLimits<float>::Max() ? 0.0f : Minimum;
}
#pragma endregion K2 moonyfli

void ASCustomerDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return;
	}

#pragma region K2 moonyfli
	// 这是白天唯一常驻的 Tick，开店倒计时挂在这里；闭店后 Phase 立刻变，下面自然停。
	GameInstance->TickDayClock(DeltaSeconds);
#pragma endregion K2 moonyfli

	if (!GameInstance->IsShopOpen() || !bDayServiceActive)
	{
		return;
	}

	for (int32 SeatIndex = 0; SeatIndex < SeatCooldowns.Num(); ++SeatIndex)
	{
		if (IsSeatOccupied(SeatIndex))
		{
			continue;
		}

		SeatCooldowns[SeatIndex] = FMath::Max(0.0f, SeatCooldowns[SeatIndex] - DeltaSeconds);
		if (SeatCooldowns[SeatIndex] <= 0.0f && !bOrderQueueExhausted)
		{
			TryFillSeat(SeatIndex);
		}
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

	if (!GameInstance->IsShopOpen())
	{
		SetFeedback(TEXT("尚未开店，无法交付。"));
		return false;
	}

	if (ActiveCustomers.IsEmpty())
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

	// Legacy HUD/debug delivery chooses the first waiting guest who ordered this dish.
	const FSCustomerState* Target = ActiveCustomers.FindByPredicate(
		[&Piece](const FSCustomerState& Customer) { return Customer.Order.RecipeId == Piece.RecipeId; });
	if (!Target)
	{
		SetFeedback(FString::Printf(
			TEXT("当前普通顾客都不需要 %s，已回弹，未扣棋子/营业额。"),
			*Piece.RecipeId.ToString()));
		Board->ClearActiveDrag();
		return false;
	}
	return TryDeliverFromCellToCustomer(CellIndex, Target->CustomerId);
}

bool ASCustomerDirector::TryDeliverFromCellToCustomer(const int32 CellIndex, const FString& CustomerId)
{
	USChefGameInstance* GameInstance = GetChefGameInstance();
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	if (!GameInstance || !Board)
	{
		return false;
	}

	if (!GameInstance->IsShopOpen())
	{
		SetFeedback(TEXT("尚未开店，无法交付。"));
		return false;
	}

	const FSCustomerState* Target = ActiveCustomers.FindByPredicate(
		[&CustomerId](const FSCustomerState& Customer) { return Customer.CustomerId == CustomerId; });
	if (!Target)
	{
		SetFeedback(TEXT("该座位当前没有等候顾客。"));
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

	if (Piece.RecipeId != Target->Order.RecipeId)
	{
		SetFeedback(FString::Printf(
			TEXT("%s 需要 %s，当前是 %s，已回弹，未扣棋子/营业额。"),
			*Target->DisplayName,
			*Target->Order.RecipeId.ToString(),
			*Piece.RecipeId.ToString()));
		Board->ClearActiveDrag();
		return false;
	}

	const int32 SellValue = Target->Order.SellValue > 0
		? Target->Order.SellValue
		: GameInstance->GetRecipeSellValue(Piece.RecipeId);
	const int32 RevenueBefore = GameInstance->Revenue;
	const FString ServedId = Target->CustomerId;
	const FString ServedName = Target->DisplayName;
	const int32 ServedSeatIndex = Target->SeatIndex;

	if (!Board->RemovePieceAt(CellIndex))
	{
		SetFeedback(TEXT("移除棋子失败，交付中止。"));
		return false;
	}

	GameInstance->AddRevenue(SellValue);
	ClearCustomer(ServedId, FString::Printf(
		TEXT("交付成功：%s（%s）从座位 %d 离店，营业额 %d→%d。该座位约 %.0fs 后补客。%s"),
		*ServedName,
		*ServedId,
		ServedSeatIndex + 1,
		RevenueBefore,
		GameInstance->Revenue,
		SpawnIntervalSeconds,
		*GameInstance->GetPlannedOrderSummary()));
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
	BuildNpcsFromPlan();
	const int32 Waiting = Npcs.Num();
	SetFeedback(FString::Printf(
		TEXT("特殊 NPC 名册已就绪（%d 人），按订单队列前半段陆续到店。"),
		Waiting));
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
	if (const USChefGameInstance* GameInstance = GetChefGameInstance())
	{
		Order.SellValue = GameInstance->GetRecipeSellValue(Order.RecipeId);
	}
	else
	{
		Order.SellValue = USChefGameInstance::GetBuiltInRecipeSellValue(Level);
	}
	return Order;
}

void ASSpecialNpcDirector::BuildNpcsFromPlan()
{
	Npcs.Reset();
	USChefGameInstance* GameInstance = GetChefGameInstance();
	if (!GameInstance)
	{
		return;
	}

	for (const FSPlannedOrder& Slot : GameInstance->GetPlannedDayOrders())
	{
		if (Slot.Kind != ESOrderSlotKind::Npc || Slot.NpcId.IsNone())
		{
			continue;
		}

		FSSpecialNpcState Npc;
		Npc.NpcId = Slot.NpcId;
		Npc.DisplayName = DefaultNpcDisplayName(GameInstance, Slot.NpcId);
		Npc.Order = Slot.Order.SellValue > 0 ? Slot.Order : MakeOrder(Slot.Order.IngredientId, Slot.Order.Level);
		Npc.GiftId = DefaultNpcGift(GameInstance, Slot.NpcId);
		Npc.bPresent = false;
		Npc.bServed = false;
		Npcs.Add(Npc);
	}
}

bool ASSpecialNpcDirector::RevealNpc(const FName NpcId, const int32 SeatIndex)
{
	for (FSSpecialNpcState& Npc : Npcs)
	{
		if (Npc.NpcId == NpcId)
		{
			if (Npc.bServed) //add by K2 已服务的 NPC 已经离店，不再重新入座。
			{
				return false;
			}
			if (!Npc.bPresent)
			{
				Npc.bPresent = true;
				Npc.SeatIndex = SeatIndex;
				SetFeedback(FString::Printf(
					TEXT("%s 到店并坐入座位 %d，订单 %s（售价 %d）。"),
					*Npc.DisplayName,
					SeatIndex + 1,
					*Npc.Order.RecipeId.ToString(),
					Npc.Order.SellValue));
			}
			return true;
		}
	}
	return false;
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

	if (!GameInstance->IsShopOpen() || !bDayServiceActive)
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
		: GameInstance->GetRecipeSellValue(Piece.RecipeId);
	const int32 RevenueBefore = GameInstance->Revenue;
	const int32 ServedSeatIndex = Target->SeatIndex;

	if (!Board->RemovePieceAt(CellIndex))
	{
		SetFeedback(TEXT("移除棋子失败，NPC 交付中止。"));
		return false;
	}

	Target->bServed = true;
	Target->bPresent = false; //add by K2 订单完成即离店，座位立刻空出。
	Target->SeatIndex = INDEX_NONE;
	GameInstance->GrantGift(Target->GiftId); //add by K2
	GameInstance->AddRevenue(SellValue);
	if (ASCustomerDirector* CustomerDirector = ASCustomerDirector::FindDirector(this))
	{
		CustomerDirector->NotifySeatVacated(ServedSeatIndex);
	}

	SetFeedback(FString::Printf(
		TEXT("服务成功：%s 从座位 %d 离店，营业额 %d→%d，谢礼 %s 立即生效；该座位独立补客。"),
		*Target->DisplayName,
		ServedSeatIndex + 1,
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

	// The 3D day whitebox is opt-in per level: only a level that places a
	// BP_SDayBoardPresenter switches to it, so the legacy debug sandbox stays untouched.
	DayBoardPresenter = nullptr;
	for (TActorIterator<ASDayBoardPresenter> It(GetWorld()); It; ++It)
	{
		DayBoardPresenter = *It;
		break;
	}

	if (DayBoardPresenter)
	{
		NightCleanupPassesRemaining = 12;
		CleanupNightPresentation();
		GetWorldTimerManager().SetTimer(
			NightCleanupTimerHandle,
			this,
			&ASFakeNightGateway::CleanupNightPresentation,
			0.25f,
			true);
	}

	// GameInstance::Init may restore an open shop before actors exist; re-open it now.
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		if (GameInstance->IsShopOpen())
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
#pragma region K2 moonyfli
		if (DayBoardPresenter)
		{
			// The whitebox level may still inherit the Night test GameMode. Remove its
			// HUD/UMG before installing the Day presentation so the two never overlap.
			if (AHUD* ExistingHud = PlayerController->GetHUD())
			{
				ExistingHud->bShowHUD = false;
			}
			TArray<UUserWidget*> ExistingWidgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
				this,
				ExistingWidgets,
				UUserWidget::StaticClass(),
				false);
			for (UUserWidget* ExistingWidget : ExistingWidgets)
			{
				if (ExistingWidget)
				{
					ExistingWidget->RemoveFromParent();
				}
			}

			UClass* DayHUDClass = LoadClass<USDayHUD>(
				nullptr,
				TEXT("/Game/Game/Day/UI/WBP_SDayHUD.WBP_SDayHUD_C"));
			if (!DayHUDClass)
			{
				DayHUDClass = USDayHUD::StaticClass();
			}
			DayHUD = CreateWidget<USDayHUD>(PlayerController, DayHUDClass);
			if (DayHUD)
			{
				DayHUD->AddToViewport(80);
				UE_LOG(LogSSandbox, Display, TEXT("白天正式 HUD 已加入视口：%s"), *DayHUDClass->GetName());
			}
		}
#pragma endregion K2 moonyfli

		if (bShowDebugPanel || !DayBoardPresenter)
		{
			UClass* PanelClass = LoadClass<USDebugPanel>(
				nullptr,
				TEXT("/Game/Game/Day/UI/WBP_SDebugPanel.WBP_SDebugPanel_C"));
			if (!PanelClass)
			{
				PanelClass = USDebugPanel::StaticClass();
			}
			DebugPanel = CreateWidget<USDebugPanel>(PlayerController, PanelClass);
			if (DebugPanel)
			{
				DebugPanel->AddToViewport(100);
				UE_LOG(LogSSandbox, Display, TEXT("调试面板已加入视口：%s"), *PanelClass->GetName());
			}
		}

		PlayerController->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}

#pragma region K2 moonyfli
	if (FParse::Param(FCommandLine::Get(), TEXT("SDaySmokeTest")))
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ASFakeNightGateway::RunDayWhiteboxSmokeTest);
	}
#pragma endregion K2 moonyfli
}

#pragma region K2 moonyfli
void ASFakeNightGateway::CleanupNightPresentation()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this)
		{
			continue;
		}
		const FString ClassName = Actor->GetClass()->GetName();
		if (!ClassName.Contains(TEXT("NightCourse")))
		{
			continue;
		}

		if (ClassName.Contains(TEXT("Host")) || ClassName.Contains(TEXT("Stone")))
		{
			Actor->Destroy();
			continue;
		}

		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->SetActorTickEnabled(false);
		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (Component)
			{
				Component->SetComponentTickEnabled(false);
			}
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetVisibility(false, true);
			}
		}
	}

	--NightCleanupPassesRemaining;
	if (NightCleanupPassesRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(NightCleanupTimerHandle);
	}
}

void ASFakeNightGateway::RunDayWhiteboxSmokeTest()
{
	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	ASMergeBoard* Board = ASMergeBoard::FindBoard(this);
	ASCustomerDirector* CustomerDirector = ASCustomerDirector::FindDirector(this);
	ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this);
	bool bPassed = true;
	auto Check = [&bPassed](const bool bCondition, const TCHAR* Label)
	{
		UE_LOG(
			LogSSandbox,
			Display,
			TEXT("[SDaySmoke] %s: %s"),
			Label,
			bCondition ? TEXT("PASS") : TEXT("FAIL"));
		bPassed &= bCondition;
	};
	auto SubmitDay = [GameInstance](const FString& Id)
	{
		FSNightResult Result;
		Result.ResultId = Id;
		Result.bSuccess = true;
		Result.Ingredients =
		{
			{LingGuId, 12},
			{YinShanJunId, 12},
			{ChiYanJiaoId, 12},
			{YueLinYuId, 12},
			{XuanYuQinId, 12}
		};
		return GameInstance->ConsumeNightResult(Result);
	};
	auto FindPiece = [Board](const FName IngredientId, const int32 Level) -> int32
	{
		for (int32 Index = 0; Index < Board->GetCells().Num(); ++Index)
		{
			FSDishPiece Piece;
			if (Board->TryGetPiece(Index, Piece)
				&& Piece.IngredientId == IngredientId
				&& Piece.Level == Level)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	};

	Check(GameInstance && Board && CustomerDirector && NpcDirector, TEXT("core actors"));
	Check(DayBoardPresenter && DayBoardPresenter->GetLogicBoard() == Board, TEXT("presenter bound to logic"));
	Check(DayHUD && DayHUD->IsInViewport(), TEXT("formal HUD visible"));
	Check(DayBoardPresenter && DayBoardPresenter->Camera
		&& DayBoardPresenter->Camera->ProjectionMode == ECameraProjectionMode::Orthographic,
		TEXT("portrait orthographic camera"));

	if (GameInstance && Board && CustomerDirector && NpcDirector)
	{
		GameInstance->ResetSandbox();
		Check(SubmitDay(TEXT("SDAY-SMOKE-DAY-1")), TEXT("success result opens day"));

		const TArray<FSPlannedOrder> DayPlan = GameInstance->GetPlannedDayOrders();
		const int32 PlanHalf = FMath::DivideAndRoundUp(DayPlan.Num(), 2);
		bool bNpcInFirstHalf = true;
		TSet<int32> PlanLevels;
		for (int32 Index = 0; Index < DayPlan.Num(); ++Index)
		{
			PlanLevels.Add(DayPlan[Index].Order.Level);
			if (DayPlan[Index].Kind == ESOrderSlotKind::Npc && Index >= PlanHalf)
			{
				bNpcInFirstHalf = false;
			}
		}
		Check(
			DayPlan.Num() > 0
			&& GameInstance->GetPlannedOrderTotalValue() >= GameInstance->RevenueTarget,
			TEXT("planned orders cover revenue target"));
		Check(bNpcInFirstHalf, TEXT("NPC slots sit in first half"));
		Check(
			DayPlan.Num() < 3 || PlanLevels.Num() >= 2,
			TEXT("planned order levels are mixed"));
		Check(
			GameInstance->GetRecipeSellValue(USChefGameInstance::MakeRecipeId(LingGuId, 0))
				== USChefGameInstance::GetBuiltInRecipeSellValue(0)
			|| GameInstance->GetRecipeSellValue(USChefGameInstance::MakeRecipeId(LingGuId, 0)) > 0,
			TEXT("recipe sell value resolves"));

		auto ProduceDish = [&](const FName IngredientId, const int32 TargetLevel) -> int32
		{
			Board->ReclaimPiecesToInventory();
			TFunction<int32(int32)> Build = [&](const int32 Level) -> int32
			{
				if (Level <= 0)
				{
					TSet<int32> Before;
					for (int32 Index = 0; Index < Board->GetCells().Num(); ++Index)
					{
						FSDishPiece Piece;
						if (Board->TryGetPiece(Index, Piece)
							&& Piece.IngredientId == IngredientId
							&& Piece.Level == 0)
						{
							Before.Add(Index);
						}
					}
					if (!Board->TrySpawnFromMotherPiece(IngredientId))
					{
						return INDEX_NONE;
					}
					for (int32 Index = 0; Index < Board->GetCells().Num(); ++Index)
					{
						FSDishPiece Piece;
						if (Board->TryGetPiece(Index, Piece)
							&& Piece.IngredientId == IngredientId
							&& Piece.Level == 0
							&& !Before.Contains(Index))
						{
							return Index;
						}
					}
					return INDEX_NONE;
				}
				const int32 Left = Build(Level - 1);
				const int32 Right = Build(Level - 1);
				if (Left == INDEX_NONE || Right == INDEX_NONE || !Board->TryDropPiece(Left, Right))
				{
					return INDEX_NONE;
				}
				// TryDropPiece keeps the merged dish on the destination cell.
				return Right;
			};
			return Build(TargetLevel);
		};

		const int32 BeforeMerge = GameInstance->GetQuantity(LingGuId);
		Check(Board->TrySpawnFromMotherPiece(LingGuId), TEXT("spawn first mother piece"));
		const int32 FirstLingGu = FindPiece(LingGuId, 0);
		Check(Board->TrySpawnFromMotherPiece(LingGuId), TEXT("spawn second mother piece"));
		int32 SecondLingGu = INDEX_NONE;
		for (int32 Index = 0; Index < Board->GetCells().Num(); ++Index)
		{
			if (Index == FirstLingGu)
			{
				continue;
			}
			FSDishPiece Piece;
			if (Board->TryGetPiece(Index, Piece) && Piece.IngredientId == LingGuId && Piece.Level == 0)
			{
				SecondLingGu = Index;
				break;
			}
		}
		Check(
			FirstLingGu != INDEX_NONE
			&& SecondLingGu != INDEX_NONE
			&& Board->TryDropPiece(FirstLingGu, SecondLingGu)
			&& Board->GetHighestLevel(LingGuId) == 1
			&& GameInstance->GetQuantity(LingGuId) == BeforeMerge - 2,
			TEXT("same-chain merge"));

		// 高级食材拖到任意基础食材篮区域都会撤销合成，按基础单位完整退库。
		const int32 MergedLingGu = FindPiece(LingGuId, 1);
		ASDayIngredientBinVisual* LingGuBin = DayBoardPresenter
			? DayBoardPresenter->GetIngredientBin(LingGuId)
			: nullptr;
		FVector2D BinScreen = FVector2D::ZeroVector;
		APlayerController* SmokePlayerController = UGameplayStatics::GetPlayerController(this, 0);
		const bool bBinProjected = LingGuBin
			&& SmokePlayerController
			&& UGameplayStatics::ProjectWorldToScreen(
				SmokePlayerController,
				LingGuBin->GetActorLocation(),
				BinScreen);
		Check(
			MergedLingGu != INDEX_NONE
			&& bBinProjected
			&& Board->BeginPieceDrag(MergedLingGu, 0),
			TEXT("select advanced dish for decomposition"));
		if (bBinProjected)
		{
			DayBoardPresenter->SimulatePointerEvent(BinScreen, true);
			DayBoardPresenter->SimulatePointerEvent(BinScreen, false);
		}
		Check(
			GameInstance->GetQuantity(LingGuId) == BeforeMerge
			&& Board->CountPiecesAtLevel(LingGuId, 1) == 0,
			TEXT("ingredient area decomposes advanced dish to inventory"));

		const bool bCustomerReady = CustomerDirector->HasActiveCustomer()
			|| CustomerDirector->SpawnNextPlannedCustomer();
		Check(CustomerDirector->HasActiveCustomer(), TEXT("customer waits indefinitely"));
		const FSCustomerState GuestCustomer = CustomerDirector->GetActiveCustomer();
		const FSOrderRequest GuestOrder = GuestCustomer.Order;
		const int32 CustomerCell = ProduceDish(GuestOrder.IngredientId, GuestOrder.Level);
		Check(CustomerCell != INDEX_NONE, TEXT("spawn customer dish"));
		if (DayBoardPresenter)
		{
			DayBoardPresenter->RefreshFromLogic();
		}
		Check(
			DayBoardPresenter
			&& DayBoardPresenter->GetDeliverySeatCount() == GameInstance->GetServiceSeatCount()
			&& GameInstance->GetServiceSeatCount() == GameInstance->CustomerConcurrentMax
			&& GameInstance->CustomerConcurrentMax == 2,
			TEXT("T0 seats match CustomerConcurrentMax"));
		const ASDayCharacterStandIn* CustomerSeat = DayBoardPresenter
			? DayBoardPresenter->GetSeat(NAME_None)
			: nullptr;
		Check(
			bCustomerReady
			&& CustomerSeat
			&& !GuestCustomer.DisplayName.IsEmpty()
			&& CustomerSeat->Label->Text.ToString().Contains(
				GuestCustomer.DisplayName),
			TEXT("customer seat shows guest name"));
		Check(
			bCustomerReady
			&& CustomerDirector->TryDeliverFromCellToCustomer(CustomerCell, GuestCustomer.CustomerId),
			TEXT("customer delivery"));
		if (DayBoardPresenter)
		{
			DayBoardPresenter->RefreshFromLogic();
		}
		Check(
			CustomerDirector->GetSeatCooldownRemaining(GuestCustomer.SeatIndex) > 0.0f
			&& CustomerDirector->HasActiveCustomer()
			&& CustomerSeat
			&& !CustomerSeat->bOccupied,
			TEXT("served guest frees only its own seat"));
		CustomerDirector->Tick(3.0f);
		FSCustomerState EarlyReplacement;
		Check(
			!CustomerDirector->TryGetCustomerAtSeat(GuestCustomer.SeatIndex, EarlyReplacement),
			TEXT("vacant seat respects its independent cooldown"));
		CustomerDirector->Tick(5.0f);
		FSCustomerState Replacement;
		Check(
			CustomerDirector->TryGetCustomerAtSeat(GuestCustomer.SeatIndex, Replacement)
			&& Replacement.CustomerId != GuestCustomer.CustomerId,
			TEXT("vacant seat automatically receives next customer"));

		FSSpecialNpcState ALingBefore;
		Check(
			NpcDirector->TryGetNpc(NpcALingId, ALingBefore)
			&& ALingBefore.bPresent
			&& !ALingBefore.bServed,
			TEXT("ALing revealed from planned queue"));
		const int32 ALingCell = ProduceDish(ALingBefore.Order.IngredientId, ALingBefore.Order.Level);
		Check(ALingCell != INDEX_NONE, TEXT("spawn ALing dish"));

		// Pointer path: click the cell to select, then click the seat circle to deliver.
		auto ProjectActor = [this](const AActor* Actor, FVector2D& OutScreen)
		{
			APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
			return Actor
				&& PlayerController
				&& UGameplayStatics::ProjectWorldToScreen(PlayerController, Actor->GetActorLocation(), OutScreen);
		};
		ASDayCellVisual* ALingCellVisual = DayBoardPresenter
			? DayBoardPresenter->GetCellVisual(ALingCell)
			: nullptr;
		ASDayCharacterStandIn* ALingSeat = DayBoardPresenter
			? DayBoardPresenter->GetSeat(NpcALingId)
			: nullptr;
		FVector2D CellScreen = FVector2D::ZeroVector;
		FVector2D SeatScreen = FVector2D::ZeroVector;
		const bool bProjected =
			ProjectActor(ALingCellVisual, CellScreen) && ProjectActor(ALingSeat, SeatScreen);
		Check(bProjected, TEXT("project board and seat to screen"));
		Check(
			ALingSeat
			&& ALingSeat->bOccupied
			&& ALingSeat->Label->Text.ToString().Contains(TEXT("阿翎")),
			TEXT("seat shows NPC name and order"));
		if (bProjected)
		{
			DayBoardPresenter->SimulatePointerEvent(CellScreen, true);
			Check(
				Board->IsDragging() && Board->GetActiveDragCellIndex() == ALingCell,
				TEXT("pointer press selects piece"));
			Check(
				ALingCellVisual->PieceMesh->GetRelativeLocation().Z > 50.0f,
				TEXT("selected piece is highlighted"));
			DayBoardPresenter->SimulatePointerEvent(CellScreen, false);
			DayBoardPresenter->SimulatePointerEvent(SeatScreen, true);
		}
		FSSpecialNpcState ALingState;
		Check(
			bProjected
			&& NpcDirector->TryGetNpc(NpcALingId, ALingState)
			&& ALingState.bServed,
			TEXT("ALing NPC delivery via seat circle"));
		// 订单完成后特殊顾客也会离店，座位立刻空出给下一位。
		if (DayBoardPresenter)
		{
			DayBoardPresenter->RefreshFromLogic();
		}
		Check(
			!ALingState.bPresent
			&& ALingSeat
			&& !ALingSeat->bOccupied
			&& ALingSeat->Label->Text.ToString().Contains(TEXT("空座"))
			&& DayBoardPresenter->GetSeat(NpcALingId) == nullptr,
			TEXT("served NPC leaves and frees the seat"));
		CustomerDirector->Tick(3.0f);
		FSCustomerState EarlyNpcSeatReplacement;
		Check(
			!CustomerDirector->TryGetCustomerAtSeat(ALingBefore.SeatIndex, EarlyNpcSeatReplacement),
			TEXT("served NPC seat keeps its own cooldown"));
		CustomerDirector->Tick(5.0f);
		FSCustomerState NpcSeatReplacement;
		Check(
			CustomerDirector->TryGetCustomerAtSeat(ALingBefore.SeatIndex, NpcSeatReplacement),
			TEXT("served NPC seat independently receives next customer"));

		FSSpecialNpcState SangPoBefore;
		Check(
			NpcDirector->TryGetNpc(NpcSangPoId, SangPoBefore)
			&& SangPoBefore.bPresent
			&& !SangPoBefore.bServed,
			TEXT("SangPo revealed from planned queue"));
		const int32 SangPoCell = ProduceDish(SangPoBefore.Order.IngredientId, SangPoBefore.Order.Level);
		Check(
			SangPoCell != INDEX_NONE
			&& Board->BeginPieceDrag(SangPoCell, 8)
			&& NpcDirector->TryDeliverToNpc(NpcSangPoId),
			TEXT("SangPo NPC delivery"));

		const int32 BeforeReclaim = GameInstance->GetQuantity(ChiYanJiaoId);
		Check(Board->TrySpawnFromMotherPiece(ChiYanJiaoId), TEXT("spawn reclaim dish"));
		Check(
			Board->ReclaimPiecesToInventory() >= 1
			&& GameInstance->GetQuantity(ChiYanJiaoId) == BeforeReclaim,
			TEXT("close/reclaim returns paid inventory"));

		// 谢礼即得即用：交付 NPC 当场生效，无勾选、无确认。
		Check(
			GameInstance->ActiveGiftIds.Contains(GiftGuideKiteId)
			&& GameInstance->GetGiftBuffState().bGuideKite
			&& GameInstance->GetPendingNightBootstrap().GiftBuffState.bGuideKite,
			TEXT("gift applies immediately on delivery"));
		Check(
			GameInstance->GetGiftTabSummary().Contains(TEXT("引路纸鸢")),
			TEXT("gift tab lists the earned card"));

		// 时间到自动闭店：未达标回档日初，达标才日结进下一关。
		GameInstance->ResetSandbox();
		Check(SubmitDay(TEXT("SDAY-SMOKE-DAY-TIMER")), TEXT("open day for timer"));
		Check(
			GameInstance->Phase == ESGamePhase::DayRunning
			&& GameInstance->GetDayTimeRemaining() > 0.0f,
			TEXT("day opens with a countdown"));
		const int32 GapStock = GameInstance->GetQuantity(LingGuId);
		Board->TrySpawnFromMotherPiece(LingGuId);
		GameInstance->TickDayClock(GameInstance->GetDayTimeRemaining() + 1.0f);
		Check(
			GameInstance->Phase == ESGamePhase::DayRunning
			&& GameInstance->Revenue == 0
			&& GameInstance->GetQuantity(LingGuId) == GapStock
			&& Board->GetOccupiedCellCount() == 0,
			TEXT("time up under target rolls back to day start"));

		GameInstance->AddRevenue(GameInstance->RevenueTarget);
		Check(GameInstance->Phase == ESGamePhase::DayQualified, TEXT("reaching target qualifies the day"));
		const FName StageBeforeSettle = GameInstance->StageId;
		GameInstance->TickDayClock(GameInstance->GetDayTimeRemaining() + 1.0f);
		Check(
			GameInstance->Phase == ESGamePhase::PrepareNight
			&& GameInstance->StageId != StageBeforeSettle
			&& GameInstance->CarryOverTargetBonus > 0
			&& GameInstance->RevenueTarget > GameInstance->GetActiveStageRow().RevenueTarget,
			TEXT("qualified time up settles and raises next target"));

		// 夜败回档夜初：本次收获全部清除。
		GameInstance->ResetSandbox();
		const int32 BeforeFailure = GameInstance->GetQuantity(LingGuId);
		Check(GameInstance->StartNight(), TEXT("start night snapshot"));
		FSNightResult Failure;
		Failure.ResultId = TEXT("SDAY-SMOKE-FAILURE");
		Failure.bSuccess = false;
		Failure.bFailedMidway = true;
		Failure.Ingredients = {{LingGuId, 9}};
		Check(
			GameInstance->ConsumeNightResult(Failure)
			&& GameInstance->GetQuantity(LingGuId) == BeforeFailure
			&& GameInstance->Phase == ESGamePhase::PrepareNight,
			TEXT("failed night rolls back to night start"));

		GameInstance->ResetSandbox();
		const int32 SavedQuantity = GameInstance->GetQuantity(YueLinYuId);
		Check(GameInstance->SaveChefProfile(), TEXT("save profile"));
		GameInstance->AddIngredient(YueLinYuId, 3);
		Check(
			GameInstance->LoadChefProfile()
			&& GameInstance->GetQuantity(YueLinYuId) == SavedQuantity,
			TEXT("load profile restores inventory"));

		// 沙盒调试路径：开店中「五类各 +10」后存档，读档不该被日初回档吃掉。
		GameInstance->ResetSandbox();
		SubmitDay(TEXT("SDAY-SMOKE-GRANT"));
		const int32 BeforeGrant = GameInstance->GetQuantity(XuanYuQinId);
		GameInstance->GrantPermanentStock(XuanYuQinId, 10);
		Check(
			GameInstance->Phase == ESGamePhase::DayRunning
			&& GameInstance->GetQuantity(XuanYuQinId) == BeforeGrant + 10
			&& GameInstance->SaveChefProfile(),
			TEXT("grant stock mid-day then save"));
		Check(
			GameInstance->LoadChefProfile()
			&& GameInstance->GetQuantity(XuanYuQinId) == BeforeGrant + 10,
			TEXT("mid-day load keeps granted stock"));
		GameInstance->DeleteChefProfile();
	}

	bDayWhiteboxSmokePassed = bPassed;
	const FString ScreenshotPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Saved"),
		TEXT("Automation"),
		TEXT("DayBoardWhitebox.png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	FTimerHandle ExitTimerHandle;
	GetWorldTimerManager().SetTimer(
		ExitTimerHandle,
		this,
		&ASFakeNightGateway::FinishDayWhiteboxSmokeTest,
		2.0f,
		false);
}

void ASFakeNightGateway::FinishDayWhiteboxSmokeTest()
{
	const FString ScreenshotPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Saved"),
		TEXT("Automation"),
		TEXT("DayBoardWhitebox.png"));
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		TArray<FColor> Pixels;
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (GetViewportScreenShot(GEngine->GameViewport->Viewport, Pixels))
		{
			TArray64<uint8> PngData;
			FImageUtils::PNGCompressImageArray(
				Size.X,
				Size.Y,
				TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()),
				PngData);
			const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *ScreenshotPath);
			UE_LOG(
				LogSSandbox,
				Display,
				TEXT("[SDaySmoke] screenshot=%s path=%s"),
				bSaved ? TEXT("PASS") : TEXT("FAIL"),
				*ScreenshotPath);
		}
	}
	UE_LOG(
		LogSSandbox,
		Display,
		TEXT("[SDaySmoke] RESULT=%s"),
		bDayWhiteboxSmokePassed ? TEXT("PASS") : TEXT("FAIL"));
	// Only the headless -SDaySmokeTest launch owns the process; a console run stays in PIE.
	if (FParse::Param(FCommandLine::Get(), TEXT("SDaySmokeTest")))
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}

static FAutoConsoleCommandWithWorld GSDayOpenDayCmd(
	TEXT("S.Day.OpenDay"),
	TEXT("Submit a success night result so the shop opens (manual testing)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (ASFakeNightGateway* Gateway = Cast<ASFakeNightGateway>(
			UGameplayStatics::GetActorOfClass(World, ASFakeNightGateway::StaticClass())))
		{
			Gateway->SubmitNewSuccessResult();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSDaySelectCellCmd(
	TEXT("S.Day.Select"),
	TEXT("S.Day.Select <cellIndex> selects a board piece as if clicked"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1)
		{
			return;
		}
		if (ASMergeBoard* Board = ASMergeBoard::FindBoard(World))
		{
			Board->BeginPieceDrag(FCString::Atoi(*Args[0]), 0);
		}
	}));

static FAutoConsoleCommandWithWorld GSDayRunSmokeCmd(
	TEXT("S.Day.RunSmoke"),
	TEXT("Run the day whitebox smoke test in the current game/PIE world"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogSSandbox, Warning, TEXT("[SDaySmoke] no world for RunSmoke"));
			return;
		}
		AActor* GatewayActor = UGameplayStatics::GetActorOfClass(World, ASFakeNightGateway::StaticClass());
		if (!GatewayActor)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if ((*It)->GetClass()->GetName().Contains(TEXT("FakeNightGateway")))
				{
					GatewayActor = *It;
					break;
				}
			}
		}
		if (ASFakeNightGateway* Gateway = Cast<ASFakeNightGateway>(GatewayActor))
		{
			Gateway->RunDayWhiteboxSmokeTest();
			return;
		}
		if (GatewayActor)
		{
			if (UFunction* Func = GatewayActor->FindFunction(TEXT("RunDayWhiteboxSmokeTest")))
			{
				GatewayActor->ProcessEvent(Func, nullptr);
				return;
			}
		}
		UE_LOG(
			LogSSandbox,
			Warning,
			TEXT("[SDaySmoke] no gateway in world %s (pie=%d)"),
			*World->GetName(),
			World->IsPlayInEditor() ? 1 : 0);
	}));
#pragma endregion K2 moonyfli

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
			GameInstance->GrantPermanentStock(Id, 10); //add by K2
		}
		GameInstance->LastBoardFeedback = TEXT("五类食材各 +10（永久入库，存档可保留）。");
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
			GameInstance->OpenShopForDebug(); //add by K2
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

void ASFakeNightGateway::DebugFailDay()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->FailDayForDebug();
	}
}

#pragma region K2 moonyfli
void ASFakeNightGateway::AdvanceFlow()
{
	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	switch (GameInstance->Phase)
	{
	case ESGamePhase::Boot:
	case ESGamePhase::PrepareNight:
		GameInstance->StartNight();
		break;
	case ESGamePhase::NightRunning:
		// 白模没有可玩夜晚，这里替 R2 的夜关卡回一个成功结果。
		SubmitNewSuccessResult();
		break;
	case ESGamePhase::DayRunning:
	case ESGamePhase::DayQualified:
		GameInstance->CloseShopNow(ESDayEndReason::TimeUp);
		break;
	default:
		GameInstance->LastBoardFeedback = FString::Printf(
			TEXT("阶段 %s 无需手动推进。"),
			*GameInstance->GetPhaseDisplayName());
		GameInstance->OnSandboxStateChanged.Broadcast();
		break;
	}
}
#pragma endregion K2 moonyfli

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
#pragma region K2 moonyfli
	// 开店倒计时只在 Tick 里走，不发状态广播，这里轮询刷新，否则读数要点一下才更新。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &USDebugPanel::Refresh, 0.2f, true);
	}
#pragma endregion K2 moonyfli
	Refresh();
}

void USDebugPanel::NativeDestruct()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &USDebugPanel::Refresh);
	}
	if (UWorld* World = GetWorld()) //add by K2
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
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
	GiftGuideKiteButton = AddButton(GiftRow, TEXT("GiftGuideKiteButton"), TEXT("发·纸鸢"));
	GiftGuideKiteButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleGrantGiftGuideKite);
	GiftLifeLampButton = AddButton(GiftRow, TEXT("GiftLifeLampButton"), TEXT("发·纸灯"));
	GiftLifeLampButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleGrantGiftLifeLamp);
	ConfirmGiftsButton = AddButton(GiftRow, TEXT("ConfirmGiftsButton"), TEXT("推进流程"));
	ConfirmGiftsButton->OnClicked.AddDynamic(this, &USDebugPanel::HandleAdvanceFlow);

	UWrapBox* StageRow = AddButtonRow(TEXT("StageRow"));
	AddButton(StageRow, TEXT("JumpT0Button"), TEXT("跳T0"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpT0);
	AddButton(StageRow, TEXT("JumpL1Button"), TEXT("跳L1"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL1);
	AddButton(StageRow, TEXT("JumpL2Button"), TEXT("跳L2"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL2);
	AddButton(StageRow, TEXT("JumpL3Button"), TEXT("跳L3"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleJumpL3);
	AddButton(StageRow, TEXT("PrintBootstrapButton"), TEXT("打印Bootstrap"))->OnClicked.AddDynamic(this, &USDebugPanel::HandlePrintBootstrap);
	AddButton(StageRow, TEXT("FailDayButton"), TEXT("白天判败回档"))->OnClicked.AddDynamic(this, &USDebugPanel::HandleFailDay);

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
		SetFeedback(TEXT("已提交失败结果：50% 所得已入库，阶段保持 PrepareNight。"));
	}
}

void USDebugPanel::HandleRepeatClicked()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->RepeatLastResult();
		SetFeedback(TEXT("已重复提交当前 ResultId；库存应保持不变。"));
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
		SetFeedback(TEXT("五类食材各 +10（永久入库，存档可保留）。"));
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

void USDebugPanel::GrantGift(const FName GiftId)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->GrantGift(GiftId);
		SetFeedback(GameInstance->LastBoardFeedback);
		Refresh();
	}
}

void USDebugPanel::HandleGrantGiftGuideKite()
{
	GrantGift(GiftGuideKiteId);
}

void USDebugPanel::HandleGrantGiftLifeLamp()
{
	GrantGift(GiftLifeLampId);
}

void USDebugPanel::HandleAdvanceFlow()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->AdvanceFlow();
		if (const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
		{
			SetFeedback(GameInstance->LastBoardFeedback);
		}
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

void USDebugPanel::HandleFailDay()
{
	if (ASFakeNightGateway* Gateway = GetGateway())
	{
		Gateway->DebugFailDay();
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
	FString Detail = TEXT("开店后按预生成订单队列依次入座。"); //add by K2

	if (Director && Director->HasActiveCustomer())
	{
		const FSCustomerState Customer = Director->GetActiveCustomer();
#pragma region K2 moonyfli
		ButtonText = FString::Printf(TEXT("%s←%s"), *Customer.DisplayName, *Customer.Order.RecipeId.ToString());
		Detail = FString::Printf(
			TEXT("%s（%s）要 %s｜可一直等待｜售价 %d"),
			*Customer.DisplayName,
			*Customer.CustomerId,
			*Customer.Order.RecipeId.ToString(),
			Customer.Order.SellValue);
#pragma endregion K2 moonyfli
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
				Npc.bServed ? TEXT("[已离店]") : (Npc.bPresent ? TEXT("[在店]") : TEXT("[未到店]")), //add by K2
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

	const bool bHasKite = GameInstance && GameInstance->ActiveGiftIds.Contains(GiftGuideKiteId);
	const bool bHasLamp = GameInstance && GameInstance->ActiveGiftIds.Contains(GiftLifeLampId);

	SetGiftButtonText(GiftGuideKiteButton, bHasKite ? TEXT("纸鸢✓已生效") : TEXT("发·纸鸢"));
	SetGiftButtonText(GiftLifeLampButton, bHasLamp ? TEXT("纸灯✓已生效") : TEXT("发·纸灯"));

	FString FlowLabel = TEXT("推进流程");
	if (GameInstance)
	{
		switch (GameInstance->Phase)
		{
		case ESGamePhase::Boot:
		case ESGamePhase::PrepareNight: FlowLabel = TEXT("入夜"); break;
		case ESGamePhase::NightRunning: FlowLabel = TEXT("模拟夜成功"); break;
		case ESGamePhase::DayRunning: FlowLabel = TEXT("闭店(未达标回档)"); break;
		case ESGamePhase::DayQualified: FlowLabel = TEXT("闭店日结"); break;
		case ESGamePhase::Ending: FlowLabel = TEXT("尾声"); break;
		default: break;
		}
	}
	SetGiftButtonText(ConfirmGiftsButton, FlowLabel);
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

	const FString GiftTab = GameInstance->GetGiftTabSummary(); //add by K2

	const ASCustomerDirector* Director = GetDirector();
	FString CustomerLine = TEXT("顾客: 无");
	if (Director && Director->HasActiveCustomer())
	{
		const FSCustomerState Customer = Director->GetActiveCustomer();
		CustomerLine = FString::Printf(
			TEXT("顾客: %s(%s) 订单=%s 一直等待"),
			*Customer.DisplayName,
			*Customer.CustomerId,
			*Customer.Order.RecipeId.ToString());
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
			TEXT("%s ｜ %s(%s) ｜ 营业额 %d/%d 缺口%d ｜ 剩余 %.0fs ｜ Retry=%s ｜ %s ｜ NPC %d/%d"),
			*GameInstance->GetPhaseDisplayName(),
			*GameInstance->StageId.ToString(),
			*GameInstance->ActiveStageRow.DisplayName,
			GameInstance->Revenue,
			GameInstance->RevenueTarget,
			GameInstance->GetRevenueGap(),
			GameInstance->GetDayTimeRemaining(), //add by K2
			GameInstance->bAwaitingNightRetry ? TEXT("Y") : TEXT("N"),
			*CustomerLine,
			NpcServed,
			NpcTotal)));
	}

	StateText->SetText(FText::FromString(FString::Printf(
		TEXT("永久库存  灵谷:%d 阴山菌:%d 赤焰椒:%d 月鳞鱼:%d 玄羽禽:%d\n")
		TEXT("棋盘  启用:%d 占用:%d 空格:%d 拖拽格:%s 待退回:%d ｜ 最高等级 灵:%d 阴:%d 赤:%d 月:%d 玄:%d\n")
		TEXT("%s\n")
		TEXT("%s\n")
		TEXT("GiftBuff: %s ｜ CompletedDays: %s ｜ 结转目标 +%d ｜ 上次闭店: %s\n")
		TEXT("Stage  Seed:%d Fork:%s 昼:%.0fs 夜:%.0fs Next:%s%s\n")
		TEXT("Bootstrap: %s ｜ LastResult: %s\n")
		TEXT("Save: %s"),
		GameInstance->GetInventoryQuantity(LingGuId),
		GameInstance->GetInventoryQuantity(YinShanJunId),
		GameInstance->GetInventoryQuantity(ChiYanJiaoId),
		GameInstance->GetInventoryQuantity(YueLinYuId),
		GameInstance->GetInventoryQuantity(XuanYuQinId),
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
		*GiftTab,
		*GameInstance->GetPlannedOrderSummary(),
		*GameInstance->GiftBuffState.ToDebugString(),
		GameInstance->CompletedDayFlags.IsEmpty()
			? TEXT("None")
			: *FString::JoinBy(GameInstance->CompletedDayFlags, TEXT(","), [](const FName Id) { return Id.ToString(); }),
		GameInstance->CarryOverTargetBonus,
		GameInstance->LastDayEndReason == ESDayEndReason::TimeUp
			? TEXT("时间结束")
			: (GameInstance->LastDayEndReason == ESDayEndReason::OutOfIngredients ? TEXT("食材耗尽") : TEXT("无")),
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

#pragma region K2 moonyfli
void ASDayWhiteboxGameMode::BeginPlay()
{
	// Create the 3D presentation before Super spawns the gateway, so the gateway
	// detects it and installs the day HUD instead of the legacy debug panel.
	bool bHasPresenter = false;
	for (TActorIterator<ASDayBoardPresenter> It(GetWorld()); It; ++It)
	{
		bHasPresenter = true;
		break;
	}

	if (!bHasPresenter)
	{
		UClass* PresenterClass = LoadClass<ASDayBoardPresenter>(
			nullptr,
			TEXT("/Game/Game/Day/Board/BP_SDayBoardPresenter.BP_SDayBoardPresenter_C"));
		if (!PresenterClass)
		{
			PresenterClass = ASDayBoardPresenter::StaticClass();
		}
		GetWorld()->SpawnActor<ASDayBoardPresenter>(PresenterClass);
	}

	Super::BeginPlay();
}
#pragma endregion K2 moonyfli
