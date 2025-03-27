// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayer.h"
#include "Camera/CameraComponent.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputActionValue.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputSubsystems.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputComponent.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputAction.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputMappingContext.h"
#include "MotionControllerComponent.h"
#include "../../../../Plugins/FX/Niagara/Source/Niagara/Public/NiagaraComponent.h"
#include "../../../../Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Haptics/HapticFeedbackEffect_Curve.h"
#include "Components/WidgetInteractionComponent.h"

// Sets default values
AVRPlayer::AVRPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(RootComponent);

	// 모션컨트롤러 컴포넌트 추가
	LeftHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(RootComponent);
	LeftHand->SetTrackingMotionSource(TEXT("Left"));

	RightHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(RootComponent);
	RightHand->SetTrackingMotionSource(TEXT("Right"));

	// Aim
	RightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightAim"));
	RightAim->SetupAttachment(RootComponent);
	RightAim->SetTrackingMotionSource(TEXT("RightAim"));


	ConstructorHelpers::FObjectFinder<UInputMappingContext> TempIMC(TEXT("'/Game/Input/IMC_VRInput.IMC_VRInput'"));
	if (TempIMC.Succeeded())
	{
		IMC_VRInput = TempIMC.Object;
	}

	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Move(TEXT("'/Game/Input/IA_VRMove.IA_VRMove'"));
	if( TempIA_Move.Succeeded() )
	{
		IA_Move = TempIA_Move.Object;
	}

	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Mouse(TEXT("'/Game/Input/IA_VRMouse.IA_VRMouse'"));
	if( TempIA_Mouse.Succeeded() )
	{
		IA_Mouse = TempIA_Mouse.Object;
	}

	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Teleport(TEXT("'/Game/Input/IA_VRTeleport.IA_VRTeleport'"));
	if( TempIA_Teleport.Succeeded() )
	{
		IA_Teleport = TempIA_Teleport.Object;
	}

	// 총쏘기
	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Fire(TEXT("'/Game/Input/IA_VRFire.IA_VRFire'"));
	if( TempIA_Fire.Succeeded() )
	{
		IA_Fire = TempIA_Fire.Object;
	}

	// 잡기
	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Grab(TEXT("'/Game/Input/IA_VRGrab.IA_VRGrab'"));
	if( TempIA_Grab.Succeeded() )
	{
		IA_Grab = TempIA_Grab.Object;
	}

	TeleportCircle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportCircle"));
	TeleportCircle->SetupAttachment(RootComponent);

	// Teleport UI
	TeleportUIComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportUIComponent"));
	TeleportUIComponent->SetupAttachment(RootComponent);

	CrosshairComp = CreateDefaultSubobject<UChildActorComponent>(TEXT("CrosshairComp"));
	CrosshairComp->SetupAttachment(RootComponent);
	ConstructorHelpers::FClassFinder<AActor> TempCrosshair(TEXT("'/Game/Blueprints/BP_Crosshair.BP_Crosshair_C'"));
	if (TempCrosshair.Succeeded())
	{
		CrosshairComp->SetChildActorClass(TempCrosshair.Class);
	}

	ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Curve> TempHaptic(TEXT("'/Game/Haptics/HF_Fire.HF_Fire'"));
	if( TempHaptic.Succeeded() )
	{
		FireHaptic = TempHaptic.Object;
	}

	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RightAim);
}

// Called when the game starts or when spawned
void AVRPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	ResetTeleport();

	// HMD 가 연결되어 있으면, HMD 의 Tracking 위치를 조절해보자
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::View);
		//UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(90);
	}
}

// Called every frame
void AVRPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawCrosshair();

	Grabbing();

	DrawDebugRemoteGrab();

	// 텔레포트 활성화시 처리
	if (bTeleporting == true)
	{
		// 텔레포트 그리기
		if (bTeleportCurve)
		{
			DrawTeleportCurve();
		}
		else
		{
			DrawTeleportStraight();
		}

		// 나이아가라커브가 보여지면 데이터 세팅하자
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(TeleportUIComponent, TEXT("User.PointArray"), Lines);
	}
}

