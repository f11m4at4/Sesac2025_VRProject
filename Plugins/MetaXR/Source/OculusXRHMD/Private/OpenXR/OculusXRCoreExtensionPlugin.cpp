// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "OculusXRCoreExtensionPlugin.h"

#include "DefaultSpectatorScreenController.h"
#include "OculusXRFunctionLibrary.h"
#include "OculusXRXRFunctions.h"
#include "OculusXROpenXRUtilities.h"
#include "OpenXRCore.h"

#if PLATFORM_ANDROID
// #include <openxr_oculus.h>
#include <dlfcn.h>
#endif // PLATFORM_ANDROID

DEFINE_LOG_CATEGORY(LogOculusOpenXRPlugin);

namespace OculusXR
{

	bool FCoreExtensionPlugin::IsStandaloneStereoOnlyDevice()
	{
#if PLATFORM_ANDROID
		const bool bIsStandaloneStereoDevice = FAndroidMisc::GetDeviceMake() == FString("Oculus");
#else
		const bool bIsStandaloneStereoDevice = false;
#endif
		return bIsStandaloneStereoDevice;
	}

	bool FCoreExtensionPlugin::GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions)
	{
		return true;
	}

	bool FCoreExtensionPlugin::GetSpectatorScreenController(FHeadMountedDisplayBase* InHMDBase, TUniquePtr<FDefaultSpectatorScreenController>& OutSpectatorScreenController)
	{
#if PLATFORM_ANDROID
		OutSpectatorScreenController = nullptr;
		return true;
#else  // PLATFORM_ANDROID
		OutSpectatorScreenController = MakeUnique<FDefaultSpectatorScreenController>(InHMDBase);
		return false;
#endif // PLATFORM_ANDROID
	}

	const void* FCoreExtensionPlugin::OnCreateSession(XrInstance InInstance, XrSystemId InSystem, const void* InNext)
	{
		check(IsInGameThread());

		InitOpenXRFunctions(InInstance);

#if PLATFORM_ANDROID
#if UE_VERSION_OLDER_THAN(5, 5, 0)
		if (GRHISupportsRHIThread && GIsThreadedRendering && GUseRHIThread_InternalUseOnly)
		{
			SetRHIThreadEnabled(false, false);
		}
#else
		GPendingRHIThreadMode = ERHIThreadMode::None;
#endif // UE_VERSION_OLDER_THAN
#endif // PLATFORM_ANDROID
		return InNext;
	}

} // namespace OculusXR
