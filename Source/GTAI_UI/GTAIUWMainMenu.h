// GTAIUWMainMenu.h
// Title / main menu: Continue / New Game / Load / Options / Credits over an
// animated NYC skyline backdrop. See design doc 5.3. Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWMainMenu.generated.h"

UCLASS()
class GTAIUI_API UGTAIUWMainMenu : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> ContinueButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> NewGameButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> LoadButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> OptionsButton;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UGTAIButton> CreditsButton;

protected:
    UFUNCTION() void OnContinue();
    UFUNCTION() void OnNewGame();
    UFUNCTION() void OnLoad();
    UFUNCTION() void OnOptions();
    UFUNCTION() void OnCredits();
};
