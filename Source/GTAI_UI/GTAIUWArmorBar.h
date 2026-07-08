// GTAIUWArmorBar.h
// Armor bar (cyan fill). Collapses its slot when armor == 0. See design 2.3.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWArmorBar.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWArmorBar : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UProgressBar> ArmorBar;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetArmorPercent(float Percent);

    /** Hide entirely when no armor (collapse slot, not zero-width). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetHasArmor(bool bHasArmor);
};
