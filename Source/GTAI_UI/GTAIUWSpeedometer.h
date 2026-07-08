// GTAIUWSpeedometer.h
// Vehicle speedometer: numeric MPH + Slate arc gauge. Visible only in vehicle.
// See design doc 2.7. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWSpeedometer.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWSpeedometer : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> SpeedLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> GearLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UProgressBar> FuelBar;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetSpeedMph(float Mph);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetGear(int32 Gear);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetFuelPercent(float Percent);
};
