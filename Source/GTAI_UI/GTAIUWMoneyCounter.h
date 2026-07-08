// GTAIUWMoneyCounter.h
// Money readout with a count-up tween on change + floating "+$" toast.
// See design doc 2.5. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWMoneyCounter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAIOnCashChanged, int32, Delta);

UCLASS()
class GTAIUI_API UGTAIUWMoneyCounter : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> CashLabel;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetCash(int32 NewCash); // animates from current to NewCash

    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI|HUD")
    FGTAIOnCashChanged OnCashChanged;

protected:
    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    float CountUpDuration = 0.4f;

    int32 DisplayedCash = 0;
    int32 TargetCash = 0;
    float TweenTime = 0.f;
};
