// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "OculusXRMovementXRIncludes.h"
#include "IOpenXRExtensionPlugin.h"
#include "OculusXRMovementTypes.h"

#define LOCTEXT_NAMESPACE "OculusXRMovement"

class FOpenXRHMD;

namespace XRMovement
{
	extern PFN_xrCreateFaceTracker2FB xrCreateEyeTracker2FB;
	extern PFN_xrDestroyFaceTracker2FB xrDestroyEyeTracker2FB;
	extern PFN_xrGetFaceExpressionWeights2FB xrGetFaceExpressionWeights2FB;

	class FFaceTrackingXR : public IOpenXRExtensionPlugin
	{
	public:
		// IOculusXROpenXRHMDPlugin
		virtual bool GetRequiredExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
		virtual bool GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
		virtual const void* OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext) override;
		virtual const void* OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext) override;
		virtual void OnDestroySession(XrSession InSession) override;
		virtual void* OnWaitFrame(XrSession InSession, void* InNext) override;

	public:
		FFaceTrackingXR();
		virtual ~FFaceTrackingXR();
		void RegisterAsOpenXRExtension();

		bool IsFaceTrackingSupported() const { return bExtFaceTrackingEnabled; }
		bool IsFaceTrackingEnabled() const { return FaceTracker != XR_NULL_HANDLE; }

		XrResult StartFaceTracking();
		XrResult StopFaceTracking();
		XrResult GetCachedFaceState(FOculusXRFaceState& OutState);

	private:
		void InitOpenXRFunctions(XrInstance InInstance);
		void Update_GameThread(XrSession InSession);

		bool bExtFaceTrackingEnabled;

		FOpenXRHMD* OpenXRHMD;
		FOculusXRFaceState CachedFaceState;
		XrFaceTracker2FB FaceTracker = XR_NULL_HANDLE;
	};

} // namespace XRMovement

#undef LOCTEXT_NAMESPACE
