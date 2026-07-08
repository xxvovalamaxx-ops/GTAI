// GTAIButton.h
// Custom button with an explicit Focused visual state + focus/click sounds.
// Addresses the long-standing UMG gap where keyboard/gamepad focus was
// indistinguishable from mouse hover. Keyboard focus and mouse hover share
// the "highlighted" style for consistency (per design doc 6.2).
#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GTAI_UI.h"
#include "GTAIButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGTAIButtonFocusSignature);

UCLASS()
class GTAIUI_API UGTAIButton : public UButton
{
    GENERATED_BODY()

public:
    UGTAIButton();

    // UWidget interface.
    virtual void SynchronizeProperties() override;

    // Focus state (driven by navigation, not just hover).
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    bool IsFocused() const { return bIsFocused; }

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI")
    void SetFocused(bool bInFocused);

    /** Raised when this button gains keyboard/gamepad focus. */
    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI")
    FGTAIButtonFocusSignature OnFocusGained;

    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI")
    FGTAIButtonFocusSignature OnFocusLost;

protected:
    virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|Style")
    FSlateBrush FocusedBrush;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|Audio")
    TObjectPtr<USoundBase> FocusSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|Audio")
    TObjectPtr<USoundBase> ClickSound;

    bool bIsFocused = false;
};