// Called to bind functionality to input
void AVRPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		auto LocalPlayer = PC->GetLocalPlayer();
		auto SS = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
		if (SS)
		{
			SS->AddMappingContext(IMC_VRInput, 1);
			SS->AddMappingContext(IMC_Hand, 2);
		}
	}

	auto InputSystem = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (InputSystem)
	{
		InputSystem->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AVRPlayer::Move);
		InputSystem->BindAction(IA_Mouse, ETriggerEvent::Triggered, this, &AVRPlayer::Turn);

		// 텔레포트
		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Started, this, &AVRPlayer::TeleportStart);
		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Completed, this, &AVRPlayer::TeleportEnd);

		// 총쏘기
		InputSystem->BindAction(IA_Fire, ETriggerEvent::Started, this, &AVRPlayer::FireInput);
		InputSystem->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AVRPlayer::ReleaseUIInput);

		// 잡기
		InputSystem->BindAction(IA_Grab, ETriggerEvent::Started, this, &AVRPlayer::TryGrab);
		InputSystem->BindAction(IA_Grab, ETriggerEvent::Completed, this, &AVRPlayer::TryUnGrab);

	}
}

void AVRPlayer::Move(const FInputActionValue& Values)
{
	FVector2d Scale = Values.Get<FVector2d>();
	//FVector Direction = VRCamera->GetForwardVector() * Scale.X + VRCamera->GetRightVector() * Scale.Y;
	//FVector Direction = FVector(Scale.X, Scale.Y, 0);
	//VRCamera->GetComponentTransform().TransformVector(Direction);
	//AddMovementInput(Direction);
	AddMovementInput(VRCamera->GetForwardVector(), Scale.X);
	AddMovementInput(VRCamera->GetRightVector(), Scale.Y);
}

void AVRPlayer::Turn(const FInputActionValue& Values)
{
	if( bUsingMouse  == false)
	{
		return;	
	}
	FVector2d Scale = Values.Get<FVector2d>();
	AddControllerPitchInput(Scale.Y);
	AddControllerYawInput(Scale.X);
}

void AVRPlayer::ActiveDebugDraw()
{
	bIsDebugDraw = !bIsDebugDraw;
}

// 텔레포트를 설정을 초기화
// 현재 텔레포트가 가능한지도 결과로 넘겨주자
bool AVRPlayer::ResetTeleport()
{
	// 현재 텔레포트 써클이 보여지고 있으면 이동가능
	// 그렇지 않으면 NO
	bool bCanTeleprot = TeleportCircle->GetVisibleFlag();
	// 텔레포트 종료
	bTeleporting = false;
	TeleportCircle->SetVisibility(false);
	TeleportUIComponent->SetVisibility(false);

	return bCanTeleprot;
}

void AVRPlayer::TeleportStart(const FInputActionValue& Values)
{
	bTeleporting = true;
	TeleportUIComponent->SetVisibility(true);
}

void AVRPlayer::TeleportEnd(const FInputActionValue& Values)
{
	// 텔레포트가 가능하지 않으면
	if(ResetTeleport() == false)
	{
		// 아무처리하지 않는다.
		return;
	}
	
	// 이동
	if(IsWarp)
	{
		DoWarp();
	}
	else
	{
		SetActorLocation(TeleportLocation);
	}
}

void AVRPlayer::DrawTeleportStraight()
{
	// 1. Line 을 만들기
	FVector StartPoint = RightHand->GetComponentLocation();
	FVector EndPoint = StartPoint + RightHand->GetForwardVector() * 1000;
	// 2. Line 을 쏘기
	bool bHit = CheckHitTeleport(StartPoint, EndPoint);
	
	Lines.Empty();
	Lines.Add(StartPoint);
	Lines.Add(EndPoint);

	//// 선그리기
	//DrawDebugLine(GetWorld(), StartPoint, EndPoint, FColor::Red, false, -1, 0, 1);
	//if(bIsDebugDraw)
	//	DrawDebugSphere(GetWorld(), StartPoint,  200, 20, FColor::Yellow);
}

