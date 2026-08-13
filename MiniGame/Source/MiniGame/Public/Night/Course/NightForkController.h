#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightForkController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightForkResolved, ENightRouteId, RouteTaken, bool, bTimedOut);

#pragma region K2 moonyfli
/**
 * Unique fork choice: left/right cards, 2.4s timeout default-left.
 * Choice never counts as a feel miss.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightForkController : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Night|Fork")
	FOnNightForkResolved OnForkResolved;

	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	void BeginFork(ENightForkPair Pair, float TimeoutSeconds, bool bTimeoutPickLeft = true);

	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	void TickFork(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	void ChooseLeft();

	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	void ChooseRight();

	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	void CancelFork();

	UFUNCTION(BlueprintPure, Category = "Night|Fork")
	bool IsForkActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Night|Fork")
	float GetSecondsRemaining() const { return FMath::Max(0.f, SecondsRemaining); }

	UFUNCTION(BlueprintPure, Category = "Night|Fork")
	ENightRouteId GetLeftRoute() const { return LeftRoute; }

	UFUNCTION(BlueprintPure, Category = "Night|Fork")
	ENightRouteId GetRightRoute() const { return RightRoute; }

	UFUNCTION(BlueprintPure, Category = "Night|Fork")
	ENightForkPair GetActivePair() const { return ActivePair; }

	/** Resolve ForkPair into left/right route ids (AB / AC / BC). */
	UFUNCTION(BlueprintCallable, Category = "Night|Fork")
	static void ResolvePairRoutes(ENightForkPair Pair, ENightRouteId& OutLeft, ENightRouteId& OutRight, bool& bForcedAB);

protected:
	void Resolve(ENightRouteId Route, bool bTimedOut);

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	ENightForkPair ActivePair = ENightForkPair::AB;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	ENightRouteId LeftRoute = ENightRouteId::A;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	ENightRouteId RightRoute = ENightRouteId::B;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	float SecondsRemaining = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Fork")
	bool bPickLeftOnTimeout = true;
};
#pragma endregion K2 moonyfli
