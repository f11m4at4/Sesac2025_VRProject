// @lint-ignore-every LICENSELINT
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/SceneComponent.h"
#include "OculusXRControllerLegacyPoseTransformComponent.generated.h"

static const FQuat OculusPoseToGripRotation = FQuat(FVector(0, 1, 0), -FMath::DegreesToRadians(double(60)));
static const FVector OculusPoseToGripPosition = FVector(-0.04, 0, -0.03);

/**
 * Handles conversion of models created for the legacy Oculus controller pose into
 * the OpenXR Grip pose. By default, the conversion is done to the direct parent of
 * this component, but an explicit component to convert can also be specified.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = OculusHand, DisplayName = "OculusXR Legacy Pose Transform Component")
class OCULUSXRINPUT_API UOculusXRControllerLegacyPoseTransformComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Applies the transformation from legacy Oculus pose to OpenXR grip pose onto the parent component.
	 */
	virtual void BeginPlay() override;
};
