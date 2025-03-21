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

	TeleportCircle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportCircle"));
	TeleportCircle->SetupAttachment(RootComponent);

	// Teleport UI
	TeleportUIComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportUIComponent"));
	TeleportUIComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AVRPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	ResetTeleport();
}

// Called every frame
void AVRPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
			// 2.1 시간이 흘러야 
			CurrentTime += GetWorld()->DeltaTimeSeconds;
			// 2.2 warp time 까지 이동하고 싶다.
			// target
			FVector TargetPos = TeleportLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			// 현재위치
			FVector CurPos = GetActorLocation();
			// 2.3 이동하고 싶다.
			CurPos = FMath::Lerp(CurPos, TargetPos, CurrentTime / WarpTime);
			SetActorLocation(CurPos);

			// 목적지에 도착했다면
			if(CurrentTime - WarpTime >= 0)
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

