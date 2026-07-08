// GTAIUWSettings.h
// Settings menu with tabbed pages: Controls / Audio / Graphics / Interface /
// Accessibility. Writes to GTAI developer settings + DPI manager. See design 5.2.
// Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWSettings.generated.h"

UENUM(BlueprintType)
enum class EGTAIUISettingsTab : uint8
{
    Controls, Audio, Graphics, Interface, Accessibility
};

UCLASS()
class GTAIUI_API UGTAIUWSettings : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UWidgetSwitcher> TabSwitcher;

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SelectTab(EGTAIUISettingsTab Tab);

    // --- Interface tab hooks ---
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetApplicationScale(float Scale); // -> UGTAI_DPIManager

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetRadarMode(EGTAIRadarMode Mode); // -> Map VM

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetMapLayerEnabled(FGameplayTag Layer, bool bEnabled);

    // --- Accessibility tab hooks ---
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetColorblindMode(int32 Mode); // 0 none,1 deu,2 pro,3 tri

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetSubtitleScale(float Scale);

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Settings")
    void SetReduceMotion(bool bReduce);

protected:
    EGTAIUISettingsTab CurrentTab = EGTAIUISettingsTab::Controls;
};
