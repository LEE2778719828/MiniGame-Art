#include "Day/Presentation/SDayBoardPresentation.h"

#include "../../../SStandaloneSandbox.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SafeZone.h"
#include "Components/SkyLightComponent.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName LingGuId(TEXT("LingGu"));
	const FName YinShanJunId(TEXT("YinShanJun"));
	const FName ChiYanJiaoId(TEXT("ChiYanJiao"));
	const FName YueLinYuId(TEXT("YueLinYu"));
	const FName XuanYuQinId(TEXT("XuanYuQin"));
	const FName NpcALingId(TEXT("ALing"));
	const FName NpcSangPoId(TEXT("SangPo"));
	const FName GiftGuideKiteId(TEXT("GuideKite"));
	const FName GiftLifeLampId(TEXT("LifeLamp"));

	UStaticMesh* LoadBasicShape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	FLinearColor IngredientColor(const FName IngredientId)
	{
		if (IngredientId == LingGuId) return FLinearColor(0.98f, 0.14f, 0.28f);
		if (IngredientId == YinShanJunId) return FLinearColor(0.10f, 0.78f, 0.64f);
		if (IngredientId == ChiYanJiaoId) return FLinearColor(1.00f, 0.35f, 0.05f);
		if (IngredientId == YueLinYuId) return FLinearColor(0.10f, 0.42f, 0.95f);
		if (IngredientId == XuanYuQinId) return FLinearColor(0.72f, 0.18f, 0.90f);
		return FLinearColor::White;
	}

	FString IngredientShortName(const FName IngredientId)
	{
		if (IngredientId == LingGuId) return TEXT("灵谷");
		if (IngredientId == YinShanJunId) return TEXT("阴山菌");
		if (IngredientId == ChiYanJiaoId) return TEXT("赤焰椒");
		if (IngredientId == YueLinYuId) return TEXT("月鳞鱼");
		if (IngredientId == XuanYuQinId) return TEXT("玄羽禽");
		return IngredientId.ToString();
	}

	void SetButtonText(UButton* Button, const FString& Text)
	{
		if (Button && Button->GetChildrenCount() > 0)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
			{
				Label->SetText(FText::FromString(Text));
			}
		}
	}

	/** The portrait camera uses Yaw 90 so world +Y is screen-up; mirror X keeps the art's left/right composition. */
	FVector MirrorX(const FVector& In)
	{
		return FVector(-In.X, In.Y, In.Z);
	}

	/**
	 * World text lies flat with its front face toward the portrait camera, so it reads
	 * horizontally (and unmirrored) on a phone screen. Pitch 90 turns the glyph plane up,
	 * Yaw -90 lines the reading direction up with the camera's Yaw 90 framing.
	 */
	FRotator LabelFacingRotation()
	{
		return FRotator(90.0f, -90.0f, 0.0f);
	}

	FLinearColor IdleCellColor()
	{
		return FLinearColor(0.34f, 0.30f, 0.26f);
	}

	FLinearColor SelectedCellColor()
	{
		return FLinearColor(1.00f, 0.84f, 0.25f);
	}

	/** Labels that sit on top of pale meshes need dark text to stay readable. */
	FColor LabelInkColor()
	{
		return FColor(24, 20, 16);
	}

	/** SetFont recomputes the glyph scale, so the world size has to be re-applied afterwards. */
	void ApplyLabelFont(UTextRenderComponent* Label, UFont* Font, const float WorldSize)
	{
		if (!Label || !Font)
		{
			return;
		}
		Label->SetFont(Font);
		Label->SetWorldSize(WorldSize);
	}

	/** Placeholder materials derive from BasicShapeMaterial ("Color"); set both names so future art materials tint too. */
	void ApplyTint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh)
		{
			return;
		}
		const FVector Rgb(Color.R, Color.G, Color.B);
		Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), Rgb);
		Mesh->SetVectorParameterValueOnMaterials(TEXT("Tint"), Rgb);
	}
}

#pragma region K2 moonyfli

ASDayCellVisual::ASDayCellVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CellMesh"));
	CellMesh->SetupAttachment(Root);
	CellMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CellMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CellMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
	PieceMesh->SetupAttachment(Root);
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	PieceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PieceLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PieceLabel"));
	PieceLabel->SetupAttachment(Root);
	PieceLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	PieceLabel->SetRelativeRotation(LabelFacingRotation());
	PieceLabel->SetHorizontalAlignment(EHTA_Center);
	PieceLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PieceLabel->SetWorldSize(36.0f);
	PieceLabel->SetTextRenderColor(FColor::White);
}

void ASDayCellVisual::Configure(const int32 InCellIndex, const float InRadius, ASMergeBoard* InBoard)
{
	CellIndex = InCellIndex;
	VisualRadius = InRadius;
	Board = InBoard;

	if (!CellMesh->GetStaticMesh())
	{
		CellMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	}
	if (!PieceMesh->GetStaticMesh())
	{
		PieceMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}

	const float CellScale = VisualRadius / 50.0f;
	CellMesh->SetRelativeScale3D(FVector(CellScale, CellScale * 0.82f, 0.12f));
	PieceLabel->SetRelativeRotation(LabelFacingRotation());
	RefreshVisual();
}

