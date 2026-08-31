#include "Night/Course/NightFoeShatterComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConvexElem.h"
#include "ProceduralMeshComponent.h"
#include "RawIndexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "StaticMeshResources.h"
#include "UObject/ConstructorHelpers.h"

#pragma region K2 moonyfli
namespace
{
	struct FLocalChunk
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		FVector LocalCentroid = FVector::ZeroVector;
		int32 MaterialIndex = 0;
	};

	void ConfigureShardComp(UMeshComponent* Comp, USceneComponent* Parent)
	{
		Comp->SetupAttachment(Parent);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCastShadow(false);
		Comp->bCastDynamicShadow = false;
		Comp->SetReceivesDecals(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetMobility(EComponentMobility::Movable);
	}

	void AppendBoxSamples(const FVector& Origin, const FVector& Extent, TArray<FVector>& Out)
	{
		const FVector Safe(FMath::Max(8.f, Extent.X), FMath::Max(8.f, Extent.Y), FMath::Max(10.f, Extent.Z));
		Out.Add(Origin);
		for (int32 X = -1; X <= 1; X += 2)
		{
			for (int32 Y = -1; Y <= 1; Y += 2)
			{
				for (int32 Z = -1; Z <= 1; Z += 2)
				{
					Out.Add(Origin + FVector(Safe.X * X * 0.55f, Safe.Y * Y * 0.55f, Safe.Z * Z * 0.45f));
				}
			}
		}
	}

	void AppendStaticMeshSamples(UStaticMesh* Mesh, const FTransform& LocalToWorld, TArray<FVector>& Out)
	{
		if (!Mesh)
		{
			return;
		}

		if (const FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
		{
			if (RenderData->LODResources.IsValidIndex(0))
			{
				const FPositionVertexBuffer& VB =
					RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
				const int32 Num = static_cast<int32>(VB.GetNumVertices());
				if (Num > 0)
				{
					const int32 Take = FMath::Min(Num, 80);
					for (int32 i = 0; i < Take; ++i)
					{
						const int32 Index = (Take == Num) ? i : FMath::RandHelper(Num);
						Out.Add(LocalToWorld.TransformPosition(FVector(VB.VertexPosition(Index))));
					}
					return;
				}
			}
		}

		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			for (const FKConvexElem& Convex : Body->AggGeom.ConvexElems)
			{
				for (const FVector& Vertex : Convex.VertexData)
				{
					Out.Add(LocalToWorld.TransformPosition(Vertex));
				}
			}
		}
	}

	void AppendBoneSamples(USkeletalMeshComponent* Skeletal, TArray<FVector>& Out)
	{
		if (!Skeletal || !Skeletal->GetSkeletalMeshAsset())
		{
			return;
		}

		const int32 BoneCount = Skeletal->GetNumBones();
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			const FName BoneName = Skeletal->GetBoneName(BoneIndex);
			if (BoneName.IsNone())
			{
				continue;
			}
			Out.Add(Skeletal->GetBoneLocation(BoneName));
		}
	}

	void PickSpreadPositions(
		const TArray<FVector>& Candidates,
		const FVector& Origin,
		int32 Count,
		TArray<FVector>& Out)
	{
		Out.Reset();
		if (Candidates.Num() == 0)
		{
			Out.Add(Origin);
			return;
		}

		TArray<FVector> Pool = Candidates;
		if (Pool.Num() > 96)
		{
			for (int32 i = 0; i < 96; ++i)
			{
				const int32 SwapWith = i + FMath::RandHelper(Pool.Num() - i);
				Pool.Swap(i, SwapWith);
			}
			Pool.SetNum(96, EAllowShrinking::No);
		}

		int32 First = 0;
		float Best = -1.f;
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			const float DistSq = FVector::DistSquared(Pool[i], Origin);
			if (DistSq > Best)
			{
				Best = DistSq;
				First = i;
			}
		}
		Out.Add(Pool[First]);

		while (Out.Num() < Count && Out.Num() < Pool.Num())
		{
			int32 Next = INDEX_NONE;
			float NextBest = -1.f;
			for (int32 i = 0; i < Pool.Num(); ++i)
			{
				float MinDist = TNumericLimits<float>::Max();
				for (const FVector& Chosen : Out)
				{
					MinDist = FMath::Min(MinDist, FVector::DistSquared(Pool[i], Chosen));
				}
				if (MinDist > NextBest)
				{
					NextBest = MinDist;
					Next = i;
				}
			}
			if (Next == INDEX_NONE)
			{
				break;
			}
			Out.Add(Pool[Next]);
		}
	}

	bool ExtractLocalChunks(UStaticMesh* Mesh, int32 Wanted, TArray<FLocalChunk>& OutChunks)
	{
		OutChunks.Reset();
		if (!Mesh)
		{
			return false;
		}

		const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
		if (!RenderData || !RenderData->LODResources.IsValidIndex(0))
		{
			return false;
		}

		const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
		const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& Verts = LOD.VertexBuffers.StaticMeshVertexBuffer;
		const int32 VertexCount = static_cast<int32>(Positions.GetNumVertices());
		if (VertexCount < 3)
		{
			return false;
		}

		TArray<uint32> Indices;
		LOD.IndexBuffer.GetCopy(Indices);
		if (Indices.Num() < 3)
		{
			const FIndexArrayView View = LOD.IndexBuffer.GetArrayView();
			Indices.Reserve(View.Num());
			for (int32 i = 0; i < View.Num(); ++i)
			{
				Indices.Add(View[i]);
			}
		}
		if (Indices.Num() < 3)
		{
			return false;
		}

		TArray<FVector> LocalPositions;
		LocalPositions.SetNum(VertexCount);
		for (int32 i = 0; i < VertexCount; ++i)
		{
			LocalPositions[i] = FVector(Positions.VertexPosition(i));
		}

		const int32 SeedCount = FMath::Clamp(Wanted, 10, 15);
		TArray<FVector> Seeds;
		PickSpreadPositions(LocalPositions, Mesh->GetBounds().Origin, SeedCount, Seeds);
		if (Seeds.Num() == 0)
		{
			return false;
		}

		struct FTri
		{
			int32 I0 = 0;
			int32 I1 = 0;
			int32 I2 = 0;
			int32 MaterialIndex = 0;
			FVector Centroid = FVector::ZeroVector;
		};

		TArray<FTri> Tris;
		auto AddTri = [&](int32 I0, int32 I1, int32 I2, int32 MaterialIndex)
		{
			if (!LocalPositions.IsValidIndex(I0)
				|| !LocalPositions.IsValidIndex(I1)
				|| !LocalPositions.IsValidIndex(I2))
			{
				return;
			}
			FTri Tri;
			Tri.I0 = I0;
			Tri.I1 = I1;
			Tri.I2 = I2;
			Tri.MaterialIndex = MaterialIndex;
			Tri.Centroid = (LocalPositions[I0] + LocalPositions[I1] + LocalPositions[I2]) / 3.f;
			Tris.Add(Tri);
		};

		if (LOD.Sections.Num() > 0)
		{
			for (const FStaticMeshSection& Section : LOD.Sections)
			{
				const int32 First = static_cast<int32>(Section.FirstIndex);
				const int32 TriCount = static_cast<int32>(Section.NumTriangles);
				for (int32 TriIndex = 0; TriIndex < TriCount; ++TriIndex)
				{
					const int32 Base = First + TriIndex * 3;
					if (Indices.IsValidIndex(Base + 2))
					{
						AddTri(Indices[Base], Indices[Base + 1], Indices[Base + 2], Section.MaterialIndex);
					}
				}
			}
		}
		else
		{
			for (int32 Base = 0; Base + 2 < Indices.Num(); Base += 3)
			{
				AddTri(Indices[Base], Indices[Base + 1], Indices[Base + 2], 0);
			}
		}
		if (Tris.Num() == 0)
		{
			return false;
		}

		TArray<TArray<int32>> ClusterTris;
		ClusterTris.SetNum(Seeds.Num());
		for (int32 TriIndex = 0; TriIndex < Tris.Num(); ++TriIndex)
		{
			int32 BestSeed = 0;
			float BestDist = TNumericLimits<float>::Max();
			for (int32 SeedIndex = 0; SeedIndex < Seeds.Num(); ++SeedIndex)
			{
				const float Dist = FVector::DistSquared(Tris[TriIndex].Centroid, Seeds[SeedIndex]);
				if (Dist < BestDist)
				{
					BestDist = Dist;
					BestSeed = SeedIndex;
				}
			}
			ClusterTris[BestSeed].Add(TriIndex);
		}

		const bool bHasUV = Verts.GetNumTexCoords() > 0;
		OutChunks.Reserve(Seeds.Num());
		for (int32 SeedIndex = 0; SeedIndex < Seeds.Num(); ++SeedIndex)
		{
			const TArray<int32>& Members = ClusterTris[SeedIndex];
			if (Members.Num() == 0)
			{
				continue;
			}

			FLocalChunk Chunk;
			TMap<int32, int32> Remap;
			TArray<int32> MaterialVotes;
			FVector Accum = FVector::ZeroVector;
			int32 AccumCount = 0;
			for (const int32 TriIndex : Members)
			{
				const FTri& Tri = Tris[TriIndex];
				const int32 Src[3] = {Tri.I0, Tri.I1, Tri.I2};
				if (MaterialVotes.IsValidIndex(Tri.MaterialIndex))
				{
					++MaterialVotes[Tri.MaterialIndex];
				}
				else
				{
					MaterialVotes.SetNumZeroed(Tri.MaterialIndex + 1);
					MaterialVotes[Tri.MaterialIndex] = 1;
				}
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					if (int32* Found = Remap.Find(Src[Corner]))
					{
						Chunk.Triangles.Add(*Found);
						continue;
					}
					const int32 NewIndex = Chunk.Vertices.Num();
					Remap.Add(Src[Corner], NewIndex);
					Chunk.Vertices.Add(LocalPositions[Src[Corner]]);
					Chunk.Normals.Add(FVector(Verts.VertexTangentZ(Src[Corner])));
					Chunk.UVs.Add(bHasUV
						? FVector2D(Verts.GetVertexUV(Src[Corner], 0))
						: FVector2D::ZeroVector);
					Chunk.Tangents.Add(FProcMeshTangent(FVector(Verts.VertexTangentX(Src[Corner])), false));
					Chunk.Triangles.Add(NewIndex);
					Accum += LocalPositions[Src[Corner]];
					++AccumCount;
				}
			}
			if (Chunk.Triangles.Num() < 3 || Chunk.Vertices.Num() < 3)
			{
				continue;
			}

			Chunk.LocalCentroid = AccumCount > 0 ? (Accum / AccumCount) : Seeds[SeedIndex];
			int32 BestMat = 0;
			int32 BestVotes = -1;
			for (int32 MatIndex = 0; MatIndex < MaterialVotes.Num(); ++MatIndex)
			{
				if (MaterialVotes[MatIndex] > BestVotes)
				{
					BestVotes = MaterialVotes[MatIndex];
					BestMat = MatIndex;
				}
			}
			Chunk.MaterialIndex = BestMat;
			for (FVector& Vertex : Chunk.Vertices)
			{
				Vertex -= Chunk.LocalCentroid;
			}
			OutChunks.Add(MoveTemp(Chunk));
		}

		return OutChunks.Num() > 0;
	}

}

