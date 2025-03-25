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
	// 모션컨트롤러 등록
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

	// 텔레포트 진행여부
	bool bTeleporting = false;

	// 텔레포트 리셋
	bool ResetTeleport();

	void TeleportStart(const struct FInputActionValue& Values);
	void TeleportEnd(const struct FInputActionValue& Values);

	// 직선 텔레포트 그리기
	void DrawTeleportStraight();

	// 텔레포트 위치
	FVector TeleportLocation;

public: // Curve Teleport
	// P = P0 + vt
	// v = v0 + at
	// F = ma
	// 곡선을 이루는 점의 개수(곡선의 부드럽기 정도)
	UPROPERTY(EditAnywhere, Category="Teleport")
	int LineSmooth = 40;
	UPROPERTY(EditAnywhere, Category="Teleport")
	float CurveForce = 1500;
	// 중력가속도
	UPROPERTY(EditAnywhere, Category="Teleport")
	float Gravity = -5000;
	// Delta Time
	UPROPERTY(EditAnywhere, Category="Teleport")
	float SimulateTime = 0.02f;

	// 기억할 점 리스트
	TArray<FVector> Lines;

	// 텔레포트 모드전환(Curve or not)
	UPROPERTY(EditAnywhere, Category="Teleport")
	bool bTeleportCurve = true;

	// 곡선텔레포트 그리기
	void DrawTeleportCurve();

	bool CheckHitTeleport(FVector LastPos, FVector& CurPos);

public:	// Teleport UI
	UPROPERTY(VisibleAnywhere)
	class UNiagaraComponent* TeleportUIComponent;

private:	// Warp
	// Warp 활성화 여부
	UPROPERTY(EditAnywhere, Category="Warp", meta = (AllowPrivateAccess=true))
	bool IsWarp = true;
	// 1. 등속 -> 정해진 시간에 맞춰 이동하기
	// 경과시간
	float CurrentTime = 0;
	// 워프타임
	UPROPERTY(EditAnywhere, Category="Warp", meta = (AllowPrivateAccess=true))
	float WarpTime = 0.2f;

	FTimerHandle WarpTimer;
	// 워프수행함수
	void DoWarp();

public:		// 총쏘기
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Fire;
	void FireInput(const struct FInputActionValue& Values);

	UPROPERTY(VisibleAnywhere)
	class UChildActorComponent* CrosshairComp;

	// 크로스헤어 그리기
	void DrawCrosshair();

public:		// 잡기
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* IA_Grab;

	// 물체를 잡고있는지 여부
	bool bIsGrabbing = false;

	// 필요속성 : 잡을 범위
	UPROPERTY(EditAnywhere, Category="Grab")
	float GrabRadius = 100;

	// 잡은 물체 기억할 변수
	UPROPERTY()
	class UPrimitiveComponent* grabbedObject;

	void TryGrab(const struct FInputActionValue& Values);
	void TryUnGrab(const struct FInputActionValue& Values);
	// 물체를 잡은 상태로 컨트롤 하기
	void Grabbing();
};
