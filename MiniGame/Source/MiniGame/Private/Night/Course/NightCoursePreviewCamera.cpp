#include "Night/Course/NightCoursePreviewCamera.h"
#include "Night/Course/NightCourseHost.h"
#include "Camera/CameraComponent.h"

#pragma region K2 moonyfli
ANightCoursePreviewCamera::ANightCoursePreviewCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;
	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->bConstrainAspectRatio = false;
	}
}

void ANightCoursePreviewCamera::SetPreviewHost(ANightCourseHost* InHost)
{
	PreviewHost = InHost;
}

void ANightCoursePreviewCamera::NotifyHostEdited()
{
#if WITH_EDITOR
	if (ANightCourseHost* Host = PreviewHost.Get())
	{
		Host->NotifyPreviewCameraEdited(this);
	}
#endif
}

#if WITH_EDITOR
void ANightCoursePreviewCamera::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NotifyHostEdited();
}

void ANightCoursePreviewCamera::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	NotifyHostEdited();
}

void ANightCoursePreviewCamera::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bFinished)
	{
		NotifyHostEdited();
	}
}
#endif
#pragma endregion K2 moonyfli