void ASDayCellVisual::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(PieceLabel, InFont, 36.0f);
}

void ASDayCellVisual::RefreshVisual()
{
	ASMergeBoard* MergeBoard = Board.Get();
	if (!MergeBoard)
	{
		// A level-placed presenter can build before the gateway spawns the board, so rebind lazily.
		MergeBoard = ASMergeBoard::FindBoard(this);
		Board = MergeBoard;
	}
	if (!MergeBoard)
	{
		PieceMesh->SetVisibility(false);
		PieceLabel->SetVisibility(false);
		return;
	}

	FSDishPiece Piece;
	const bool bHasPiece = MergeBoard->TryGetPiece(CellIndex, Piece);
	const bool bSelected = MergeBoard->GetActiveDragCellIndex() == CellIndex;
	PieceMesh->SetVisibility(bHasPiece);
	PieceLabel->SetVisibility(bHasPiece);
	CellMesh->SetCustomDepthStencilValue(bSelected ? 2 : 1);
	CellMesh->SetRenderCustomDepth(bSelected);
	// The whitebox has no outline post-process, so the selection reads through tint and lift instead.
	ApplyTint(CellMesh, bSelected ? SelectedCellColor() : IdleCellColor());

	if (!bHasPiece)
	{
		return;
	}

	const float SelectionBoost = bSelected ? 1.25f : 1.0f;
	const float LevelScale = (0.52f + static_cast<float>(Piece.Level) * 0.10f) * SelectionBoost;
	PieceMesh->SetRelativeScale3D(FVector(
		LevelScale,
		LevelScale,
		(0.25f + Piece.Level * 0.05f) * SelectionBoost));
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, bSelected ? 62.0f : 32.0f));

	const FLinearColor PieceColor = IngredientColor(Piece.IngredientId);
	ApplyTint(PieceMesh, bSelected ? PieceColor * 1.8f + FLinearColor(0.15f, 0.15f, 0.05f, 0.0f) : PieceColor);
	PieceLabel->SetRelativeLocation(FVector(0.0f, 0.0f, bSelected ? 108.0f : 75.0f));
	// Ink instead of white/amber: the board and the highlight tint are both pale.
	PieceLabel->SetTextRenderColor(LabelInkColor());
	PieceLabel->SetText(FText::FromString(FString::Printf(
		TEXT("%s%s Lv%d"),
		bSelected ? TEXT("▲ ") : TEXT(""),
		*IngredientShortName(Piece.IngredientId),
		Piece.Level)));
}

ASDayIngredientBinVisual::ASDayIngredientBinVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BinMesh"));
	BinMesh->SetupAttachment(Root);
	BinMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BinMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BinMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(28.0f);
}

void ASDayIngredientBinVisual::Configure(const FName InIngredientId, const FString& InDisplayName)
{
	IngredientId = InIngredientId;
	if (!BinMesh->GetStaticMesh())
	{
		BinMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube")));
	}
	BinMesh->SetRelativeScale3D(FVector(1.15f, 0.72f, 0.55f));
	ApplyTint(BinMesh, IngredientColor(IngredientId));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetText(FText::FromString(InDisplayName));
	// The bin mesh already carries the ingredient colour, so tinting the text to match hides it.
	Label->SetTextRenderColor(LabelInkColor());
}

void ASDayIngredientBinVisual::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(Label, InFont, 28.0f);
}

ASDayCharacterStandIn::ASDayCharacterStandIn()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CharacterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(Root);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	// -Y is screen-down under the portrait camera; the seats hug the top edge, so the
	// name plate goes underneath them where there is empty space.
	Label->SetRelativeLocation(FVector(0.0f, -120.0f, 150.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.0f);
}

void ASDayCharacterStandIn::Configure(const FString& InLabel, const FLinearColor& InColor)
{
	if (!CharacterMesh->GetStaticMesh())
	{
		CharacterMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	}
	CharacterMesh->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.25f));
	// Live Coding keeps existing instances, so re-apply pointer settings outside the constructor.
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ApplyTint(CharacterMesh, InColor);
	Label->SetRelativeLocation(FVector(0.0f, -120.0f, 150.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.0f);
	SetHeadline(InLabel, InColor);
}

void ASDayCharacterStandIn::SetHeadline(const FString& InText, const FLinearColor& InColor)
{
	if (!Label)
	{
		return;
	}
	Label->SetText(FText::FromString(InText));
	Label->SetTextRenderColor(InColor.ToFColor(true));
}

void ASDayCharacterStandIn::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(Label, InFont, 24.0f);
}

