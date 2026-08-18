#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightForkController.h"
#include "Night/Shared/NightSharedTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

#pragma region K2 moonyfli
namespace NightCourseHUD_Private
{
	static const TCHAR* RouteLabel(ENightRouteId Id)
	{
		switch (Id)
		{
		case ENightRouteId::A: return TEXT("A CLEAR");
		case ENightRouteId::B: return TEXT("B FOG");
		case ENightRouteId::C: return TEXT("C REVERSE");
		default: return TEXT("-");
		}
	}
}

void ANightCourseHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	UNightCourseDirector* Director = nullptr;
	float Soul = 100.f;
	FString Prompt;
	FLinearColor PromptColor = FLinearColor(1.f, 0.9f, 0.3f);
	bool bSwapped = false;

	if (APawn* Pawn = GetOwningPawn())
	{
		if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(Pawn))
		{
			Director = CoursePawn->CourseDirector;
			if (UNightFeelStubComponent* Feel = CoursePawn->FeelStub)
			{
				Soul = Feel->Soul;
				bSwapped = (Feel->ControlScheme == ENightControlScheme::Swapped);
				if (Feel->bHasActiveRequest)
				{
					const bool bAttack = (Feel->ActiveRequest.Kind == ENightNodeKind::Enemy);
					if (bSwapped)
					{
						Prompt = bAttack ? TEXT("NOW: ATTACK (Q)") : TEXT("NOW: JUMP (E)");
					}
					else
					{
						Prompt = bAttack ? TEXT("NOW: ATTACK") : TEXT("NOW: JUMP");
					}
					PromptColor = bAttack ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.3f, 0.75f, 1.f);
				}
			}
			if (Director)
			{
				bSwapped = (Director->GetActiveControlScheme() == ENightControlScheme::Swapped);
			}
		}
	}

	const bool bFork = Director && Director->IsForkChoiceActive();
	const bool bSwapWarn = Director && Director->IsKeySwapWarningActive();
	const bool bSwapSafe = Director && Director->IsKeySwapSafetyActive();

	if (bFork)
	{
		UNightForkController* Fork = Director->GetForkController();
		const ENightRouteId Left = Fork ? Fork->GetLeftRoute() : ENightRouteId::A;
		const ENightRouteId Right = Fork ? Fork->GetRightRoute() : ENightRouteId::B;
		const float Remain = Fork ? Fork->GetSecondsRemaining() : 0.f;

		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.8f), 0.f, H * 0.72f, W, H * 0.28f);
		DrawRect(FLinearColor(0.2f, 0.65f, 0.35f, 0.92f), W * 0.08f, H * 0.78f, W * 0.36f, H * 0.14f);
		DrawRect(FLinearColor(0.55f, 0.25f, 0.7f, 0.92f), W * 0.56f, H * 0.78f, W * 0.36f, H * 0.14f);

		FCanvasTextItem LeftText(
			FVector2D(W * 0.12f, H * 0.82f),
			FText::FromString(FString::Printf(TEXT("Q  %s"), NightCourseHUD_Private::RouteLabel(Left))),
			GEngine->GetLargeFont(),
			FLinearColor::White);
		LeftText.Scale = FVector2D(1.3f, 1.3f);
		Canvas->DrawItem(LeftText);

		FCanvasTextItem RightText(
			FVector2D(W * 0.6f, H * 0.82f),
			FText::FromString(FString::Printf(TEXT("E  %s"), NightCourseHUD_Private::RouteLabel(Right))),
			GEngine->GetLargeFont(),
			FLinearColor::White);
		RightText.Scale = FVector2D(1.3f, 1.3f);
		Canvas->DrawItem(RightText);

		Prompt = FString::Printf(TEXT("FORK  %.1fs"), Remain);
		PromptColor = FLinearColor(1.f, 0.85f, 0.25f);
	}
	else if (bSwapWarn || bSwapSafe)
	{
		DrawRect(FLinearColor(0.15f, 0.02f, 0.18f, 0.85f), 0.f, H * 0.72f, W, H * 0.28f);
		const float Remain = Director->GetKeySwapSecondsRemaining();
		Prompt = bSwapWarn
			? FString::Printf(TEXT("KEY SWAP WARN  %.1fs"), Remain)
			: FString::Printf(TEXT("KEY SWAP HOLD  %.1fs"), Remain);
		PromptColor = FLinearColor(1.f, 0.4f, 1.f);

		FCanvasTextItem Hint(
			FVector2D(W * 0.18f, H * 0.84f),
			FText::FromString(TEXT("NO BEATS — SCHEME CHANGING")),
			GEngine->GetLargeFont(),
			FLinearColor::White);
		Hint.Scale = FVector2D(1.2f, 1.2f);
		Canvas->DrawItem(Hint);
	}
	else
	{
		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), 0.f, H * 0.78f, W, H * 0.22f);
		DrawRect(FLinearColor(0.15f, 0.55f, 0.95f, 0.9f), W * 0.08f, H * 0.84f, W * 0.36f, H * 0.1f);
		DrawRect(FLinearColor(0.9f, 0.22f, 0.18f, 0.9f), W * 0.56f, H * 0.84f, W * 0.36f, H * 0.1f);

		const FString LeftLabel = bSwapped ? TEXT("Q  ATTACK") : TEXT("Q  JUMP");
		const FString RightLabel = bSwapped ? TEXT("E  JUMP") : TEXT("E  ATTACK");

		FCanvasTextItem JumpText(FVector2D(W * 0.12f, H * 0.87f), FText::FromString(LeftLabel), GEngine->GetLargeFont(), FLinearColor::White);
		JumpText.Scale = FVector2D(1.4f, 1.4f);
		Canvas->DrawItem(JumpText);

		FCanvasTextItem AttackText(FVector2D(W * 0.6f, H * 0.87f), FText::FromString(RightLabel), GEngine->GetLargeFont(), FLinearColor::White);
		AttackText.Scale = FVector2D(1.4f, 1.4f);
		Canvas->DrawItem(AttackText);
	}

	FCanvasTextItem SoulText(FVector2D(W * 0.06f, H * 0.05f), FText::FromString(FString::Printf(TEXT("SOUL  %.0f"), Soul)), GEngine->GetLargeFont(), FLinearColor(1.f, 0.95f, 0.7f));
	SoulText.Scale = FVector2D(1.6f, 1.6f);
	Canvas->DrawItem(SoulText);

	if (Director && Director->GetRouteTaken() != ENightRouteId::None)
	{
		FCanvasTextItem RouteText(
			FVector2D(W * 0.06f, H * 0.1f),
			FText::FromString(FString::Printf(TEXT("ROUTE  %s%s"),
				NightCourseHUD_Private::RouteLabel(Director->GetRouteTaken()),
				bSwapped ? TEXT("  [SWAPPED]") : TEXT(""))),
			GEngine->GetLargeFont(),
			FLinearColor(0.75f, 0.9f, 1.f));
		RouteText.Scale = FVector2D(1.2f, 1.2f);
		Canvas->DrawItem(RouteText);
	}

	if (!Prompt.IsEmpty())
	{
		FCanvasTextItem PromptText(FVector2D(W * 0.18f, H * 0.14f), FText::FromString(Prompt), GEngine->GetLargeFont(), PromptColor);
		PromptText.Scale = FVector2D(1.8f, 1.8f);
		Canvas->DrawItem(PromptText);
	}

#pragma region K2 moonyfli
	if (Director && Director->IsAwaitingInput() && !bFork)
	{
		FCanvasTextItem Hint(
			FVector2D(W * 0.18f, H * 0.2f),
			FText::FromString(TEXT("WRONG KEY = NO ADVANCE")),
			GEngine->GetLargeFont(),
			FLinearColor(1.f, 0.55f, 0.2f, 0.85f));
		Hint.Scale = FVector2D(1.0f, 1.0f);
		Canvas->DrawItem(Hint);
	}
#pragma endregion K2 moonyfli
}
#pragma endregion K2 moonyfli
