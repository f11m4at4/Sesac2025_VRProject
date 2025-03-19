// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayer.h"
#include "Camera/CameraComponent.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputActionValue.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputSubsystems.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputComponent.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputAction.h"
#include "../../../../Plugins/EnhancedInput/Source/EnhancedInput/Public/InputMappingContext.h"

// Sets default values
AVRPlayer::AVRPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(RootComponent);

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

	TeleportCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportCircle"));
	TeleportCircle->SetupAttachment(RootComponent);
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

		// �ڷ���Ʈ
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

	return bCanTeleprot;
}

void AVRPlayer::TeleportStart(const FInputActionValue& Values)
{
	bTeleporting = true;
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
	SetActorLocation(TeleportLocation);
}

void AVRPlayer::DrawTeleportStraight()
{
	// 1. Line �� �����
	FVector StartPoint = VRCamera->GetComponentLocation();
	FVector EndPoint = StartPoint + VRCamera->GetForwardVector() * 1000;
	// 2. Line �� ���
	FHitResult HitInfo;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, StartPoint, EndPoint, ECC_Visibility, Params);
	// 3. Line �� �ε����ٸ�
	// 4. �׸��� �ε��� �༮�� �̸��� Floor �� �ִٸ�

	if(bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hit Object : %s"), *HitInfo.GetActor()->GetActorNameOrLabel());

	}
	if(bHit && HitInfo.GetActor()->GetActorNameOrLabel().Contains("GroundFloor") == true)
	{
		// �ڷ���Ʈ UI Ȱ��ȭ
		TeleportCircle->SetVisibility(true);
		// -> �ε��� ������ �ڷ���Ʈ ��Ŭ ��ġ��Ű��
		TeleportCircle->SetWorldLocation(HitInfo.Location);

		// �ڷ���Ʈ ��ġ ����
		TeleportLocation = HitInfo.Location;
	}
	// 4. �Ⱥε�������
	else
	{
		// -> ��Ŭ �ȱ׷����� �ϱ�
		TeleportCircle->SetVisibility(false);
	}

	// ���׸���
	DrawDebugLine(GetWorld(), StartPoint, EndPoint, FColor::Red, false, -1, 0, 1);
	if(bIsDebugDraw)
		DrawDebugSphere(GetWorld(), StartPoint,  200, 20, FColor::Yellow);
}

void AVRPlayer::DrawTeleportCurve()
{
	// �������Ѵ�.
	Lines.Empty();
	// ���� ����� ��(����)
	FVector Velocity = VRCamera->GetForwardVector() * CurveForce;
	// P0
	FVector Pos = VRCamera->GetComponentLocation();
	Lines.Add(Pos);
	// 40�� �ݺ��ϰڴ�.
	for(int i=0;i<LineSmooth;i++)
	{
		FVector LastPos = Pos;
		// v = v0 + at
		Velocity += FVector::UpVector * Gravity * SimulateTime;
		// P = P0 + vt
		Pos += Velocity * SimulateTime;


		Lines.Add(Pos);
		
	}

	int LineCount = Lines.Num();
	for (int i=0;i<LineCount-1;i++)
	{
		DrawDebugLine(GetWorld(), Lines[i], Lines[i + 1], FColor::Red, false, -1, 0, 1);
	}
}

