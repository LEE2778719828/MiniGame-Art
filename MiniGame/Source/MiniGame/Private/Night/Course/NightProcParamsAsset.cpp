#include "Night/Course/NightProcParamsAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#pragma region K2 moonyfli
namespace NightProcJson_Private
{
	static int32 ReadInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32 Fallback)
	{
		double V = Fallback;
		if (Obj.IsValid() && Obj->TryGetNumberField(Key, V))
		{
			return static_cast<int32>(V);
		}
		return Fallback;
	}

	static float ReadFloat(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float Fallback)
	{
		double V = Fallback;
		if (Obj.IsValid() && Obj->TryGetNumberField(Key, V))
		{
			return static_cast<float>(V);
		}
		return Fallback;
	}

	static bool ReadBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool Fallback)
	{
		bool V = Fallback;
		if (Obj.IsValid() && Obj->TryGetBoolField(Key, V))
		{
			return V;
		}
		return Fallback;
	}

	static void WriteNumber(TSharedRef<FJsonObject> Obj, const TCHAR* Key, double V)
	{
		Obj->SetNumberField(Key, V);
	}

	static ENightForkEnv ParseForkEnv(const FString& S)
	{
		if (S.Equals(TEXT("FogAC"), ESearchCase::IgnoreCase)) return ENightForkEnv::FogAC;
		if (S.Equals(TEXT("ReverseBC"), ESearchCase::IgnoreCase)) return ENightForkEnv::ReverseBC;
		if (S.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) return ENightForkEnv::Custom;
		return ENightForkEnv::ClearAB;
	}

	static FString ForkEnvToString(ENightForkEnv E)
	{
		switch (E)
		{
		case ENightForkEnv::FogAC: return TEXT("FogAC");
		case ENightForkEnv::ReverseBC: return TEXT("ReverseBC");
		case ENightForkEnv::Custom: return TEXT("Custom");
		default: return TEXT("ClearAB");
		}
	}

	static ENightForkPair ParseForkPair(const FString& S)
	{
		if (S.Equals(TEXT("AC"), ESearchCase::IgnoreCase)) return ENightForkPair::AC;
		if (S.Equals(TEXT("BC"), ESearchCase::IgnoreCase)) return ENightForkPair::BC;
		return ENightForkPair::AB;
	}

	static FString ForkPairToString(ENightForkPair P)
	{
		switch (P)
		{
		case ENightForkPair::AC: return TEXT("AC");
		case ENightForkPair::BC: return TEXT("BC");
		default: return TEXT("AB");
		}
	}

	static FNightStoneSpec ReadStone(const TSharedPtr<FJsonObject>& Obj)
	{
		FNightStoneSpec S;
		S.TrackDistance = ReadFloat(Obj, TEXT("trackDistance"), 0.f);
		S.bUseWorldPose = ReadBool(Obj, TEXT("useWorldPose"), true);
		S.YawDeg = ReadFloat(Obj, TEXT("yawDeg"), 0.f);
		S.bHasFoe = ReadBool(Obj, TEXT("hasFoe"), false);
		S.DropCount = ReadInt(Obj, TEXT("dropCount"), 1);
		double X = 0, Y = 0, Z = 0;
		Obj->TryGetNumberField(TEXT("x"), X);
		Obj->TryGetNumberField(TEXT("y"), Y);
		Obj->TryGetNumberField(TEXT("z"), Z);
		S.WorldLocation = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
		return S;
	}

	static TSharedRef<FJsonObject> WriteStone(const FNightStoneSpec& S)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		WriteNumber(Obj, TEXT("trackDistance"), S.TrackDistance);
		Obj->SetBoolField(TEXT("useWorldPose"), S.bUseWorldPose);
		WriteNumber(Obj, TEXT("yawDeg"), S.YawDeg);
		Obj->SetBoolField(TEXT("hasFoe"), S.bHasFoe);
		WriteNumber(Obj, TEXT("dropCount"), S.DropCount);
		WriteNumber(Obj, TEXT("x"), S.WorldLocation.X);
		WriteNumber(Obj, TEXT("y"), S.WorldLocation.Y);
		WriteNumber(Obj, TEXT("z"), S.WorldLocation.Z);
		return Obj;
	}

	static FNightBeatSpec ReadBeat(const TSharedPtr<FJsonObject>& Obj)
	{
		FNightBeatSpec B;
		B.FromStoneIndex = ReadInt(Obj, TEXT("from"), 0);
		B.ToStoneIndex = ReadInt(Obj, TEXT("to"), 1);
		const FString Action = Obj->GetStringField(TEXT("action"));
		B.Action = Action.Equals(TEXT("Attack"), ESearchCase::IgnoreCase)
			? ENightNodeKind::Enemy
			: ENightNodeKind::Hazard;
		return B;
	}

	static TSharedRef<FJsonObject> WriteBeat(const FNightBeatSpec& B)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		WriteNumber(Obj, TEXT("from"), B.FromStoneIndex);
		WriteNumber(Obj, TEXT("to"), B.ToStoneIndex);
		Obj->SetStringField(TEXT("action"),
			B.Action == ENightNodeKind::Enemy ? TEXT("Attack") : TEXT("Jump"));
		return Obj;
	}

	static FNightBridgeSpec ReadBridge(const TSharedPtr<FJsonObject>& Obj)
	{
		FNightBridgeSpec B;
		B.FromStoneIndex = ReadInt(Obj, TEXT("from"), 0);
		B.ToStoneIndex = ReadInt(Obj, TEXT("to"), 1);
		B.MeshVariant = ReadInt(Obj, TEXT("meshVariant"), 0);
		B.YawDeg = ReadFloat(Obj, TEXT("yawDeg"), 0.f);
		B.LengthScale = ReadFloat(Obj, TEXT("lengthScale"), 1.f);
		double X = 0, Y = 0, Z = 0;
		Obj->TryGetNumberField(TEXT("x"), X);
		Obj->TryGetNumberField(TEXT("y"), Y);
		Obj->TryGetNumberField(TEXT("z"), Z);
		B.WorldLocation = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
		return B;
	}

	static TSharedRef<FJsonObject> WriteBridge(const FNightBridgeSpec& B)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		WriteNumber(Obj, TEXT("from"), B.FromStoneIndex);
		WriteNumber(Obj, TEXT("to"), B.ToStoneIndex);
		WriteNumber(Obj, TEXT("meshVariant"), B.MeshVariant);
		WriteNumber(Obj, TEXT("yawDeg"), B.YawDeg);
		WriteNumber(Obj, TEXT("lengthScale"), B.LengthScale);
		WriteNumber(Obj, TEXT("x"), B.WorldLocation.X);
		WriteNumber(Obj, TEXT("y"), B.WorldLocation.Y);
		WriteNumber(Obj, TEXT("z"), B.WorldLocation.Z);
		return Obj;
	}
}

