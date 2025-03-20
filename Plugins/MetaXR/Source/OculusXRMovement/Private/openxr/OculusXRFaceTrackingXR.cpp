// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRFaceTrackingXR.h"
#include "OpenXRCore.h"
#include "IOpenXRHMDModule.h"
#include "OpenXRHMD.h"
#include "OculusXRMovementLog.h"
#include "OpenXR/OculusXROpenXRUtilities.h"

#define LOCTEXT_NAMESPACE "OculusXRMovement"

namespace XRMovement
{
	PFN_xrCreateFaceTracker2FB xrCreateFaceTracker2FB = nullptr;
	PFN_xrDestroyFaceTracker2FB xrDestroyFaceTracker2FB = nullptr;
	PFN_xrGetFaceExpressionWeights2FB xrGetFaceExpressionWeights2FB = nullptr;

	FFaceTrackingXR::FFaceTrackingXR()
		: bExtFaceTrackingEnabled(false)
		, OpenXRHMD(nullptr)
		, FaceTracker(nullptr)
	{
		CachedFaceState.ExpressionWeights.SetNum(XR_FACE_EXPRESSION2_COUNT_FB);
		CachedFaceState.ExpressionWeightConfidences.SetNum(XR_FACE_CONFIDENCE2_COUNT_FB);
	}

	FFaceTrackingXR::~FFaceTrackingXR()
	{
	}

	void FFaceTrackingXR::RegisterAsOpenXRExtension()
	{
#if defined(WITH_OCULUS_BRANCH)
		// Feature not enabled on Marketplace build. Currently only for the meta fork
		RegisterOpenXRExtensionModularFeature();
#endif
	}

