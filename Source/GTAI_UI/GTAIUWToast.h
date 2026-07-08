// GTAIUWToast.h
// A single pooled notification toast. Slide+fade in/out via UMG timeline.
// See design doc 9.2. Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWToast.generated.h"

class UGTAI_NotificationData;

UCLASS()
class GTAIUI_API UGTAIUWToast : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> TitleLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> BodyLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UImage> IconImage;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> ActivateButton;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Notify")
    void Show(UGTAI_NotificationData* Data);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Notify")
    void Dismiss();

    /** True while visible/active (used by the pool). */
    bool IsActive() const { return bActive; }

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAIOnToastDismissed, UGTAIUWToast*, Toast);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI|Notify")
    FGTAIOnToastDismissed OnDismissed;

protected:
    UPROPERTY()
    TObjectPtr<UGTAI_NotificationData> SourceData;

    bool bActive = false;
    float LifeRemaining = 0.f;
};
