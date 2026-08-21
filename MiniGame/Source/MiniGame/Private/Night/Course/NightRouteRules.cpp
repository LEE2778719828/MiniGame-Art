#include "Night/Course/NightRouteRules.h"

#pragma region K2 moonyfli
FNightRouteRuleRow UNightRouteRulesAsset::MakeDefaultRule(ENightRouteId RouteId)
{
	FNightRouteRuleRow Row;
	Row.RouteId = RouteId;
	Row.DropRhythmEveryN = 1;
	Row.BranchDropCountMul = 1;
	Row.EnterDropId = EIngredientId::None;
	Row.bReverseFire = false;

	switch (RouteId)
	{
	case ENightRouteId::B:
		Row.VisibleBlockCount = 3;
		Row.SoulPenaltyScale = 1.25f;
		Row.DotSoulPerSecond = 2.f;
		Row.EnterDropCount = 1;
		Row.CarryOutBonus = 0.2f;
		break;
	case ENightRouteId::C:
		Row.VisibleBlockCount = 4;
		Row.SoulPenaltyScale = 1.35f;
		Row.DotSoulPerSecond = 3.f;
		Row.bReverseFire = true;
		Row.EnterDropCount = 2;
		Row.CarryOutBonus = 0.3f;
		Row.DropCycle = {
			EIngredientId::F01_LingGu,
			EIngredientId::F02_YinShanJun,
			EIngredientId::F03_ChiYanJiao
		};
		break;
	case ENightRouteId::A:
	default:
		Row.RouteId = ENightRouteId::A;
		Row.VisibleBlockCount = 8;
		Row.SoulPenaltyScale = 1.f;
		Row.DotSoulPerSecond = 0.f;
		Row.EnterDropCount = 0;
		Row.CarryOutBonus = 0.f;
		break;
	}
	return Row;
}

FNightRouteRuleRow UNightRouteRulesAsset::GetRule(ENightRouteId RouteId) const
{
	for (const FNightRouteRuleRow& Row : Rows)
	{
		if (Row.RouteId == RouteId)
		{
			return Row;
		}
	}
	return MakeDefaultRule(RouteId);
}

bool UNightRouteRulesAsset::TryGetRule(
	ENightRouteId RouteId,
	FNightRouteRuleRow& OutRule) const
{
	for (const FNightRouteRuleRow& Row : Rows)
	{
		if (Row.RouteId == RouteId)
		{
			OutRule = Row;
			return true;
		}
	}
	return false;
}

bool UNightRouteRulesAsset::ValidateRules(FString& OutError) const
{
	OutError.Reset();
	TSet<ENightRouteId> SeenRoutes;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FNightRouteRuleRow& Row = Rows[Index];
		if (Row.RouteId == ENightRouteId::None)
		{
			OutError = FString::Printf(
				TEXT("Route rule row %d has RouteId=None."),
				Index);
			return false;
		}
		if (SeenRoutes.Contains(Row.RouteId))
		{
			OutError = FString::Printf(
				TEXT("Route rule row %d duplicates RouteId=%d."),
				Index,
				static_cast<int32>(Row.RouteId));
			return false;
		}
		SeenRoutes.Add(Row.RouteId);
		if (Row.VisibleBlockCount <= 0
			|| Row.SoulPenaltyScale < 0.f
			|| Row.DotSoulPerSecond < 0.f
			|| Row.EnterDropCount < 0
			|| Row.CarryOutBonus < 0.f
			|| Row.DropRhythmEveryN < 1
			|| Row.BranchDropCountMul < 1)
		{
			OutError = FString::Printf(
				TEXT("Route rule row %d contains a non-positive visible count, negative value, or invalid drop rhythm."),
				Index);
			return false;
		}
		for (const EIngredientId IngredientId : Row.DropCycle)
		{
			if (IngredientId == EIngredientId::None)
			{
				OutError = FString::Printf(
					TEXT("Route rule row %d contains None in DropCycle."),
					Index);
				return false;
			}
		}
	}
	return true;
}
#pragma endregion K2 moonyfli
