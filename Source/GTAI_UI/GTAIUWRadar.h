// GTAIUWRadar.h
// GTA V-style circular radar. Custom Slate-drawn widget (OnPaint) with a
// circular clip mask, rotating/north-up world, zoom, optional sweep, and
// blip rendering from UGTAIViewModel_World. Off-screen entities project to
// edge arrows. See design doc section 4. Under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWRadar.generated.h"

class SGTAIRadar;
class UGTAIViewModel_World;
class UGTAIViewModel_Map;

UCLASS()
class GTAIUI_API UGTAIUWRadar : public UGTAIUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Source of blips + rotation state. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Radar")
    void BindViewModels(UGTAIViewModel_World* WorldVM, UGTAIViewModel_Map* MapVM);

    /** Radius (Slate units) of the circular radar. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Radar")
    float RadarRadius = 160.f;

    /** Optional sweep arc animation period (seconds); 0 disables. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Radar")
    float SweepPeriod = 4.f;

    /** Throttle for blip recompute (Hz). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Radar")
    float UpdateHz = 18.f;

protected:
    // UWidget interface.
    virtual TSharedRef<SWidget> RebuildWidget() override;

    /** Map a blip type to its on-radar color. */
    static FLinearColor BlipColor(EGTAIBlipType Type);

    UPROPERTY()
    TObjectPtr<UGTAIViewModel_World> WorldVM;

    UPROPERTY()
    TObjectPtr<UGTAIViewModel_Map> MapVM;

    TSharedPtr<SGTAIRadar> MyRadar;
    float AccumulatedTime = 0.f;
    float SweepAngle = 0.f;
    float BlipAccumulator = 0.f;
};