bool UNightProcParamsAsset::ImportFromJsonString(const FString& JsonText, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Invalid JSON");
		return false;
	}

	using namespace NightProcJson_Private;
	FNightProcCourseParams& P = Params;
	P.TotalNodes = ReadInt(Root, TEXT("totalNodes"), P.TotalNodes);
	P.MaxYawDeltaDeg = ReadFloat(Root, TEXT("maxYawDeltaDeg"), P.MaxYawDeltaDeg);
	P.ForkNodeMin = ReadInt(Root, TEXT("forkNodeMin"), P.ForkNodeMin);
	P.ForkNodeMax = ReadInt(Root, TEXT("forkNodeMax"), P.ForkNodeMax);
	P.Seed = ReadInt(Root, TEXT("seed"), P.Seed);
	P.KeySwapEveryNNodes = ReadInt(Root, TEXT("keySwapEveryNNodes"), P.KeySwapEveryNNodes);
	P.KeySwapCountPerPeriod = ReadInt(Root, TEXT("keySwapCountPerPeriod"), P.KeySwapCountPerPeriod);
	P.JumpGapCm = ReadFloat(Root, TEXT("jumpGapCm"), P.JumpGapCm);
	P.KillGapCm = ReadFloat(Root, TEXT("killGapCm"), P.KillGapCm);
	P.BranchEntryGapCm = ReadFloat(Root, TEXT("branchEntryGapCm"), P.BranchEntryGapCm);
	P.AttackBias = ReadFloat(Root, TEXT("attackBias"), P.AttackBias);
	P.MaxSameActionStreak = ReadInt(Root, TEXT("maxSameActionStreak"), P.MaxSameActionStreak);
	P.BranchANodes = ReadInt(Root, TEXT("branchANodes"), P.BranchANodes);
	P.BranchBNodes = ReadInt(Root, TEXT("branchBNodes"), P.BranchBNodes);
	P.BranchCNodes = ReadInt(Root, TEXT("branchCNodes"), P.BranchCNodes);
	P.ForkTimeoutSeconds = ReadFloat(Root, TEXT("forkTimeoutSeconds"), P.ForkTimeoutSeconds);
	P.KeySwapWarningSeconds = ReadFloat(Root, TEXT("keySwapWarningSeconds"), P.KeySwapWarningSeconds);
	P.KeySwapSafetySeconds = ReadFloat(Root, TEXT("keySwapSafetySeconds"), P.KeySwapSafetySeconds);
	P.BridgeMeshAWeight = ReadFloat(Root, TEXT("bridgeMeshAWeight"), P.BridgeMeshAWeight);
	P.StartingSoul = ReadFloat(Root, TEXT("startingSoul"), P.StartingSoul);
	P.WrongPenalty = ReadFloat(Root, TEXT("wrongPenalty"), P.WrongPenalty);
	P.bEnableProcGenerator = ReadBool(Root, TEXT("enableProcGenerator"), P.bEnableProcGenerator);
	P.bPreviewOnly = ReadBool(Root, TEXT("previewOnly"), P.bPreviewOnly);

	FString EnvStr;
	if (Root->TryGetStringField(TEXT("forkEnv"), EnvStr))
	{
		P.ForkEnv = ParseForkEnv(EnvStr);
	}
	FString PairStr;
	if (Root->TryGetStringField(TEXT("forkPair"), PairStr))
	{
		P.ForkPair = ParseForkPair(PairStr);
	}

	BakedStones.Reset();
	BakedBeats.Reset();
	BakedBridges.Reset();
	bPreferBakedCourse = false;

	const TArray<TSharedPtr<FJsonValue>>* StonesArr = nullptr;
	if (Root->TryGetArrayField(TEXT("stones"), StonesArr) && StonesArr)
	{
		bPreferBakedCourse = true;
		for (const TSharedPtr<FJsonValue>& V : *StonesArr)
		{
			if (V.IsValid() && V->Type == EJson::Object)
			{
				BakedStones.Add(ReadStone(V->AsObject()));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* BeatsArr = nullptr;
	if (Root->TryGetArrayField(TEXT("beats"), BeatsArr) && BeatsArr)
	{
		for (const TSharedPtr<FJsonValue>& V : *BeatsArr)
		{
			if (V.IsValid() && V->Type == EJson::Object)
			{
				BakedBeats.Add(ReadBeat(V->AsObject()));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* BridgesArr = nullptr;
	if (Root->TryGetArrayField(TEXT("bridges"), BridgesArr) && BridgesArr)
	{
		for (const TSharedPtr<FJsonValue>& V : *BridgesArr)
		{
			if (V.IsValid() && V->Type == EJson::Object)
			{
				BakedBridges.Add(ReadBridge(V->AsObject()));
			}
		}
	}

	OutError.Reset();
	return true;
}

bool UNightProcParamsAsset::ImportFromJsonFile(const FString& AbsoluteOrProjectPath, FString& OutError)
{
	FString Path = AbsoluteOrProjectPath;
	if (FPaths::IsRelative(Path))
	{
		Path = FPaths::Combine(FPaths::ProjectDir(), Path);
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("Failed to read %s"), *Path);
		return false;
	}
	return ImportFromJsonString(Text, OutError);
}

FString UNightProcParamsAsset::ExportToJsonString(bool bIncludeBaked) const
{
	using namespace NightProcJson_Private;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const FNightProcCourseParams& P = Params;
	WriteNumber(Root, TEXT("totalNodes"), P.TotalNodes);
	WriteNumber(Root, TEXT("maxYawDeltaDeg"), P.MaxYawDeltaDeg);
	WriteNumber(Root, TEXT("forkNodeMin"), P.ForkNodeMin);
	WriteNumber(Root, TEXT("forkNodeMax"), P.ForkNodeMax);
	Root->SetStringField(TEXT("forkEnv"), ForkEnvToString(P.ForkEnv));
	Root->SetStringField(TEXT("forkPair"), ForkPairToString(P.ForkPair));
	WriteNumber(Root, TEXT("seed"), P.Seed);
	WriteNumber(Root, TEXT("keySwapEveryNNodes"), P.KeySwapEveryNNodes);
	WriteNumber(Root, TEXT("keySwapCountPerPeriod"), P.KeySwapCountPerPeriod);
	WriteNumber(Root, TEXT("jumpGapCm"), P.JumpGapCm);
	WriteNumber(Root, TEXT("killGapCm"), P.KillGapCm);
	WriteNumber(Root, TEXT("branchEntryGapCm"), P.BranchEntryGapCm);
	WriteNumber(Root, TEXT("attackBias"), P.AttackBias);
	WriteNumber(Root, TEXT("maxSameActionStreak"), P.MaxSameActionStreak);
	WriteNumber(Root, TEXT("branchANodes"), P.BranchANodes);
	WriteNumber(Root, TEXT("branchBNodes"), P.BranchBNodes);
	WriteNumber(Root, TEXT("branchCNodes"), P.BranchCNodes);
	WriteNumber(Root, TEXT("forkTimeoutSeconds"), P.ForkTimeoutSeconds);
	WriteNumber(Root, TEXT("keySwapWarningSeconds"), P.KeySwapWarningSeconds);
	WriteNumber(Root, TEXT("keySwapSafetySeconds"), P.KeySwapSafetySeconds);
	WriteNumber(Root, TEXT("bridgeMeshAWeight"), P.BridgeMeshAWeight);
	WriteNumber(Root, TEXT("startingSoul"), P.StartingSoul);
	WriteNumber(Root, TEXT("wrongPenalty"), P.WrongPenalty);
	Root->SetBoolField(TEXT("enableProcGenerator"), P.bEnableProcGenerator);
	Root->SetBoolField(TEXT("previewOnly"), P.bPreviewOnly);

	if (bIncludeBaked)
	{
		TArray<TSharedPtr<FJsonValue>> Stones;
		for (const FNightStoneSpec& S : BakedStones)
		{
			Stones.Add(MakeShared<FJsonValueObject>(WriteStone(S)));
		}
		Root->SetArrayField(TEXT("stones"), Stones);

		TArray<TSharedPtr<FJsonValue>> Beats;
		for (const FNightBeatSpec& B : BakedBeats)
		{
			Beats.Add(MakeShared<FJsonValueObject>(WriteBeat(B)));
		}
		Root->SetArrayField(TEXT("beats"), Beats);

		TArray<TSharedPtr<FJsonValue>> Bridges;
		for (const FNightBridgeSpec& B : BakedBridges)
		{
			Bridges.Add(MakeShared<FJsonValueObject>(WriteBridge(B)));
		}
		Root->SetArrayField(TEXT("bridges"), Bridges);
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

UNightProcParamsAsset* UNightProcParamsAsset::CreateTransientFromJson(const FString& JsonText, FString& OutError)
{
	UNightProcParamsAsset* Asset = NewObject<UNightProcParamsAsset>();
	if (!Asset->ImportFromJsonString(JsonText, OutError))
	{
		return nullptr;
	}
	return Asset;
}
#pragma endregion K2 moonyfli