ASDayBoardPresenter::ASDayBoardPresenter()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BoardFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardFrame"));
	BoardFrame->SetupAttachment(Root);
	BoardFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Counter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Counter"));
	Counter->SetupAttachment(Root);
	Counter->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("PortraitCamera"));
	Camera->SetupAttachment(Root);
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	Camera->SetRelativeLocation(FVector(0.0f, 60.0f, 2200.0f));
	// Yaw 90 maps world +Y to screen-up so the tall board axis fills the portrait frame.
	Camera->SetRelativeRotation(FRotator(-90.0f, 90.0f, 0.0f));
	Camera->OrthoWidth = PortraitOrthoWidth;
	Camera->bConstrainAspectRatio = true;
	Camera->AspectRatio = 9.0f / 16.0f;

	WhiteboxKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("WhiteboxKeyLight"));
	WhiteboxKeyLight->SetupAttachment(Root);
	WhiteboxKeyLight->SetRelativeRotation(FRotator(-55.0f, -35.0f, 0.0f));
	WhiteboxKeyLight->SetIntensity(7.0f);
	WhiteboxKeyLight->SetLightColor(FLinearColor(1.0f, 0.90f, 0.78f));

	WhiteboxFillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("WhiteboxFillLight"));
	WhiteboxFillLight->SetupAttachment(Root);
	WhiteboxFillLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	WhiteboxFillLight->SetIntensity(1.5f);
	WhiteboxFillLight->SetLightColor(FLinearColor(0.55f, 0.68f, 1.0f));

	VisualConfig = TSoftObjectPtr<USDayBoardVisualConfig>(
		FSoftObjectPath(TEXT("/Game/Game/Day/Data/DA_SDayBoardVisualConfig.DA_SDayBoardVisualConfig")));
}

void ASDayBoardPresenter::BeginPlay()
{
	Super::BeginPlay();
	LogicBoard = ASMergeBoard::FindBoard(this);
	BuildWhitebox();

	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}

	if (bTakeCameraOnBeginPlay)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			// Re-apply the portrait framing at runtime so Live Coding picks it up without an editor restart.
			Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
			Camera->SetRelativeLocation(FVector(0.0f, 60.0f, 2200.0f));
			Camera->SetRelativeRotation(FRotator(-90.0f, 90.0f, 0.0f));
			Camera->OrthoWidth = PortraitOrthoWidth;
			Camera->bConstrainAspectRatio = true;
			Camera->AspectRatio = 9.0f / 16.0f;
			PlayerController->SetViewTarget(this);
		}
	}
	RefreshFromLogic();
}

void ASDayBoardPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}
	Super::EndPlay(EndPlayReason);
}

void ASDayBoardPresenter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Patience and spawn countdowns move without any board event, so poll instead of
	// waiting for the next pointer interaction to redraw seats and cells.
	SeatRefreshCountdown -= DeltaSeconds;
	if (SeatRefreshCountdown <= 0.0f)
	{
		SeatRefreshCountdown = 0.25f;
		RefreshFromLogic();
	}

	FVector2D ScreenPosition;
	const bool bPointerDown = GetPointerState(ScreenPosition);
	if (bPointerDown)
	{
		LastPointerPosition = ScreenPosition;
	}

	if (bPointerDown && !bPointerWasDown)
	{
		HandlePointerPressed(ScreenPosition);
	}
	else if (!bPointerDown && bPointerWasDown)
	{
		HandlePointerReleased(LastPointerPosition);
	}
	bPointerWasDown = bPointerDown;
}

void ASDayBoardPresenter::BuildWhitebox()
{
	USDayBoardVisualConfig* Config = VisualConfig.LoadSynchronous();
	UStaticMesh* Cube = LoadBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UMaterialInterface* BoardMaterial = Config ? Config->BoardMaterial.LoadSynchronous() : nullptr;
	UMaterialInterface* CellMaterial = Config ? Config->CellMaterial.LoadSynchronous() : nullptr;

	BoardFrame->SetStaticMesh(Config && Config->BoardFrameMesh.LoadSynchronous()
		? Config->BoardFrameMesh.Get()
		: Cube);
	// Sized to enclose the 12 playable cells (X +/-330, Y -150..450) plus their visual radius.
	BoardFrame->SetRelativeLocation(FVector(0.0f, 150.0f, -25.0f));
	BoardFrame->SetRelativeScale3D(FVector(9.0f, 8.0f, 0.25f));
	if (BoardMaterial)
	{
		BoardFrame->SetMaterial(0, BoardMaterial);
	}

	Counter->SetStaticMesh(Cube);
	Counter->SetRelativeLocation(FVector(0.0f, 760.0f, 65.0f));
	Counter->SetRelativeScale3D(FVector(9.0f, 0.45f, 0.65f));
	if (BoardMaterial)
	{
		Counter->SetMaterial(0, BoardMaterial);
	}

	const TArray<FVector> DecorativeLocations =
	{
		{-390, 470, 15}, {350, 470, 15}, {-400, 350, 15}, {390, 350, 15},
		{-410, 210, 15}, {400, 150, 15}, {-400, -50, 15}, {390, -100, 15},
		{-360, -240, 15}, {350, -260, 15}, {-300, -360, 15}, {270, -390, 15},
		{-20, 390, 15}, {40, -280, 15}, {300, 80, 15}
	};
	for (int32 Index = 0; Index < DecorativeLocations.Num(); ++Index)
	{
		UStaticMeshComponent* Decor = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("DecorativeCell_%02d"), Index));
		Decor->SetStaticMesh(Cylinder);
		Decor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Decor->SetRelativeLocation(MirrorX(DecorativeLocations[Index]));
		const float S = 0.65f + static_cast<float>(Index % 4) * 0.10f;
		Decor->SetRelativeScale3D(FVector(S, S * 0.75f, 0.08f));
		Decor->SetupAttachment(RootComponent);
		Decor->RegisterComponent();
		if (CellMaterial)
		{
			Decor->SetMaterial(0, CellMaterial);
		}
	}

	BuildCells();
	BuildBins();
	BuildCharacters();
}

