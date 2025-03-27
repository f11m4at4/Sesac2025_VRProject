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

	// �����Ʈ�ѷ� ������Ʈ �߰�
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

	// �ѽ��
	ConstructorHelpers::FObjectFinder<UInputAction> TempIA_Fire(TEXT("'/Game/Input/IA_VRFire.IA_VRFire'"));
	if( TempIA_Fire.Succeeded() )
	{
		IA_Fire = TempIA_Fire.Object;
	}

	// ���
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

	// HMD �� ����Ǿ� ������, HMD �� Tracking ��ġ�� �����غ���
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

	// �ڷ���Ʈ Ȱ��ȭ�� ó��
	if (bTeleporting == true)
	{
		// �ڷ���Ʈ �׸���
		if (bTeleportCurve)
		{
			DrawTeleportCurve();
		}
		else
		{
			DrawTeleportStraight();
		}

		// ���̾ư���Ŀ�갡 �������� ������ ��������
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

		// �ڷ���Ʈ
		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Started, this, &AVRPlayer::TeleportStart);
		InputSystem->BindAction(IA_Teleport, ETriggerEvent::Completed, this, &AVRPlayer::TeleportEnd);

		// �ѽ��
		InputSystem->BindAction(IA_Fire, ETriggerEvent::Started, this, &AVRPlayer::FireInput);
		InputSystem->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AVRPlayer::ReleaseUIInput);

		// ���
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

// �ڷ���Ʈ�� ������ �ʱ�ȭ
// ���� �ڷ���Ʈ�� ���������� ����� �Ѱ�����
bool AVRPlayer::ResetTeleport()
{
	// ���� �ڷ���Ʈ ��Ŭ�� �������� ������ �̵�����
	// �׷��� ������ NO
	bool bCanTeleprot = TeleportCircle->GetVisibleFlag();
	// �ڷ���Ʈ ����
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
	// �ڷ���Ʈ�� �������� ������
	if(ResetTeleport() == false)
	{
		// �ƹ�ó������ �ʴ´�.
		return;
	}
	
	// �̵�
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
	// 1. Line �� �����
	FVector StartPoint = RightHand->GetComponentLocation();
	FVector EndPoint = StartPoint + RightHand->GetForwardVector() * 1000;
	// 2. Line �� ���
	bool bHit = CheckHitTeleport(StartPoint, EndPoint);
	
	Lines.Empty();
	Lines.Add(StartPoint);
	Lines.Add(EndPoint);

	//// ���׸���
	//DrawDebugLine(GetWorld(), StartPoint, EndPoint, FColor::Red, false, -1, 0, 1);
	//if(bIsDebugDraw)
	//	DrawDebugSphere(GetWorld(), StartPoint,  200, 20, FColor::Yellow);
}