void AVRPlayer::DrawTeleportCurve()
{
	// 비워줘야한다.
	Lines.Empty();
	// 선이 진행될 힘(방향)
	FVector Velocity = RightHand->GetForwardVector() * CurveForce;
	// P0
	FVector Pos = RightHand->GetComponentLocation();
	Lines.Add(Pos);
	// 40번 반복하겠다.
	for(int i=0;i<LineSmooth;i++)
	{
		FVector LastPos = Pos;
		// v = v0 + at
		Velocity += FVector::UpVector * Gravity * SimulateTime;
		// P = P0 + vt
		Pos += Velocity * SimulateTime;

		bool bHit = CheckHitTeleport(LastPos, Pos);
		// 부딪혔을 때 반복 중단
		Lines.Add(Pos);

		if (bHit)
		{
			break;
		}
	}

	/*int LineCount = Lines.Num();
	for (int i=0;i<LineCount-1;i++)
	{
		DrawDebugLine(GetWorld(), Lines[i], Lines[i + 1], FColor::Red, false, -1, 0, 1);
	}*/
}

bool AVRPlayer::CheckHitTeleport(FVector LastPos, FVector& CurPos)
{
	FHitResult HitInfo;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, LastPos, CurPos, ECC_Visibility, Params);
	// 3. Line 과 부딪혔다면
	// 4. 그리고 부딪힌 녀석의 이름에 Floor 가 있다면

	if( bHit && HitInfo.GetActor()->GetActorNameOrLabel().Contains("GroundFloor") == true )
	{
		// 텔레포트 UI 활성화
		TeleportCircle->SetVisibility(true);
		// -> 부딪힌 지점에 텔레포트 써클 위치시키기
		TeleportCircle->SetWorldLocation(HitInfo.Location);

		// 텔레포트 위치 지정
		TeleportLocation = HitInfo.Location;

		CurPos = TeleportLocation;
	}
	// 4. 안부딪혔으면
	else
	{
		// -> 써클 안그려지게 하기
		TeleportCircle->SetVisibility(false);
	}
	return bHit;
}

void AVRPlayer::DoWarp()
{
	// 1. 워프 활성화 되어 있을 때만 수행
	if (IsWarp == false)
	{
		return;
	}
	// 2. 일정시간동안 정해진 위치로 이동하고 싶다.
	CurrentTime = 0;
	// 충돌체 꺼주자
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	GetWorldTimerManager().SetTimer(WarpTimer, FTimerDelegate::CreateLambda(
		[this]()
		{
			// From -> To 로 Percent 를 이용해 이동한다.
			// 만약 목적지에 거의 도착하면 도착한것으로 처리한다.
			// 2.1 시간이 흘러야 
			//CurrentTime += GetWorld()->DeltaTimeSeconds;
			// 2.2 warp time 까지 이동하고 싶다.
			// target
			FVector TargetPos = TeleportLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			// 현재위치
			FVector CurPos = GetActorLocation();
			// 2.3 이동하고 싶다.
			CurPos = FMath::Lerp(CurPos, TargetPos, 10 * GetWorld()->DeltaTimeSeconds);
			SetActorLocation(CurPos);

			// 목적지에 도착했다면
			// -> 일정 범위안에 들어왔다면 (구충돌, 원충돌)
			//if(CurrentTime - WarpTime >= 0)
			float Distance = FVector::Distance(CurPos, TargetPos);
			if(Distance < 10)
			{
				// -> 목적지 위치로 위치 보정
				SetActorLocation(TargetPos);
				// -> 타이머 끄기
				GetWorldTimerManager().ClearTimer(WarpTimer);
				// -> 충돌체 다시 활성화
				GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	), 0.02f, true);
}

void AVRPlayer::FireInput(const FInputActionValue& Values)
{
	// UI Interaction
	WidgetInteractionComp->PressPointerKey(EKeys::LeftMouseButton);

	// haptic 재생
	auto PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		PC->PlayHapticEffect(FireHaptic, EControllerHand::Right);
	}


	FVector StartPos = RightAim->GetComponentLocation();
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000;
	FHitResult HitInfo;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, StartPos, EndPos, ECC_Visibility, Params);
	if (bHit)
	{
		// 부딪힌 녀석이 물리기능이 활성화 되어 있으면 날려보내자
		auto HitComp = HitInfo.GetComponent();
		if (HitComp && HitComp->IsSimulatingPhysics())
		{
			HitComp->AddImpulseAtLocation(RightAim->GetForwardVector() * 1000, HitInfo.Location);
		}
	}
}