TArray<FSDayBoardLayoutRow> ASDayBoardPresenter::GetLayoutRows() const
{
	TArray<FSDayBoardLayoutRow> Rows;
	const USDayBoardVisualConfig* Config = VisualConfig.Get();
	if (Config)
	{
		if (const UDataTable* Layout = Config->CellLayout.LoadSynchronous())
		{
			TArray<FSDayBoardLayoutRow*> TableRows;
			Layout->GetAllRows(TEXT("SDayBoardPresenter"), TableRows);
			for (const FSDayBoardLayoutRow* Row : TableRows)
			{
				if (Row)
				{
					Rows.Add(*Row);
				}
			}
		}
	}

	if (Rows.Num() > 0)
	{
		Rows.Sort([](const FSDayBoardLayoutRow& A, const FSDayBoardLayoutRow& B)
		{
			return A.CellIndex < B.CellIndex;
		});
		return Rows;
	}

	const TArray<int32> CellIndices = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14};
	const TArray<FVector> Locations =
	{
		{-185, 450, 35}, {150, 445, 35}, {-285, 300, 35}, {-65, 290, 35},
		{175, 300, 35}, {330, 230, 35}, {-305, 105, 35}, {-75, 105, 35},
		{165, 100, 35}, {320, -25, 35}, {-165, -125, 35}, {125, -150, 35}
	};
	for (int32 Index = 0; Index < CellIndices.Num(); ++Index)
	{
		FSDayBoardLayoutRow Row;
		Row.CellIndex = CellIndices[Index];
		Row.Transform = FTransform(FRotator::ZeroRotator, Locations[Index], FVector::OneVector);
		Row.VisualRadius = Index % 3 == 0 ? 92.0f : 78.0f;
		Rows.Add(Row);
	}
	return Rows;
}

void ASDayBoardPresenter::BuildCells()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayCellVisual> VisualClass = ASDayCellVisual::StaticClass();
	if (Config && Config->CellVisualClass)
	{
		VisualClass = Config->CellVisualClass;
	}

	for (const FSDayBoardLayoutRow& Row : GetLayoutRows())
	{
		FTransform LocalTransform = Row.Transform;
		LocalTransform.SetLocation(MirrorX(Row.Transform.GetLocation()));

		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayCellVisual* Visual = World->SpawnActor<ASDayCellVisual>(
			VisualClass,
			LocalTransform * GetActorTransform(),
			Params);
		if (!Visual)
		{
			continue;
		}
		Visual->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		// Assign meshes/materials before Configure so the tint pass binds to the placeholder material.
		if (Config)
		{
			if (UStaticMesh* Mesh = Config->CellMesh.LoadSynchronous())
			{
				Visual->CellMesh->SetStaticMesh(Mesh);
			}
			if (UStaticMesh* Mesh = Config->PieceMesh.LoadSynchronous())
			{
				Visual->PieceMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->CellMaterial.LoadSynchronous())
			{
				Visual->CellMesh->SetMaterial(0, Material);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Visual->PieceMesh->SetMaterial(0, Material);
			}
		}
		Visual->SetLabelFont(ResolveLabelFont());
		Visual->Configure(Row.CellIndex, Row.VisualRadius, LogicBoard.Get());
		CellVisuals.Add(Visual);
	}
}

void ASDayBoardPresenter::BuildBins()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayIngredientBinVisual> BinClass = ASDayIngredientBinVisual::StaticClass();
	if (Config && Config->IngredientBinClass)
	{
		BinClass = Config->IngredientBinClass;
	}
	const TArray<FName> Ids = {LingGuId, YinShanJunId, ChiYanJiaoId, YueLinYuId, XuanYuQinId};

	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		const FVector Location(-380.0f + Index * 190.0f, -690.0f + FMath::Abs(2 - Index) * 18.0f, 45.0f);
		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayIngredientBinVisual* Bin = World->SpawnActor<ASDayIngredientBinVisual>(
			BinClass,
			FTransform(MirrorX(Location)) * GetActorTransform(),
			Params);
		if (!Bin)
		{
			continue;
		}
		Bin->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		if (Config)
		{
			if (UStaticMesh* Mesh = Config->IngredientBinMesh.LoadSynchronous())
			{
				Bin->BinMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Bin->BinMesh->SetMaterial(0, Material);
			}
		}
		Bin->SetLabelFont(ResolveLabelFont());
		Bin->Configure(Ids[Index], IngredientShortName(Ids[Index]));
		IngredientBins.Add(Bin);
	}
}