void AVRPlayer::DrawTeleportCurve()
{
	// �������Ѵ�.
	Lines.Empty();
	// ���� ����� ��(����)
	FVector Velocity = RightHand->GetForwardVector() * CurveForce;
	// P0
	FVector Pos = RightHand->GetComponentLocation();
	Lines.Add(Pos);
	// 40�� �ݺ��ϰڴ�.
	for(int i=0;i<LineSmooth;i++)
	{
		FVector LastPos = Pos;
		// v = v0 + at
		Velocity += FVector::UpVector * Gravity * SimulateTime;
		// P = P0 + vt
		Pos += Velocity * SimulateTime;

		bool bHit = CheckHitTeleport(LastPos, Pos);
		// �ε����� �� �ݺ� �ߴ�
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
	// 3. Line �� �ε����ٸ�
	// 4. �׸��� �ε��� �༮�� �̸��� Floor �� �ִٸ�

	if( bHit && HitInfo.GetActor()->GetActorNameOrLabel().Contains("GroundFloor") == true )
	{
		// �ڷ���Ʈ UI Ȱ��ȭ
		TeleportCircle->SetVisibility(true);
		// -> �ε��� ������ �ڷ���Ʈ ��Ŭ ��ġ��Ű��
		TeleportCircle->SetWorldLocation(HitInfo.Location);

		// �ڷ���Ʈ ��ġ ����
		TeleportLocation = HitInfo.Location;

		CurPos = TeleportLocation;
	}
	// 4. �Ⱥε�������
	else
	{
		// -> ��Ŭ �ȱ׷����� �ϱ�
		TeleportCircle->SetVisibility(false);
	}
	return bHit;
}

void AVRPlayer::DoWarp()
{
	// 1. ���� Ȱ��ȭ �Ǿ� ���� ���� ����
	if (IsWarp == false)
	{
		return;
	}
	// 2. �����ð����� ������ ��ġ�� �̵��ϰ� �ʹ�.
	CurrentTime = 0;
	// �浹ü ������
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	GetWorldTimerManager().SetTimer(WarpTimer, FTimerDelegate::CreateLambda(
		[this]()
		{
			// From -> To �� Percent �� �̿��� �̵��Ѵ�.
			// ���� �������� ���� �����ϸ� �����Ѱ����� ó���Ѵ�.
			// 2.1 �ð��� �귯�� 
			//CurrentTime += GetWorld()->DeltaTimeSeconds;
			// 2.2 warp time ���� �̵��ϰ� �ʹ�.
			// target
			FVector TargetPos = TeleportLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			// ������ġ
			FVector CurPos = GetActorLocation();
			// 2.3 �̵��ϰ� �ʹ�.
			CurPos = FMath::Lerp(CurPos, TargetPos, 10 * GetWorld()->DeltaTimeSeconds);
			SetActorLocation(CurPos);

			// �������� �����ߴٸ�
			// -> ���� �����ȿ� ���Դٸ� (���浹, ���浹)
			//if(CurrentTime - WarpTime >= 0)
			float Distance = FVector::Distance(CurPos, TargetPos);
			if(Distance < 10)
			{
				// -> ������ ��ġ�� ��ġ ����
				SetActorLocation(TargetPos);
				// -> Ÿ�̸� ����
				GetWorldTimerManager().ClearTimer(WarpTimer);
				// -> �浹ü �ٽ� Ȱ��ȭ
				GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	), 0.02f, true);
}

void AVRPlayer::FireInput(const FInputActionValue& Values)
{
	// UI Interaction
	WidgetInteractionComp->PressPointerKey(EKeys::LeftMouseButton);

	// haptic ���
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
		// �ε��� �༮�� ��������� Ȱ��ȭ �Ǿ� ������ ����������
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
	// �ε����� ���
	if (bHit)
	{	
		CrosshairComp->SetWorldLocation(HitInfo.Location);
		Distance = FVector::Distance(VRCamera->GetComponentLocation(), HitInfo.Location);
	}
	// �׷��� ���� ���
	else
	{
		CrosshairComp->SetWorldLocation(EndPos);
		Distance = FVector::Distance(VRCamera->GetComponentLocation(), EndPos);
	}

	// �Ÿ� ���� ������ ũ�� ������ ���ְڴ�.
	// �ּ� ���� 1. �ִ��? ������ ���Ѱ�
	Distance = FMath::Max(1, Distance);
	// ũ�� ����
	CrosshairComp->SetWorldScale3D(FVector(Distance));

	// ������
	// ->  ī�޶������� �ٶ󺸵��� ����
	FVector Direction = CrosshairComp->GetComponentLocation() - VRCamera->GetComponentLocation();
	CrosshairComp->SetWorldRotation(Direction.Rotation());
}

// ���� ���� �ȿ� �ִ� ��ü�� ��� �ʹ�.
void AVRPlayer::TryGrab(const FInputActionValue& Values)
{
	// ���Ÿ� ��ü ���
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

	// �浹�� ��ü�� ������ �ƹ�ó������ �ʴ´�.
	if (bHit == false)
	{
		return;
	}
	
	// ���� ����� ��ü�� ��������.
	int Closest = -1;
	// �հ� ���� ����� ��ü�� �˻�
	for (int i=0; i< HitObjects.Num();i++)
	{
		// ��ü�� �����⸦ �ϰ�ʹ�.
		// ������ Ȱ��ȭ �Ǿ� �ִ� ��ü�� ����
		auto HitComp = HitObjects[i].GetComponent();
		if (HitComp->IsSimulatingPhysics() == false)
		{
			continue;
		}

		if (Closest == -1)
		{
			Closest = i;
		}

		// ��ü�� ��� �ִ� ���·� ����
		bIsGrabbing = true;

		// ���� ���� �հ�  ����� ��ġ
		FVector ClosestPos = HitObjects[Closest].GetActor()->GetActorLocation();
		float ClosestDistance = FVector::Distance(ClosestPos, HandPos);

		// ���� ��ü���� �Ÿ�
		FVector NextPos = HitObjects[i].GetActor()->GetActorLocation();
		float NextDistance = FVector::Distance(NextPos, HandPos);

		// ���� ��ü�� �� �հ� ������
		if(NextDistance < ClosestDistance)
		{
			// -> ���� ����� ��ü �ε��� ��ü
			Closest = i;
		}
	}

	// ��ü�� ��Ҵٸ�
	if(bIsGrabbing)
	{
		grabbedObject = HitObjects[Closest].GetComponent();
		// ���̱� ���� ���� ������� ������.
		grabbedObject->SetSimulatePhysics(false);
		// �浹ü��? ������
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitObjects[Closest].GetActor()->SetActorLocation(HandPos);
		// -> �տ� �ٿ�����
		grabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);

		// �ʱⰪ ����
		PrePos = RightHand->GetComponentLocation();
		PreRot = RightHand->GetComponentQuat();

		UE_LOG(LogTemp, Warning, TEXT("Grab!!!!!!!!!!!!!!!!!"));
	}
}

void AVRPlayer::TryUnGrab(const FInputActionValue& Values)
{
	// ��ü�� ��� ���� �ʴٸ�
	if (bIsGrabbing == false)
	{
		return;
	}

	bIsGrabbing = false;
	// �տ��� ������.
	// �տ� �پ� �ִ� ��ü�� Detach ������.
	grabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// ������� Ȱ��ȭ
	grabbedObject->SetSimulatePhysics(true);
	// �浹ü Ȱ��ȭ
	grabbedObject->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// ������
	grabbedObject->AddImpulse(ThrowDirection * ThrowPower, NAME_None, true);

	// ȸ����Ű��
	// ���ӵ� = (1 / dt) * d@ * axis
	float Angle;
	FVector Axis;
	DeltaRotation.ToAxisAndAngle(Axis, Angle);
	FVector AngularVelocity = (1.0f / GetWorld()->DeltaTimeSeconds) * Angle * Axis;

	// 1. ��ũ���� ���� �־��ٶ�
	//grabbedObject->AddTorqueInRadians(AngularVelocity * ToquePower);

	// 2. ������ ���ӵ� �ֱ�
	grabbedObject->SetPhysicsAngularVelocityInRadians(AngularVelocity, true);

	grabbedObject = nullptr;
}

// ���� ����, ȸ���� ���ϱ�
// ������ġ -> ������ġ �Ѱ��� ���� �̿��� ������ ���ϱ�
void AVRPlayer::Grabbing()
{
	// ������ ������?
	if (bIsGrabbing == false)
	{
		return;
	}

	// ��������
	ThrowDirection = RightHand->GetComponentLocation() - PrePos;

	// ȸ����ȭ�� ���ϱ�
	// ����
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
	// �浹üũ
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightHand);
	FVector HandPos = RightAim->GetComponentLocation();
	FVector EndPos = HandPos + RightAim->GetForwardVector() * 2000;

	FHitResult HitInfo;
	bool bHit = GetWorld()->SweepSingleByChannel(HitInfo, HandPos, EndPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(20), Params);

	if (bHit && HitInfo.GetComponent()->IsSimulatingPhysics())
	{
		// 1. ���� ���·� ��ȯ
		bIsGrabbing = true;
		// 2. ���� ��ü�� ���
		grabbedObject = HitInfo.GetComponent();
		// 3. ������� ��Ȱ��ȭ
		grabbedObject->SetSimulatePhysics(false);
		// 4. �浹 ��Ȱ��ȭ
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 5. ������Ʈ �տ� ���̱�
		grabbedObject->AttachToComponent(RightHand,FAttachmentTransformRules::KeepWorldTransform);
		// ��ü�� ���������� Ÿ�̸Ӹ� ������
		auto RemoteGrabFunc = [this]()
		{
			// ���� ��ü�� ������ Ÿ�̸� ����
			if (grabbedObject == nullptr)
			{
				GetWorldTimerManager().ClearTimer(GrabHandle);
				return;
			}

			FVector Pos = grabbedObject->GetComponentLocation();
			Pos = FMath::Lerp(Pos, RightHand->GetComponentLocation(), 20 * GetWorld()->DeltaRealTimeSeconds);

			grabbedObject->SetWorldLocation(Pos);

			// ���� ���������� ����������
			float Dist = FVector::Distance(Pos, RightHand->GetComponentLocation());
			if (Dist < 10)
			{
				grabbedObject->SetWorldLocation(RightHand->GetComponentLocation());
				GetWorldTimerManager().ClearTimer(GrabHandle);

				// grab �ʱⰪ ����
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

	// �浹üũ
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

