// GTAI_DPIManager.h
// Resolution-independent UI scaling controller. Encapsulates the UE5.8 DPI
// Scaling system: design resolution, Shortest-Side rule, a curve keyed by
// shortest-side resolution, and a user-adjustable Application Scale.
// See design doc section 1.5.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_UI.h"
#include "GTAI_DPIManager.generated.h"

USTRUCT(BlueprintType)
struct FGTAIResolutionScalePoint
{
    GENERATED_BODY()

    /** Shortest side of the viewport in pixels. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float ShortestSide = 720.f;

    /** Multiplier applied at this resolution. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Scale = 1.f;
};

UCLASS()
class GTAIUI_API UGTAI_DPIManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Authoring resolution the widgets were designed at (Slate units). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    FVector2D GetDesignResolution() const { return DesignResolution; }

    /** Rebuild the engine DPI curve from our scale points. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    void ApplyScalePoints(const TArray<FGTAIResolutionScalePoint>& Points);

    /** Clamp + set the user Application Scale (accessibility/preference). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    void SetApplicationScale(float Scale);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    float GetApplicationScale() const;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    void SetDPIScaleRule(EUIScalingRule Rule) { ScaleRule = Rule; }

    /** Current effective scale (curve * application scale). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|DPI")
    float GetEffectiveScale() const;

    /** Raised after any scale change so widgets can rebuild cached geometry. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGTAIOnScaleChanged);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI|DPI")
    FGTAIOnScaleChanged OnScaleChanged;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|UI|DPI")
    FVector2D DesignResolution = FVector2D(1280.f, 720.f);

    UPROPERTY(EditDefaultsOnly, Category = "GTAI|UI|DPI")
    TArray<FGTAIResolutionScalePoint> ScalePoints;

    UPROPERTY(EditDefaultsOnly, Category = "GTAI|UI|DPI")
    EUIScalingRule ScaleRule = EUIScalingRule::ShortestSide;

    UPROPERTY(EditDefaultsOnly, Category = "GTAI|UI|DPI")
    float MinApplicationScale = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "GTAI|UI|DPI")
    float MaxApplicationScale = 2.0f;

    UPROPERTY()
    TObjectPtr<UCurveFloat> DPIScaleCurve;
};
