// GTAIUWPauseMenu.h
// Pause menu: glassmorphic overlay + activatable back-stack. Resume / Map /
// Phone / Settings / Save / Quit. Game ticks paused while open.
// See design doc 5.1. Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWPauseMenu.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWPauseMenu : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnActivated() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> ResumeButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> MapButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> PhoneButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> SettingsButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> SaveButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> QuitButton;

protected:
    UFUNCTION() void OnResume();
    UFUNCTION() void OnOpenMap();
    UFUNCTION() void OnOpenPhone();
    UFUNCTION() void OnOpenSettings();
    UFUNCTION() void OnSave();
    UFUNCTION() void OnQuit();
};
