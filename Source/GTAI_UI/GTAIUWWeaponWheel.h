// GTAIUWWeaponWheel.h
// Radial weapon-selection menu. Custom Slate wedges; selection by analog-stick
// angle (gamepad) or mouse-position angle (keyboard). Soft-pauses gameplay via
// the UI activation stack. See design doc 2.6 & 6.3. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWWeaponWheel.generated.h"

class SGTAIWeaponWheel;

USTRUCT(BlueprintType)
struct FGTAIWeaponWheelSlot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WeaponSlot = 0;
};

UCLASS()
class GTAIUI_API UGTAIUWWeaponWheel : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Wheel")
    TArray<FGTAIWeaponWheelSlot> Slots;

    /** Open/close with radial reveal animation. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Wheel")
    void Open();

    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Wheel")
    void Close();

    /** Feed analog direction (gamepad stick or mouse delta from center). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|Wheel")
    void SetSelectionDirection(const FVector2D& Dir);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAIOnWeaponSelected, int32, WeaponSlot);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|UI|Wheel")
    FGTAIOnWeaponSelected OnWeaponSelected;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    TSharedPtr<SGTAIWeaponWheel> MyWheel;
    int32 HighlightedIndex = 0;
    bool bIsOpen = false;

    /** Owning storage for resolved slot icon brushes (kept alive for Slate). */
    UPROPERTY()
    TArray<FSlateBrush> ResolvedBrushes;
};