void AVRPlayer::DrawCrosshair()
{
	FVector StartPos = RightAim->GetComponentLocation();
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000;
	FHitResult HitInfo;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);


	DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Yellow);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, StartPos, EndPos, ECC_Visibility, Params);

	float Distance = 0;
	// 부딪혔을 경우
	if (bHit)
	{	
		CrosshairComp->SetWorldLocation(HitInfo.Location);
		Distance = FVector::Distance(VRCamera->GetComponentLocation(), HitInfo.Location);
	}
	// 그렇지 않을 경우
	else
	{
		CrosshairComp->SetWorldLocation(EndPos);
		Distance = FVector::Distance(VRCamera->GetComponentLocation(), EndPos);
	}

	// 거리 값을 가지고 크기 설정을 해주겠다.
	// 최소 값은 1. 최대는? 위에서 구한값
	Distance = FMath::Max(1, Distance);
	// 크기 설정
	CrosshairComp->SetWorldScale3D(FVector(Distance));

	// 빌보딩
	// ->  카메라쪽으로 바라보도록 하자
	FVector Direction = CrosshairComp->GetComponentLocation() - VRCamera->GetComponentLocation();
	CrosshairComp->SetWorldRotation(Direction.Rotation());
}

// 일정 범위 안에 있는 물체를 잡고 싶다.
void AVRPlayer::TryGrab(const FInputActionValue& Values)
{
	// 원거리 물체 잡기
	if (bIsRemoteGrab)
	{
		RemoteGrab();
		return;
	}
	


	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FVector HandPos = RightHand->GetComponentLocation();
	TArray<FOverlapResult> HitObjects;
	bool bHit = GetWorld()->OverlapMultiByChannel(HitObjects, HandPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(GrabRadius), Params);

	// 충돌한 물체가 없으면 아무처리하지 않는다.
	if (bHit == false)
	{
		return;
	}
	
	// 가장 가까운 물체를 검출하자.
	int Closest = -1;
	// 손과 가장 가까운 물체를 검색
	for (int i=0; i< HitObjects.Num();i++)
	{
		// 물체를 던지기를 하고싶다.
		// 물리가 활성화 되어 있는 물체만 검출
		auto HitComp = HitObjects[i].GetComponent();
		if (HitComp->IsSimulatingPhysics() == false)
		{
			continue;
		}

		if (Closest == -1)
		{
			Closest = i;
		}

		// 물체를 잡고 있는 상태로 설정
		bIsGrabbing = true;

		// 현재 가장 손과  가까운 위치
		FVector ClosestPos = HitObjects[Closest].GetActor()->GetActorLocation();
		float ClosestDistance = FVector::Distance(ClosestPos, HandPos);

		// 다음 물체와의 거리
		FVector NextPos = HitObjects[i].GetActor()->GetActorLocation();
		float NextDistance = FVector::Distance(NextPos, HandPos);

		// 다음 물체가 더 손과 가까우면
		if(NextDistance < ClosestDistance)
		{
			// -> 가장 가까운 물체 인덱스 교체
			Closest = i;
		}
	}

	// 물체를 잡았다면
	if(bIsGrabbing)
	{
		grabbedObject = HitObjects[Closest].GetComponent();
		// 붙이기 전에 먼저 물리기능 꺼주자.
		grabbedObject->SetSimulatePhysics(false);
		// 충돌체는? 꺼주자
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitObjects[Closest].GetActor()->SetActorLocation(HandPos);
		// -> 손에 붙여주자
		grabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);

		// 초기값 설정
		PrePos = RightHand->GetComponentLocation();
		PreRot = RightHand->GetComponentQuat();

		UE_LOG(LogTemp, Warning, TEXT("Grab!!!!!!!!!!!!!!!!!"));
	}
}

void AVRPlayer::TryUnGrab(const FInputActionValue& Values)
{
	// 물체를 잡고 있지 않다면
	if (bIsGrabbing == false)
	{
		return;
	}

	bIsGrabbing = false;
	// 손에서 떼주자.
	// 손에 붙어 있는 물체를 Detach 해주자.
	grabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// 물리기능 활성화
	grabbedObject->SetSimulatePhysics(true);
	// 충돌체 활성화
	grabbedObject->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// 던지기
	grabbedObject->AddImpulse(ThrowDirection * ThrowPower, NAME_None, true);

	// 회전시키기
	// 각속도 = (1 / dt) * d@ * axis
	float Angle;
	FVector Axis;
	DeltaRotation.ToAxisAndAngle(Axis, Angle);
	FVector AngularVelocity = (1.0f / GetWorld()->DeltaTimeSeconds) * Angle * Axis;

	// 1. 토크값을 직접 넣어줄때
	//grabbedObject->AddTorqueInRadians(AngularVelocity * ToquePower);

	// 2. 물리에 각속도 넣기
	grabbedObject->SetPhysicsAngularVelocityInRadians(AngularVelocity, true);

	grabbedObject = nullptr;
}

