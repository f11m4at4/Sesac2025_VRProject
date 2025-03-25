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
	// �����Ʈ�ѷ� ���
	UPROPERTY(VisibleAnywhere, Category="MotionController")
	class UMotionControllerComponent* LeftHand;
	UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UMotionControllerComponent* RightHand;
	UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UMotionControllerComponent* RightAim;


	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* IMC_VRInput;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Move;

	void Move(const struct FInputActionValue& Values);

public: // mouse
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Mouse;
	void Turn(const struct FInputActionValue& Values);
	UPROPERTY(EditAnywhere, Category="Input")
	bool bUsingMouse = false;

public: // Teleport
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Teleport;

	bool bIsDebugDraw = false;

	UFUNCTION(Exec)
	void ActiveDebugDraw();

	UPROPERTY(VisibleAnywhere)
	class UNiagaraComponent* TeleportCircle;
	//class UStaticMeshComponent* TeleportCircle;

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

public:	// Teleport UI
	UPROPERTY(VisibleAnywhere)
	class UNiagaraComponent* TeleportUIComponent;

private:	// Warp
	// Warp Ȱ��ȭ ����
	UPROPERTY(EditAnywhere, Category="Warp", meta = (AllowPrivateAccess=true))
	bool IsWarp = true;
	// 1. ��� -> ������ �ð��� ���� �̵��ϱ�
	// ����ð�
	float CurrentTime = 0;
	// ����Ÿ��
	UPROPERTY(EditAnywhere, Category="Warp", meta = (AllowPrivateAccess=true))
	float WarpTime = 0.2f;

	FTimerHandle WarpTimer;
	// ���������Լ�
	void DoWarp();

public:		// �ѽ��
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Fire;
	void FireInput(const struct FInputActionValue& Values);

	UPROPERTY(VisibleAnywhere)
	class UChildActorComponent* CrosshairComp;

	// ũ�ν���� �׸���
	void DrawCrosshair();

public:		// ���
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Grab;

	// ��ü�� ����ִ��� ����
	bool bIsGrabbing = false;

	// �ʿ�Ӽ� : ���� ����
	UPROPERTY(EditAnywhere, Category="Grab")
	float GrabRadius = 100;

	// ���� ��ü ����� ����
	UPROPERTY()
	class UPrimitiveComponent* grabbedObject;

	void TryGrab(const struct FInputActionValue& Values);
	void TryUnGrab(const struct FInputActionValue& Values);
	// ��ü�� ���� ���·� ��Ʈ�� �ϱ�
	void Grabbing();
};
