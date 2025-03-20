// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VRPlayer.generated.h"

UCLASS()
class VRPROJECT_API AVRPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVRPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* VRCamera;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* IMC_VRInput;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Move;

	void Move(const struct FInputActionValue& Values);

public: // mouse
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Mouse;
	void Turn(const struct FInputActionValue& Values);

public: // Teleport
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Teleport;

	bool bIsDebugDraw = false;

	UFUNCTION(Exec)
	void ActiveDebugDraw();

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* TeleportCircle;

	// �ڷ���Ʈ ���࿩��
	bool bTeleporting = false;

	// �ڷ���Ʈ ����
	bool ResetTeleport();

	void TeleportStart(const struct FInputActionValue& Values);
	void TeleportEnd(const struct FInputActionValue& Values);

	// ���� �ڷ���Ʈ �׸���
	void DrawTeleportStraight();

	// �ڷ���Ʈ ��ġ
	FVector TeleportLocation;

public: // Curve Teleport
	// P = P0 + vt
	// v = v0 + at
	// F = ma
	// ��� �̷�� ���� ����(��� �ε巴�� ����)
	UPROPERTY(EditAnywhere, Category="Teleport")
	int LineSmooth = 40;
	UPROPERTY(EditAnywhere, Category="Teleport")
	float CurveForce = 1500;
	// �߷°��ӵ�
	UPROPERTY(EditAnywhere, Category="Teleport")
	float Gravity = -5000;
	// Delta Time
	UPROPERTY(EditAnywhere, Category="Teleport")
	float SimulateTime = 0.02f;

	// ����� �� ����Ʈ
	TArray<FVector> Lines;

	// �ڷ���Ʈ �����ȯ(Curve or not)
	UPROPERTY(EditAnywhere, Category="Teleport")
	bool bTeleportCurve = true;

	// ��ڷ���Ʈ �׸���
	void DrawTeleportCurve();

	bool CheckHitTeleport(FVector LastPos, FVector& CurPos);
};
