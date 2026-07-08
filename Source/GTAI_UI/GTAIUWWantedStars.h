// GTAIUWWantedStars.h
// Wanted level: 0-5 stars with empty/partial/full brushes and a scale-pop
// animation on level-up. See design doc 2.4. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWWantedStars.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWWantedStars : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UHorizontalBox> StarContainer;

    /** Fractional level (0-5) for smooth GTA-style fade-in. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|UI|HUD")
    void SetWantedLevel(float Level);

    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    TObjectPtr<UTexture2D> StarFull;

    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    TObjectPtr<UTexture2D> StarEmpty;

protected:
    UPROPERTY(EditAnywhere, Category = "GTAI|UI|HUD")
    int32 MaxStars = 5;

    float CurrentLevel = 0.f;
};
