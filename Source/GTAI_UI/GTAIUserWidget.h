// GTAIUserWidget.h
// Base class for EVERY GTAI user widget. Adds global behavior in one place:
// focus sound hook, safe-area compliance, debug overlay, and a typed
// ViewModel accessor pattern. All UGTAIUW* widgets inherit from this.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUserWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class GTAIUI_API UGTAIUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // UUserWidget interface.
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnInitialized() override;

    /** Play a focus/selection sound. Override per-widget for custom cues. */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GTAI|UI")
    void PlayFocusSound() const;

    /** Called when DPI / application scale changes so cached geometry can rebuild. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    virtual void RebuildCachedGeometry() {}

    /** Convenience accessor for the UI manager. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    UGTAIUIManager* GetUIManager() const;

protected:
    /** If true, this widget wraps its root in a SafeZone (default true). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GTAI|UI")
    bool bRespectSafeArea = true;

    /** Sound played on focus gain (keyboard/gamepad). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GTAI|Audio")
    TObjectPtr<USoundBase> FocusSound;
};
