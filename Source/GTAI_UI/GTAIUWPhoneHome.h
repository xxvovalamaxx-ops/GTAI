// GTAIUWPhoneHome.h
// Phone home screen: status bar, app grid (WrapBox of UGTAIUWAppIcon), and a
// persistent dock. Swipe/LB-RB pages. See design doc 3.2. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWPhoneHome.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWPhoneHome : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UWrapBox> AppGrid;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UHorizontalBox> Dock;

    /** Populate grid+dock from a data-table of installed apps. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Phone")
    void BuildAppGrid(const TArray<TSubclassOf<UGTAIUWAppBase>>& Apps);
};