UNightFoeShatterComponent::UNightFoeShatterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		FallbackShardMesh = CubeFinder.Object;
	}

	Tiers.SetNum(4);
	Tiers[0] = {10, 0.62f};
	Tiers[1] = {12, 0.70f};
	Tiers[2] = {13, 0.78f};
	Tiers[3] = {14, 0.86f};
	Tier5 = {15, 0.95f};
}

const FNightFoeShatterTierSettings& UNightFoeShatterComponent::ResolveTier(
	const FNightFoeShatterRequest& Request) const
{
	if (Request.bTier5)
	{
		return Tier5;
	}
	if (Tiers.IsValidIndex(Request.VFXTier))
	{
		return Tiers[Request.VFXTier];
	}
	return Tiers.Num() > 0 ? Tiers[0] : Tier5;
}

float UNightFoeShatterComponent::ComputeStrikeForce(int32 Combo) const
{
	const float Span = FMath::Max(1, ForceFullAtCombo - 1);
	const float Alpha = FMath::Clamp(static_cast<float>(Combo - 1) / Span, 0.f, 1.f);
	return FMath::Lerp(ForceMin, ForceMax, FMath::Pow(Alpha, ForceExponent));
}

UStaticMesh* UNightFoeShatterComponent::ResolveVisualMesh(const FNightFoeShatterRequest& Request) const
{
	if (const UStaticMeshComponent* SourceStatic = Cast<UStaticMeshComponent>(Request.SourceMeshComponent))
	{
		if (SourceStatic->GetStaticMesh())
		{
			return SourceStatic->GetStaticMesh();
		}
	}
	if (Request.SourceStaticMesh)
	{
		return Request.SourceStaticMesh;
	}
	return nullptr;
}