	bool FFaceTrackingXR::GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		OutExtensions.Add(XR_FB_FACE_TRACKING2_EXTENSION_NAME);
		return true;
	}

	bool FFaceTrackingXR::GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		return true;
	}

	const void* FFaceTrackingXR::OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext)
	{
		if (InModule != nullptr)
		{
			bExtFaceTrackingEnabled = InModule->IsExtensionEnabled(XR_FB_FACE_TRACKING2_EXTENSION_NAME);
		}
		return InNext;
	}

	const void* FFaceTrackingXR::OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
	{
		InitOpenXRFunctions(InInstance);

		OpenXRHMD = (FOpenXRHMD*)GEngine->XRSystem.Get();

		return InNext;
	}

	void FFaceTrackingXR::OnDestroySession(XrSession InSession)
	{
		OpenXRHMD = nullptr;
	}

	void* FFaceTrackingXR::OnWaitFrame(XrSession InSession, void* InNext)
	{
		Update_GameThread(InSession);
		return InNext;
	}

	XrResult FFaceTrackingXR::StartFaceTracking()
	{
		if (!OpenXRHMD || !OpenXRHMD->GetInstance() || !OpenXRHMD->GetSession())
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StartFaceTracking] Cannot start face tracking, the instance or session is null."));
			return XR_ERROR_VALIDATION_FAILURE;
		}

		if (!IsFaceTrackingSupported())
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StartFaceTracking] Cannot start face tracking, body tracking is unsupported."));
			return XR_ERROR_VALIDATION_FAILURE;
		}

		if (FaceTracker != XR_NULL_HANDLE)
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StartFaceTracking] Cannot start face tracking, body tracking is already started."));
			return XR_SUCCESS;
		}

		XrFaceTrackerCreateInfo2FB createInfo = { XR_TYPE_FACE_TRACKER_CREATE_INFO2_FB, nullptr };

		auto result = XRMovement::xrCreateFaceTracker2FB(OpenXRHMD->GetSession(), &createInfo, &FaceTracker);
		if (XR_FAILED(result))
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StartFaceTracking] Failed to start face tracking. Result(%d)"), result);
		}

		return result;
	}

	XrResult FFaceTrackingXR::StopFaceTracking()
	{
		if (!OpenXRHMD || !OpenXRHMD->GetInstance() || !OpenXRHMD->GetSession())
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StopFaceTracking] Cannot stop face tracking, the instance or session is null."));
			return XR_ERROR_VALIDATION_FAILURE;
		}

		if (!IsFaceTrackingSupported())
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[StopFaceTracking] Cannot stop face tracking, eye tracking is unsupported."));
			return XR_ERROR_VALIDATION_FAILURE;
		}

		XrResult result = XR_SUCCESS;
		if (IsFaceTrackingEnabled())
		{
			result = XRMovement::xrDestroyFaceTracker2FB(FaceTracker);
			if (XR_FAILED(result))
			{
				UE_LOG(LogOculusXRMovement, Warning, TEXT("[StopFaceTracking] Failed to stop face tracking. Result(%d)"), result);
			}
		}

		FaceTracker = XR_NULL_HANDLE;

		return result;
	}

	XrResult FFaceTrackingXR::GetCachedFaceState(FOculusXRFaceState& OutState)
	{
		if (!IsFaceTrackingEnabled())
		{
			return XR_ERROR_VALIDATION_FAILURE;
		}

		OutState = CachedFaceState;
		return XR_SUCCESS;
	}

	void FFaceTrackingXR::InitOpenXRFunctions(XrInstance InInstance)
	{
		// XR_FB_Eye_Tracking_Social
		OculusXR::XRGetInstanceProcAddr(InInstance, "xrCreateFaceTracker2FB", &xrCreateFaceTracker2FB);
		OculusXR::XRGetInstanceProcAddr(InInstance, "xrDestroyFaceTracker2FB", &xrDestroyFaceTracker2FB);
		OculusXR::XRGetInstanceProcAddr(InInstance, "xrGetFaceExpressionWeights2FB", &xrGetFaceExpressionWeights2FB);
	}

	void FFaceTrackingXR::Update_GameThread(XrSession InSession)
	{
		check(IsInGameThread());

		if (!OpenXRHMD || !OpenXRHMD->GetInstance() || !OpenXRHMD->GetSession() || !IsFaceTrackingSupported() || !IsFaceTrackingEnabled())
		{
			return;
		}

		XrFaceExpressionInfo2FB info{ XR_TYPE_FACE_EXPRESSION_INFO2_FB, nullptr };
		info.time = OpenXRHMD->GetDisplayTime();

		float weightsArray[XR_FACE_EXPRESSION2_COUNT_FB];
		float confidencesArray[XR_FACE_CONFIDENCE2_COUNT_FB];

		XrFaceExpressionWeights2FB weights{ XR_TYPE_FACE_EXPRESSION_WEIGHTS2_FB, nullptr };
		weights.weights = weightsArray;
		weights.weightCount = XR_FACE_EXPRESSION2_COUNT_FB;
		weights.confidences = confidencesArray;
		weights.confidenceCount = XR_FACE_CONFIDENCE2_COUNT_FB;

		auto result = XRMovement::xrGetFaceExpressionWeights2FB(FaceTracker, &info, &weights);
		if (XR_FAILED(result))
		{
			UE_LOG(LogOculusXRMovement, Warning, TEXT("[FaceExpressionStateUpdate] Failed to get face tracking state. Result(%d)"), result);
			return;
		}

		CachedFaceState.bIsValid = (weights.isValid == XR_TRUE);
		CachedFaceState.bIsEyeFollowingBlendshapesValid = (weights.isEyeFollowingBlendshapesValid == XR_TRUE);
		CachedFaceState.Time = weights.time * 1e-9; // FromXrTime

		switch (weights.dataSource)
		{
			case XR_FACE_TRACKING_DATA_SOURCE2_AUDIO_FB:
				CachedFaceState.DataSource = EFaceTrackingDataSource::Audio;
				break;
			case XR_FACE_TRACKING_DATA_SOURCE2_VISUAL_FB:
				CachedFaceState.DataSource = EFaceTrackingDataSource::Visual;
				break;
			case XR_FACE_TRACKING_DATA_SOURCE_2FB_MAX_ENUM_FB:
				CachedFaceState.DataSource = EFaceTrackingDataSource::MAX;
				break;
		}

		for (int i = 0; i < XR_FACE_EXPRESSION2_COUNT_FB; ++i)
		{
			CachedFaceState.ExpressionWeights[i] = weights.weights[i];
		}

		for (int i = 0; i < XR_FACE_CONFIDENCE2_COUNT_FB; ++i)
		{
			CachedFaceState.ExpressionWeightConfidences[i] = weights.confidences[i];
		}
	}

} // namespace XRMovement

#undef LOCTEXT_NAMESPACE
