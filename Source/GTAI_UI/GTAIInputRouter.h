// GTAIInputRouter.h
// Thin wrapper over CommonUI's action router. Decides, per active UI layer,
// whether navigation uses directional FOCUS (menus) or a FREE synthetic cursor
// (phone/map). Also pushes/pops Enhanced Input contexts so gameplay actions
// are suppressed while a modal UI layer is active (design doc section 6).
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GTAI_UI.h"
#include "GTAIInputRouter.generated.h"

UENUM(BlueprintType)
enum class EGTAIUINavigationMode : uint8
{
    Focus,        // directional focus navigation (menus)
    SyntheticCursor // free-moving cursor via gamepad (phone/map)
};

UCLASS()
class GTAIUI_API UGTAIInputRouter : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with the owning player controller. */
    void Init(APlayerController* PC);

    /** Enter a UI layer with the given navigation style; suppress gameplay input. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Input")
    void EnterUIMode(EGTAIUINavigationMode Mode);

    /** Exit UI mode, restore gameplay input. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Input")
    void ExitUIMode();

    EGTAIUINavigationMode GetCurrentMode() const { return CurrentNavMode; }

    /** Convert a gamepad right-stick / mouse delta into a wedge/angle selection.
     *  Returns angle in radians; deadzone applied. (Weapon wheel, radar waypoint.) */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Input")
    static float VectorToHeading(const FVector2D& Direction, float Deadzone = 0.2f);

protected:
    UPROPERTY()
    TObjectPtr<APlayerController> PlayerController;

    EGTAIUINavigationMode CurrentNavMode = EGTAIUINavigationMode::Focus;

    // Handle to the UI-only Enhanced Input context we push while in UI mode.
    FModifiedInputContextStack InputContextHandle;
};
