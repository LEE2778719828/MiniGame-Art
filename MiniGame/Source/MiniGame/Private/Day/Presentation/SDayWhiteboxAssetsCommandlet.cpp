#include "Day/Presentation/SDayWhiteboxAssetsCommandlet.h"

#if WITH_EDITOR

#include "Day/Presentation/SDayBoardPresentation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Font.h"
#include "Engine/FontImportOptions.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/TrueTypeFontFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
	UObject* LoadAssetByPackage(const FString& PackagePath, const FString& AssetName)
	{
		return LoadObject<UObject>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName));
	}

	bool SaveAsset(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString PackageName = Package->GetName();
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackageName,
			FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.Error = GError;
		return UPackage::SavePackage(Package, Asset, *FilePath, Args);
	}

	UBlueprint* CreateActorBlueprint(
		const FString& PackagePath,
		const FString& AssetName,
		UClass* ParentClass)
	{
		if (UBlueprint* Existing = Cast<UBlueprint>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			*AssetName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			FName(TEXT("SDayWhiteboxAssets")));
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			SaveAsset(Blueprint);
		}
		return Blueprint;
	}

	UWidgetBlueprint* CreateDayHudBlueprint()
	{
		const FString PackagePath(TEXT("/Game/Day/UI/WBP_SDayHUD"));
		const FString AssetName(TEXT("WBP_SDayHUD"));
		if (UWidgetBlueprint* Existing = Cast<UWidgetBlueprint>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = USDayHUD::StaticClass();
		UObject* Asset = FAssetToolsModule::GetModule().Get().CreateAsset(
			AssetName,
			TEXT("/Game/Day/UI"),
			UWidgetBlueprint::StaticClass(),
			Factory);
		UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset);
		if (WidgetBlueprint)
		{
			FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
			SaveAsset(WidgetBlueprint);
		}
		return WidgetBlueprint;
	}

	/** Creates the unlit masked material used by plated-food textures. */
	UMaterial* CreateDishIconMaterial()
	{
		const FString PackagePath(TEXT("/Game/Day/Materials/M_SDayDishIcon"));
		const FString AssetName(TEXT("M_SDayDishIcon"));
		if (UMaterial* Existing = Cast<UMaterial>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Material = NewObject<UMaterial>(
			Package,
			*AssetName,
			RF_Public | RF_Standalone);
		if (!Material)
		{
			return nullptr;
		}

		UMaterialExpressionTextureSampleParameter2D* Sample =
			NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
		Sample->ParameterName = TEXT("Tex");
		Sample->SamplerType = SAMPLERTYPE_Color;
		Sample->Texture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/Day/Art/food/food_rice_V0.food_rice_V0"));
		Sample->MaterialExpressionEditorX = -400;
		Material->GetExpressionCollection().AddExpression(Sample);

		UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
		Data->EmissiveColor.Connect(0, Sample);
		Data->OpacityMask.Connect(4, Sample);
		Material->BlendMode = BLEND_Masked;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		Material->PostEditChange();

		FAssetRegistryModule::AssetCreated(Material);
		SaveAsset(Material);
		return Material;
	}

	UMaterialInstanceConstant* CreatePlaceholderMaterial(
		const FString& AssetName,
		const FLinearColor& Color)
	{
		const FString PackagePath = FString::Printf(TEXT("/Game/Day/Materials/%s"), *AssetName);
		if (UMaterialInstanceConstant* Existing =
			Cast<UMaterialInstanceConstant>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UObject* Asset = FAssetToolsModule::GetModule().Get().CreateAsset(
			AssetName,
			TEXT("/Game/Day/Materials"),
			UMaterialInstanceConstant::StaticClass(),
			Factory);
		UMaterialInstanceConstant* Material = Cast<UMaterialInstanceConstant>(Asset);
		if (Material)
		{
			Material->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), Color);
			Material->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Tint")), Color);
			SaveAsset(Material);
		}
		return Material;
	}

	/**
	 * TextRenderComponent only accepts offline (texture) fonts, and the engine default ships
	 * ASCII only, so every Chinese glyph the day board can print has to be baked in here.
	 * Add new characters to this list whenever a world-space label gains new wording.
	 */
	const TCHAR* DayLabelChineseCharacters =
		TEXT("灵谷阴山菌赤焰椒月鳞鱼玄羽禽")
		TEXT("小满阿桃青禾石榴团霜叶")
		TEXT("空座等待顾客下一位未到店已服务")
		TEXT("翎桑婆厨师")
		TEXT("引路纸鸢借命灯定键铜钱饕餮食盒")
		TEXT("▲✓→｜·");

	/**
	 * The importer only bakes an ASCII glyph when it appears in Chars, whatever
	 * bIncludeASCIIRange says, so levels and countdowns need the range spelled out.
	 */
	FString BuildLabelCharacterSet()
	{
		FString Chars;
		for (TCHAR Char = TEXT(' '); Char <= TEXT('~'); ++Char)
		{
			Chars.AppendChar(Char);
		}
		Chars.Append(DayLabelChineseCharacters);
		return Chars;
	}

	UFont* CreateLabelFont()
	{
		const FString PackagePath(TEXT("/Game/Day/UI/F_SDayLabel"));
		const FString AssetName(TEXT("F_SDayLabel"));
		if (UFont* Existing = Cast<UFont>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UTrueTypeFontFactory* Factory = NewObject<UTrueTypeFontFactory>();
		Factory->AddToRoot();
		FFontImportOptionsData& Options = Factory->ImportOptions->Data;
		Options.FontName = TEXT("Microsoft YaHei");
		Options.Height = 32.0f;
		Options.CharacterSet = FontICS_Default;
		Options.Chars = BuildLabelCharacterSet();
		Options.bIncludeASCIIRange = true;
		// DefaultTextMaterialOpaque masks on the red channel, but a plain bitmap import puts
		// the glyph coverage in alpha and leaves red white, which renders as solid blocks.
		// Distance field import is the same path the engine's own TextRender font uses.
		Options.bUseDistanceFieldAlpha = true;
		Options.DistanceFieldScaleFactor = 8;
		Options.DistanceFieldScanRadiusScale = 1.0f;
		Options.TexturePageWidth = 512;
		Options.TexturePageMaxHeight = 512;

		UPackage* Package = CreatePackage(*PackagePath);
		UFont* Font = Cast<UFont>(Factory->FactoryCreateNew(
			UFont::StaticClass(),
			Package,
			*AssetName,
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));
		Factory->RemoveFromRoot();

		if (Font)
		{
			FAssetRegistryModule::AssetCreated(Font);
			SaveAsset(Font);
		}
		return Font;
	}

	UDataTable* CreateLayoutTable()
	{
		const FString PackagePath(TEXT("/Game/Day/Data/DT_SDayBoardLayout"));
		const FString AssetName(TEXT("DT_SDayBoardLayout"));
		if (UDataTable* Existing = Cast<UDataTable>(LoadAssetByPackage(PackagePath, AssetName)))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UDataTable* Table = NewObject<UDataTable>(
			Package,
			*AssetName,
			RF_Public | RF_Standalone);
		Table->RowStruct = FSDayBoardLayoutRow::StaticStruct();

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
			Row.Transform = FTransform(Locations[Index]);
			Row.VisualRadius = Index % 3 == 0 ? 92.0f : 78.0f;
			Table->AddRow(*FString::Printf(TEXT("Cell_%02d"), CellIndices[Index]), Row);
		}
		FAssetRegistryModule::AssetCreated(Table);
		SaveAsset(Table);
		return Table;
	}

	UDataTable* CreateDishIconTuneTable()
	{
		const FString PackagePath(TEXT("/Game/Day/Data/DT_SDayDishIconTune"));
		const FString AssetName(TEXT("DT_SDayDishIconTune"));
		UDataTable* Table = Cast<UDataTable>(LoadAssetByPackage(PackagePath, AssetName));
		if (!Table)
		{
			UPackage* Package = CreatePackage(*PackagePath);
			Table = NewObject<UDataTable>(
				Package,
				*AssetName,
				RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Table);
		}

		Table->RowStruct = FSDayDishIconTuneRow::StaticStruct();
		FSDayDishIconTuneRow Row;
		Table->AddRow(TEXT("Default"), Row);
		SaveAsset(Table);
		return Table;
	}

	USDayBoardVisualConfig* CreateVisualConfig(
		UBlueprint* PresenterBlueprint,
		UBlueprint* CellBlueprint,
		UBlueprint* BinBlueprint,
		UBlueprint* CharacterBlueprint,
		UDataTable* LayoutTable,
		UDataTable* DishIconTuneTable,
		UMaterialInterface* BoardMaterial,
		UMaterialInterface* CellMaterial,
		UMaterialInterface* PieceMaterial,
		UFont* LabelFont)
	{
		const FString PackagePath(TEXT("/Game/Day/Data/DA_SDayBoardVisualConfig"));
		const FString AssetName(TEXT("DA_SDayBoardVisualConfig"));
		USDayBoardVisualConfig* Config =
			Cast<USDayBoardVisualConfig>(LoadAssetByPackage(PackagePath, AssetName));
		if (!Config)
		{
			UPackage* Package = CreatePackage(*PackagePath);
			Config = NewObject<USDayBoardVisualConfig>(
				Package,
				*AssetName,
				RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Config);
		}

		Config->BoardFrameMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Config->CellMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		Config->PieceMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		Config->IngredientBinMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Config->ChefMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		Config->CustomerMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		Config->NpcMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		Config->BoardMaterial = BoardMaterial;
		Config->CellMaterial = CellMaterial;
		Config->PieceMaterial = PieceMaterial;
		Config->CellLayout = LayoutTable;
		Config->DishIconTune = DishIconTuneTable;
		Config->LabelFont = LabelFont;
		Config->CellVisualClass = ASDayCellVisual::StaticClass();
		Config->IngredientBinClass = ASDayIngredientBinVisual::StaticClass();
		Config->CharacterStandInClass = ASDayCharacterStandIn::StaticClass();
		if (CellBlueprint && CellBlueprint->GeneratedClass)
		{
			Config->CellVisualClass = CellBlueprint->GeneratedClass;
		}
		if (BinBlueprint && BinBlueprint->GeneratedClass)
		{
			Config->IngredientBinClass = BinBlueprint->GeneratedClass;
		}
		if (CharacterBlueprint && CharacterBlueprint->GeneratedClass)
		{
			Config->CharacterStandInClass = CharacterBlueprint->GeneratedClass;
		}
		Config->IngredientBinOutputs.Reset();
		const TArray<FName> DefaultIngredientIds = {
			TEXT("LingGu"),
			TEXT("YinShanJun"),
			TEXT("ChiYanJiao"),
			TEXT("YueLinYu"),
			TEXT("XuanYuQin")
		};
		for (int32 BinIndex = 0; BinIndex < DefaultIngredientIds.Num(); ++BinIndex)
		{
			FSDayIngredientBinOutput Output;
			Output.BinIndex = BinIndex;
			Output.IngredientId = DefaultIngredientIds[BinIndex];
			Config->IngredientBinOutputs.Add(Output);
		}
		Config->DishArtStemByIngredient.Reset();
		Config->DishArtStemByIngredient.Add(TEXT("LingGu"), TEXT("rice"));
		Config->DishArtStemByIngredient.Add(TEXT("YinShanJun"), TEXT("egg"));
		Config->DishArtStemByIngredient.Add(TEXT("ChiYanJiao"), TEXT("hand"));
		Config->DishArtStemByIngredient.Add(TEXT("YueLinYu"), TEXT("fish"));
		Config->DishArtStemByIngredient.Add(TEXT("XuanYuQin"), TEXT("leg"));
		Config->DishIconMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		Config->DishIconMaterial = CreateDishIconMaterial();
		SaveAsset(Config);
		return Config;
	}
}

