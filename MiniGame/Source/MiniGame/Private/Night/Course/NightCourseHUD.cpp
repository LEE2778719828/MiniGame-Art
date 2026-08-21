#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

#pragma region K2 moonyfli
void ANightCourseHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwningPawn());
	UNightCourseDirector* Director = CoursePawn
		? CoursePawn->GetCourseDirector()
		: nullptr;
	UNightFeelStubComponent* Feel = CoursePawn ? CoursePawn->FeelStub : nullptr;
	const bool bForkChoice = Director && Director->IsForkChoiceActive();
	const bool bSwappedControls =
		Feel && Feel->ControlScheme == ENightControlScheme::Swapped;

	// Bottom control bar (portrait-friendly).
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), 0.f, H * 0.78f, W, H * 0.22f);
	DrawRect(FLinearColor(0.15f, 0.55f, 0.95f, 0.9f), W * 0.08f, H * 0.84f, W * 0.36f, H * 0.1f);
	DrawRect(FLinearColor(0.9f, 0.22f, 0.18f, 0.9f), W * 0.56f, H * 0.84f, W * 0.36f, H * 0.1f);

	const FString LeftControl = bForkChoice
		? TEXT("Q  LEFT")
		: (bSwappedControls ? TEXT("Q  ATTACK") : TEXT("Q  JUMP"));
	const FString RightControl = bForkChoice
		? TEXT("E  RIGHT")
		: (bSwappedControls ? TEXT("E  JUMP") : TEXT("E  ATTACK"));
	FCanvasTextItem JumpText(FVector2D(W * 0.14f, H * 0.87f), FText::FromString(LeftControl), GEngine->GetLargeFont(), FLinearColor::White);
	JumpText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(JumpText);

	FCanvasTextItem AttackText(FVector2D(W * 0.62f, H * 0.87f), FText::FromString(RightControl), GEngine->GetLargeFont(), FLinearColor::White);
	AttackText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(AttackText);

	float Soul = 100.f;
	FString Prompt;
	FLinearColor PromptColor = FLinearColor(1.f, 0.9f, 0.3f);
	if (CoursePawn)
	{
		if (Feel)
		{
			Soul = Feel->Soul;
			if (Feel->bHasActiveRequest && !bForkChoice)
			{
				const bool bAttack = (Feel->ActiveRequest.Kind == ENightNodeKind::Enemy);
				Prompt = bAttack ? TEXT("NOW: ATTACK") : TEXT("NOW: JUMP");
				PromptColor = bAttack ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.3f, 0.75f, 1.f);
			}
		}
	}

	FCanvasTextItem SoulText(FVector2D(W * 0.06f, H * 0.05f), FText::FromString(FString::Printf(TEXT("SOUL  %.0f"), Soul)), GEngine->GetLargeFont(), FLinearColor(1.f, 0.95f, 0.7f));
	SoulText.Scale = FVector2D(1.6f, 1.6f);
	Canvas->DrawItem(SoulText);

	if (Director)
	{
		const FString PhaseText = [&Director]()
		{
			switch (Director->GetPhase())
			{
			case ENightCoursePhase::ForkChoice: return FString(TEXT("FORK CHOICE"));
			case ENightCoursePhase::BranchEnterBuffer: return FString(TEXT("BRANCH BUFFER"));
			case ENightCoursePhase::BranchSegment: return FString(TEXT("BRANCH"));
			case ENightCoursePhase::KeySwapWarning: return FString(TEXT("KEY SWAP WARNING"));
			case ENightCoursePhase::KeySwapSafetyHold: return FString(TEXT("KEY SWAP HOLD"));
			case ENightCoursePhase::Failed: return FString(TEXT("NIGHT FAILED"));
			case ENightCoursePhase::Finished: return FString(TEXT("NIGHT CLEAR"));
			default: return FString(TEXT("BASE"));
			}
		}();
		const FString RouteText = Director->GetCurrentRoute() == ENightRouteId::None
			? TEXT("-")
			: FString::Printf(TEXT("%d"), static_cast<int32>(Director->GetCurrentRoute()));
		const int32 VisibleBlocks = Director->GetVisibleBlockCount();
		FCanvasTextItem StatusText(
			FVector2D(W * 0.06f, H * 0.12f),
			FText::FromString(FString::Printf(TEXT("%s   ROUTE %s"), *PhaseText, *RouteText)),
			GEngine->GetLargeFont(),
			Director->IsCourseFailed() ? FLinearColor(1.f, 0.15f, 0.1f) : FLinearColor::White);
		StatusText.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(StatusText);
		if (VisibleBlocks > 0)
		{
			FCanvasTextItem VisibilityText(
				FVector2D(W * 0.06f, H * 0.17f),
				FText::FromString(FString::Printf(TEXT("VISIBLE BLOCKS  %d"), VisibleBlocks)),
				GEngine->GetLargeFont(),
				FLinearColor(0.65f, 0.85f, 1.f));
			VisibilityText.Scale = FVector2D(0.9f, 0.9f);
			Canvas->DrawItem(VisibilityText);
		}

		if (bForkChoice)
		{
			FCanvasTextItem ForkText(
				FVector2D(W * 0.25f, H * 0.2f),
				FText::FromString(FString::Printf(
					TEXT("LEFT %d     RIGHT %d     %.1fs"),
					static_cast<int32>(Director->GetForkLeftRoute()),
					static_cast<int32>(Director->GetForkRightRoute()),
					Director->GetForkSecondsRemaining())),
				GEngine->GetLargeFont(),
				FLinearColor(1.f, 0.85f, 0.25f));
			ForkText.Scale = FVector2D(1.35f, 1.35f);
			Canvas->DrawItem(ForkText);
			const FString ForkHint = Director->GetForkHintText();
			if (!ForkHint.IsEmpty())
			{
				FCanvasTextItem HintText(
					FVector2D(W * 0.16f, H * 0.26f),
					FText::FromString(ForkHint),
					GEngine->GetLargeFont(),
					FLinearColor(0.65f, 0.9f, 1.f));
				HintText.Scale = FVector2D(0.85f, 0.85f);
				Canvas->DrawItem(HintText);
			}
		}
		else if (Director->IsKeySwapWarningActive())
		{
			FCanvasTextItem SwapText(
				FVector2D(W * 0.24f, H * 0.2f),
				FText::FromString(FString::Printf(
					TEXT("CONTROL SWAP  %.1fs"),
					Director->GetKeySwapSecondsRemaining())),
				GEngine->GetLargeFont(),
				FLinearColor(0.9f, 0.55f, 1.f));
			SwapText.Scale = FVector2D(1.35f, 1.35f);
			Canvas->DrawItem(SwapText);
		}
	}

	if (!Prompt.IsEmpty())
	{
		FCanvasTextItem PromptText(FVector2D(W * 0.28f, H * 0.12f), FText::FromString(Prompt), GEngine->GetLargeFont(), PromptColor);
		PromptText.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(PromptText);
	}
}
#pragma endregion K2 moonyfli
