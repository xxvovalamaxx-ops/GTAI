// Copyright GTAI. All Rights Reserved.
// STRIKE — Projectile Weapon Implementation
// Spawns a physics-driven projectile actor (RPG, grenade launcher, thrown).
// The projectile travels under UProjectileMovementComponent and resolves its
// own damage on impact (direct hit + optional radial explosion).

#include "GTA7ProjectileWeapon.h"
#include "GTA7Character.h"
#include "GTA7HitDetection.h"
#include "GTA7CombatTypes.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"

// ---------------------------------------------------------------------------
// UGTA7ProjectileWeapon
// ---------------------------------------------------------------------------

void UGTA7ProjectileWeapon::PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation)
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

	// Resolve the projectile class from the weapon config (falls back to the
	// base AGTA7Projectile if the DataAsset/BP didn't specify one).
	TSubclassOf<AGTA7Projectile> ProjClass = Config.ProjectileClass.LoadSynchronous();
	if (!ProjClass)
	{
		ProjClass = AGTA7Projectile::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AGTA7Projectile* Projectile = World->SpawnActor<AGTA7Projectile>(
		ProjClass, MuzzleLocation, AimRotation, SpawnParams);

	if (Projectile)
	{
		// Carry over weapon tuning into the projectile.
		Projectile->Damage = Config.Damage;
		Projectile->DirectDamage = Config.Damage.BaseDamage;

		if (Projectile->Movement)
		{
			const FVector LaunchVelocity = AimRotation.Vector() * Projectile->Movement->InitialSpeed;
			Projectile->Movement->Velocity = LaunchVelocity;
		}
	}
}

// ---------------------------------------------------------------------------
// AGTA7Projectile
// ---------------------------------------------------------------------------

AGTA7Projectile::AGTA7Projectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Collision root (projectiles need a moving query/physics body).
	USphereComponent* Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetNotifyRigidBodyCollision(true); // enable Hit events
	RootComponent = Collision;

	// Optional visual mesh (Blueprint can swap the static/skeletal mesh).
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Ballistic movement.
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 3000.f;
	Movement->MaxSpeed = 8000.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 1.f; // grenades arc; RPGs can set 0 in BP
	Movement->bAutoActivate = true;

	// Sensible defaults (overridable in Blueprint/config).
	DirectDamage = 100.f;
	ExplosionRadius = 300.f;
	bExplodes = false;
}

void AGTA7Projectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other,
	UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
	FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// Ignore the instigator (don't blow up on the shooter at spawn).
	if (Other && Other == GetInstigator())
	{
		return;
	}

	AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;

	if (Other)
	{
		if (AGTA7Character* Target = Cast<AGTA7Character>(Other))
		{
			const EGTA7HitZone Zone = UGTA7HitDetection::ResolveZone(Hit.BoneName, FGTA7BoneZoneMap());
			UGTA7HitDetection::ApplyHitToTarget(
				Target, DirectDamage, Zone, Damage, InstigatorController, this);
		}
	}

	// Optional radial explosion (grenade / RPG splash).
	if (bExplodes)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		if (GetWorld()->OverlapMultiByChannel(
			Overlaps, HitLocation, FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(ExplosionRadius), Params))
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AGTA7Character* Target = Cast<AGTA7Character>(Overlap.GetActor()))
				{
					if (Target == GetInstigator())
					{
						continue;
					}

					const float Dist = FVector::Dist(Target->GetActorLocation(), HitLocation);
					const float Falloff = FMath::Clamp(1.f - (Dist / ExplosionRadius), 0.f, 1.f);
					const float SplashDamage = DirectDamage * Falloff;

					UGTA7HitDetection::ApplyHitToTarget(
						Target, SplashDamage, EGTA7HitZone::Torso, Damage, InstigatorController, this);
				}
			}
		}

#if !UE_BUILD_SHIPPING
		DrawDebugSphere(GetWorld(), HitLocation, ExplosionRadius, 16, FColor::Orange, false, 0.5f);
#endif
	}

	Destroy();
}