void ASDayBoardPresenter::BuildCharacters()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayCharacterStandIn> CharacterClass = ASDayCharacterStandIn::StaticClass();
	if (Config && Config->CharacterStandInClass)
	{
		CharacterClass = Config->CharacterStandInClass;
	}
	const TArray<FString> Labels = {TEXT("顾客"), TEXT("阿翎"), TEXT("桑婆"), TEXT("厨师")};
	const TArray<FVector> Locations =
	{
		{-280, 815, 105}, {-80, 815, 105}, {130, 815, 105}, {405, -365, 105}
	};
	const TArray<FLinearColor> Colors =
	{
		FLinearColor(0.95f, 0.75f, 0.65f), FLinearColor(0.15f, 0.80f, 0.65f),
		FLinearColor(0.95f, 0.35f, 0.45f), FLinearColor(0.92f, 0.92f, 0.88f)
	};

	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayCharacterStandIn* Character = World->SpawnActor<ASDayCharacterStandIn>(
			CharacterClass,
			FTransform(MirrorX(Locations[Index])) * GetActorTransform(),
			Params);
		if (!Character)
		{
			continue;
		}
		Character->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		if (Config)
		{
			UStaticMesh* Mesh = Index == 3
				? Config->ChefMesh.LoadSynchronous()
				: (Index == 0 ? Config->CustomerMesh.LoadSynchronous() : Config->NpcMesh.LoadSynchronous());
			if (Mesh)
			{
				Character->CharacterMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Character->CharacterMesh->SetMaterial(0, Material);
			}
		}
		// Seats 0-2 are the three circles at the top of the portrait view and accept deliveries;
		// seat 3 is the chef and stays inert.
		Character->NpcId = Index == 1 ? NpcALingId : (Index == 2 ? NpcSangPoId : NAME_None);
		Character->bDeliveryTarget = Index != 3;
		Character->SetLabelFont(ResolveLabelFont());
		Character->Configure(Labels[Index], Colors[Index]);
		CharacterStandIns.Add(Character);
	}

	RefreshCharacters();
}

UFont* ASDayBoardPresenter::ResolveLabelFont() const
{
	if (const USDayBoardVisualConfig* Config = VisualConfig.Get())
	{
		if (UFont* Font = Config->LabelFont.LoadSynchronous())
		{
			return Font;
		}
	}
	// Fall back to the generated asset so labels stay readable before the config is re-saved.
	return LoadObject<UFont>(nullptr, TEXT("/Game/Game/Day/UI/F_SDayLabel.F_SDayLabel"));
}

void ASDayBoardPresenter::RefreshCharacters()
{
	const ASCustomerDirector* CustomerDirector = ASCustomerDirector::FindDirector(this);
	const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this);

	for (ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (!Character || !Character->bDeliveryTarget)
		{
			continue;
		}

		// Seats sit ~200 units apart, so every line stays short enough not to collide
		// with the neighbouring plate; the full wording lives on the HUD order bar.
		if (Character->NpcId.IsNone())
		{
			FString Headline = TEXT("空座\n等待顾客");
			FLinearColor Color(0.62f, 0.58f, 0.54f);
			if (CustomerDirector)
			{
				if (CustomerDirector->HasActiveCustomer())
				{
					const FSCustomerState Customer = CustomerDirector->GetActiveCustomer();
					Headline = FString::Printf(
						TEXT("%s\n%sLv%d\n%.0fs"),
						Customer.DisplayName.IsEmpty() ? *Customer.CustomerId : *Customer.DisplayName,
						*IngredientShortName(Customer.Order.IngredientId),
						Customer.Order.Level,
						Customer.PatienceRemaining);
					Color = FLinearColor(0.98f, 0.86f, 0.42f);
				}
				else
				{
					Headline = FString::Printf(
						TEXT("空座\n下一位\n%.0fs"),
						CustomerDirector->GetSpawnCooldownRemaining());
				}
			}
			Character->SetHeadline(Headline, Color);
			continue;
		}

		FSSpecialNpcState Npc;
		if (!NpcDirector || !NpcDirector->TryGetNpc(Character->NpcId, Npc))
		{
			Character->SetHeadline(
				FString::Printf(TEXT("%s\n未到店"), *Character->NpcId.ToString()),
				FLinearColor(0.62f, 0.58f, 0.54f));
			continue;
		}

		if (Npc.bServed)
		{
			Character->SetHeadline(
				FString::Printf(TEXT("%s\n已服务✓"), *Npc.DisplayName),
				FLinearColor(0.45f, 0.80f, 0.58f));
			continue;
		}

		Character->SetHeadline(
			FString::Printf(
				TEXT("%s\n%sLv%d\n→%s"),
				*Npc.DisplayName,
				*IngredientShortName(Npc.Order.IngredientId),
				Npc.Order.Level,
				*USChefGameInstance::GetGiftDisplayName(Npc.GiftId)),
			FLinearColor(0.40f, 0.95f, 0.82f));
	}
}

bool ASDayBoardPresenter::TryDeliverToCharacter(ASDayCharacterStandIn* Character, ASMergeBoard* Board)
{
	if (!Character || !Character->bDeliveryTarget || !Board || !Board->IsDragging())
	{
		return false;
	}

	if (Character->NpcId.IsNone())
	{
		if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
		{
			Director->TryDeliverFromCell(Board->GetActiveDragCellIndex());
			return true;
		}
		return false;
	}

	if (ASSpecialNpcDirector* Director = ASSpecialNpcDirector::FindDirector(this))
	{
		Director->TryDeliverToNpc(Character->NpcId);
		return true;
	}
	return false;
}