#endif

#pragma region K2 moonyfli

USDayWhiteboxAssetsCommandlet::USDayWhiteboxAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 USDayWhiteboxAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	if (Params.Contains(TEXT("DishIconOnly")))
	{
		UMaterial* DishIcon = CreateDishIconMaterial();
		UE_LOG(LogTemp, Display, TEXT("S Day dish icon material: %s"), DishIcon ? TEXT("ready") : TEXT("failed"));
		return DishIcon ? 0 : 1;
	}
	if (Params.Contains(TEXT("DishIconTuneOnly")))
	{
		UDataTable* Tune = CreateDishIconTuneTable();
		if (USDayBoardVisualConfig* Config = Cast<USDayBoardVisualConfig>(
			LoadAssetByPackage(TEXT("/Game/Day/Data/DA_SDayBoardVisualConfig"), TEXT("DA_SDayBoardVisualConfig"))))
		{
			Config->DishIconTune = Tune;
			SaveAsset(Config);
		}
		UE_LOG(LogTemp, Display, TEXT("S Day dish icon tune table: %s"), Tune ? TEXT("ready") : TEXT("failed"));
		return Tune ? 0 : 1;
	}

	UBlueprint* Presenter = CreateActorBlueprint(
		TEXT("/Game/Day/Board/BP_SDayBoardPresenter"),
		TEXT("BP_SDayBoardPresenter"),
		ASDayBoardPresenter::StaticClass());
	UBlueprint* Cell = CreateActorBlueprint(
		TEXT("/Game/Day/Board/BP_SDayCellVisual"),
		TEXT("BP_SDayCellVisual"),
		ASDayCellVisual::StaticClass());
	UBlueprint* Bin = CreateActorBlueprint(
		TEXT("/Game/Day/Board/BP_SDayIngredientBinVisual"),
		TEXT("BP_SDayIngredientBinVisual"),
		ASDayIngredientBinVisual::StaticClass());
	UBlueprint* Character = CreateActorBlueprint(
		TEXT("/Game/Day/Board/BP_SDayCharacterStandIn"),
		TEXT("BP_SDayCharacterStandIn"),
		ASDayCharacterStandIn::StaticClass());
	UWidgetBlueprint* Hud = CreateDayHudBlueprint();

	UMaterialInstanceConstant* BoardMaterial =
		CreatePlaceholderMaterial(TEXT("MI_SDayBoard_Placeholder"), FLinearColor(0.43f, 0.39f, 0.31f));
	UMaterialInstanceConstant* CellMaterial =
		CreatePlaceholderMaterial(TEXT("MI_SDayCell_Placeholder"), FLinearColor(0.025f, 0.025f, 0.03f));
	UMaterialInstanceConstant* PieceMaterial =
		CreatePlaceholderMaterial(TEXT("MI_SDayPiece_Placeholder"), FLinearColor(0.95f, 0.18f, 0.30f));
	UDataTable* Layout = CreateLayoutTable();
	UDataTable* DishIconTune = CreateDishIconTuneTable();
	UFont* LabelFont = CreateLabelFont();
	USDayBoardVisualConfig* Config = CreateVisualConfig(
		Presenter,
		Cell,
		Bin,
		Character,
		Layout,
		DishIconTune,
		BoardMaterial,
		CellMaterial,
		PieceMaterial,
		LabelFont);

	const bool bOk = Presenter && Cell && Bin && Character && Hud && Layout && DishIconTune && Config && LabelFont;
	UE_LOG(LogTemp, Display, TEXT("S Day whitebox assets: %s"), bOk ? TEXT("created") : TEXT("failed"));
	return bOk ? 0 : 1;
#else
	(void)Params;
	return 1;
#endif
}

#pragma endregion K2 moonyfli