void UNightFoeShatterComponent::GatherSamplePositions(
	const FNightFoeShatterRequest& Request,
	int32 Wanted,
	TArray<FVector>& OutPositions) const
{
	TArray<FVector> Candidates;
	if (UStaticMeshComponent* SourceStatic = Cast<UStaticMeshComponent>(Request.SourceMeshComponent))
	{
		AppendStaticMeshSamples(
			SourceStatic->GetStaticMesh(),
			SourceStatic->GetComponentTransform(),
			Candidates);
	}
	else if (Request.SourceStaticMesh && Request.SourceMeshComponent)
	{
		AppendStaticMeshSamples(
			Request.SourceStaticMesh,
			Request.SourceMeshComponent->GetComponentTransform(),
			Candidates);
	}
	else if (Request.SourceStaticMesh)
	{
		AppendStaticMeshSamples(Request.SourceStaticMesh, FTransform(Request.Origin), Candidates);
	}

	if (USkeletalMeshComponent* SourceSkeletal = Cast<USkeletalMeshComponent>(Request.SourceMeshComponent))
	{
		AppendBoneSamples(SourceSkeletal, Candidates);
	}

	if (Candidates.Num() == 0)
	{
		AppendBoxSamples(Request.Origin, Request.Extent, Candidates);
	}

	PickSpreadPositions(Candidates, Request.Origin, Wanted, OutPositions);
}