void ASDayBoardPresenter::RefreshFromLogic()
{
	if (!LogicBoard.IsValid())
	{
		LogicBoard = ASMergeBoard::FindBoard(this);
	}
	for (ASDayCellVisual* Visual : CellVisuals)
	{
		if (Visual)
		{
			Visual->RefreshVisual();
		}
	}

	RefreshCharacters();
}

void ASDayBoardPresenter::SimulatePointerEvent(const FVector2D ScreenPosition, const bool bPressed)
{
	if (bPressed)
	{
		HandlePointerPressed(ScreenPosition);
	}
	else
	{
		HandlePointerReleased(ScreenPosition);
	}
}

ASDayCellVisual* ASDayBoardPresenter::GetCellVisual(const int32 CellIndex) const
{
	for (ASDayCellVisual* Visual : CellVisuals)
	{
		if (Visual && Visual->CellIndex == CellIndex)
		{
			return Visual;
		}
	}
	return nullptr;
}

ASDayCharacterStandIn* ASDayBoardPresenter::GetSeat(const FName InNpcId) const
{
	for (ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (Character && Character->bDeliveryTarget && Character->NpcId == InNpcId)
		{
			return Character;
		}
	}
	return nullptr;
}

bool ASDayBoardPresenter::GetPointerState(FVector2D& OutScreenPosition) const
{
	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return false;
	}

	float TouchX = 0.0f;
	float TouchY = 0.0f;
	bool bTouchPressed = false;
	PlayerController->GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bTouchPressed);
	if (bTouchPressed)
	{
		OutScreenPosition = FVector2D(TouchX, TouchY);
		return true;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PlayerController->GetMousePosition(MouseX, MouseY);
	OutScreenPosition = FVector2D(MouseX, MouseY);
	return PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
}

AActor* ASDayBoardPresenter::HitTest(const FVector2D& ScreenPosition) const
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return nullptr;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SDayBoardPointer), false);
	if (PlayerController->GetHitResultAtScreenPosition(ScreenPosition, ECC_Visibility, Params, Hit))
	{
		return Hit.GetActor();
	}
	return nullptr;
}

void ASDayBoardPresenter::HandlePointerPressed(const FVector2D& ScreenPosition)
{
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (!Board)
	{
		return;
	}
	bDropHandledOnPress = false;

	if (ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitTest(ScreenPosition)))
	{
		// Second click of the click-release-click flow: a selected piece lands on the seat.
		if (TryDeliverToCharacter(Character, Board))
		{
			bDropHandledOnPress = true;
		}
		RefreshFromLogic();
		return;
	}

	if (ASDayIngredientBinVisual* Bin = Cast<ASDayIngredientBinVisual>(HitTest(ScreenPosition)))
	{
		Board->TrySpawnFromMotherPiece(Bin->IngredientId);
		RefreshFromLogic();
		return;
	}

	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitTest(ScreenPosition)))
	{
		if (Board->IsDragging())
		{
			const int32 FromIndex = Board->GetActiveDragCellIndex();
			if (Cell->CellIndex != FromIndex)
			{
				// Second click completes the click-release-click interaction immediately.
				Board->TryDropPiece(FromIndex, Cell->CellIndex);
				bDropHandledOnPress = true;
				RefreshFromLogic();
			}
			return;
		}

		FSDishPiece Piece;
		if (Board->TryGetPiece(Cell->CellIndex, Piece))
		{
			Board->BeginPieceDrag(Cell->CellIndex, 0);
			RefreshFromLogic();
		}
	}
}

void ASDayBoardPresenter::HandlePointerReleased(const FVector2D& ScreenPosition)
{
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (!Board || !Board->IsDragging())
	{
		return;
	}
	if (bDropHandledOnPress)
	{
		bDropHandledOnPress = false;
		return;
	}

	const int32 FromIndex = Board->GetActiveDragCellIndex();
	AActor* HitActor = HitTest(ScreenPosition);
	if (ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitActor))
	{
		// Dragging a piece onto a seat delivers it, same as the HUD buttons.
		TryDeliverToCharacter(Character, Board);
		RefreshFromLogic();
		return;
	}

	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitActor))
	{
		if (Cell->CellIndex != FromIndex)
		{
			// Holding on A and releasing over B completes the drag interaction.
			Board->TryDropPiece(FromIndex, Cell->CellIndex);
		}
	}
	// Releasing a simple click over A (or outside the board) keeps A selected,
	// allowing the next click on B to move or merge it.
	RefreshFromLogic();
}

TSharedRef<SWidget> USDayHUD::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void USDayHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &USDayHUD::Refresh);
	}
	Refresh();
}

void USDayHUD::NativeDestruct()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &USDayHUD::Refresh);
	}
	Super::NativeDestruct();
}

