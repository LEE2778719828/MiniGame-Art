#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "NightFoeShatterComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;
class UNiagaraSystem;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

#pragma region K2 moonyfli
USTRUCT(BlueprintType)
struct FNightFoeShatterTierSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "块数", ToolTip = "从原怪网格不同位置切出的小块数。", ClampMin = "10", ClampMax = "15"))
	int32 Count = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "存活秒", ClampMin = "0.15"))
	float LifetimeSec = 0.7f;
};

USTRUCT(BlueprintType)
struct FNightFoeShatterRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	FVector Extent = FVector(20.f, 20.f, 30.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	FVector ImpulseDir = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	TObjectPtr<UMeshComponent> SourceMeshComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	TObjectPtr<UStaticMesh> SourceStaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	int32 Combo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	int32 VFXTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter")
	bool bTier5 = false;
};

/**
 * Shatter by sampling different regions of the foe static mesh.
 * Each piece is a local triangle cluster, not a scaled copy of the whole mesh.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightFoeShatterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UNightFoeShatterComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "启用碎裂"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "回退碎片网格", ToolTip = "原怪没有静态网格时才用。"))
	TObjectPtr<UStaticMesh> FallbackShardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "附加Niagara"))
	TObjectPtr<UNiagaraSystem> ExtraBurstFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "一二三四档"))
	TArray<FNightFoeShatterTierSettings> Tiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "五档(40+)"))
	FNightFoeShatterTierSettings Tier5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "力度满连击", ToolTip = "连击到此值时打击力度到顶，和刀光五档对齐。", ClampMin = "1"))
	int32 ForceFullAtCombo = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "最低力度", ClampMin = "0.1"))
	float ForceMin = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "最高力度", ToolTip = "40 连相对 1 连的冲量倍数。超线性，高连击是重斩。", ClampMin = "1.0"))
	float ForceMax = 3.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "力度指数", ToolTip = "大于 1 时后期涨得更快。", ClampMin = "0.2"))
	float ForceExponent = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "斩向速度", ToolTip = "沿刀刃穿过身体的基准速度，再乘力度。", ClampMin = "0.0"))
	float SlashSpeedCm = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "径向速度", ToolTip = "从身体中心裂开的速度，模拟切开后的分离。", ClampMin = "0.0"))
	float RadialSpeedCm = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "上抛速度", ClampMin = "0.0"))
	float UpSpeedCm = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "重力", ClampMin = "0.0"))
	float GravityCm = 2400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "空气阻力", ToolTip = "出手后减速，避免高连击飞出屏幕。", ClampMin = "0.0"))
	float AirDrag = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "低连击散射", ClampMin = "1.0", ClampMax = "40.0"))
	float JitterConeMinDeg = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "高连击散射", ToolTip = "高连击更吃刀向，锥角收窄。", ClampMin = "1.0", ClampMax = "40.0"))
	float JitterConeMaxDeg = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|VFX|Shatter", meta = (DisplayName = "初始姿态抖动", ClampMin = "0.0", ClampMax = "45.0"))
	float PoseJitterDeg = 14.f;

	UFUNCTION(BlueprintCallable, Category = "Night|VFX|Shatter")
	bool PlayBurst(const FNightFoeShatterRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Night|VFX|Shatter")
	void ClearShards();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	const FNightFoeShatterTierSettings& ResolveTier(const FNightFoeShatterRequest& Request) const;
	float ComputeStrikeForce(int32 Combo) const;
	UStaticMesh* ResolveVisualMesh(const FNightFoeShatterRequest& Request) const;
	void GatherSamplePositions(const FNightFoeShatterRequest& Request, int32 Wanted, TArray<FVector>& OutPositions) const;
	void SpawnExtraNiagara(const FNightFoeShatterRequest& Request, const FNightFoeShatterTierSettings& Settings, float Force) const;

	struct FShardState
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Velocity = FVector::ZeroVector;
		FRotator AngularDeg = FRotator::ZeroRotator;
		FVector BaseScale = FVector::OneVector;
		float Age = 0.f;
		float Lifetime = 0.7f;
	};

	TArray<FShardState> LiveShards;
};
#pragma endregion K2 moonyfli
