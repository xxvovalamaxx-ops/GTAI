// GTAIUWAppBase.h
// Base class for every phone app screen (Contacts, Messages, Map, Camera,
// Settings, Social, ...). All apps are UCommonActivatableWidget children of
// the phone root, giving a uniform back-stack. See design doc 3.4.
// Under namespace GTAI::UI.
#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GTAI_UI.h"
#include "GTAIUWAppBase.generated.h"

UCLASS(Abstract, Blueprintable)
class GTAIUI_API UGTAIUWAppBase : public UCommonActivatableWidget
{
    GENERATED_BODY()
public:
    /** Stable id used for dock/badge mapping (e.g. "Messages"). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Phone")
    FName AppId;

    /** Display name shown under the icon. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Phone")
    FText AppDisplayName;

    /** Icon for the home-grid/dock. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|UI|Phone")
    TSoftObjectPtr<UTexture2D> AppIcon;

    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
};