void UNightFoeShatterComponent::ClearShards()
{
	for (FShardState& Shard : LiveShards)
	{
		if (UMeshComponent* Mesh = Shard.Mesh.Get())
		{
			Mesh->DestroyComponent();
		}
	}
	LiveShards.Reset();
	SetComponentTickEnabled(false);
}

void UNightFoeShatterComponent::SpawnExtraNiagara(
	const FNightFoeShatterRequest& Request,
	const FNightFoeShatterTierSettings& Settings,
	float Force) const
{
	if (!ExtraBurstFX)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Speed = SlashSpeedCm * Force;
	if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ExtraBurstFX,
			Request.Origin,
			FRotator::ZeroRotator,
			FVector(0.45f)))
	{
		if (Request.SourceStaticMesh)
		{
			UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMesh(
				Comp,
				TEXT("Mesh"),
				Request.SourceStaticMesh);
		}
		if (Request.Material)
		{
			Comp->SetVariableObject(TEXT("Material"), Request.Material);
			Comp->SetVariableObject(TEXT("User.Material"), Request.Material);
		}
		Comp->SetVariableFloat(TEXT("Speed"), Speed);
		Comp->SetVariableFloat(
			TEXT("ConeAngle"),
			FMath::Lerp(
				JitterConeMinDeg,
				JitterConeMaxDeg,
				FMath::Clamp((Force - ForceMin) / FMath::Max(0.01f, ForceMax - ForceMin), 0.f, 1.f)));
		Comp->SetVariableVec3(TEXT("Velocity"), Request.ImpulseDir * Speed);
		Comp->SetVariableLinearColor(TEXT("Color"), Request.Tint);
	}
}

