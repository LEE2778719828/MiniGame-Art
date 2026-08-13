#include "Night/Course/NightForkController.h"

#pragma region K2 moonyfli
void UNightForkController::ResolvePairRoutes(ENightForkPair Pair, ENightRouteId& OutLeft, ENightRouteId& OutRight, bool& bForcedAB)
{
	bForcedAB = false;
	switch (Pair)
	{
	case ENightForkPair::AC:
		OutLeft = ENightRouteId::A;
		OutRight = ENightRouteId::C;
		break;
	case ENightForkPair::BC:
		OutLeft = ENightRouteId::B;
		OutRight = ENightRouteId::C;
		break;
	case ENightForkPair::AB:
	default:
		OutLeft = ENightRouteId::A;
		OutRight = ENightRouteId::B;
		break;
	}
}

void UNightForkController::BeginFork(ENightForkPair Pair, float TimeoutSeconds, bool bTimeoutPickLeft)
{
	bool bForcedAB = false;
	ResolvePairRoutes(Pair, LeftRoute, RightRoute, bForcedAB);
	(void)bForcedAB;
	ActivePair = Pair;
	bPickLeftOnTimeout = bTimeoutPickLeft;
	SecondsRemaining = FMath::Max(0.1f, TimeoutSeconds);
	bActive = true;
}

void UNightForkController::TickFork(float DeltaSeconds)
{
	if (!bActive)
	{
		return;
	}
	SecondsRemaining -= DeltaSeconds;
	if (SecondsRemaining <= 0.f)
	{
		Resolve(bPickLeftOnTimeout ? LeftRoute : RightRoute, true);
	}
}

void UNightForkController::ChooseLeft()
{
	if (bActive)
	{
		Resolve(LeftRoute, false);
	}
}

void UNightForkController::ChooseRight()
{
	if (bActive)
	{
		Resolve(RightRoute, false);
	}
}

void UNightForkController::CancelFork()
{
	bActive = false;
	SecondsRemaining = 0.f;
}

void UNightForkController::Resolve(ENightRouteId Route, bool bTimedOut)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	SecondsRemaining = 0.f;
	OnForkResolved.Broadcast(Route, bTimedOut);
}
#pragma endregion K2 moonyfli
