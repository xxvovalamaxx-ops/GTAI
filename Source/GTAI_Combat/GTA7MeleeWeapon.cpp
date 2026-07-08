// Copyright GTAI. All Rights Reserved.
// STRIKE — Melee Weapon Implementation
// Short arc / sweep weapon (fists, bat, knife). Trace-based hit detection
// fanned across a forward cone (SweepArcDegrees) out to Reach.

#include "GTA7MeleeWeapon.h"
#include "GTA7Character.h"
#include "GTA7HitDetection.h"
#include "GTA7CombatTypes.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void UGTA7MeleeWeapon::PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation)
{
	if (!OwnerCharacter)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AController* Instigator = OwnerCharacter->GetController();

	// SweepArcDegrees is the half-angle of the swing cone. Fan multiple line
	// traces across the full arc (2x half-angle) to catch anything in range.
	const float HalfDeg = SweepArcDegrees;
	const int32 Samples = FMath::Max(1, FMath::RoundToInt((HalfDeg * 2.f) / 15.f));

	TSet<AGTA7Character*> AlreadyHit;
	FGTA7BoneZoneMap EmptyBoneMap; // default zone mapping (Torso fallback)

	for (int32 i = 0; i < Samples; ++i)
	{
		const float T = (Samples > 1) ? (static_cast<float>(i) / static_cast<float>(Samples - 1)) : 0.5f;
		const float YawOffset = FMath::DegreesToRadians(FMath::Lerp(-HalfDeg, HalfDeg, T));

		FRotator SampleRot = AimRotation;
		SampleRot.Yaw += YawOffset;

		const FVector Dir = SampleRot.Vector();
		const FVector End = MuzzleLocation + Dir * Reach;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(OwnerCharacter);
		Params.bReturnPhysicalMaterial = true;

		if (World->LineTraceSingleByChannel(Hit, MuzzleLocation, End, ECC_GameTraceChannel1, Params))
		{
			if (AGTA7Character* Target = Cast<AGTA7Character>(Hit.GetActor()))
			{
				if (AlreadyHit.Contains(Target))
				{
					continue;
				}
				AlreadyHit.Add(Target);

				const EGTA7HitZone Zone = UGTA7HitDetection::ResolveZone(Hit.BoneName, EmptyBoneMap);
				UGTA7HitDetection::ApplyHitToTarget(
					Target,
					Config.Damage.BaseDamage,
					Zone,
					Config.Damage,
					Instigator,
					OwnerCharacter);
			}
		}

#if !UE_BUILD_SHIPPING
		DrawDebugLine(World, MuzzleLocation, End, FColor::Green, false, 0.2f, 0, 1.f);
#endif
	}

	// Small forward lunge impulse on swing (GTA-style shove forward).
	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		FVector LungeDir = AimRotation.Vector();
		LungeDir.Z = 0.f;
		if (LungeDir.Normalize())
		{
			MoveComp->AddImpulse(LungeDir * LungeImpulse, false);
		}
	}
}
