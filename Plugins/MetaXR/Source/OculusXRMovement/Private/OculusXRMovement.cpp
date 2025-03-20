// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRMovement.h"
#include "OculusXRMovementLog.h"
#include "OculusXRMovementModule.h"
#include "OculusXRHMDPrivate.h"
#include "OculusXRHMD.h"
#include "OculusXRPluginWrapper.h"
#include "OculusXRMovementFunctionsOVR.h"
#include "OculusXRMovementFunctionsOpenXR.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "OculusXRMovement"

bool OculusXRMovement::GetBodyState(FOculusXRBodyState& outOculusXRBodyState, float WorldToMeters)
{
	return GetOculusXRMovementFunctionsImpl()->GetBodyState(outOculusXRBodyState, WorldToMeters);
}

bool OculusXRMovement::GetBodySkeleton(FOculusXRBodySkeleton& outOculusXRBodyState, float WorldToMeters)
{
	return GetOculusXRMovementFunctionsImpl()->GetBodySkeleton(outOculusXRBodyState, WorldToMeters);
}

bool OculusXRMovement::IsBodyTrackingEnabled()
{
	return GetOculusXRMovementFunctionsImpl()->IsBodyTrackingEnabled();
}

bool OculusXRMovement::IsBodyTrackingSupported()
{
	return GetOculusXRMovementFunctionsImpl()->IsBodyTrackingSupported();
}

bool OculusXRMovement::RequestBodyTrackingFidelity(EOculusXRBodyTrackingFidelity fidelity)
{
	return GetOculusXRMovementFunctionsImpl()->RequestBodyTrackingFidelity(fidelity);
}

bool OculusXRMovement::ResetBodyTrackingCalibration()
{
	return GetOculusXRMovementFunctionsImpl()->ResetBodyTrackingCalibration();
}

bool OculusXRMovement::SuggestBodyTrackingCalibrationOverride(float height)
{
	return GetOculusXRMovementFunctionsImpl()->SuggestBodyTrackingCalibrationOverride(height);
}

bool OculusXRMovement::StartBodyTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StartBodyTracking();
}

bool OculusXRMovement::StartBodyTrackingByJointSet(EOculusXRBodyJointSet jointSet)
{
	return GetOculusXRMovementFunctionsImpl()->StartBodyTrackingByJointSet(jointSet);
}

bool OculusXRMovement::StopBodyTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StopBodyTracking();
}

bool OculusXRMovement::GetFaceState(FOculusXRFaceState& outOculusXRFaceState)
{
	return GetOculusXRMovementFunctionsImpl()->GetFaceState(outOculusXRFaceState);
}

bool OculusXRMovement::IsFaceTrackingEnabled()
{
	return GetOculusXRMovementFunctionsImpl()->IsFaceTrackingEnabled();
}

bool OculusXRMovement::IsFaceTrackingSupported()
{
	return GetOculusXRMovementFunctionsImpl()->IsFaceTrackingSupported();
}


bool OculusXRMovement::StartFaceTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StartFaceTracking();
}

bool OculusXRMovement::StopFaceTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StopFaceTracking();
}

bool OculusXRMovement::GetEyeGazesState(FOculusXREyeGazesState& outOculusXREyeGazesState, float WorldToMeters)
{
	return GetOculusXRMovementFunctionsImpl()->GetEyeGazesState(outOculusXREyeGazesState, WorldToMeters);
}

bool OculusXRMovement::IsEyeTrackingEnabled()
{
	return GetOculusXRMovementFunctionsImpl()->IsEyeTrackingEnabled();
}

bool OculusXRMovement::IsEyeTrackingSupported()
{
	return GetOculusXRMovementFunctionsImpl()->IsEyeTrackingSupported();
}

bool OculusXRMovement::StartEyeTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StartEyeTracking();
}

bool OculusXRMovement::StopEyeTracking()
{
	return GetOculusXRMovementFunctionsImpl()->StopEyeTracking();
}

bool OculusXRMovement::IsFullBodyTrackingEnabled()
{
	return GetOculusXRMovementFunctionsImpl()->IsFullBodyTrackingEnabled();
}

TSharedPtr<IOculusXRMovementFunctions> OculusXRMovement::MovementFunctionsImpl = nullptr;
TSharedPtr<IOculusXRMovementFunctions> OculusXRMovement::GetOculusXRMovementFunctionsImpl()
{
	if (MovementFunctionsImpl == nullptr)
	{
		const FName SystemName(TEXT("OpenXR"));
		const bool IsOpenXR = GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName);
		if (OculusXRHMD::FOculusXRHMD::GetOculusXRHMD() != nullptr)
		{
			MovementFunctionsImpl = MakeShared<FOculusXRMovementFunctionsOVR>();
		}
		else if (IsOpenXR)
		{
			MovementFunctionsImpl = MakeShared<FOculusXRMovementFunctionsOpenXR>();
		}
	}

	check(MovementFunctionsImpl);
	return MovementFunctionsImpl;
}

#undef LOCTEXT_NAMESPACE