void USDayHUD::BuildWidgetTree()
{
	USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("DayHUDSafeZone"));
	SafeZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = SafeZone;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DayHUDRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SafeZone->AddChild(Root);

	auto MakeText = [this](const TCHAR* Name, const int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	};
	auto MakeButton = [this, &MakeText](const TCHAR* Name, const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeText(*FString::Printf(TEXT("%s_Label"), Name), 22, FLinearColor::White);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		Button->AddChild(Text);
		return Button;
	};

	PhaseText = MakeText(TEXT("PhaseText"), 24, FLinearColor(0.10f, 0.95f, 0.75f));
	Root->AddChildToVerticalBox(PhaseText)->SetPadding(FMargin(18.0f, 12.0f, 18.0f, 4.0f));

	OrderText = MakeText(TEXT("DayOrderText"), 20, FLinearColor(0.98f, 0.90f, 0.42f));
	Root->AddChildToVerticalBox(OrderText)->SetPadding(FMargin(18.0f, 2.0f, 18.0f, 4.0f));

	UHorizontalBox* Orders = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrderRow"));
	Root->AddChildToVerticalBox(Orders)->SetPadding(FMargin(12.0f, 4.0f));
	CustomerButton = MakeButton(TEXT("DayCustomerButton"), TEXT("普通顾客"));
	ALingButton = MakeButton(TEXT("DayALingButton"), TEXT("阿翎"));
	SangPoButton = MakeButton(TEXT("DaySangPoButton"), TEXT("桑婆"));
	Orders->AddChildToHorizontalBox(CustomerButton)->SetPadding(FMargin(4.0f));
	Orders->AddChildToHorizontalBox(ALingButton)->SetPadding(FMargin(4.0f));
	Orders->AddChildToHorizontalBox(SangPoButton)->SetPadding(FMargin(4.0f));
	CustomerButton->OnClicked.AddDynamic(this, &USDayHUD::HandleCustomer);
	ALingButton->OnClicked.AddDynamic(this, &USDayHUD::HandleALing);
	SangPoButton->OnClicked.AddDynamic(this, &USDayHUD::HandleSangPo);

	USizeBox* WorldSpacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WorldBoardInputSpace"));
	WorldSpacer->SetHeightOverride(680.0f);
	WorldSpacer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBoxSlot* SpacerSlot = Root->AddChildToVerticalBox(WorldSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	FeedbackText = MakeText(TEXT("DayFeedbackText"), 20, FLinearColor(1.0f, 0.82f, 0.20f));
	Root->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(18.0f, 4.0f));

	InventoryText = MakeText(TEXT("DayInventoryText"), 20, FLinearColor::White);
	Root->AddChildToVerticalBox(InventoryText)->SetPadding(FMargin(18.0f, 4.0f));

	UWrapBox* Ingredients = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("IngredientRow"));
	Ingredients->SetInnerSlotPadding(FVector2D(4.0f, 4.0f));
	Root->AddChildToVerticalBox(Ingredients)->SetPadding(FMargin(12.0f, 2.0f));
	UButton* LingGu = MakeButton(TEXT("DayLingGuButton"), TEXT("灵谷"));
	UButton* Yin = MakeButton(TEXT("DayYinShanJunButton"), TEXT("阴山菌"));
	UButton* Chi = MakeButton(TEXT("DayChiYanJiaoButton"), TEXT("赤焰椒"));
	UButton* Yue = MakeButton(TEXT("DayYueLinYuButton"), TEXT("月鳞鱼"));
	UButton* Xuan = MakeButton(TEXT("DayXuanYuQinButton"), TEXT("玄羽禽"));
	Ingredients->AddChildToWrapBox(LingGu);
	Ingredients->AddChildToWrapBox(Yin);
	Ingredients->AddChildToWrapBox(Chi);
	Ingredients->AddChildToWrapBox(Yue);
	Ingredients->AddChildToWrapBox(Xuan);
	LingGu->OnClicked.AddDynamic(this, &USDayHUD::HandleLingGu);
	Yin->OnClicked.AddDynamic(this, &USDayHUD::HandleYinShanJun);
	Chi->OnClicked.AddDynamic(this, &USDayHUD::HandleChiYanJiao);
	Yue->OnClicked.AddDynamic(this, &USDayHUD::HandleYueLinYu);
	Xuan->OnClicked.AddDynamic(this, &USDayHUD::HandleXuanYuQin);

	UHorizontalBox* Gifts = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GiftRow"));
	Root->AddChildToVerticalBox(Gifts)->SetPadding(FMargin(12.0f, 2.0f, 12.0f, 10.0f));
	GuideKiteButton = MakeButton(TEXT("DayGuideKiteButton"), TEXT("纸鸢"));
	LifeLampButton = MakeButton(TEXT("DayLifeLampButton"), TEXT("纸灯"));
	ConfirmNightButton = MakeButton(TEXT("DayConfirmNightButton"), TEXT("确认入夜"));
	Gifts->AddChildToHorizontalBox(GuideKiteButton)->SetPadding(FMargin(4.0f));
	Gifts->AddChildToHorizontalBox(LifeLampButton)->SetPadding(FMargin(4.0f));
	Gifts->AddChildToHorizontalBox(ConfirmNightButton)->SetPadding(FMargin(4.0f));
	GuideKiteButton->OnClicked.AddDynamic(this, &USDayHUD::HandleGuideKite);
	LifeLampButton->OnClicked.AddDynamic(this, &USDayHUD::HandleLifeLamp);
	ConfirmNightButton->OnClicked.AddDynamic(this, &USDayHUD::HandleConfirmNight);
}

