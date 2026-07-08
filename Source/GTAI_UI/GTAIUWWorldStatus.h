// GTAIUWWorldStatus.h
// Clock + weather strip (top-right). Bound to UGTAIViewModel_World.
// See design doc 2.9. Under GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "GTAIUserWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWWorldStatus.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWWorldStatus : public UGTAIUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> ClockLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> WeatherLabel;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UImage> WeatherIcon;
};
