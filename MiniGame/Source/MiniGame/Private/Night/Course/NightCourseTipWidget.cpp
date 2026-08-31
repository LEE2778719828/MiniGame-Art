#include "Night/Course/NightCourseTipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

UNightCourseTipWidget::UNightCourseTipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UNightCourseTipWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureFallbackTree();
	RefreshBoundWidgets();
}

void UNightCourseTipWidget::PresentTip(
	const FText& InSpeakerName,
	const FText& InBody,
	const FText& InContinueHint,
	UTexture2D* Portrait)
{
	bDismissed = false;
	SpeakerName = InSpeakerName;
	BodyText = InBody;
	ContinueHint = InContinueHint;
	PortraitTexture = Portrait;
	EnsureFallbackTree();
	RefreshBoundWidgets();
}

void UNightCourseTipWidget::DismissTip()
{
	if (bDismissed)
	{
		return;
	}
	bDismissed = true;
	OnTipDismissed.Broadcast();
}

FReply UNightCourseTipWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	DismissTip();
	return FReply::Handled();
}

FReply UNightCourseTipWidget::NativeOnTouchStarted(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent)
{
	DismissTip();
	return FReply::Handled();
}

void UNightCourseTipWidget::EnsureFallbackTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UImage* Dim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Dim"));
	Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	if (UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row))
	{
		RowSlot->SetAnchors(FAnchors(0.06f, 0.62f, 0.94f, 0.92f));
		RowSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PortraitBox"));
	PortraitBox->SetWidthOverride(220.f);
	PortraitBox->SetHeightOverride(220.f);
	if (UHorizontalBoxSlot* PortraitSlot = Row->AddChildToHorizontalBox(PortraitBox))
	{
		PortraitSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		PortraitSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	Img_Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_Portrait"));
	Img_Portrait->SetVisibility(ESlateVisibility::Collapsed);
	PortraitBox->AddChild(Img_Portrait);

	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextCol"));
	if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextCol))
	{
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	Txt_Speaker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Speaker"));
	Txt_Speaker->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.86f, 0.45f, 1.f)));
	if (UVerticalBoxSlot* SpeakerSlot = TextCol->AddChildToVerticalBox(Txt_Speaker))
	{
		SpeakerSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	Txt_Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Body"));
	Txt_Body->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Txt_Body->SetAutoWrapText(true);
	if (UVerticalBoxSlot* BodySlot = TextCol->AddChildToVerticalBox(Txt_Body))
	{
		BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	Txt_Continue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Continue"));
	Txt_Continue->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 0.9f)));
	TextCol->AddChildToVerticalBox(Txt_Continue);
}

void UNightCourseTipWidget::RefreshBoundWidgets()
{
	if (Txt_Speaker)
	{
		Txt_Speaker->SetText(SpeakerName);
	}
	if (Txt_Body)
	{
		Txt_Body->SetText(BodyText);
	}
	if (Txt_Continue)
	{
		Txt_Continue->SetText(ContinueHint);
	}
	if (Img_Portrait)
	{
		if (PortraitTexture)
		{
			Img_Portrait->SetBrushFromTexture(PortraitTexture, true);
			Img_Portrait->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			Img_Portrait->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
