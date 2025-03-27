// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CMenuUI.generated.h"

/**
 * 
 */
UCLASS()
class VRPROJECT_API UCMenuUI : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 앱을 종료하는 함수
	UFUNCTION(BlueprintCallable, Category="MenuEvent")
	void QuitVRGame();
};