bool UNightFoeShatterComponent::PlayBurst(const FNightFoeShatterRequest& Request)
{
	if (!bEnabled)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	ClearShards();

	const FNightFoeShatterTierSettings& Settings = ResolveTier(Request);
	const int32 Count = FMath::Clamp(Settings.Count, 10, 15);
	const float Force = ComputeStrikeForce(Request.Combo);
	const float ForceAlpha = FMath::Clamp(
		(Force - ForceMin) / FMath::Max(0.01f, ForceMax - ForceMin),
		0.f,
		1.f);
	const FVector SlashDir = Request.ImpulseDir.GetSafeNormal();
	const FVector SafeSlash = SlashDir.IsNearlyZero() ? FVector::ForwardVector : SlashDir;
	const float ConeRad = FMath::DegreesToRadians(
		FMath::Clamp(FMath::Lerp(JitterConeMinDeg, JitterConeMaxDeg, ForceAlpha), 1.f, 40.f));
	const FTransform SourceXf = Request.SourceMeshComponent
		? Request.SourceMeshComponent->GetComponentTransform()
		: FTransform(FRotator::ZeroRotator, Request.Origin, FVector::OneVector);
	const FVector SourceScale = SourceXf.GetScale3D();
	const FRotator SourceRot = SourceXf.Rotator();

	TArray<FLocalChunk> Chunks;
	UStaticMesh* SourceMesh = ResolveVisualMesh(Request);
	const bool bHasChunks = ExtractLocalChunks(SourceMesh, Count, Chunks);

	auto FinishShard = [&](UMeshComponent* Comp, const FVector& WorldSample)
	{
		FShardState Shard;
		Shard.Mesh = Comp;
		Shard.Location = WorldSample;
		Shard.Rotation = SourceRot + FRotator(
			FMath::FRandRange(-PoseJitterDeg, PoseJitterDeg),
			FMath::FRandRange(-PoseJitterDeg, PoseJitterDeg),
			FMath::FRandRange(-PoseJitterDeg * 0.5f, PoseJitterDeg * 0.5f));
		Shard.BaseScale = SourceScale;
		Shard.Lifetime = FMath::Max(0.2f, Settings.LifetimeSec * FMath::Lerp(0.9f, 1.15f, ForceAlpha));
		FVector Radial = WorldSample - Request.Origin;
		const float RadialLen = Radial.Size();
		if (RadialLen > KINDA_SMALL_NUMBER)
		{
			Radial /= RadialLen;
			Radial = (Radial - SafeSlash * FVector::DotProduct(Radial, SafeSlash)).GetSafeNormal();
		}
		if (Radial.IsNearlyZero())
		{
			Radial = FVector::UpVector;
		}
		Shard.Velocity = FMath::VRandCone(SafeSlash, ConeRad) * (SlashSpeedCm * Force)
			+ Radial * (RadialSpeedCm * Force)
			+ FVector::UpVector * (UpSpeedCm * Force * FMath::Lerp(0.75f, 1.2f, ForceAlpha));
		const FVector TorqueAxis = FVector::CrossProduct(WorldSample - Request.Origin, SafeSlash);
		if (!TorqueAxis.IsNearlyZero())
		{
			const FRotator AxisRot = TorqueAxis.Rotation();
			Shard.AngularDeg = FRotator(AxisRot.Pitch, AxisRot.Yaw, (220.f + 480.f * Force) * (RadialLen / 40.f));
		}
		else
		{
			Shard.AngularDeg = FRotator(0.f, 180.f * Force, 0.f);
		}
		Comp->SetWorldLocation(Shard.Location);
		Comp->SetWorldRotation(Shard.Rotation);
		Comp->SetWorldScale3D(Shard.BaseScale);
		LiveShards.Add(Shard);
	};

	if (bHasChunks)
	{
		for (FLocalChunk& Chunk : Chunks)
		{
			UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(
				Owner,
				UProceduralMeshComponent::StaticClass(),
				NAME_None,
				RF_Transient);
			if (!Comp)
			{
				continue;
			}

			ConfigureShardComp(Comp, this);
			Comp->RegisterComponent();
			Comp->CreateMeshSection(
				0,
				Chunk.Vertices,
				Chunk.Triangles,
				Chunk.Normals,
				Chunk.UVs,
				TArray<FColor>(),
				Chunk.Tangents,
				false);

			UMaterialInterface* ChunkMat = nullptr;
			if (Request.SourceMeshComponent)
			{
				ChunkMat = Request.SourceMeshComponent->GetMaterial(Chunk.MaterialIndex);
			}
			if (!ChunkMat && SourceMesh)
			{
				ChunkMat = SourceMesh->GetMaterial(Chunk.MaterialIndex);
			}
			if (!ChunkMat)
			{
				ChunkMat = Request.Material;
			}
			if (ChunkMat)
			{
				Comp->SetMaterial(0, ChunkMat);
			}

			FinishShard(Comp, SourceXf.TransformPosition(Chunk.LocalCentroid));
		}
	}
	else
	{
		TArray<FVector> SamplePoints;
		GatherSamplePositions(Request, Count, SamplePoints);
		UStaticMesh* FallbackMesh = FallbackShardMesh;
		for (const FVector& Sample : SamplePoints)
		{
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(
				Owner,
				UStaticMeshComponent::StaticClass(),
				NAME_None,
				RF_Transient);
			if (!Comp || !FallbackMesh)
			{
				continue;
			}
			ConfigureShardComp(Comp, this);
			Comp->SetStaticMesh(FallbackMesh);
			if (Request.Material)
			{
				Comp->SetMaterial(0, Request.Material);
			}
			Comp->RegisterComponent();
			FinishShard(Comp, Sample);
			if (LiveShards.Num() > 0)
			{
				LiveShards.Last().BaseScale = SourceScale * 0.12f;
				Comp->SetWorldScale3D(LiveShards.Last().BaseScale);
			}
		}
	}

	SpawnExtraNiagara(Request, Settings, Force);
	if (LiveShards.Num() == 0)
	{
		return false;
	}
	SetComponentTickEnabled(true);
	return true;
}

void UNightFoeShatterComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (LiveShards.Num() == 0)
	{
		ClearShards();
		return;
	}

	TArray<FShardState> NextShards;
	NextShards.Reserve(LiveShards.Num());
	for (FShardState& Shard : LiveShards)
	{
		UMeshComponent* Mesh = Shard.Mesh.Get();
		Shard.Age += DeltaTime;
		if (!Mesh || Shard.Age >= Shard.Lifetime)
		{
			if (Mesh)
			{
				Mesh->DestroyComponent();
			}
			continue;
		}

		Shard.Velocity.Z -= GravityCm * DeltaTime;
		Shard.Velocity *= FMath::Max(0.2f, 1.f - AirDrag * DeltaTime);
		Shard.Location += Shard.Velocity * DeltaTime;
		Shard.Rotation += Shard.AngularDeg * DeltaTime;
		const float FadeT = FMath::Clamp(Shard.Age / Shard.Lifetime, 0.f, 1.f);
		const float Fade = FadeT > 0.72f ? (1.f - (FadeT - 0.72f) / 0.28f) : 1.f;
		Mesh->SetWorldLocation(Shard.Location);
		Mesh->SetWorldRotation(Shard.Rotation);
		Mesh->SetWorldScale3D(Shard.BaseScale * FMath::Max(0.01f, Fade));
		NextShards.Add(Shard);
	}

	if (NextShards.Num() == 0)
	{
		LiveShards.Reset();
		SetComponentTickEnabled(false);
		return;
	}
	LiveShards = MoveTemp(NextShards);
}
#pragma endregion K2 moonyfli
