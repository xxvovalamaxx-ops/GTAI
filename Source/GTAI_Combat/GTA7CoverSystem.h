// GTA7CoverSystem.h
// Auto-cover detection (surface normal + height checks), attach, lean, vault.
// Namespace: GTA7::Player
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTA7CombatTypes.h"
#include "GTA7CoverSystem.generated.h"

UENUM(BlueprintType)
enum class EGTA7CoverSide : uint8
{
    None, Left, Right
};

USTRUCT(BlueprintType)
struct FGTA7CoverData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    bool bInCover = false;

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    FVector CoverLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    FVector CoverNormal = FVector::ForwardVector; // points away from wall, toward player

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    FVector WallTangent = FVector::RightVector;   // slide direction along wall

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    bool bLowCover = false; // wall shorter than CoverCrouchHeight

    UPROPERTY(BlueprintReadOnly, Category = "Cover")
    EGTA7CoverSide PeekSide = EGTA7CoverSide::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoverChanged, const FGTA7CoverData&, Cover);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoverExit);

/**
 * UGTA7CoverSystem — GTA-style auto cover.
 * 1) probe traces for a wall in front, 2) read surface normal (faces player?),
 * 3) vertical height check (tall enough to hide?), 4) tangent for sliding,
 * 5) edge traces for corner peeking. Detected client-side; server validates.
 */
UCLASS(ClassGroup = (GTAI), BlueprintType, meta = (BlueprintSpawnableComponent))
class GTAI_COMBAT_API UGTA7CoverSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UGTA7CoverSystem();

    // Detection tuning.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
    float ProbeDistance = 80.f;        // forward probe length
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
    float CoverMinHeight = 120.f;      // min wall height to count as cover
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
    float CoverCrouchHeight = 90.f;    // below this -> low cover (peek over)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
    float CoverRadius = 40.f;          // capsule offset from wall
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
    float DetectInterval = 0.1f;       // 10 Hz

    UFUNCTION(BlueprintCallable, Category = "Cover")
    void TryEnterCover();

    UFUNCTION(BlueprintCallable, Category = "Cover")
    void ExitCover();

    UFUNCTION(BlueprintCallable, Category = "Cover")
    bool IsInCover() const { return Cover.bInCover; }

    UFUNCTION(BlueprintCallable, Category = "Cover")
    const FGTA7CoverData& GetCoverData() const { return Cover; }

    // Slide along the wall tangent based on input (-1..1).
    UFUNCTION(BlueprintCallable, Category = "Cover")
    void MoveAlongCover(float Axis);

    // Peek: aim raises character; side chosen by shoulder swap.
    UFUNCTION(BlueprintCallable, Category = "Cover")
    void SetPeek(EGTA7CoverSide Side, bool bPeeking);

    UPROPERTY(BlueprintAssignable, Category = "Cover")
    FOnCoverChanged OnCoverChanged;

    UPROPERTY(BlueprintAssignable, Category = "Cover")
    FOnCoverExit OnCoverExit;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Core detection: returns true if valid cover found and fills OutData.
    bool DetectCover(FGTA7CoverData& OutData) const;

    UPROPERTY()
    FGTA7CoverData Cover;

    UPROPERTY()
    float DetectAccumulator = 0.f;
};
