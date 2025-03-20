// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include "OculusXRAsyncRequestSystem.h"
#include "OculusXRAsyncRequest.h"
#include "OculusXRAnchorTypes.h"

namespace OculusXRAnchors
{
	// Share anchors with group
	struct OCULUSXRANCHORS_API FShareAnchorsWithGroups :
		OculusXR::FAsyncRequest<FShareAnchorsWithGroups, EOculusXRAnchorResult::Type, TTuple<TArray<FOculusXRUUID>, TArray<FOculusXRUInt64>>>
	{
	public:
		FShareAnchorsWithGroups(const TArray<FOculusXRUUID>& TargetGroups, const TArray<FOculusXRUInt64>& AnchorsToShare);
		~FShareAnchorsWithGroups();

		const TArray<FOculusXRUUID>& GetGroups() const { return Groups; }
		const TArray<FOculusXRUInt64>& GetAnchors() const { return Anchors; }

	protected:
		virtual void OnInitRequest() override;

	private:
		static void OnShareComplete(FOculusXRUInt64 RequestId, EOculusXRAnchorResult::Type Result);

		TArray<FOculusXRUUID> Groups;
		TArray<FOculusXRUInt64> Anchors;
		FDelegateHandle CallbackHandle;
	};

	// Get shared anchors from group
	struct OCULUSXRANCHORS_API FGetAnchorsSharedWithGroup :
		OculusXR::FAsyncRequest<FGetAnchorsSharedWithGroup, EOculusXRAnchorResult::Type, TArray<FOculusXRAnchor>>
	{
	public:
		FGetAnchorsSharedWithGroup(const FOculusXRUUID& TargetGroup, const TArray<FOculusXRUUID>& WantedAnchors = {});
		~FGetAnchorsSharedWithGroup();

		void OnResultsAvailable(const TArray<FOculusXRAnchor>& Results);
		const TArray<FOculusXRAnchor>& GetRetrievedAnchors() const { return RetrievedAnchors; }

	protected:
		virtual void OnInitRequest() override;

	private:
		static void OnQueryComplete(FOculusXRUInt64 RequestId, EOculusXRAnchorResult::Type Result);
		static void OnQueryResultAvailable(FOculusXRUInt64 RequestId, FOculusXRUInt64 AnchorHandle, FOculusXRUUID AnchorUuid);

		FOculusXRUUID Group;
		TArray<FOculusXRUUID> RequestedAnchors;
		TArray<FOculusXRAnchor> RetrievedAnchors;
		FDelegateHandle CallbackHandleComplete;
		FDelegateHandle CallbackHandleResults;
	};
} // namespace OculusXRAnchors
