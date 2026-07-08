// GTAIUWHealthBar.h
// Health bar with event-driven color thresholds + low-health pulse. Bound to
// UGTAIViewModel_Player.HealthPercent. See design doc 2.2. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWHealthBar.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWHealthBar : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UProgressBar> HealthBar;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> HealthLabel;

    /** Set by the ViewModel binding; triggers color + pulse on threshold cross. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetHealthPercent(float Percent);

protected:
    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    float LowThreshold = 0.3f;

    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    FLinearColor HealthyColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    FLinearColor WarningColor = FLinearColor(1.f, 0.6f, 0.f);

    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    FLinearColor CriticalColor = FLinearColor::Red;

    float LastPercent = 1.f;
};