// 던질 방향, 회전값 구하기
// 이전위치 -> 현재위치 둘간의 차를 이용해 방향을 구하기
void AVRPlayer::Grabbing()
{
	// 잡은게 없으면?
	if (bIsGrabbing == false)
	{
		return;
	}

	// 던질방향
	ThrowDirection = RightHand->GetComponentLocation() - PrePos;

	// 회전변화량 구하기
	// 공식
	// angle1 = Q1, angle2 = Q2
	// angle1 + angle2 = Q1 * Q2
	// -angle2 = Q2.Inverse()
	// angle2 + (- angle1) = Q2 *  Q1.Inverse()
	DeltaRotation = RightHand->GetComponentQuat() * PreRot.Inverse();

	PrePos = RightHand->GetComponentLocation();
	PreRot = RightHand->GetComponentQuat();
}

void AVRPlayer::RemoteGrab()
{
	// 충돌체크
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightHand);
	FVector HandPos = RightAim->GetComponentLocation();
	FVector EndPos = HandPos + RightAim->GetForwardVector() * 2000;

	FHitResult HitInfo;
	bool bHit = GetWorld()->SweepSingleByChannel(HitInfo, HandPos, EndPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(20), Params);

	if (bHit && HitInfo.GetComponent()->IsSimulatingPhysics())
	{
		// 1. 잡은 상태로 전환
		bIsGrabbing = true;
		// 2. 잡은 물체를 기억
		grabbedObject = HitInfo.GetComponent();
		// 3. 물리기능 비활성화
		grabbedObject->SetSimulatePhysics(false);
		// 4. 충돌 비활성화
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 5. 컴포넌트 손에 붙이기
		grabbedObject->AttachToComponent(RightHand,FAttachmentTransformRules::KeepWorldTransform);
		// 물체가 끌려오도록 타이머를 돌리자
		auto RemoteGrabFunc = [this]()
		{
			// 잡은 물체가 없으면 타이머 종료
			if (grabbedObject == nullptr)
			{
				GetWorldTimerManager().ClearTimer(GrabHandle);
				return;
			}

			FVector Pos = grabbedObject->GetComponentLocation();
			Pos = FMath::Lerp(Pos, RightHand->GetComponentLocation(), 20 * GetWorld()->DeltaRealTimeSeconds);

			grabbedObject->SetWorldLocation(Pos);

			// 대충 도착했으면 종료해주자
			float Dist = FVector::Distance(Pos, RightHand->GetComponentLocation());
			if (Dist < 10)
			{
				grabbedObject->SetWorldLocation(RightHand->GetComponentLocation());
				GetWorldTimerManager().ClearTimer(GrabHandle);

				// grab 초기값 설정
				PrePos = RightHand->GetComponentLocation();
				PreRot = RightHand->GetComponentQuat();
			}
		};
		GetWorldTimerManager().SetTimer(GrabHandle, FTimerDelegate::CreateLambda(RemoteGrabFunc), 0.02f, true);
	}
}

void AVRPlayer::DrawDebugRemoteGrab()
{
	if (bIsRemoteGrab == false || bIsDrawDebugRemoteGrab == false)
	{
		return;
	}

	// 충돌체크
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightHand);
	FVector HandPos = RightAim->GetComponentLocation();
	FVector EndPos = HandPos + RightAim->GetForwardVector() * 2000;

	FHitResult HitInfo;
	bool bHit = GetWorld()->SweepSingleByChannel(HitInfo, HandPos, EndPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(20), Params);
	DrawDebugSphere(GetWorld(), HandPos, 20, 20, FColor::Cyan);
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), HitInfo.Location, 20, 20, FColor::Cyan);
	}
}

void AVRPlayer::ReleaseUIInput(const FInputActionValue& Values)
{
	WidgetInteractionComp->ReleasePointerKey(EKeys::LeftMouseButton);
}

