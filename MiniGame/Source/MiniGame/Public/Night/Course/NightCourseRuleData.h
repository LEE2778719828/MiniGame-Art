#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightCourseRuleData.generated.h"

class UNightCourseAtomRouteData;

#pragma region K2 moonyfli
USTRUCT(BlueprintType)
struct MINIGAME_API FNightRuleAtomEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (DisplayName = "AtomKey", ToolTip = "填写 DA_Atoms 中的 AtomKey；开启空Key自动选Atom时可以留空。"))
	FString AtomKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (DisplayName = "动作序列", ToolTip = "按 LandingPoint 顺序填写动作；每个动作只能是 Enemy 或 Hazard，数量必须与 Atom 的落脚点间隔一致。"))
	TArray<ENightNodeKind> Actions;

	/** Weighted template probability when the queue is expanded to its target count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (DisplayName = "权重", ToolTip = "模板被抽取的相对概率；必须大于 0。", ClampMin = "1"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct MINIGAME_API FNightRuleAtomQueue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (DisplayName = "Atom模板池", ToolTip = "同一路线模式内可随机抽取的 Atom+动作模板；每个模板的 Weight 决定被选中的概率。"))
	TArray<FNightRuleAtomEntry> Atoms;

	/**
	 * Target number of generated Atoms for this route queue.
	 * Zero uses Atoms.Num().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (DisplayName = "生成Atom数量", ToolTip = "该路线队列最终生成的 Atom 数量；填 0 时使用模板池数量。", ClampMin = "0"))
	int32 TargetAtomCount = 0;
};

/**
 * Planner-owned course rule. It deliberately contains only AtomKey and
 * action templates plus generation counts/weights; meshes, transforms,
 * landing positions and visual classes live in the Atom BP/library.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightCourseRuleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule", meta = (DisplayName = "启用规则", ToolTip = "关闭后 Night 课程不会使用这份规则数据。"))
	bool bEnabled = true;

	/** Empty AtomKey values select deterministically from the configured Atom library. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule", meta = (DisplayName = "空Key自动选Atom", ToolTip = "开启后，AtomKey 为空的模板会按 Seed 从 DA_Atoms 选择兼容 Atom；关闭后所有模板都必须填写有效 AtomKey。"))
	bool bAutoSelectAtomKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule", meta = (DisplayName = "随机种子", ToolTip = "相同种子会得到相同的路线模板与 Atom 组合；修改它可生成另一套确定性结果。"))
	int32 Seed = 1001;

	/**
	 * Day selects the default route mode. The selected mode supplies the
	 * pre-fork queue; BranchRoutes supply the post-choice queue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues", meta = (DisplayName = "路线模式队列", ToolTip = "按 A/B/C 路线配置岔路前的主段模板池。Day 的 DefaultRoute 决定本局使用哪一组；每组填写 Atoms 与 TargetAtomCount。"))
	TMap<ENightRouteId, FNightRuleAtomQueue> RouteModes;

	/** Planner-owned branch queues. The Atom Library still owns only key -> BP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues", meta = (DisplayName = "岔路后队列", ToolTip = "按 A/B/C 配置选路后的分支模板池；当前 ForkPair 对应的两条路线必须有有效队列。"))
	TMap<ENightRouteId, FNightRuleAtomQueue> BranchRoutes;

	/** Optional text buffer used by the CallInEditor import/export helpers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule|JSON", meta = (DisplayName = "规则JSON缓存", ToolTip = "可粘贴 routeModes/branchRoutes 新格式 JSON；导入时兼容旧 baseRoute，但导出只生成新格式。", MultiLine = true))
	FString EditorJson;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRule(FString& OutError) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRuleAgainstLibrary(
		const UNightCourseAtomRouteData* AtomLibrary,
		FString& OutError) const;

	UFUNCTION(BlueprintPure, Category = "Night|Rule")
	bool HasBranchRoute(ENightRouteId RouteId) const;

	UFUNCTION(BlueprintPure, Category = "Night|Rule")
	/** Returns the generated count for a route mode, not the number of templates. */
	int32 GetRouteModeLength(ENightRouteId RouteId) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule|JSON")
	bool ImportJson(const FString& JsonText, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Rule|JSON")
	bool ExportJson(FString& OutJson, FString& OutError) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|JSON")
	void ImportEditorJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|JSON")
	void ExportEditorJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|Editor")
	void MarkPackageDirtyForEditor();
};
#pragma endregion K2 moonyfli