void USDayHUD::Refresh()
{
	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	if (PhaseText)
	{
		PhaseText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %s  营业额 %d/%d  缺口 %d"),
			*GameInstance->GetPhaseDisplayName(),
			*GameInstance->StageId.ToString(),
			GameInstance->Revenue,
			GameInstance->RevenueTarget,
			GameInstance->GetRevenueGap())));
	}
	if (InventoryText)
	{
		InventoryText->SetText(FText::FromString(FString::Printf(
			TEXT("库存  灵谷 %d｜阴山菌 %d｜赤焰椒 %d｜月鳞鱼 %d｜玄羽禽 %d"),
			GameInstance->GetQuantity(LingGuId),
			GameInstance->GetQuantity(YinShanJunId),
			GameInstance->GetQuantity(ChiYanJiaoId),
			GameInstance->GetQuantity(YueLinYuId),
			GameInstance->GetQuantity(XuanYuQinId))));
	}
	if (OrderText)
	{
		FString OrderLine = TEXT("当前顾客：等待入座");
		FString CustomerButtonText = TEXT("顾客：等待");
		if (const ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
		{
			if (Director->HasActiveCustomer())
			{
				const FSCustomerState Customer = Director->GetActiveCustomer();
				const FString Name = Customer.DisplayName.IsEmpty() ? Customer.CustomerId : Customer.DisplayName;
				CustomerButtonText = FString::Printf(TEXT("交付给 %s"), *Name);
				OrderLine = FString::Printf(
					TEXT("当前顾客：%s（%s）｜订单：%s Lv%d｜售价：%d｜耐心：%.0f/%.0fs"),
					*Name,
					*Customer.CustomerId,
					*IngredientShortName(Customer.Order.IngredientId),
					Customer.Order.Level,
					Customer.Order.SellValue,
					Customer.PatienceRemaining,
					Customer.PatienceMax);
			}
			else
			{
				OrderLine = FString::Printf(TEXT("当前顾客：等待入座｜下一位约 %.1fs"), Director->GetSpawnCooldownRemaining());
			}
		}
		if (const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
		{
			TArray<FString> NpcParts;
			for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
			{
				NpcParts.Add(Npc.bServed
					? FString::Printf(TEXT("%s 已服务✓"), *Npc.DisplayName)
					: FString::Printf(
						TEXT("%s 要 %s Lv%d → %s"),
						*Npc.DisplayName,
						*IngredientShortName(Npc.Order.IngredientId),
						Npc.Order.Level,
						*USChefGameInstance::GetGiftDisplayName(Npc.GiftId)));
			}
			if (NpcParts.Num() > 0)
			{
				OrderLine += FString::Printf(TEXT("\n特殊委托：%s"), *FString::Join(NpcParts, TEXT("　｜　")));
			}
		}
		OrderText->SetText(FText::FromString(OrderLine));
		SetButtonText(CustomerButton, CustomerButtonText);
	}
	if (FeedbackText)
	{
		FeedbackText->SetText(FText::FromString(FString::Printf(TEXT("操作：%s"), *GameInstance->LastBoardFeedback)));
	}

	const bool bGiftSelect = GameInstance->Phase == ESGamePhase::GiftSelect;
	GuideKiteButton->SetVisibility(bGiftSelect ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	LifeLampButton->SetVisibility(bGiftSelect ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ConfirmNightButton->SetVisibility(bGiftSelect ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetButtonText(
		GuideKiteButton,
		GameInstance->PendingGiftIds.Contains(GiftGuideKiteId) ? TEXT("纸鸢 ✓") : TEXT("纸鸢"));
	SetButtonText(
		LifeLampButton,
		GameInstance->PendingGiftIds.Contains(GiftLifeLampId) ? TEXT("纸灯 ✓") : TEXT("纸灯"));
}

void USDayHUD::HandleCustomer()
{
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->TryDeliverSelectedPiece();
	}
}

void USDayHUD::DeliverNpc(const FName NpcId)
{
	if (ASSpecialNpcDirector* Director = ASSpecialNpcDirector::FindDirector(this))
	{
		Director->TryDeliverToNpc(NpcId);
	}
}

void USDayHUD::HandleALing() { DeliverNpc(NpcALingId); }
void USDayHUD::HandleSangPo() { DeliverNpc(NpcSangPoId); }

void USDayHUD::SpawnIngredient(const FName IngredientId)
{
	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->TrySpawnFromMotherPiece(IngredientId);
	}
}

void USDayHUD::HandleLingGu() { SpawnIngredient(LingGuId); }
void USDayHUD::HandleYinShanJun() { SpawnIngredient(YinShanJunId); }
void USDayHUD::HandleChiYanJiao() { SpawnIngredient(ChiYanJiaoId); }
void USDayHUD::HandleYueLinYu() { SpawnIngredient(YueLinYuId); }
void USDayHUD::HandleXuanYuQin() { SpawnIngredient(XuanYuQinId); }

void USDayHUD::ToggleGift(const FName GiftId)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->TogglePendingGiftSelection(GiftId);
	}
}

void USDayHUD::HandleGuideKite() { ToggleGift(GiftGuideKiteId); }
void USDayHUD::HandleLifeLamp() { ToggleGift(GiftLifeLampId); }

void USDayHUD::HandleConfirmNight()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->ConfirmGiftSelection();
	}
}

#pragma endregion K2 moonyfli
