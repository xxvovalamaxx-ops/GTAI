// Copyright GTAI. All Rights Reserved.
// STRIKE — Cover System Implementation

#include "GTA7CoverSystem.h"
#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void UGTA7CoverSystem::BeginPlay()
{
    OwnerCharacter = Cast<AGTA7Character>(GetOwner());
}

bool UGTA7CoverSystem::TryEnterCover()
{
    if (!OwnerCharacter || IsInCover()) return false;

    FVector CoverNormal, CoverPosition;
    if (!DetectCover(CoverNormal, CoverPosition)) return false;

    // Snap to cover position
    OwnerCharacter->SetActorLocation(CoverPosition);

    // Face away from cover
    FRotator CoverRot = FRotationMatrix::MakeFromX(-CoverNormal).Rotator();
    OwnerCharacter->SetActorRotation(CoverRot);

    bIsInCover = true;
    CurrentCoverNormal = CoverNormal;
    OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);
    OwnerCharacter->bUseControllerRotationYaw = false;

    OnCoverStateChanged.Broadcast(true);

    return true;
}

void UGTA7CoverSystem::ExitCover()
{
    if (!OwnerCharacter || !IsInCover()) return;

    bIsInCover = false;
    CurrentCoverNormal = FVector::ZeroVector;
    OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    OwnerCharacter->bUseControllerRotationYaw = true;

    // Step slightly away from cover
    FVector ExitPos = OwnerCharacter->GetActorLocation() - CurrentCoverNormal * 150.f;
    OwnerCharacter->SetActorLocation(ExitPos);

    OnCoverStateChanged.Broadcast(false);
}

void UGTA7CoverSystem::PeekFromCover(float PeekAmount)
{
    if (!OwnerCharacter || !IsInCover()) return;
    CurrentPeekAmount = FMath::Clamp(PeekAmount, -1.f, 1.f);

    // Offset character from cover normal by peek amount
    FVector BasePos = CoverEntryPosition;
    FVector PerpDir = FVector::CrossProduct(CurrentCoverNormal, FVector::UpVector);
    FVector PeekOffset = PerpDir * PeekAmount * PeekDistance;
    OwnerCharacter->SetActorLocation(BasePos + PeekOffset);
}

bool UGTA7CoverSystem::DetectCover(FVector& OutNormal, FVector& OutPosition) const
{
    if (!OwnerCharacter) return false;

    FVector CharPos = OwnerCharacter->GetActorLocation();
    FVector CharForward = OwnerCharacter->GetActorForwardVector();

    // Sphere sweep forward to find cover surface
    static const float SearchDistance = 200.f;
    static const float SphereRadius = 30.f;
    static const float CoverHeight = 100.f;

    FVector Start = CharPos - FVector(0.f, 0.f, OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f);
    FVector End = Start + FVector(0.f, 0.f, CoverHeight);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    // Check at multiple heights for a cover surface
    for (float Height = 0.f; Height <= CoverHeight; Height += CoverHeight * 0.33f)
    {
        FVector ProbeStart = CharPos + FVector(0.f, 0.f, Height);
        FVector ProbeEnd = ProbeStart + CharForward * SearchDistance;

        if (GetWorld()->LineTraceSingleByChannel(Hit, ProbeStart, ProbeEnd, ECC_WorldStatic, Params))
        {
            // Check if surface is roughly vertical (a wall, not floor/ceiling)
            float SurfaceAngle = FMath::Abs(FVector::DotProduct(Hit.Normal, FVector::UpVector));
            if (SurfaceAngle < 0.3f) // Less than ~17 degrees from horizontal = wall
            {
                OutNormal = Hit.Normal;
                OutPosition = Hit.Location - CharForward * 50.f; // Slightly off the wall
                return true;
            }
        }
    }

    return false;
}

void UGTA7CoverSystem::SlideAlongCover(FVector Direction)
{
    if (!OwnerCharacter || !IsInCover()) return;

    // Project movement direction onto cover plane
    FVector CoverRight = FVector::CrossProduct(CurrentCoverNormal, FVector::UpVector);
    float SlideAmount = FVector::DotProduct(Direction, CoverRight);

    if (FMath::Abs(SlideAmount) > 0.1f)
    {
        FVector SlideDir = CoverRight * FMath::Sign(SlideAmount);
        FVector NewPos = OwnerCharacter->GetActorLocation() + SlideDir * SlideSpeed * GetWorld()->GetDeltaSeconds();
        OwnerCharacter->SetActorLocation(NewPos);
    }
}

bool UGTA7CoverSystem::CanVault() const
{
    if (!OwnerCharacter) return false;

    FVector Start = OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorForwardVector() * 50.f + FVector(0, 0, 80.f);
    FVector End = Start + FVector(0, 0, -120.f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    {
        // Obstacle is vaultable if it's between knee and waist height
        float ObstacleHeight = Hit.Location.Z - OwnerCharacter->GetActorLocation().Z;
        return ObstacleHeight > 40.f && ObstacleHeight < 120.f;
    }

    return false;
}
