// Copyright GTAI. All Rights Reserved.
// VELOCE — Base Vehicle Implementation
// Based on UE5.8 Chaos vehicle best practices:
// - Skeletal mesh + AnimBP eliminates jitter (not pure Geometry Collection)
// - Spring Arm with lag/rotation-lag for smooth chase camera
// - Arcade handling via tuned torque curves, not simulation

#include "GTAI_BaseVehicle.h"
#include "ChaosWheeledVehicleSimulationComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GTAI_VehicleTypes.h"
#include "GTAI_VehicleDamageComponent.h"

AGTAI_BaseVehicle::AGTAI_BaseVehicle(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Chaos vehicle setup
    PrimaryActorTick.bCanEverTick = true;

    // Damage component
    DamageComponent = CreateDefaultSubobject<UGTAI_VehicleDamageComponent>(TEXT("DamageComponent"));

    // Camera boom (spring arm for smooth chase)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetMesh());
    CameraBoom->TargetArmLength = CameraBoomLength;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = CameraLagSpeed;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = CameraLagSpeed;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeSize = 12.0f;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->SocketOffset = FVector(0.f, 0.f, 80.f); // Slightly above vehicle

    // Chase camera
    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ChaseCamera->FieldOfView = MinSpeedFOV;

    // Initialize assists — all ON by default for GTA-like arcade feel
    AssistStates.Add(EGTAI_VehicleAssist::TractionControl, true);
    AssistStates.Add(EGTAI_VehicleAssist::ABS, true);
    AssistStates.Add(EGTAI_VehicleAssist::StabilityControl, true);
    AssistStates.Add(EGTAI_VehicleAssist::SteerAssist, true);
    AssistStates.Add(EGTAI_VehicleAssist::DriftAssist, false); // Player toggles for skill
}

void AGTAI_BaseVehicle::BeginPlay()
{
    Super::BeginPlay();
    ApplyHandlingProfile();
}

void AGTAI_BaseVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateCameraFOV(DeltaSeconds);
}

void AGTAI_BaseVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Enhanced Input
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Throttle, steering, braking, handbrake bound in Blueprint per input mapping
        // C++ handles the vehicle response in ApplyHandlingProfile / physics
    }
}

bool AGTAI_BaseVehicle::EnterVehicle(APawn* Pawn, EGTAI_VehicleSeat Seat)
{
    if (Seat != EGTAI_VehicleSeat::Driver || IsValid(DriverPawn))
        return false;

    DriverPawn = Pawn;
    ControlMode = EGTAI_VehicleControlMode::Player;

    if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
    {
        PC->Possess(this);
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            // Switch to vehicle input mapping (IMC_Vehicle from Blueprint)
            Subsystem->AddMappingContext(VehicleInputMapping, 1);
        }
    }

    OnOccupantChanged.Broadcast(Pawn, Seat);
    ApplyHandlingProfile();
    return true;
}

void AGTAI_BaseVehicle::ExitVehicle()
{
    if (!DriverPawn) return;

    FVector ExitLocation = GetActorLocation() + GetActorRightVector() * 250.f;
    FRotator ExitRotation = DriverPawn->GetActorRotation();

    // Detach player
    if (APlayerController* PC = Cast<APlayerController>(DriverPawn->GetController()))
    {
        PC->UnPossess();
        DriverPawn->SetActorLocation(ExitLocation);
        DriverPawn->SetActorRotation(ExitRotation);
        PC->Possess(DriverPawn);
    }

    OnOccupantChanged.Broadcast(DriverPawn, EGTAI_VehicleSeat::Driver);
    DriverPawn = nullptr;
    ControlMode = EGTAI_VehicleControlMode::Idle;
}

void AGTAI_BaseVehicle::ToggleAssist(EGTAI_VehicleAssist Assist, bool bEnabled)
{
    AssistStates.FindOrAdd(Assist) = bEnabled;
    ApplyHandlingProfile();
}

void AGTAI_BaseVehicle::UpdateCameraFOV(float DeltaSeconds)
{
    if (!ChaseCamera) return;

    float Speed = GetVelocity().Size() / 100.f; // cm/s → m/s
    float SpeedRatio = FMath::Clamp(Speed / 50.f, 0.f, 1.f); // 0-50 m/s range
    float TargetFOV = FMath::Lerp(MinSpeedFOV, MaxSpeedFOV, SpeedRatio);
    float CurrentFOV = ChaseCamera->FieldOfView;
    ChaseCamera->FieldOfView = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaSeconds, 3.0f);
}

void AGTAI_BaseVehicle::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other,
    UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
    FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (DamageComponent)
    {
        DamageComponent->ApplyImpactDamage(NormalImpulse.Size(), HitLocation);
    }
}
