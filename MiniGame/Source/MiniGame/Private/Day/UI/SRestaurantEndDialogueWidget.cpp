#include "Day/UI/SRestaurantEndDialogueWidget.h"

#include "../../../SStandaloneSandbox.h"

DEFINE_LOG_CATEGORY_STATIC(LogSRestaurantDialogue, Log, All);

USRestaurantEndDialogueWidget::USRestaurantEndDialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	BuildDefaultDialogueLines();
}

void USRestaurantEndDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	StartDialogue();
}

void USRestaurantEndDialogueWidget::StartDialogue()
{
	if (DialogueLines.IsEmpty())
	{
		BuildDefaultDialogueLines();
	}

	bFinished = false;
	CurrentLineIndex = DialogueLines.IsEmpty() ? INDEX_NONE : 0;
	PresentCurrentLine();
}

void USRestaurantEndDialogueWidget::AdvanceDialogue()
{
	if (bFinished)
	{
		return;
	}
	if (!DialogueLines.IsValidIndex(CurrentLineIndex))
	{
		FinishDialogue();
		return;
	}

	if (CurrentLineIndex + 1 < DialogueLines.Num())
	{
		++CurrentLineIndex;
		PresentCurrentLine();
		return;
	}

	FinishDialogue();
}

bool USRestaurantEndDialogueWidget::FinishDialogue()
{
	if (bFinished)
	{
		return false;
	}

	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance || !GameInstance->IsAwaitingRestaurantEndDialogue())
	{
		UE_LOG(LogSRestaurantDialogue, Warning, TEXT("Dialogue finish rejected: no pending restaurant dialogue."));
		return false;
	}

	bFinished = true;
	BP_OnDialogueFinished();
	if (!GameInstance->CompleteRestaurantEndDialogue())
	{
		bFinished = false;
		return false;
	}
	return true;
}

FSRestaurantDialogueLine USRestaurantEndDialogueWidget::GetCurrentLine() const
{
	return DialogueLines.IsValidIndex(CurrentLineIndex)
		? DialogueLines[CurrentLineIndex]
		: FSRestaurantDialogueLine();
}

void USRestaurantEndDialogueWidget::PresentCurrentLine()
{
	if (!DialogueLines.IsValidIndex(CurrentLineIndex))
	{
		return;
	}
	BP_OnDialogueLineChanged(DialogueLines[CurrentLineIndex], CurrentLineIndex, DialogueLines.Num());
}

void USRestaurantEndDialogueWidget::BuildDefaultDialogueLines()
{
	DialogueLines.Reset();

	auto AddLine = [this](
		const int32 SceneNumber,
		const ESRestaurantDialogueSpeaker Speaker,
		const ESRestaurantDialoguePresentation Presentation,
		const TCHAR* SpeakerName,
		const TCHAR* Text)
	{
		FSRestaurantDialogueLine& Line = DialogueLines.AddDefaulted_GetRef();
		Line.SceneNumber = SceneNumber;
		Line.Speaker = Speaker;
		Line.Presentation = Presentation;
		Line.SpeakerName = FText::FromString(SpeakerName);
		Line.Text = FText::FromString(Text);
	};

	AddLine(0, ESRestaurantDialogueSpeaker::System, ESRestaurantDialoguePresentation::Reward,
		TEXT("系统"), TEXT("订单完成 · 奖励饭勺\n顾客满意度 +1\n\n顾客赠礼，获得道具【野生牛马奶】\n· 保质期：永远\n· 濒死加 40 血\n· 来自牛马街的野生牛马奶"));
	AddLine(1, ESRestaurantDialogueSpeaker::Customer, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("牛马哥"), TEXT("这顿真吃住了……哎，等下，给你个东西。"));
	AddLine(2, ESRestaurantDialogueSpeaker::Customer, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("牛马哥"), TEXT("我们这条街上加班的人，兜里都揣一瓶。"));
	AddLine(3, ESRestaurantDialogueSpeaker::Customer, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("牛马哥"), TEXT("你看，保质期：永远。\n——听着离谱？这东西比我们的合同还稳，一辈子不过期。"));
	AddLine(4, ESRestaurantDialogueSpeaker::Customer, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("牛马哥"), TEXT("熬到快撑不住的时候灌一口，第二天照样能来上班。"));
	AddLine(5, ESRestaurantDialogueSpeaker::XiaoYao, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("小妖"), TEXT("那……这瓶送我做什么？"));
	AddLine(6, ESRestaurantDialogueSpeaker::Customer, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("牛马哥"), TEXT("这顿饭保住了我一命，作为回礼。留着——反正它不会过期，你什么时候用都行。"));
	AddLine(7, ESRestaurantDialogueSpeaker::System, ESRestaurantDialoguePresentation::Transition,
		TEXT(""), TEXT("——顾客推门走了。K易斯凑过来，看了眼柜台上那个瓶子——"));
	AddLine(8, ESRestaurantDialogueSpeaker::KYisi, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("K易斯"), TEXT("这是好东西啊，关键时候可以救命的道具。你在妖界采集时肯定会帮大忙的。\n但是别乱喝——只在你快倒下的时候喝一口，能立马回血。"));
	AddLine(9, ESRestaurantDialogueSpeaker::XiaoYao, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("小妖"), TEXT("我真的会遇到这种困难的时候吗？"));
	AddLine(10, ESRestaurantDialogueSpeaker::KYisi, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("K易斯"), TEXT("你现在走的都是安全的采集路线，所以食材也很普通。"));
	AddLine(11, ESRestaurantDialogueSpeaker::KYisi, ESRestaurantDialoguePresentation::RouteHintSmall,
		TEXT("K易斯"), TEXT("夜里进山，有时候会撞上一个岔口，两条路二选一。\n每条路上东西不一样，效果也不一样。"));
	AddLine(12, ESRestaurantDialogueSpeaker::XiaoYao, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("小妖"), TEXT("是不是越难走的路，食材越好？可以给客人做的料理就越好？"));
	AddLine(13, ESRestaurantDialogueSpeaker::KYisi, ESRestaurantDialoguePresentation::RouteHintLarge,
		TEXT("K易斯"), TEXT("是的，一条稳当，走起来不费劲，能捡的东西少。另一条邪门，看不太清路，但是里面摘的食材更加稀有。"));
	AddLine(14, ESRestaurantDialogueSpeaker::KYisi, ESRestaurantDialoguePresentation::Dialogue,
		TEXT("K易斯"), TEXT("到岔口按不同的键，选不同的路。\n想给客人做更好的菜，你自己拿主意。"));
}
